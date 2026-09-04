#!/usr/bin/env python3
"""ONE-AT-A-TIME container-call-form sweep over a TU (lesson #48).

Why this and not jointdecl.py: jointdecl takes the CARTESIAN PRODUCT of its edits, so
probing k candidate functions costs 2**k compiles. Lesson #48's discipline is to ISOLATE
each function before landing anything, which needs only k+1. This applies each edit ALONE
against the pristine TU and prints the whole TU's marker vector for it, so the collateral
damage (v116: DrawRect) is visible in the same row.

It reuses jointdecl.measure() VERBATIM, so "exact" here is the predicate progress.py counts
(v100/v101 — never re-derive that block). Baseline is measured first and --expect-exact
HARD-FAILS if the TU's exact count disagrees with the anchor.

⭐ LENGTH IS REPORTED AGAINST GHIDRA'S EXTENT (toolchain/test/app_funcs.txt), NOT against
jointdecl.measure()'s `orig_len`. That field is VACUOUS as a length oracle — measure() slices
`EXE[foff:foff+L]` with L = our own trimmed length, so orig_len == L by construction and a
`L == OL` test can never fail. Lesson #46/#48 want our length vs the ORIGINAL's extent, which
is a genuinely independent signal; a conversion that cuts diff while moving length AWAY from
the extent is a number, not a fact. (Same family as the v100/v109/v111 harness bugs: a column
that cannot ever disagree is not a measurement.)

⚠ MUTATES the source file; restores on every exit path. Never run concurrently with
progress.py / exactset.py / residuals.py / another sweep — they share build/*.obj.

Usage:  python3 tools/formsweep.py <spec.py> --expect-exact N

Spec is plain Python:
    TU    = "src/Worldgen.cpp"
    EDITS = [(label, BASE_TEXT, NEW_TEXT), ...]     # each BASE unique in the TU
      or (label, [(BASE, NEW), ...])                # several sites applied TOGETHER
"""
import sys, os, atexit, importlib.util

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import jointdecl

EXTENT = {}
for _l in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
    _p = _l.split()
    if len(_p) == 2:
        EXTENT[int(_p[0], 16)] = int(_p[1])


def main():
    args = sys.argv[1:]
    expect = None
    if "--expect-exact" in args:
        i = args.index("--expect-exact")
        expect = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 1:
        raise SystemExit(__doc__)

    spec = importlib.util.spec_from_file_location("spec", args[0])
    S = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(S)

    cpp = S.TU if os.path.isabs(S.TU) else os.path.join(ROOT, S.TU)
    ORIG = open(cpp).read()
    nl = ORIG.count("\n")

    def pairs_of(e):
        """(label, [(base, new), ...]) for either spec form."""
        return (e[0], list(e[1])) if len(e) == 2 else (e[0], [(e[1], e[2])])

    EDITS = [pairs_of(e) for e in S.EDITS]
    for label, ps in EDITS:
        for base, _ in ps:
            if ORIG.count(base) != 1:
                raise SystemExit("edit %r: BASE must appear EXACTLY once (found %d)\n%r"
                                 % (label, ORIG.count(base), base[:120]))

    def restore():
        try:
            if open(cpp).read() != ORIG:
                open(cpp, "w").write(ORIG)
        except Exception:
            open(cpp + ".bak", "w").write(ORIG)
            sys.stderr.write("!! RESTORE FAILED — original saved to %s.bak\n" % cpp)
    atexit.register(restore)

    order = base_diffs = None
    try:
        for label, ps in [("BASELINE", [])] + EDITS:
            text = ORIG
            for base, new in ps:
                text = text.replace(base, new)
            if text.count("\n") != nl:
                print("%-34s !! NOT LINE-NEUTRAL (%+d lines) — skipped (lesson #23)"
                      % (label, text.count("\n") - nl))
                continue
            open(cpp, "w").write(text)
            r = jointdecl.measure(cpp)
            if r is None:
                print("%-34s COMPILE FAILED" % label)
                sys.stdout.flush()
                continue
            nexact = sum(1 for _, _, L, OL, nd in r if nd == 0 and L == OL)
            diffs = {va: (nd, L, OL) for va, _, L, OL, nd in r}
            if order is None:
                order = [va for va, _, _, _, _ in r]
                base_diffs, base_exact = diffs, nexact
                print("# %s — %d markers, %d edits" % (S.TU, len(order), len(EDITS)))
                print("# markers (file order): " + " ".join("%#x" % v for v in order))
                print("%-34s exact=%-3d %s   <-- BASELINE"
                      % (label, nexact, " ".join("%5d" % diffs[v][0] for v in order)))
                off = [(v, diffs[v][1], EXTENT[v]) for v in order
                       if v in EXTENT and diffs[v][1] != EXTENT[v]]
                if off:
                    print("#   baseline LENGTH != Ghidra extent for %d marker(s): %s"
                          % (len(off), ", ".join("%#x %d/%d" % o for o in off)))
                if expect is not None and nexact != expect:
                    raise SystemExit(
                        "\n!! BASELINE MISMATCH: TU exact=%d, --expect-exact %d.\n"
                        "   Fix the expectation or the tool BEFORE trusting any delta "
                        "(v100/v101).\n" % (nexact, expect))
                sys.stdout.flush()
                continue
            # per-marker delta vs baseline; flag LENGTH changes (lesson #48 / #46)
            cells, notes = [], []
            for v in order:
                nd, L, OL = diffs[v]
                bnd, bL, bOL = base_diffs[v]
                cells.append("%5d" % nd)
                ext = EXTENT.get(v)
                if nd != bnd or (ext is not None and L != base_diffs[v][1]):
                    tag = ""
                    if ext is not None:
                        was, now = bL == ext, L == ext
                        if was != now or L != bL:
                            tag = " LEN %d->%d/ext %d %s" % (
                                bL, L, ext, "MATCHES" if now else "off by %+d" % (L - ext))
                    notes.append("%#x %d->%d%s" % (v, bnd, nd, tag))
            print("%-34s exact=%-3d %s  %s%s"
                  % (label, nexact, " ".join(cells),
                     "+%d" % (nexact - base_exact) if nexact > base_exact else
                     ("%d" % (nexact - base_exact) if nexact < base_exact else "  ="),
                     ("   " + "; ".join(notes)) if notes else ""))
            sys.stdout.flush()
    finally:
        restore()


if __name__ == "__main__":
    main()
