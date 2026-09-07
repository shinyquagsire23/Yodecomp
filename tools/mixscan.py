#!/usr/bin/env python3
"""THE INSTRUCTION-MIX CENSUS — what a residual is actually MISSING, both sides (v131).

A byte diff shows you the ORIGINAL clearly and OURS only as "the other column", and once
the schedule shifts it is unreadable: `WorldgenPlacePuzzles` 0x421930 differed in 47 % of
its bytes and `sbs.py` printed 600 lines of noise. But decode BOTH sides, count mnemonics,
and subtract, and that same function reduces to FIVE numbers:

    jmp -2, jle +2, jg -2, mov +1, movsx -1, test -1, jge -1

— the orig has one more `test` and one more `jge` than we do, i.e. it tests its loop
condition at the BOTTOM and we test at the top. That IS the whole find (lesson #46, the
rotation dial): the guarded do-while took it from -11 to -8 with the mix agreeing exactly
on test/jge/jmp/ret/pop afterwards. Nothing else in the toolbox could see it.

Read it in two directions, the same way as savescan/framescan/pushscan:

  * A NEGATIVE count = an instruction the ORIGINAL emits and we do not. This is the
    structural half — `test`/`jcc` pairs are a control-flow difference, `mov` is usually a
    reload we CSE'd away (lesson #50), `movsx` a promotion we skipped (movsxscan.py).
  * A POSITIVE count = something WE emit and the original does not.

⭐ THE STRONGEST SIGNAL IS AN EMPTY DELTA WITH A NONZERO BYTE DIFF. It means the two
instruction streams agree instruction-for-instruction and ONLY the registers and encodings
differ — the lesson-#44 scratch-register bijection / lesson-#54 compare-encoding class,
which is source-CLOSED. Park those on sight instead of sweeping spellings at them. They are
printed as `PURE-REG` and are the cheapest triage in the project (v129's "30-second
instruction-mix diff" note, promoted to a tool because it found the v131 win).

⚠ A hit is a CANDIDATE, not a defect — confirm with `sbs.py` before investing.

⚠ Two decode traps, both handled here, both of which fabricated targets in earlier sessions:
  1. Decode the ORIGINAL from raw EXE bytes at its OWN Ghidra extent, never through a buffer
     sliced to OUR length (the v121 sbb-census bug: slicing desyncs the stream and the orig
     column reads 0 for features it visibly has).
  2. A trailing switch JUMP TABLE lives inside our COMDAT but outside the extent, and its
     reloc'd entries decode as `add byte ptr [eax], al` on our side (v129). Those functions
     are excluded, exactly as residuals.py --lenmis excludes them.

POSITIVE CONTROL: every BYTE-EXACT function whose length equals its extent must report an
EMPTY delta. The tool checks this itself and exits 1 if one does not — same discipline as
aritycheck/framescan/pushscan (a tool must agree with the anchor at zero perturbation).

Usage:  python3 tools/mixscan.py [--all] [--min N]
        --all   include length-EXACT residuals too (default: only length mismatches,
                which is the structural seam of lesson #49)
        --min N only show functions whose total |delta| is at least N

⚠ COMPILES (it needs our own COMDATs) — do not run it while a vartest/declorder sweep is in
flight; they fight over build/*.obj.
"""
import sys, os, glob, collections

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import residuals, match, capstone

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)


def mix(buf, va):
    """Counter(mnemonic) over a linear decode."""
    c = collections.Counter()
    for ins in _md.disasm(bytes(buf), va):
        c[ins.mnemonic] += 1
    return c


def comparable_extent(va, code, L):
    """Ghidra's extent for va, or None when it is not honestly comparable."""
    ext = residuals.EXTENT.get(va)
    if ext is None:
        return None
    if residuals.has_jumptable(code[:L], va):
        return None                      # embedded/trailing jump table: data, not code
    return ext


def main():
    show_all = "--all" in sys.argv
    floor = 0
    if "--min" in sys.argv:
        floor = int(sys.argv[sys.argv.index("--min") + 1])

    rows, control, n_stub, n_table = [], 0, 0, 0
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            L = match.trim_pad(code)
            ext = comparable_extent(va, code, L)
            if ext is None:
                if residuals.EXTENT.get(va) is None:
                    n_stub += 1
                else:
                    n_table += 1
                continue
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = residuals.EXE[foff:foff + ext]
            # ⚠ exactness is judged over OUR length with residuals.py's own masked compare —
            # NOT over `orig`, which is cut to the extent. Padding a short extent out to L
            # with zeros fabricates differences and resurrected four byte-EXACT functions
            # (??1StatsDlg, ??0CBitmap, OnAppAbout, Serialize) as phantom residuals.
            cm = match.mask(code, relocs, L)
            om = match.mask(residuals.EXE[foff:foff + L], relocs, L)
            ndiff = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            # ⚠ Decode both sides RAW. Reloc'd OPERANDS are harmless (a `call rel32` is 5
            # bytes whether the target is real or zeroed), but reloc-MASKING the original is
            # not an option: our reloc offsets are positions in OUR stream, and once the
            # schedules shift they land mid-instruction in the original's and corrupt the
            # decode — the v121 sbb-census trap. Jump TABLES are the one case raw decoding
            # gets wrong (zeroed entries read as `add byte ptr [eax], al` on our side only),
            # and those functions are excluded above instead.
            co, cu = mix(orig, va), mix(code[:L], va)
            delta = {k: cu[k] - co[k] for k in set(co) | set(cu) if co[k] != cu[k]}

            if ndiff == 0 and L == ext:                 # byte-exact: the positive control
                control += 1
                if delta:
                    sys.stderr.write(
                        "CONTROL FAILED: byte-exact 0x%08x %s reports a mix delta %r\n"
                        % (va, name, delta))
                    return 1
                continue
            if ndiff == 0:
                continue
            if not show_all and L == ext:
                continue
            tot = sum(abs(v) for v in delta.values())
            if tot < floor:
                continue
            rows.append((L - ext, tot, va, os.path.relpath(cpp, ROOT), name,
                         sum(co.values()), sum(cu.values()), ndiff, delta))

    rows.sort(key=lambda r: (-abs(r[0]), -r[1]))
    print("# INSTRUCTION-MIX delta per residual: OURS minus ORIGINAL, per mnemonic (v131).")
    print("# negative = the original emits it and we do not (the structural half).")
    print("# PURE-REG = streams agree instruction-for-instruction; only registers/encodings")
    print("#            differ => lesson #44/#54, source-CLOSED. Park it, do not sweep it.")
    print("%-11s %-22s %-42s %6s %5s %9s  %s"
          % ("addr", "TU", "name", "len", "|d|", "insns o/u", "delta"))
    for d, tot, va, cpp, name, no, nu, ndiff, delta in rows:
        body = ", ".join("%s %+d" % (k, v)
                         for k, v in sorted(delta.items(), key=lambda x: -abs(x[1])))
        print("0x%08x %-22s %-42s %+6d %5d %5d/%-5d  %s"
              % (va, os.path.basename(cpp), name[:42], d, tot, no, nu,
                 body or "PURE-REG"))
    print("\n%d residual(s) shown; positive control CLEAN over %d byte-exact function(s)."
          % (len(rows), control))
    print("%d marker(s) have no usable Ghidra extent (stub / overlapping) and %d carry a jump\n"
          "TABLE, whose reloc-zeroed entries decode as instructions on our side only — the mix\n"
          "cannot measure those, so they are EXCLUDED rather than silently mis-reported.\n"
          "(That bucket includes real targets, e.g. Layout 0x4176f0 at -35; use sbs.py there.)"
          % (n_stub, n_table))
    return 0


if __name__ == "__main__":
    sys.exit(main())
