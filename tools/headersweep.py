#!/usr/bin/env python3
"""Multi-header dial sweep — WHERE does the missing declaration mass live?  (v96)

tools/dialsweep.py established the dial is a pure file-scope SYMBOL COUNT, and that +7 symbols
through src/Worldgen.h takes the project 211 -> 215 with FOUR GAINED AND ZERO LOST. That is a
measurement, not a patch: the original's headers carried ~7 more symbols than ours, and the job is
to find the real ones.

This narrows WHERE. A header only perturbs the TUs that (transitively) include it, so the set of
functions that move under a given header is a REACH FINGERPRINT. Sweeping every shared header and
comparing fingerprints localizes which header the missing declarations belong to — a second
coordinate on top of the count.

⚡ Only TUs in a header's reach can change, so the baseline result is reused for all the others.
That makes a position cost (reach x ~8s) instead of a full 13-TU rebuild.

⚠ This tool NEVER commits anything. Placeholder declarations are a measuring probe; see CLAUDE.md
"THE DIAL IS AN INSTRUMENT, NOT A KNOB". A position that TRADES (+3/-3) is padding; only a position
that gains with ZERO losses is evidence of a real missing fact.

Usage:  tools/headersweep.py [--max N] [--headers a.h,b.h] [--csv out.csv]
"""
import os, sys, re, glob, json, atexit
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dialsweep as ds

ROOT = ds.ROOT
SRC = os.path.join(ROOT, "src")


def local_includes(path):
    try:
        return re.findall(r'#\s*include\s*"([^"]+)"', open(path).read())
    except OSError:
        return []


def reach_of(tu):
    """Transitive set of local headers a TU pulls in."""
    seen, stack = set(), [tu]
    while stack:
        for inc in local_includes(stack.pop()):
            p = os.path.join(SRC, inc)
            if os.path.exists(p) and p not in seen:
                seen.add(p); stack.append(p)
    return seen


def main():
    def opt(f, d):
        return sys.argv[sys.argv.index(f) + 1] if f in sys.argv else d
    nmax = int(opt("--max", "10"))
    csv_path = opt("--csv", None)

    tus = sorted(glob.glob(os.path.join(SRC, "*.cpp")))
    hdr_tus = defaultdict(set)
    for tu in tus:
        for h in reach_of(tu):
            hdr_tus[h].add(tu)

    sel = opt("--headers", None)
    if sel:
        want = set(sel.split(","))
        headers = [h for h in hdr_tus if os.path.basename(h) in want]
    else:   # every shared header (>=2 TUs); DebugLog.h is YODA_DEBUG-only, skip
        headers = [h for h, t in hdr_tus.items()
                   if len(t) >= 2 and os.path.basename(h) != "DebugLog.h"]
    headers.sort(key=lambda h: -len(hdr_tus[h]))

    # ---- baseline: per-TU exact maps, computed once -------------------------------
    print("computing baseline (%d TUs) ..." % len(tus))
    base_per_tu = {}
    for tu in tus:
        r = ds.exact_set(tu)
        if r is None:
            raise SystemExit("baseline compile failed: %s" % tu)
        base_per_tu[tu] = r

    def merge(per_tu):
        by = {}
        for r in per_tu.values():
            for va, ok in r.items():
                by[va] = by.get(va, False) or ok
        return {va for va, ok in by.items() if ok}

    base_exact = merge(base_per_tu)
    print("baseline: %d exact project-wide\n" % len(base_exact))

    rows = []
    for hdr in headers:
        affected = sorted(hdr_tus[hdr])
        orig = open(hdr).read()

        def restore(h=hdr, o=orig):
            open(h, "w").write(o)
        atexit.register(restore)
        print("=== %-24s reach=%d TUs (%s)" %
              (os.path.basename(hdr), len(affected),
               " ".join(os.path.basename(t).replace(".cpp", "") for t in affected)))
        try:
            for n in range(1, nmax + 1):
                ds.install(hdr, orig, ds.make_block("struct", n))
                per_tu = dict(base_per_tu)
                bad = False
                for tu in affected:
                    r = ds.exact_set(tu)
                    if r is None:
                        bad = True; break
                    per_tu[tu] = r
                if bad:
                    print("   n=%-3d COMPILE FAILED" % n); continue
                ex = merge(per_tu)
                gained = sorted(ex - base_exact)
                lost = sorted(base_exact - ex)
                tag = ""
                if not lost and gained:
                    tag = "   <== FREE GAIN (candidate real fact)"
                elif len(ex) > len(base_exact):
                    tag = "   <== net gain but TRADES (padding shape)"
                print("   n=%-3d exact=%-4d  +%-2d -%-2d%s%s"
                      % (n, len(ex), len(gained), len(lost),
                         ("  " + ",".join("%#x" % a for a in gained)) if gained else "", tag))
                rows.append(dict(header=os.path.basename(hdr), reach=len(affected), n=n,
                                 exact=len(ex), gained=["%#x" % a for a in gained],
                                 lost=["%#x" % a for a in lost]))
        finally:
            restore()
            atexit.unregister(restore)
        print()

    print("=" * 78)
    free = [r for r in rows if r["gained"] and not r["lost"]]
    free.sort(key=lambda r: -r["exact"])
    print("FREE-GAIN positions (gain with ZERO regressions — the afxcmn.h signature):")
    for r in free[:20]:
        print("  %-24s n=%-3d exact=%-4d  +%s"
              % (r["header"], r["n"], r["exact"], ",".join(r["gained"])))
    if not free:
        print("  (none)")
    if csv_path:
        import csv
        with open(csv_path, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["header", "reach", "n", "exact", "gained", "lost"])
            for r in rows:
                w.writerow([r["header"], r["reach"], r["n"], r["exact"],
                            json.dumps(r["gained"]), json.dumps(r["lost"])])
        print("\nwrote %s" % csv_path)


if __name__ == "__main__":
    main()
