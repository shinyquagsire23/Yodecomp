#!/usr/bin/env python3
"""Sweep DECLARATION SCOPE for one function: hoist inner-block locals to function scope, A/B each.

⭐ THE LEVER (v104, lesson #37). cl 10.20 assigns registers differently depending on whether a
local is declared at FUNCTION scope or inside an inner block (loop body / if body / for-init),
even when the generated work is identical. A residual that is a pure REGISTER PERMUTATION
(esi<->edi, ebx<->ebp, ...) is the fingerprint: same instructions, same schedule, swapped
register names. Hoisting the right local flips the assignment and lands the match.

The 1997 author wrote C-style "declare all locals at the top", so the hoisted spelling is
usually the historically correct one — but it is NOT uniform (CDeskcppDoc::~CDeskcppDoc really
does scope `p` to the loop body: hoisting DOUBLES its residual). MEASURE, never assume.

Decl ORDER at function scope matters too (PlaceZoneObjectTiles: `Zone *z` before `ZoneObj *o`
is exact, after it is 22 B), so the sweep tries a couple of orderings for multi-hoist subsets.

Measurement is delegated to tools/vartest.py verbatim — this only GENERATES the spellings, so
the anchor's exact predicate and the --expect baseline guard are inherited, not re-derived
(the v100/v101 rule: a harness must agree with progress.py at zero perturbation).

Usage:
    tools/hoisttest.py <tu.cpp> <0xADDR> --expect N [--max-hoist K] [--keep]

--expect N is REQUIRED (get it from tools/bytediff.py). --keep leaves the generated variants
file for hand-editing + re-running through vartest directly.
"""
import os, re, sys, itertools, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPES = (r"int|short|char|long|float|double|void|BOOL|UINT|DWORD|WORD|BYTE|LONG|"
         r"[A-Z]\w+")
DECL = re.compile(r"^(\s+)((?:const\s+)?(?:unsigned\s+|signed\s+)?(?:%s)\s+\**)(\w+)"
                  r"(\s*=[^;]*)?;\s*$" % TYPES)
FORD = re.compile(r"^(\s+)for\s*\(\s*((?:unsigned\s+)?(?:%s)\s+\**)(\w+)(\s*=[^;]*);" % TYPES)


def candidates(body):
    """[(line_index, replacement_line, decl_text)] for every inner-block declaration."""
    out = []
    for k, line in enumerate(body):
        src = line.split("//")[0]
        m = FORD.match(src)
        if m:
            out.append((k, line.replace(m.group(2), "", 1), "%s%s;" % (m.group(2), m.group(3))))
            continue
        m = DECL.match(src)
        if m and len(m.group(1)) > 4 and m.group(4):        # only initialised decls are safe
            out.append((k, "%s%s%s;" % (m.group(1), m.group(3), m.group(4)),
                        "%s%s;" % (m.group(2), m.group(3))))
    return out


def main():
    args = sys.argv[1:]
    expect = maxh = None
    keep = "--keep" in args
    args = [a for a in args if a != "--keep"]
    for flag, cast in (("--expect", int), ("--max-hoist", int)):
        if flag in args:
            i = args.index(flag)
            v = cast(args[i + 1]); del args[i:i + 2]
            if flag == "--expect": expect = v
            else: maxh = v
    if len(args) != 2 or expect is None:
        raise SystemExit(__doc__)
    cpp, addr = args[0], args[1]
    path = cpp if os.path.isabs(cpp) else os.path.join(ROOT, cpp)

    lines = open(path).read().split("\n")
    # the function body = the marker's `{` at column 0 .. the matching `}` at column 0
    mk = "0x%08x" % int(addr, 16)
    i = next(k for k, l in enumerate(lines) if re.search(r"FUNCTION:\s*YODA\s+" + mk, l))
    o = next(k for k in range(i, len(lines)) if lines[k] == "{")
    j = next(k for k in range(o, len(lines)) if lines[k] == "}")
    body = lines[o:j + 1]

    cands = candidates(body)
    if not cands:
        raise SystemExit("no inner-block declarations in %s" % mk)
    print("candidates:")
    for k, _, d in cands:
        print("   %-28s  <- %s" % (d, body[k].strip()[:60]))

    first = next(k for k in range(1, len(body)) if body[k].strip())
    maxh = maxh or len(cands)

    subsets = []
    for r in range(1, min(maxh, len(cands)) + 1):
        subsets += list(itertools.combinations(range(len(cands)), r))
    if len(cands) > maxh:
        subsets.append(tuple(range(len(cands))))
    seen, variants = set(), []
    for sub in subsets:
        for order in ({"fwd": list(sub), "rev": list(reversed(sub))}.items()
                      if len(sub) > 1 else [("fwd", list(sub))]):
            tag, seq = order
            b = list(body)
            for k in sub:
                b[cands[k][0]] = cands[k][1]
            uniq = []
            for k in seq:
                if cands[k][2] not in uniq: uniq.append(cands[k][2])   # same name declared once
            decls = " ".join(uniq)
            b[first] = "    " + decls + " " + b[first].strip()
            text = "\n".join(b)
            if text in seen: continue
            seen.add(text)
            variants.append(("%s[%s]" % ("+".join(cands[k][2].rstrip(";").split()[-1]
                                                  .lstrip("*") for k in sub), tag), text))
    print("\n%d spellings\n" % len(variants))

    # NOT under tools/ — an interrupted run would leave a stray tmp*.py in the repo
    vf = tempfile.NamedTemporaryFile("w", suffix=".py", prefix="yoda_hoist_", delete=False)
    vf.write("BASE = %r\nVARIANTS = %r\n" % ("\n".join(body), variants))
    vf.close()
    try:
        rc = subprocess.call([sys.executable, os.path.join(ROOT, "tools", "vartest.py"),
                              cpp, addr, vf.name, "--expect", str(expect)])
    finally:
        if keep: print("[variants kept: %s]" % vf.name)
        else: os.unlink(vf.name)
    return rc


if __name__ == "__main__":
    sys.exit(main())
