#!/usr/bin/env python3
"""The DUPLICATED-EPILOGUE target list (v120) — the sub-seam of lesson #49 that landed
`PlaySound` 0x409060 (98 B -> 6, length onto the extent).

MECHANISM.  cl 10.20 lays the *then* arm of an if/else out as the FALLTHROUGH (lesson #47).
When that arm is a bare `return;`, cl emits a LOCAL epilogue for it inline instead of
branching to the shared one at the end of the function.  So if the 1997 author wrote the two
arms in the opposite order to us, the original carries an extra `pop.../add esp,N/ret N`
(7-ish bytes) exactly where we emit a 2-byte `jmp` — and we come out SHORT.

FINGERPRINT: our length is BELOW Ghidra's extent AND the original decodes MORE `ret`
instructions than our COMDAT does.  That is a positive, countable signal, unlike the
register bijection you actually see in the byte diff (which is the symptom, not the defect).

⚠ Only the length-SHORT direction is scored — a length-LONG residual cannot be missing an
epilogue, and including it just manufactures noise from jump-table decode.
⚠ The `ret` count is a LINEAR capstone sweep, so a function with an embedded switch jump
table decodes garbage (OnMouseMove reports 25 rets).  That is tolerable here ONLY because
the measure is symmetric — both sides decode the same table — so a desync cancels in the
DIFFERENCE.  Do not repurpose the absolute count for anything (cf. the v110 savescan trap,
where reading the epilogue instead of the prologue lied silently).

⭐ POSITIVE CONTROL (v100/v103/v109 rule — a scan reporting NOTHING TO DO is as suspect as
one reporting a finding).  Validated at v120 by re-running it against the PRE-fix
`DeskcppView.cpp` (`git checkout HEAD~1 -- src/DeskcppView.cpp`): it flags PlaySound with
delta -5, oRET 2, ourRET 1.  At HEAD it reports ZERO hits over 21 length-short residuals,
i.e. the seam is MINED OUT — re-run it only on newly-transcribed functions, or after any
change that moves a function's length below its extent.

Usage:  python3 tools/epiloguescan.py
⚠ COMPILES (it shares bytediff.paired) — do not run during another sweep.
"""
import os, sys, glob, capstone
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match, bytediff
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTENT = {}
for l in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
    p = l.split()
    if len(p) == 2:
        EXTENT[int(p[0], 16)] = int(p[1])

md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

def rets(buf, va):
    n = 0
    for ins in md.disasm(bytes(buf), va):
        if ins.mnemonic == "ret":
            n += 1
    return n

rows = []
for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
    for va, name, code, relocs in bytediff.paired(cpp):
        L = match.trim_pad(code)
        foff = (va - match.TEXT_VA) + match.TEXT_RAW
        orig = bytediff.EXE[foff:foff + L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        offs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
        if not offs:
            continue
        ext = EXTENT.get(va)
        if ext is None or ext <= 1:
            continue
        d = L - ext
        if d >= 0:
            continue                      # only the "we are MISSING code" direction
        ofull = bytediff.EXE[foff:foff + ext]
        ro, rn = rets(ofull, va), rets(code[:L], va)
        rows.append((d, ro, rn, os.path.basename(cpp), va, name, len(offs)))

print("positive control: %d length-SHORT residuals scanned" % len(rows))
print("%-22s %-11s %6s %5s %5s %6s  %s" % ("TU", "addr", "delta", "oRET", "ourRET", "ndiff", "name"))
for d, ro, rn, tu, va, name, nd in sorted(rows, key=lambda r: (-(r[1] - r[2]), r[0])):
    flag = "  <-- ORIG HAS MORE" if ro > rn else ""
    print("%-22s 0x%08x %6d %5d %5d %6d  %s%s" % (tu, va, d, ro, rn, nd, name[:40], flag))
