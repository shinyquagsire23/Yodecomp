#!/usr/bin/env python3
"""G1 frontier finder — per TU, list functions in .text (== emission) order and report the
FIRST non-exact one. Rationale (the joint-pass ordering): if functions 1..k in a TU are all
byte-exact, the compiler processed them identically to the original, so the allocator state
entering function k+1 matches the original's — making k+1 the best-steerable candidate.

⚠ v36 caveat (proven): this "first non-exact is steerable" hypothesis FAILS in practice for
the register-tie-break class. The `this`/counter-register coloring of even a fresh-TU first
function (ReadIzon EBP/EBX, Puzzle this=EDI/ESI) is seeded by the TU's real class/header decl
context (called-helper sigs, struct sizeof, vtable order — see the ⭐ TU-PHASE DIAL rule), not
its own body, and resists decl-order/body-order/loop-form/compiler-flag probes. Use this tool to
MAP the frontier, but expect header-phase/joint-fixed-point residuals, not free wins.

Needs build/<TU>.obj compiled. Run from repo root:  python3 tools/frontier.py
"""
import os, sys, glob
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, asmscore

exe = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
for cpp in sorted(glob.glob(os.path.join(ROOT, "src/*.cpp"))):
    tu = os.path.splitext(os.path.basename(cpp))[0]
    obj = os.path.join(ROOT, "build", tu + ".obj")
    if not os.path.exists(obj):
        continue
    text = open(cpp).read()
    rows = []
    for addr, name, code, relocs in match.pair_by_name(text, match.coff_functions(obj)):
        L = match.trim_pad(code)
        foff = (addr - match.TEXT_VA) + match.TEXT_RAW
        orig = exe[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        exact = (len(orig) == L and cm[:L] == om[:L])
        sc = None
        if not exact:
            try:
                r = asmscore.score(orig, code[:L], relocs, exact_len=L)
                sc = (r.align, r.reg_pen, r.byte_diff)
            except Exception:
                sc = (-1, -1, -1)
        rows.append((addr, name, exact, L, sc))
    rows.sort(key=lambda r: r[0])
    n_exact = sum(1 for r in rows if r[2])
    print(f"\n=== {tu}: {n_exact}/{len(rows)} exact ===")
    first = next((i for i, r in enumerate(rows) if not r[2]), None)
    if first is None:
        print("  ALL EXACT")
        continue
    r = rows[first]
    print(f"  FRONTIER @ idx {first}: {r[0]:#08x} {r[1][:46]}  align/reg/byte={r[4]} len={r[3]}")
    for j in range(max(0, first - 2), first):
        print(f"     prev exact: {rows[j][0]:#08x} {rows[j][1][:40]}")
