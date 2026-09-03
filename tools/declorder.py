#!/usr/bin/env python3
"""Sweep the ORDER of a function's leading function-scope declaration block. A/B each permutation.

⭐ THE LEVER (lesson #38, and the v108 gap this tool closes). `hoisttest.py` asks "should this
inner-block local be HOISTED?" — a yes/no question per declaration. It never permutes the decls
that are ALREADY at function scope, yet that order is itself a register-allocation dial:

  ParseTilesMaybe 0x41a030 (v108) sat at DIFF(3) for the whole project. Every loop-form spelling
  measured 3 B dead flat (lesson #40 refuted) and the compare's operand order was inert — but
  simply declaring `int i;` BEFORE `int n = nBytes / 0x404;` made it byte-exact. Every order with
  `n` before `i` gives 40-43 B. Nothing else changed.

  Precedent: PlaceZoneObjectTiles (v104) is exact with `Zone *z` before `ZoneObj *o` and 22 B
  after; SetCurrentToIntroZone (v107) needs `i` before `pZone`.

⚠ A FLAT sweep from hoisttest.py or a loop-form probe is NOT a dead end — run this before parking.
The three dials compose: decl SCOPE (#37), decl SET+ORDER (#38), loop FORM (#40).

Measurement is delegated to tools/vartest.py verbatim — this only GENERATES the spellings, so the
anchor's exact predicate and the --expect baseline guard are inherited, not re-derived (the
v100/v101 rule: a harness must agree with progress.py at zero perturbation).

Permutations are LINE-NEUTRAL by construction: the block keeps its original line count (blank
lines are re-appended), so lesson #23 can't confound the measurement.

Usage:
    tools/declorder.py <tu.cpp> <0xADDR> --expect N [--max-perm K] [--keep]

--expect N is REQUIRED (get it from tools/bytediff.py). K caps the permutation count (default
120); with more decls than that fits, a random-free deterministic subset (adjacent swaps plus
full reversal) is used instead.
"""
import os, re, sys, itertools, subprocess, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

TYPES = (r"int|short|char|long|float|double|void|BOOL|UINT|DWORD|WORD|BYTE|LONG|"
         r"[A-Z]\w+")
# a whole single-line declaration, capturing the declared name for labelling.
# The trailing declarator is either an ARRAY extent (`char buf[32];` — v109: these were
# silently unmatched, and because a buffer is so often the FIRST local the block detector
# bailed on line 1 and reported "no permutable block" for 9 residuals, three of them the
# SaveStoryHistory* family) or an optional initializer.
DECL = re.compile(r"^\s+(?:const\s+)?(?:unsigned\s+|signed\s+)?(?:%s)[\s\*]+(\w+)"
                  r"(?:\s*\[[^\];]*\])*"
                  r"(?:\s*=[^;]*)?;\s*$" % TYPES)
# things that end the leading decl block even though they look declaration-ish
BAD = re.compile(r"\breturn\b|\bcase\b|\bgoto\b|::|\(\s*\)|\w+\s*\(")


def leading_block(body):
    """[(line_index, name)] for the run of declarations at the top of the body."""
    out = []
    for k in range(1, len(body)):
        s = body[k].split("//")[0].rstrip()
        if not s.strip():
            if out: break                     # blank line ends the block
            continue
        m = DECL.match(s)
        if not m:
            break
        if BAD.search(s) and "=" not in s:     # a call-shaped line is not a decl
            break
        if "," in s.split("=")[0]:             # multi-declarator: not safely permutable
            break
        out.append((k, m.group(1)))
    return out


def main():
    args = sys.argv[1:]
    expect = None
    maxp = 120
    keep = "--keep" in args
    args = [a for a in args if a != "--keep"]
    for flag in ("--expect", "--max-perm"):
        if flag in args:
            i = args.index(flag)
            v = int(args[i + 1], 0); del args[i:i + 2]
            if flag == "--expect": expect = v
            else: maxp = v
    if len(args) != 2 or expect is None:
        raise SystemExit(__doc__)
    cpp, addr = args[0], args[1]
    path = cpp if os.path.isabs(cpp) else os.path.join(ROOT, cpp)

    lines = open(path, encoding="utf-8").read().split("\n")
    mk = "0x%08x" % int(addr, 16)
    i = next(k for k, l in enumerate(lines) if re.search(r"FUNCTION:\s*YODA\s+" + mk, l))
    o = next(k for k in range(i, len(lines)) if lines[k] == "{")
    j = next(k for k in range(o, len(lines)) if lines[k] == "}")
    body = lines[o:j + 1]

    blk = leading_block(body)
    if len(blk) < 2:
        raise SystemExit("%s has %d leading function-scope decl(s) — nothing to permute"
                         % (mk, len(blk)))
    idx = [k for k, _ in blk]
    print("leading decl block (%d):" % len(blk))
    for k, n in blk:
        print("   %-14s  %s" % (n, body[k].strip()[:66]))

    n = len(blk)
    perms = list(itertools.permutations(range(n)))
    if len(perms) > maxp:                      # deterministic fallback: swaps + reversal
        perms = [tuple(range(n))]
        for a in range(n - 1):
            p = list(range(n)); p[a], p[a + 1] = p[a + 1], p[a]; perms.append(tuple(p))
        for a in range(n):                     # each decl rotated to the front and to the back
            p = [x for x in range(n) if x != a]
            perms.append(tuple([a] + p)); perms.append(tuple(p + [a]))
        perms.append(tuple(reversed(range(n))))
    base_order = tuple(range(n))

    seen, variants = set(), []
    for p in perms:
        if p == base_order:
            continue                           # vartest measures the unmodified body as BASELINE
        b = list(body)
        for slot, src in zip(idx, p):
            b[slot] = body[idx[src]]
        text = "\n".join(b)
        if text in seen:
            continue
        seen.add(text)
        variants.append((",".join(blk[s][1] for s in p)[:34], text))
    print("\n%d permutations\n" % len(variants))

    # NOT under tools/ — an interrupted run would leave a stray tmp*.py in the repo
    vf = tempfile.NamedTemporaryFile("w", suffix=".py", prefix="yoda_declord_", delete=False)
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
