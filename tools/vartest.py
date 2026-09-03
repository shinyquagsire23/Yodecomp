#!/usr/bin/env python3
"""A/B a batch of SOURCE SPELLINGS for ONE function against the anchor's byte oracle.

This is the instrument that found the v102 wins (CWnd::SendMessage member form -> +3 exact;
LoadZoneRecursive assign-in-condition -> 7 B to 1 B). The method it supports:

  Don't reason about the schedule — enumerate the plausible ways the 1997 author could have
  SPELLED the statement, compile each, and let the reloc-masked byte diff pick. Codegen folds
  most spellings together (that is itself the finding: "source-inert"), so the one that moves
  is real evidence about the original source.

⭐ BASELINE RULE (v100/v101 — the reason three harnesses lied for months): a measurement tool
must AGREE WITH THE ANCHOR AT ZERO PERTURBATION before any of its deltas mean anything. This
tool enforces that itself: it always measures the UNMODIFIED body first, prints it as BASELINE,
and with --expect N HARD-FAILS if that number isn't the known residual for the function (get N
from `tools/residuals.py` or `tools/bytediff.py`). Always pass --expect.

It shares progress.py's compile_obj + the COMDAT filter/pairing block verbatim, so "exact" here
is the same predicate progress.py counts. Never re-derive that block — copy it.

⚠ MUTATES the source file. Restores on every exit path (atexit + finally) and leaves a .bak if
a restore ever fails. Never run two of these concurrently, or one while progress.py is in
flight: they fight over the source file AND build/*.obj.

Usage:
    python3 tools/vartest.py <tu.cpp> <0xADDR> <variants.py> [--expect N]

The variants file is plain Python defining:
    BASE     = the exact current source text to replace (must appear VERBATIM, exactly once)
    VARIANTS = [(name, replacement_text), ...]
Keep replacements LINE-NEUTRAL where you can — adding/removing lines mid-TU can rotate the
codegen dial on its own (lesson #23), which would confound the measurement.
"""
import sys, os, re, atexit, importlib.util

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, verify, progress as prog


def usage():
    raise SystemExit(__doc__)


def main():
    args = [a for a in sys.argv[1:]]
    expect = None
    if "--expect" in args:
        i = args.index("--expect")
        expect = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 3:
        usage()
    cpp, addr, vfile = args[0], int(args[1], 0), args[2]
    cpp = cpp if os.path.isabs(cpp) else os.path.join(ROOT, cpp)

    spec = importlib.util.spec_from_file_location("variants", vfile)
    V = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(V)

    ORIG = open(cpp).read()
    if ORIG.count(V.BASE) != 1:
        raise SystemExit("BASE must appear EXACTLY once in %s (found %d)"
                         % (cpp, ORIG.count(V.BASE)))

    def restore():
        try:
            if open(cpp).read() != ORIG:
                open(cpp, "w").write(ORIG)
        except Exception:
            bak = cpp + ".bak"
            open(bak, "w").write(ORIG)
            sys.stderr.write("!! RESTORE FAILED — original saved to %s\n" % bak)
    atexit.register(restore)

    EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()

    def measure():
        """Return (our_len, orig_len, ndiff) for `addr`, using progress.py's exact predicate."""
        obj = prog.compile_obj(cpp)
        if not obj:
            return None
        text = open(cpp).read()
        # ⚠ keep a lib-owned COMDAT a marker EXPLICITLY names by mangled hint, or its marker
        # falls back POSITIONALLY and cascades mis-pairs through the TU (v100). Same block as
        # progress.py / verify.py / dialsweep.py / exactset.py — keep all five in sync.
        hinted = set(re.findall(
            r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
        funcs = [f for f in match.coff_functions(obj)
                 if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                     or any(h in f[0] for h in hinted))
                 and not f[0].lstrip("?").startswith(("_$E", "$E"))]
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            if va != addr:
                continue
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            nd = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            return (L, len(orig), nd)
        return None

    rc = 0
    try:
        for i, (name, body) in enumerate([("BASELINE", V.BASE)] + list(V.VARIANTS)):
            open(cpp, "w").write(ORIG.replace(V.BASE, body))
            r = measure()
            if r is None:
                print("%-34s COMPILE FAILED" % name)
                sys.stdout.flush()
                continue
            L, OL, nd = r
            exact = (nd == 0 and L == OL)
            print("%-34s len=%-5d origlen=%-5d diff=%-4d%s"
                  % (name, L, OL, nd, "  *** EXACT ***" if exact else ""))
            sys.stdout.flush()
            if i == 0 and expect is not None and nd != expect:
                print("\n!! BASELINE MISMATCH: measured %d, --expect %d.\n"
                      "!! The harness disagrees with the anchor at zero perturbation, so every\n"
                      "!! delta below would be untrustworthy (v100/v101). Fix this before reading\n"
                      "!! any result: check the COMDAT filter + match.pair_by_name usage against\n"
                      "!! progress.py, and confirm N with tools/bytediff.py." % (nd, expect))
                rc = 2
                break
    finally:
        restore()
        print("[restored %s]" % os.path.relpath(cpp, ROOT))
    return rc


if __name__ == "__main__":
    sys.exit(main())
