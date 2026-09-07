#!/usr/bin/env python3
"""The SELF-EXTENSION census (v130) — `movsx r32,r16` on the SAME register, BOTH SIDES.

WHAT IT MEASURES
A `movsx <r32>,<r16>` whose source and destination are the same register is cl 10.20
promoting a `short` to `int` IN PLACE, i.e. maintaining a 32-bit incarnation of a short
local in the register that already holds it.  The canonical source construct is a `short`
value reaching an `int` context — most often an `int` PARAMETER of a callee, sometimes an
`int` local initialised from a short (`Canvas::BlitFast` 0x408110's `int rows = height;`).

WHY IT IS A TOOL AND NOT A GREP
The interesting variant is the one on a CALLEE-SAVED register (ebx/esi/edi/ebp), because
that means the promoted short is LOOP-CARRIED — and those sites are the last unexplained
-6/-6/+3 entries on the `--lenmis` census (`RefreshZone` 0x403ae0, `DrawLocatorMap`
0x423df0, `ZoneTransitionStep` 0x409650).

⚠ LESSON #56 IS THE WHOLE POINT OF THIS FILE.  The v130 session first wrote this scan over
the ORIGINAL ONLY and read "8 sites, all of them inside residual functions, none inside a
byte-exact one" as "we never emit this construct".  That is FALSE: `DrawPlayer` 0x41a6d0
emits `movsx ebp,bp` at the IDENTICAL offset on both sides — the function is a residual for
an unrelated register bijection.  A site inside a residual says nothing until you have
looked at your own output at that offset.  So this tool censuses BOTH images and reports
each site as CONFIRMED (both) / ORIG-ONLY (a real target) / OURS-ONLY (we over-promote).

⚠ COMPILES (it needs our COMDATs).  Do not run it while a vartest/formsweep sweep is in
flight — they share build/*.obj.

Usage:  python3 tools/movsxscan.py
"""
import os, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, progress as prog
import residuals as R

# ⚠ v130: the FIRST draft of this tool keyed on the SELF form only (`movsx eax,ax`).  That
# is not a structural property — whether a promotion is "self" depends on which register the
# value already sits in, i.e. on register allocation, which is exactly what varies between
# the images.  `WorldgenFillQuestItemSpot2Maybe` 0x41cf10 was ranked its sharpest target on
# that basis and is a pure rename: orig `movsx eax,ax`, ours `movsx esi,ax`, same 3 bytes.
# The structural quantity is HOW MANY short->int promotions each side emits, split by where
# the source operand lives (a register vs a frame slot) — that split is the residency story.
REG_FORM = "reg"     # movsx r32, r16      (mod == 11)  — value already in a register
MEM_FORM = "mem"     # movsx r32, word ptr [..]         — value homed in a frame slot


def extents():
    out = {}
    for ln in open(os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")):
        f = ln.split()
        if len(f) >= 2:
            try:
                out[int(f[0], 16)] = int(f[1])
            except ValueError:
                pass
    return out


def counts(buf):
    """{form: n} of `movsx r32, <16-bit>` in buf, plus the self-extension subtotal."""
    out = {REG_FORM: 0, MEM_FORM: 0, "self": 0}
    SELF = {0xc0, 0xc9, 0xd2, 0xdb, 0xed, 0xf6, 0xff}
    for i in range(len(buf) - 2):
        if buf[i] != 0x0F or buf[i + 1] != 0xBF:
            continue
        modrm = buf[i + 2]
        if modrm >= 0xC0:
            out[REG_FORM] += 1
            if modrm in SELF:
                out["self"] += 1
        else:
            out[MEM_FORM] += 1
    return out


def main():
    ext = extents()

    rows, control = [], 0
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in R.paired(cpp):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            # ⭐ read the ORIGINAL at its OWN Ghidra extent, never through our length or
            # our reloc mask (the v121 xjumpscan trap: slicing to our length desyncs the
            # decode and invents/hides sites).
            olen = ext.get(va, L)
            orig = R.EXE[foff:foff + olen]
            ours = bytes(code[:L])
            # ⭐ STRUCTURAL comparison — per-register COUNTS, never offset alignment.
            # A residual's schedule shifts, so "the original has one at +0x5a0 and we do
            # not" is meaningless; "the original self-extends edi twice and we never do"
            # is the real claim (lesson #56).
            oc, uc = counts(orig), counts(ours)
            if not any(oc.values()) and not any(uc.values()):
                continue
            for form in (REG_FORM, MEM_FORM, "self"):
                a, b = oc[form], uc[form]
                if a == 0 and b == 0:
                    continue
                verdict = ("CONFIRMED" if a == b else
                           "ORIG-MORE" if a > b else "OURS-MORE")
                if a and b:
                    control += 1
                rows.append((va, form, a, b, verdict, os.path.basename(cpp), name))

    # positive control: the construct must be found on BOTH sides somewhere, otherwise the
    # decode/pairing is lying and every ORIG-MORE below is noise (the v100/v103 rule).
    print("positive control: %d function/register pair(s) carry it on BOTH sides" % control)
    if control == 0:
        print("FAIL: no site agrees on both sides — the census is not trustworthy")
        return 1

    print("\n%-12s %-9s %-5s %-5s %-11s %-22s %s"
          % ("addr", "form", "orig", "ours", "verdict", "tu", "name"))
    for va, reg, a, b, verdict, tu, name in rows:
        print("0x%08x   %-9s %-5d %-5d %-11s %-22s %s"
              % (va, reg, a, b, verdict, tu, name[:46]))

    tally = {}
    for r in rows:
        tally[r[4]] = tally.get(r[4], 0) + 1
    print("\n" + ", ".join("%s=%d" % kv for kv in sorted(tally.items())))
    print("form: reg = `movsx r32,r16` (value in a register); mem = `movsx r32,word[frame]`;")
    print("      self = the reg-form subset whose src==dst (a short promoted IN PLACE).")
    print("ORIG-MORE = a promotion the original makes and we do not (a real target);")
    print("OURS-MORE = we promote where the original does not.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
