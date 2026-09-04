#!/usr/bin/env python3
"""JOINT source-configuration search over SEVERAL functions in ONE TU at once.

⭐ Why this exists (v105/v106, asked for by the v111 pickup). A TU has a JOINT
register-allocation phase: its residuals are NOT independent, so a per-function
spelling sweep can be flat for every function while some COMBINATION of their
source configurations is exact. `vartest.py` asks about one function at a time and
scores only that function, which structurally cannot see this. The v111 verdict
("the cheap band needs a different SEARCH, not more spellings") is what this
implements.

Two facts make it tractable and cheap:
  * TUs are compiled SEPARATELY, so an edit to this TU cannot change any other TU's
    codegen. Scoring the EDITED TU's own markers is therefore sufficient AND exact —
    no need to pay for a project-wide `exactset.py` run per combination (~10x faster).
  * The phase propagates DOWNSTREAM (v106), so an upstream edit can never break a
    function EARLIER in the file. Order your edits in FILE order and read the per
    marker vector left to right.
⚠ "Downstream-only" is a heuristic for BOUNDING the search, not a guarantee — v110's
OnAppExit edit moved a function ABOVE it. This tool always prints EVERY marker in the
TU, and you must still confirm a landed win project-wide with `exactset.py` + `comm`.

⭐ BASELINE RULE (v100/v101): a measurement tool must agree with the anchor at zero
perturbation. This measures the UNMODIFIED TU first, prints it as BASELINE, and with
--expect-exact N HARD-FAILS if the TU's exact count isn't N (get N from
`tools/verify.py <tu.cpp>` or progress.py's per-TU column). Always pass it.

It shares progress.py's compile_obj + the COMDAT filter/pairing block verbatim (via
vartest's lineage), so "exact" here is the same predicate progress.py counts.

⚠ MUTATES the source file. Restores on every exit path (atexit + finally) and leaves a
.bak if a restore ever fails. Never run two of these concurrently, or one while
progress.py / exactset.py / residuals.py is in flight — they share build/*.obj.

Usage:
    python3 tools/jointdecl.py <spec.py> [--expect-exact N] [--max-combos M]

The spec file is plain Python defining:
    TU    = "src/Iact.cpp"
    EDITS = [ (label, BASE_TEXT, [(vname, text), ...]), ... ]     # in FILE order
Every BASE_TEXT must appear EXACTLY once in the TU, the BASEs must not overlap, and
each variant list's FIRST entry must be the identity (text == BASE_TEXT). Keep every
variant LINE-NEUTRAL — a line-count change mid-TU rotates the dial on its own
(lesson #23) and would confound the measurement.
"""
import sys, os, re, atexit, itertools, importlib.util

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, verify, progress as prog

EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()


def measure(cpp):
    """[(va, name, our_len, orig_len, ndiff)] for every marker in the TU, file order.
    Same COMDAT filter + pairing as progress.py / verify.py / exactset.py / vartest.py —
    keep all five in sync; a drifted copy is how the v100/v101 cascade happened."""
    obj = prog.compile_obj(cpp)
    if not obj:
        return None
    text = open(cpp).read()
    hinted = set(re.findall(
        r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
    funcs = [f for f in match.coff_functions(obj)
             if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                 or any(h in f[0] for h in hinted))
             and not f[0].lstrip("?").startswith(("_$E", "$E"))]
    out = []
    for va, name, code, relocs in match.pair_by_name(text, funcs):
        L = match.trim_pad(code)
        foff = (va - match.TEXT_VA) + match.TEXT_RAW
        orig = EXE[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        nd = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
        out.append((va, name, L, len(orig), nd))
    return out


def main():
    args = sys.argv[1:]
    expect = maxc = None
    for flag, cast in (("--expect-exact", int), ("--max-combos", int)):
        if flag in args:
            i = args.index(flag)
            v = cast(args[i + 1], 0) if cast is int else args[i + 1]
            if flag == "--expect-exact":
                expect = v
            else:
                maxc = v
            del args[i:i + 2]
    if len(args) != 1:
        raise SystemExit(__doc__)

    spec = importlib.util.spec_from_file_location("spec", args[0])
    S = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(S)

    cpp = S.TU if os.path.isabs(S.TU) else os.path.join(ROOT, S.TU)
    ORIG = open(cpp).read()
    nl = ORIG.count("\n")

    for label, base, vs in S.EDITS:
        if ORIG.count(base) != 1:
            raise SystemExit("edit %r: BASE must appear EXACTLY once (found %d)"
                             % (label, ORIG.count(base)))
        if vs[0][1] != base:
            raise SystemExit("edit %r: variant[0] must be the IDENTITY (text == BASE)" % label)
    # BASEs must not overlap, or the sequential replaces would corrupt each other.
    spans = sorted((ORIG.index(b), ORIG.index(b) + len(b), l) for l, b, _ in S.EDITS)
    for (s1, e1, l1), (s2, e2, l2) in zip(spans, spans[1:]):
        if e1 > s2:
            raise SystemExit("edits %r and %r overlap in the source" % (l1, l2))

    def restore():
        try:
            if open(cpp).read() != ORIG:
                open(cpp, "w").write(ORIG)
        except Exception:
            bak = cpp + ".bak"
            open(bak, "w").write(ORIG)
            sys.stderr.write("!! RESTORE FAILED — original saved to %s\n" % bak)
    atexit.register(restore)

    combos = list(itertools.product(*[range(len(vs)) for _, _, vs in S.EDITS]))
    if maxc and len(combos) > maxc:
        raise SystemExit("%d combinations exceeds --max-combos %d" % (len(combos), maxc))
    print("# %s — %d edits, %d combinations" % (S.TU, len(S.EDITS), len(combos)))

    order, base_row, best = None, None, None
    rc = 0
    try:
        for combo in combos:
            text = ORIG
            for (label, base, vs), k in zip(S.EDITS, combo):
                text = text.replace(base, vs[k][1])
            if text.count("\n") != nl:
                print("%-46s !! NOT LINE-NEUTRAL (%+d lines) — skipped (lesson #23)"
                      % (",".join(vs[k][0] for (_, _, vs), k in zip(S.EDITS, combo)),
                         text.count("\n") - nl))
                continue
            open(cpp, "w").write(text)
            r = measure(cpp)
            if r is None:
                print("%-46s COMPILE FAILED"
                      % ",".join(vs[k][0] for (_, _, vs), k in zip(S.EDITS, combo)))
                sys.stdout.flush()
                continue
            if order is None:
                order = [va for va, _, _, _, _ in r]
                print("# markers (file order): " + " ".join("%#x" % v for v in order))
            nexact = sum(1 for _, _, L, OL, nd in r if nd == 0 and L == OL)
            diffs = {va: nd for va, _, _, _, nd in r}
            row = " ".join("%5d" % diffs.get(va, -1) for va in order)
            tag = ",".join(vs[k][0] for (_, _, vs), k in zip(S.EDITS, combo))
            if base_row is None:
                base_row, base_exact = row, nexact
                print("%-46s exact=%-3d %s   <-- BASELINE" % (tag, nexact, row))
                if expect is not None and nexact != expect:
                    print("\n!! BASELINE MISMATCH: TU exact=%d, --expect-exact %d.\n"
                          "!! The harness disagrees with the anchor at zero perturbation, so\n"
                          "!! every row below would be untrustworthy (v100/v101). Confirm N with\n"
                          "!! `tools/verify.py %s` before reading any result."
                          % (nexact, expect, S.TU))
                    rc = 2
                    break
            else:
                mark = ""
                if nexact > base_exact:
                    mark = "  *** +%d EXACT ***" % (nexact - base_exact)
                elif nexact < base_exact:
                    mark = "  (-%d)" % (base_exact - nexact)
                print("%-46s exact=%-3d %s%s" % (tag, nexact, row, mark))
                if best is None or nexact > best[0]:
                    best = (nexact, tag)
            sys.stdout.flush()
    finally:
        restore()
        print("[restored %s]" % S.TU)
    if best and base_row is not None:
        print("# best: exact=%d  %s" % best)
    sys.exit(rc)


if __name__ == "__main__":
    main()
