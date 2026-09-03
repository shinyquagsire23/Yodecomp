#!/usr/bin/env python3
"""The ANCHOR's definition of "exact", for ONE function — reloc-masked BYTE compare.

v100 lesson ("THE INSTRUMENT ITSELF CAN LIE"): asmscore/idiomscan judge by DISASSEMBLY, which
decodes an embedded switch JUMP TABLE as instructions and reports a phantom byte_diff on
provably byte-exact code. progress.py's masked byte compare is the real oracle. Run THIS
before investing any time in a residual.

Usage:  tools/bytediff.py <src.cpp> [0xADDR ...]        (no addr = every marker in the TU)
        tools/bytediff.py --all [0xADDR ...]            (search every TU for the addr)

Prints, per function: EXACT, or the diff-offset list + a hexdump of the differing runs.
Exit status 0 if every requested function is exact, 1 otherwise.
"""
import os, sys, glob, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match, verify
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = prog.EXE


def paired(cpp):
    """[(va, name, code, relocs)] for one TU — IDENTICAL filtering/pairing to progress.py."""
    obj = prog.compile_obj(cpp)
    if not obj:
        sys.stderr.write("COMPILE FAILED: %s\n" % cpp)
        return []
    text = open(cpp).read()
    hinted = set(re.findall(
        r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
    funcs = [f for f in match.coff_functions(obj)
             if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                 or any(h in f[0] for h in hinted))
             and not f[0].lstrip("?").startswith(("_$E", "$E"))]
    return match.pair_by_name(text, funcs)


def runs(offs):
    """Collapse a sorted offset list into [(start, end_exclusive)] runs (gap <= 3 merges)."""
    out = []
    for o in offs:
        if out and o - out[-1][1] <= 3:
            out[-1][1] = o + 1
        else:
            out.append([o, o + 1])
    return [(a, b) for a, b in out]


def report(va, name, code, relocs, verbose=True):
    L = match.trim_pad(code)
    foff = (va - match.TEXT_VA) + match.TEXT_RAW
    orig = EXE[foff:foff + L]
    cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
    offs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
    exact = not offs and len(orig) == L
    print("%#010x  %-52s %4d B  %s" % (
        va, name[:52], L, "EXACT" if exact else "DIFF %d byte(s)" % len(offs)))
    if not exact and verbose:
        if len(orig) != L:
            print("        length mismatch: ours %d vs original %d" % (L, len(orig)))
        for a, b in runs(offs):
            lo, hi = max(0, a - 4), min(L, b + 4)
            print("        @+0x%03x  orig %s" % (a, om[lo:hi].hex(" ")))
            print("                 ours %s" % cm[lo:hi].hex(" "))
    return exact


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    if args[0] == "--all":
        cpps = sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp")))
        args = args[1:]
    else:
        cpps = [args[0] if os.path.exists(args[0]) else os.path.join(ROOT, args[0])]
        args = args[1:]
    want = {int(a, 16) for a in args}
    ok, seen = True, 0
    for cpp in cpps:
        rows = [r for r in paired(cpp) if not want or r[0] in want]
        if not rows:
            continue
        print("=== %s" % os.path.relpath(cpp, ROOT))
        for va, name, code, relocs in rows:
            seen += 1
            ok &= report(va, name, code, relocs)
    if want and seen < len(want):
        sys.stderr.write("WARN: %d of %d requested addresses had no marker\n" % (
            len(want) - seen, len(want)))
    return 0 if ok else 1


sys.exit(main())
