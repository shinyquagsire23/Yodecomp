#!/usr/bin/env python3
"""Ambient-dial sweep — the only remaining discriminator for docs/compiler-hunt.md.

BACKGROUND. v95/v96 proved MSVC 4.2's register allocator is steered by declaration state
ENTIRELY OUTSIDE the function being compiled: an EMPTY #include file rotates Worldgen.cpp, and
an `enum` at the tail of DeskcppView.h (which Worldgen.cpp never references) cost 6 byte-exact
functions. v96's idiom scan then showed the residual class is 41 functions differing ONLY in
register allocation / scheduling — and that instruction selection is frozen across the VC 4.x
line, so static analysis CANNOT separate "interim compiler" from "unfound dial position".

THIS TOOL settles it experimentally. It moves the dial NEUTRALLY — appending declarations to an
already-included header's tail changes no token inside any function body — and asks, per
position, which functions are byte-exact. If some position makes a currently-stuck function
exact under our 4.2, no interim compiler is needed to explain it.

The header is ALWAYS restored (atexit + finally), including on Ctrl-C.

Usage:
  tools/dialsweep.py [--tu src/Worldgen.cpp] [--header src/Worldgen.h]
                     [--max N] [--kinds enum,struct,typedef,extern] [--csv out.csv]
"""
import os, sys, re, glob, atexit, subprocess, shutil
from collections import OrderedDict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match
import verify
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()

MARK = "// ==== DIALSWEEP GENERATED BLOCK (tools/dialsweep.py) — auto-removed ===="


def make_block(kind, n):
    """n neutral declarations of `kind`. Never referenced by any TU."""
    if n == 0:
        return ""
    out = [MARK]
    for i in range(n):
        if kind == "enum":
            out.append("enum DialSweepE%d { DIAL_SWEEP_E%d_A, DIAL_SWEEP_E%d_B };" % (i, i, i))
        elif kind == "struct":
            out.append("struct DialSweepS%d;" % i)
        elif kind == "typedef":
            out.append("typedef int DialSweepT%d;" % i)
        elif kind == "extern":
            out.append("extern int g_dialSweepV%d;" % i)
        else:
            raise SystemExit("unknown kind %r" % kind)
    out.append(MARK)
    return "\n".join(out) + "\n"


def install(header, orig_text, block):
    """Insert `block` just before the header's LAST #endif (inside the include guard)."""
    if not block:
        open(header, "w").write(orig_text)
        return
    idx = orig_text.rstrip().rfind("#endif")
    assert idx != -1, "no trailing #endif in %s" % header
    open(header, "w").write(orig_text[:idx] + block + orig_text[idx:])


def exact_set(tu):
    """Compile `tu` and return {va: True/False} for every // FUNCTION marker."""
    obj = prog.compile_obj(tu)
    if not obj:
        return None
    text = open(tu).read()
    # ⚠ v100: keep a lib-owned COMDAT that a marker EXPLICITLY names by mangled hint. Without
    # this exception the filtered COMDAT makes its marker fall back POSITIONALLY inside
    # pair_by_name, stealing the COMDAT the NEXT marker wanted and CASCADING mis-pairs through
    # the rest of the TU (28 of them in DeskcppView.cpp). progress.py/idiomscan.py/verify.py
    # all carry this exception; dialsweep had drifted — and membertest/headersweep/enumfieldtest
    # all measure through exact_set(), so every sweep result before v101 inherited the bug.
    hinted = set(re.findall(
        r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
    funcs = [f for f in match.coff_functions(obj)
             if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                 or any(h in f[0] for h in hinted))
             and not f[0].lstrip("?").startswith(("_$E", "$E"))]
    res = {}
    for va, name, code, relocs in match.pair_by_name(text, funcs):
        L = match.trim_pad(code)
        foff = (va - match.TEXT_VA) + match.TEXT_RAW
        orig = EXE[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
        res[va] = (diffs == 0 and len(orig) == L)
    return res


# ⭐ v101 GUARD. All three harness bugs (v100 x2, v101 x1) would have been caught immediately by
# one check: a measurement tool must AGREE WITH THE ANCHOR AT ZERO PERTURBATION before any of its
# deltas mean anything. dialsweep silently reported 211 while progress.py said 234, and on that
# broken baseline it manufactured a "+4 gained / 0 lost" free gain that does not exist — which is
# what the whole v96 "seven missing symbols" programme was chasing. Keep this in sync with
# CLAUDE.md's anchor table; a mismatch is a HARNESS bug until proven otherwise, not a discovery.
ANCHOR_EXACT = 234


def check_baseline(n):
    if n != ANCHOR_EXACT:
        sys.stderr.write(
            "\n!! BASELINE MISMATCH: dialsweep says %d exact, the anchor says %d.\n"
            "!! Every delta below is UNTRUSTWORTHY. Fix the harness (compare this tool's COMDAT\n"
            "!! filtering + match.pair_by_name usage against progress.py) before reading results.\n"
            "!! If the anchor itself moved, re-baseline ANCHOR_EXACT deliberately.\n\n" % (n, ANCHOR_EXACT))
    return n


def project_exact(tus):
    """Whole-project exact count, deduped by address exactly as progress.py defines 211
    (an exact copy in ANY TU wins). Directly comparable to the anchor number."""
    by_addr = {}
    for tu in tus:
        r = exact_set(tu)
        if r is None:
            return None
        for va, ok in r.items():
            if ok or va not in by_addr:
                by_addr[va] = by_addr.get(va, False) or ok
    return {va for va, ok in by_addr.items() if ok}


def main():
    def opt(flag, default):
        return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else default
    tu = os.path.join(ROOT, opt("--tu", "src/Worldgen.cpp"))
    header = os.path.join(ROOT, opt("--header", "src/Worldgen.h"))
    nmax = int(opt("--max", "12"))
    kinds = opt("--kinds", "enum,struct,typedef,extern").split(",")
    csv_path = opt("--csv", None)
    all_tus = "--all-tus" in sys.argv
    tus = sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)) \
        if all_tus else [tu]

    orig_text = open(header).read()
    backup = header + ".dialsweep.bak"
    shutil.copyfile(header, backup)

    def restore():
        try:
            open(header, "w").write(orig_text)
            if os.path.exists(backup):
                os.remove(backup)
        except Exception as e:
            print("!! RESTORE FAILED for %s: %s (backup at %s)" % (header, e, backup),
                  file=sys.stderr)
    atexit.register(restore)

    rows = []
    try:
        if all_tus:
            base_exact = project_exact(tus)
            if base_exact is None:
                raise SystemExit("baseline compile failed")
            print("baseline: %d exact PROJECT-WIDE (%d TUs, deduped by address)"
                  % (check_baseline(len(base_exact)), len(tus)))
        else:
            base = exact_set(tu)
            if base is None:
                raise SystemExit("baseline compile failed")
            base_exact = {va for va, ok in base.items() if ok}
            print("baseline: %d/%d exact in %s" % (len(base_exact), len(base),
                                                   os.path.relpath(tu, ROOT)))
        print("dial header: %s   sweep: kinds=%s n=0..%d" %
              (os.path.relpath(header, ROOT), ",".join(kinds), nmax))
        print("-" * 78)
        best = (len(base_exact), "baseline", 0)
        for kind in kinds:
            for n in range(0, nmax + 1):
                if n == 0 and kind != kinds[0]:
                    continue                       # n=0 is the same position for every kind
                install(header, orig_text, make_block(kind, n))
                if all_tus:
                    ex = project_exact(tus)
                else:
                    cur = exact_set(tu)
                    ex = None if cur is None else {va for va, ok in cur.items() if ok}
                if ex is None:
                    print("  %-8s n=%-3d COMPILE FAILED" % (kind, n));  continue
                gained = sorted(ex - base_exact)
                lost = sorted(base_exact - ex)
                flag = ""
                if len(ex) > best[0]:
                    best = (len(ex), kind, n); flag = "  <== NEW BEST"
                print("  %-8s n=%-3d exact=%-4d  gained=%-2d lost=%-2d%s%s"
                      % (kind, n, len(ex), len(gained), len(lost),
                         ("  +" + ",".join("%#x" % a for a in gained)) if gained else "",
                         flag))
                rows.append(dict(kind=kind, n=n, exact=len(ex),
                                 gained=["%#x" % a for a in gained],
                                 lost=["%#x" % a for a in lost]))
    finally:
        restore()
        atexit.unregister(restore)

    print("-" * 78)
    print("BEST: %d exact (%s n=%s)   baseline was %d" % (best[0], best[1], best[2],
                                                          len(base_exact)))
    everg = sorted({g for r in rows for g in r["gained"]})
    print("functions made exact by SOME dial position that are not exact at baseline: %s"
          % (", ".join(everg) if everg else "(none)"))
    if csv_path:
        import csv, json
        with open(csv_path, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["kind", "n", "exact", "gained", "lost"])
            for r in rows:
                w.writerow([r["kind"], r["n"], r["exact"],
                            json.dumps(r["gained"]), json.dumps(r["lost"])])
        print("wrote %s" % csv_path)


if __name__ == "__main__":
    main()
