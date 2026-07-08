#!/usr/bin/env python3
"""G1 closeness survey — score every NON-exact // FUNCTION: YODA marker across all TUs and
rank by asmscore closeness (align asc, then reg_pen, then byte_diff). The top rows are the
functions nearest to exact; align=0 rows are pure register-allocation residuals (TU-phase),
align>0 rows are instruction-selection / scheduling / block-layout.

Needs build/<TU>.obj already compiled (like progress.py — compile every TU first).
Run from repo root:  python3 tools/survey.py
"""
import os, sys, glob
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, asmscore

exe = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
rows = []
for cpp in sorted(glob.glob(os.path.join(ROOT, "src/*/*.cpp"))):
    tu = os.path.splitext(os.path.basename(cpp))[0]
    obj = os.path.join(ROOT, "build", tu + ".obj")
    if not os.path.exists(obj):
        continue
    text = open(cpp).read()
    for addr, name, code, relocs in match.pair_by_name(text, match.coff_functions(obj)):
        L = match.trim_pad(code)
        foff = (addr - match.TEXT_VA) + match.TEXT_RAW
        orig = exe[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        if len(orig) == L and cm[:L] == om[:L]:
            continue  # exact
        try:
            r = asmscore.score(orig, code[:L], relocs, exact_len=L)
        except Exception:
            continue
        rows.append((r.align, r.reg_pen, r.identity_miss, r.byte_diff, L, tu, name, addr))

rows.sort(key=lambda r: (r[0], r[1], r[3]))
print(f"{'align':>6} {'regp':>5} {'idm':>5} {'bytes':>6} {'len':>5}  TU / func / addr")
for al, rp, idm, bd, L, tu, name, addr in rows:
    print(f"{al:>6} {rp:>5} {idm:>5} {bd:>6} {L:>5}  {tu:10} {name[:44]:44} {addr:#08x}")
print(f"\n{len(rows)} non-exact functions")
