#!/usr/bin/env python3
"""Idiom classification of the non-exact residuals — the compiler-version discriminator.

THE QUESTION (v96, re-opening docs/compiler-hunt.md): are the ~48 "reg-coloring" residuals
the signature of the SAME backend in a different internal state (source/dial), or of a
DIFFERENT C2 build (the interim-cl hypothesis)?

THE DISCRIMINATOR: a different backend version almost always changes at least one
INSTRUCTION-SELECTION idiom somewhere across dozens of functions — how it lowers a switch,
divide-by-constant magic multiplies, inline memset/memcpy thresholds, setcc-vs-branch,
lea-vs-add. A different internal STATE (register coloring) never does: it emits the same
instruction multiset with different register assignments and scheduling.

So for each non-exact function we compute, against the original:
  align         asmscore's structural distance with REGISTERS NORMALIZED OUT
                (0 => our source produced the identical instruction stream)
  reg_pen       is the register difference ONE consistent bijection? (0 => clean rename)
  mnem-delta    symmetric difference of the MNEMONIC MULTISETS (the direct idiom test,
                independent of alignment)

and classify:
  A  PURE-REGALLOC     align==0, mnemonics identical, reg_pen==0  -> clean bijective rename
  B  REGALLOC-MESSY    align==0, mnemonics identical, reg_pen>0   -> + scheduling/spill
  C  SCHEDULED         align>0  but mnemonics identical           -> same insns, reordered
  D  SOURCE/IDIOM      mnemonic multisets differ                  -> needs eyeballing

Only class D can carry compiler-version evidence. Within D we split the differing mnemonics
into IDIOM-SIGNAL opcodes (the lowering choices a backend version would change) vs BULK
opcodes (mov/push/pop/... — overwhelmingly just our source being different), because the
167 PARTIAL functions include many that are simply not finished yet.

Usage:  tools/idiomscan.py [--all] [--csv out.csv]
        (default: only functions whose source is believed CORRECT, i.e. align==0 or the
         TU comment marks them EFFECTIVE/PHASE-DISPLACED/reg-coloring; --all = every one)
"""
import os, sys, glob, re, json
from collections import Counter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match
import verify
import asmscore
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()

# Opcodes whose PRESENCE/ABSENCE reflects an instruction-SELECTION decision — the things a
# different C2 build changes. (Bulk data movement is excluded: it tracks source shape.)
IDIOM_SIGNAL = {
    # divide/modulo by constant: magic-number multiply vs real division
    "imul", "mul", "idiv", "div", "cdq", "cwd",
    # shift-based strength reduction
    "sar", "shr", "shl", "sal", "shrd", "shld", "rol", "ror",
    # inline string ops (memset/memcpy thresholds)
    "rep", "movsd", "movsb", "movsw", "stosd", "stosb", "stosw", "rep movsd", "rep stosd",
    # branchless vs branch
    "sete", "setne", "setl", "setle", "setg", "setge", "seta", "setae", "setb", "setbe",
    "setz", "setnz", "cmovz", "cmovnz",
    # address arithmetic idiom
    "lea",
    # sign/zero extension idiom
    "movsx", "movzx", "cbw", "cwde",
    # misc selection
    "xchg", "neg", "not", "test", "bt", "bts", "btr", "xadd",
}


def norm_mnem(m):
    return m.lower().strip()


def mnem_counter(insns):
    return Counter(norm_mnem(i.mnem) for i in insns)


def main():
    show_all = "--all" in sys.argv
    csv_path = None
    if "--csv" in sys.argv:
        csv_path = sys.argv[sys.argv.index("--csv") + 1]

    # ⚠ Do NOT use toolchain/test/app_funcs.txt extents as the byte-comparison basis: that
    # table is for MARKER-COVERAGE accounting and carries bogus entries (e.g. 0x416620 is
    # listed as 1 byte). Slicing the original to it decoded 0 instructions and fabricated a
    # whole function's worth of "idiom delta" (v96 — caught by probing a flagged case).
    # Basis = our trimmed COMDAT length L, exactly as progress.py defines exactness.

    rows = []
    seen = {}
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)):
        obj = prog.compile_obj(cpp)
        rel = os.path.relpath(cpp, ROOT)
        if not obj:
            print("COMPILE FAILED: %s" % rel, file=sys.stderr)
            continue
        text = open(cpp).read()
        # keep a lib-owned COMDAT that a marker EXPLICITLY names (see progress.py v100 note:
        # dropping them cascaded 28 positional mis-pairs through DeskcppView.cpp, which is what
        # fabricated the phantom "??_G scalar-deleting dtor" idiom family).
        hinted = set(re.findall(
            r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
        funcs = [f for f in match.coff_functions(obj)
                 if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                     or any(h in f[0] for h in hinted))
                 and not f[0].lstrip("?").startswith(("_$E", "$E"))]
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            res = asmscore.score(orig, code[:L], relocs, exact_len=L)
            # Exactness MUST use the anchor's own definition (progress.py: reloc-masked byte
            # compare), not asmscore's disassembly-based one. A function carrying an embedded
            # SWITCH JUMP TABLE decodes the table as instructions, so asmscore reports a phantom
            # byte_diff on functions that are provably byte-exact (v100: 8 of them, incl.
            # OnUpdateGameSpeedUi/OnUpdateDifficultyUi/ClassifyTile — all fake class-D targets).
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            if res.exact or (len(orig) == L and cm == om):
                seen.setdefault(va, None)
                continue
            if seen.get(va, "x") is None:
                continue                      # an exact copy in another TU already won
            a = asmscore._decode(orig)
            b = asmscore._decode(code[:L], relocs)
            ca, cb = mnem_counter(a), mnem_counter(b)
            delta = (ca - cb) + (cb - ca)     # symmetric multiset difference
            # SELF-CHECK: a zero-cost alignment over equal-length streams pairs every
            # instruction with an identical mnemonic, so the multisets MUST agree. If this
            # ever fires, the two sides are not being decoded from the same basis (the v96
            # app_funcs.txt bug) — fail loudly rather than report a phantom idiom delta.
            if res.align == 0 and len(a) == len(b) and delta:
                raise AssertionError("%#x: align==0 and n_orig==n_mine but mnemonic delta %r "
                                     "— decode basis mismatch" % (va, dict(delta)))
            # Did orig[:L] plausibly cover exactly one function? If our length is wrong the
            # slice cuts into a neighbour and every delta below is length artefact, not idiom.
            tail_ok = bool(a) and norm_mnem(a[-1].mnem) in ("ret", "jmp", "int3")
            sig = {m: n for m, n in delta.items() if m in IDIOM_SIGNAL}
            bulk = {m: n for m, n in delta.items() if m not in IDIOM_SIGNAL}
            if not delta:
                cls = "A PURE-REGALLOC" if (res.align == 0 and res.reg_pen == 0) else \
                      ("B REGALLOC-MESSY" if res.align == 0 else "C SCHEDULED")
            else:
                cls = "D SOURCE/IDIOM"
            rows.append(dict(addr=va, tu=rel, name=name, cls=cls, align=res.align,
                             reg_pen=res.reg_pen, idmiss=res.identity_miss,
                             bytes=res.byte_diff, n_orig=res.n_orig, n_mine=res.n_mine,
                             sig=sig, bulk=bulk, tail_ok=tail_ok, length=L))
            seen[va] = True

    rows.sort(key=lambda r: (r["cls"], -sum(r["sig"].values()), r["addr"]))
    counts = Counter(r["cls"] for r in rows)

    print("=" * 100)
    print(" IDIOM CLASSIFICATION of non-exact functions — compiler-version discriminator")
    print("=" * 100)
    for cls in sorted(counts):
        print("  %-18s %3d" % (cls, counts[cls]))
    print()

    # The headline: instruction-SELECTION differences among structurally-aligned functions.
    aligned = [r for r in rows if r["align"] == 0]
    aligned_sig = [r for r in aligned if r["sig"]]
    print("  structurally aligned (align==0): %d   of which ANY idiom-signal delta: %d"
          % (len(aligned), len(aligned_sig)))
    dsig = [r for r in rows if r["sig"]]
    print("  ALL non-exact with idiom-signal delta: %d / %d" % (len(dsig), len(rows)))
    clean = [r for r in rows if r["tail_ok"]]
    print("  slice-sane (orig[:L] ends in ret/jmp): %d / %d  — deltas on the rest are "
          "length artefacts, not idiom" % (len(clean), len(rows)))
    print()

    print("--- functions with IDIOM-SIGNAL mnemonic deltas (candidates for compiler evidence) ---")
    if not dsig:
        print("  (none)")
    for r in sorted(dsig, key=lambda r: -sum(r["sig"].values()))[:40]:
        print("  %#010x %-24s %-16s align=%-5d regpen=%-4d sig=%s"
              % (r["addr"], os.path.basename(r["tu"]), r["cls"], r["align"], r["reg_pen"],
                 dict(sorted(r["sig"].items(), key=lambda kv: -kv[1]))))
    print()
    print("--- class A/B/C detail (source structurally correct; residual is allocation only) ---")
    for r in rows:
        if r["cls"].startswith("D"):
            continue
        print("  %#010x %-24s %-18s align=%-5d regpen=%-4d idmiss=%-4d bytes=%-5d n=%d/%d"
              % (r["addr"], os.path.basename(r["tu"]), r["cls"], r["align"], r["reg_pen"],
                 r["idmiss"], r["bytes"], r["n_orig"], r["n_mine"]))

    if csv_path:
        import csv
        with open(csv_path, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(["addr", "tu", "name", "class", "align", "reg_pen", "idmiss",
                        "bytes", "n_orig", "n_mine", "len", "tail_ok",
                        "idiom_delta", "bulk_delta"])
            for r in rows:
                w.writerow(["%#x" % r["addr"], r["tu"], r["name"], r["cls"], r["align"],
                            r["reg_pen"], r["idmiss"], r["bytes"], r["n_orig"], r["n_mine"],
                            r["length"], r["tail_ok"],
                            json.dumps(r["sig"]), json.dumps(r["bulk"])])
        print("\nwrote %s" % csv_path)


if __name__ == "__main__":
    main()
