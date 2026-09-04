#!/usr/bin/env python3
"""Target list for LESSON #43 — non-exact functions passing a POINTER CHAIN as an argument.

⭐ THE LESSON (v110). When the original saves MORE callee-saved registers than we do, it kept a
value alive across a call that we spell as an ARGUMENT EXPRESSION. Giving that expression a NAME
and assigning it BEFORE the neighbouring call reproduces the original's allocation:

    CheckCheat  0x415820   372 B -> 0     int x = pWorld->playerX * LOCATOR_CELL_SIZE + 18;
    ConfirmExit 0x416030    10 B -> 0     CWinApp *pApp = AfxGetApp();

The dual (lesson #42) also shows up here: an argument that is a pointer chain
(`::GetNearestPaletteIndex((HPALETTE)pWorld->pPalette->m_hObject, ...)`) lets cl keep the ROOT of
the chain alive in a callee-saved register as a CSE. The MFC member form forces the reload —
that is what took DrawTextA from 663 B to 60 B. So a hit here is a candidate for EITHER lever.

⚠ This is a TARGET LIST, not a verdict. Confirm each hit with tools/bytediff.py, then measure
spellings with tools/vartest.py --expect N. `savescan.py` says WHETHER the save set is wrong;
this says WHERE in the source to look.

⭐ POSITIVE CONTROL (v103 — the rule that ad-hoc scans keep breaking). An empty result from a
scan you just wrote is a bug hypothesis, not a finding. This tool always prints how many
residuals it parsed and how many bodies it located, and exits non-zero if either is zero.

Usage:  tools/chainscan.py [--max-diff N] [--min-diff N] [--lines] [--tu FILE]
"""
import os, re, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import residuals as R

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Chain shapes worth a named local. ⚠ The position is NOT restricted to argument lists: both
# v110 wins were elsewhere — ConfirmExit's `CWinApp *pApp = AfxGetApp();` is a call RECEIVER and
# `CWinThread *pT = (CWinThread *)pMusicThread;` is an assignment RHS. Each hit is tagged with
# where it sits (arg / recv / other) so the argument-position ones can still be ranked first.
PATTERNS = [
    ("call()->",  re.compile(r"\b\w+\s*\([^()]*\)\s*->")),          # f()->m(...)
    # BOTH cast spellings: bare `(T *)p->m` and the parenthesized `((T *)p)->m`. The optional
    # wrapping parens were missing at first, which hid every hit of the canonical form.
    ("cast()->",  re.compile(r"\(?\s*\(\s*\w+\s*\*+\s*\)\s*\w+\s*(?:\[[^\]]*\])?\s*\)?\s*->")),
    ("a->b->c",   re.compile(r"->\s*\w+\s*(?:\[[^\]]*\])?\s*->")),  # a->b->c
    ("->m_hXxx",  re.compile(r"->\s*m_h\w+")),                      # a raw HANDLE
]
# a call whose arguments we care about: `Name(` or `->Name(` or `::Name(`, not a keyword
KEYWORDS = {"if", "for", "while", "switch", "return", "sizeof", "catch", "TRY"}


def _arg_spans(line):
    """[(start, end)] character spans of the argument lists of calls on `line`."""
    spans, i = [], 0
    while True:
        m = re.search(r"(\w+)\s*\(", line[i:])
        if not m:
            return spans
        if m.group(1) in KEYWORDS:
            i += m.end()
            continue
        start = i + m.end()                 # just past the '('
        depth, j = 1, start
        while j < len(line) and depth:
            if line[j] == "(":
                depth += 1
            elif line[j] == ")":
                depth -= 1
            j += 1
        spans.append((start, j - 1))
        i = start


def _in_arglist(line, pos):
    return any(a <= pos < b for a, b in _arg_spans(line))


def body_of(cpp, va):
    """Source lines of the function marked `// FUNCTION: YODA <va>`, or None."""
    try:
        lines = open(os.path.join(ROOT, cpp)).read().split("\n")
    except IOError:
        return None
    tag = "// FUNCTION: YODA 0x%08x" % va
    for i, l in enumerate(lines):
        if l.strip().lower().startswith(tag.lower()):
            break
    else:
        return None
    for j in range(i, min(i + 40, len(lines))):     # opening brace of the definition
        s = lines[j].split("//")[0].rstrip()
        if s.endswith("{") and not lines[j].lstrip().startswith("//"):
            break
    else:
        return None
    depth, out = 0, []
    for k in range(j, len(lines)):
        s = lines[k]
        out.append((k + 1, s))
        t = re.sub(r'"[^"]*"|\'[^\']*\'', "", s.split("//")[0])
        depth += t.count("{") - t.count("}")
        if depth <= 0 and k > j:
            break
    return out


def _position(s, m):
    """Where the chain sits: 'arg' (inside a call's argument list), 'recv' (the object a call
    is made on), or 'other' (assignment RHS, condition, ...). All three are lesson-#43 shapes."""
    if _in_arglist(s, m.start()):
        return "arg"
    if re.match(r"\s*\w+\s*\(", s[m.end():]):
        return "recv"
    return "other"


def hits_in(body):
    """{pattern name: [(lineno, position, text)]} for chain shapes in `body`."""
    found = {}
    for lineno, raw in body:
        s = raw.split("//")[0]
        for name, rx in PATTERNS:
            for m in rx.finditer(s):
                found.setdefault(name, []).append((lineno, _position(s, m), raw.strip()))
                break
    return found


def main():
    a = sys.argv[1:]

    def opt(flag, default=None, cast=str):
        if flag in a:
            i = a.index(flag)
            v = cast(a[i + 1]); del a[i:i + 2]; return v
        return default

    lo = opt("--min-diff", 0, int)
    hi = opt("--max-diff", 10 ** 9, int)
    tu = opt("--tu")
    show = "--lines" in a

    rows = [r for r in R.scan() if lo <= r["ndiff"] <= hi]
    if tu:
        rows = [r for r in rows if os.path.basename(r["cpp"]) == os.path.basename(tu)]
    print("residuals parsed: %d  (diff %d..%d)" % (len(rows), lo, hi))
    if not rows:
        sys.stderr.write("NO RESIDUALS PARSED — suspect the harness, not the source (v103).\n")
        return 2
    print("  e.g. %s 0x%08x ndiff=%d" % (rows[0]["cpp"], rows[0]["va"], rows[0]["ndiff"]))

    located = 0
    out = []
    for r in rows:
        body = body_of(r["cpp"], r["va"])
        if body is None:
            continue
        located += 1
        h = hits_in(body)
        if h:
            out.append((r, h))
    print("bodies located: %d/%d   with chain-shaped args: %d\n"
          % (located, len(rows), len(out)))
    if not located:
        sys.stderr.write("NO BODIES LOCATED — the marker/brace scan is broken (v103).\n")
        return 2

    print("%-24s %-12s %5s %5s  %s" % ("TU", "addr", "ndif", "len", "shapes"))
    print("-" * 92)
    for r, h in out:
        shapes = ", ".join("%s x%d" % (k, len(v)) for k, v in sorted(h.items()))
        print("%-24s 0x%08x %5d %5d  %s"
              % (r["cpp"], r["va"], r["ndiff"], r["L"], shapes))
        if show:
            for k, v in sorted(h.items()):
                for lineno, pos, text in v:
                    print("        %-10s %-5s %5d  %s" % (k, pos, lineno, text[:66]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
