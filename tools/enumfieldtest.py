#!/usr/bin/env python3
"""Do UNUSED ENUMERATORS move the codegen dial?  (v96 — direct test)

The v96 dial sweep found `enum` positions tracking `struct` positions at exactly 1:3, and each
generated enum carried 3 names (tag + 2 enumerators) against 1 name per struct. That IMPLIES
enumerators are counted as symbols — i.e. an enum's FIELD COUNT is a dial input, so an enum we
transcribed with the wrong number of enumerators is a quantified dial error.

This tests it directly instead of inferring it: hold the number of enum DECLARATIONS at exactly
one and vary how many enumerators it contains.

PREDICTION (falsifiable): one enum with k enumerators == k+1 plain symbols, so its exact-count
curve should equal the `struct n = k+1` curve measured by tools/dialsweep.py.
  - matches  => enumerators are dial-active; enum field count is load-bearing.
  - flat     => only the tag counts; the 1:3 ratio was a coincidence and the model is wrong.

Also runs two controls, because a tool that can only confirm itself is worthless:
  DETERMINISM  the same position compiled twice must give the identical exact set.
  NAMELEN      same symbol count, long identifiers vs short. If the curve moves, the dial is
               NOT a clean count (symbol-table bytes / hash occupancy) and source inference off
               a scalar is much weaker.

Usage:  tools/enumfieldtest.py [--tu src/Worldgen.cpp] [--header src/Worldgen.h] [--max 12]
The header is ALWAYS restored (atexit + finally).
"""
import os, sys, atexit, shutil

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dialsweep as ds

ROOT = ds.ROOT

# struct-n curve measured in v96 on src/Worldgen.cpp (baseline 34). Index = symbol count.
REFERENCE_STRUCT = {1: 33, 2: 32, 3: 34, 4: 35, 5: 33, 6: 35,
                    7: 36, 8: 35, 9: 36, 10: 35, 11: 34, 12: 34}


def enum_block(k, namelen=1):
    """ONE enum carrying k enumerators => k+1 names (tag + k fields)."""
    pad = "X" * max(0, namelen - 1)
    body = ", ".join("ENUMFIELD_%s%d" % (pad, i) for i in range(k))
    return "%s\nenum EnumFieldTest%s { %s };\n%s\n" % (ds.MARK, pad, body, ds.MARK) if k else ""


def plain_block(n, namelen=1):
    """n plain file-scope symbols with identifiers of the given length."""
    if n == 0:
        return ""
    pad = "X" * max(0, namelen - 1)
    lines = [ds.MARK] + ["struct PlainSym%s%d;" % (pad, i) for i in range(n)] + [ds.MARK]
    return "\n".join(lines) + "\n"


def main():
    def opt(f, d):
        return sys.argv[sys.argv.index(f) + 1] if f in sys.argv else d
    tu = os.path.join(ROOT, opt("--tu", "src/Worldgen.cpp"))
    header = os.path.join(ROOT, opt("--header", "src/Worldgen.h"))
    kmax = int(opt("--max", "12"))

    orig = open(header).read()
    bak = header + ".enumfieldtest.bak"
    shutil.copyfile(header, bak)

    def restore():
        try:
            open(header, "w").write(orig)
            if os.path.exists(bak):
                os.remove(bak)
        except Exception as e:
            print("!! RESTORE FAILED %s: %s (backup %s)" % (header, e, bak), file=sys.stderr)
    atexit.register(restore)

    def measure(block):
        ds.install(header, orig, block)
        r = ds.exact_set(tu)
        return None if r is None else {va for va, ok in r.items() if ok}

    try:
        base = measure("")
        print("baseline: %d exact in %s\n" % (len(base), os.path.relpath(tu, ROOT)))

        # ---- control 1: determinism -------------------------------------------------
        a = measure(plain_block(7))
        b = measure(plain_block(7))
        print("CONTROL determinism (7 plain symbols, compiled twice): %s  (%d vs %d exact)"
              % ("PASS — identical set" if a == b else "*** FAIL — NONDETERMINISTIC ***",
                 len(a), len(b)))

        # ---- the actual question ----------------------------------------------------
        print("\nENUM FIELD COUNT  (one enum, k enumerators => k+1 names)")
        print("  %-4s %-8s %-10s %-10s %s" % ("k", "names", "exact", "predicted", "verdict"))
        hits = tot = 0
        for k in range(0, kmax + 1):
            ex = measure(enum_block(k))
            names = k + 1 if k else 1          # k=0 => tag only
            pred = REFERENCE_STRUCT.get(names)
            v = ""
            if pred is not None:
                tot += 1
                if len(ex) == pred:
                    hits += 1; v = "match"
                else:
                    v = "MISMATCH"
            print("  %-4d %-8d %-10d %-10s %s" % (k, names, len(ex),
                                                  pred if pred is not None else "-", v))
        print("  => %d/%d positions match the plain-symbol curve" % (hits, tot))
        print("  => enumerators are %s"
              % ("DIAL-ACTIVE (field count is load-bearing)" if hits >= max(1, int(tot * 0.7))
                 else "NOT cleanly equivalent to plain symbols — model needs revision"))

        # ---- control 2: identifier length -------------------------------------------
        print("\nCONTROL identifier length (same symbol COUNT, short vs long names)")
        for n in (3, 7, 9):
            s = measure(plain_block(n, namelen=1))
            l = measure(plain_block(n, namelen=40))
            print("  n=%-3d short=%-4d long=%-4d  %s"
                  % (n, len(s), len(l),
                     "same" if s == l else "*** DIFFERS — dial is not a pure count ***"))
    finally:
        restore()
        atexit.unregister(restore)


if __name__ == "__main__":
    main()
