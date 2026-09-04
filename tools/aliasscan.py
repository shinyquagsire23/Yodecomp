#!/usr/bin/env python3
"""Target list for the MEMBER-ALIAS dial (v118, lesson #50).

A cached local alias for a POINTER MEMBER --- `CDeskcppDoc *pW = pWorld;` --- is not
semantically neutral to cl 10.20.  The original reloads the member after any store made
through it (the store MAY alias the member itself), but a local held in a register can
never be invalidated, so the alias suppresses the reload and our code comes out SHORT.
That is what `AddHealth` 0x427690 was: a single missing `mov ecx,[edi+0x44]`, worth 3
bytes of LENGTH and the whole 49-byte residual.

This scan reports every NON-EXACT function whose source declares such an alias.  A hit is
a CANDIDATE, not a defect: dropping the alias also perturbs the decl set, so the fix is
"drop the alias AND sweep the inner block's decl order" (lesson #38).  Read it together
with `residuals.py --lenmis` --- a hit that is also SHORT of its Ghidra extent is the
strong form of the signal.

READ-ONLY: no compile, no build/*.obj (it reads a CACHED exact set), so it is safe to run
while a `vartest.py` / `formsweep.py` sweep is in flight.

Usage:
    python3 tools/exactset.py > /tmp/exact.txt
    python3 tools/aliasscan.py --exact /tmp/exact.txt
    python3 tools/aliasscan.py --all        # every marker, exact ones included
"""
import sys, os, re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match

MARKER = re.compile(r"//\s*FUNCTION:\s*YODA\s+(0x[0-9a-fA-F]+)")
# `T *name = <member-ish expression>;`  -- the RHS must be a bare member/this-chain, not a
# call, an index or an arithmetic expression (those are ordinary temporaries, not aliases).
ALIAS = re.compile(
    r"^\s*(?:const\s+)?(\w+)\s*\*\s*(\w+)\s*=\s*"
    r"((?:this->)?[A-Za-z_]\w*(?:->[A-Za-z_]\w*)*)\s*;")


def extents():
    """Ghidra's function extents, for the LENGTH cross-check."""
    out = {}
    p = os.path.join(ROOT, "toolchain/test/app_funcs.txt")
    for ln in open(p):
        f = ln.split()
        if len(f) >= 2:
            try:
                out[int(f[0], 16)] = int(f[1], 0)
            except ValueError:
                pass
    return out


def main():
    argv = sys.argv[1:]
    show_all = "--all" in argv
    exact, path = set(), None
    if not show_all:
        for k, a in enumerate(argv):
            if a == "--exact":
                path = argv[k + 1]
        if not path:
            raise SystemExit(
                "give --exact <file> (from `python3 tools/exactset.py > file`) to filter to\n"
                "non-exact residuals, or --all to list every marker.")
        for line in open(path):
            if line.strip():
                exact.add(int(line.split()[0], 16))
        # positive control (v103/v109: an empty result is a bug hypothesis, not a finding)
        print("  control exact set = %d from %s (0x429150 exact? %s - expected True)"
              % (len(exact), path, 0x429150 in exact))

    ext = extents()
    hits, scanned, alias_total = [], 0, 0
    lengths = {}
    for fn in sorted(os.listdir(os.path.join(ROOT, "src"))):
        if not fn.endswith(".cpp"):
            continue
        lines = open(os.path.join(ROOT, "src", fn)).read().split("\n")
        cur = None
        for i, ln in enumerate(lines):
            m = MARKER.search(ln)
            if m:
                cur = int(m.group(1), 16)
                scanned += 1
                continue
            if cur is None:
                continue
            if ln.startswith("}"):          # end of a top-level definition
                cur = None
                continue
            a = ALIAS.match(ln)
            if not a:
                continue
            alias_total += 1
            if cur in exact and not show_all:
                continue
            L, E = lengths.get(cur), ext.get(cur)
            delta = (L - E) if (L and E and E > 1) else None
            hits.append((fn, cur, a.group(2), a.group(3), L, E, delta,
                         cur in exact))

    print("# NON-EXACT functions holding a cached MEMBER ALIAS (lesson #50)")
    print("# cross-check each hit against `residuals.py --lenmis`: a hit that is also")
    print("# SHORT of its Ghidra extent is the strong form (that is AddHealth 0x427690)")
    print("%-22s %-12s %-10s %-24s %8s" %
          ("TU", "addr", "alias", "= member", "extent"))
    hits.sort(key=lambda h: (h[0], h[1]))
    for fn, va, nm, rhs, L, E, d, ex in hits:
        print("%-22s 0x%08x %-10s %-24s %8s%s" %
              (fn, va, nm, rhs, E or "-", "  EXACT" if ex else ""))
    print("\npositive control: %d markers scanned, %d alias decls found, "
          "%d exact functions" % (scanned, alias_total, len(exact)))
    print("%d hit(s) in non-exact functions" % len([h for h in hits if not h[7]]))


if __name__ == "__main__":
    main()
