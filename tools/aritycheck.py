#!/usr/bin/env python3
"""A TWO-SIDED ARITY ORACLE — the instrument the v120 `TextDialog::Layout` bug needed (and
that nothing in this project had).

THE BUG IT CATCHES.  A `__thiscall`/`__stdcall` callee cleans its own stack args, so its
terminal instruction encodes the argument BYTE COUNT: `ret 0xc` = three dwords, `ret 8` = two,
`ret` = none.  That number is an independent statement of the function's ARITY, and it is
completely invisible to every other oracle we own — a function declared with the wrong number
of parameters still links (one caller, one callee, consistently wrong), still passes bugscan,
vtcheck and msgcheck, and shows up only as a diffuse byte residual you will read as a register
problem.  `TextDialog::Layout` 0x4176f0 was declared `(int, int)` for the life of the project;
the original ends `ret 0xc` and its single call site pushes three args.

METHOD.  Compare the terminal `ret` immediate of the ORIGINAL against OUR compiled one, per
paired marker.  Read it from the LAST bytes rather than by disassembling forward: a linear
sweep desyncs on an embedded jump table or EH data (the v110 savescan trap), and the tail is
the one place we can decode with certainty.
    ... C3           -> ret 0        ... C2 imm16  -> ret imm16
A function whose COMDAT ends in an EH funclet or a tail `jmp` has no terminal ret and is
reported as n/a rather than guessed at.

⭐ BUILT-IN POSITIVE CONTROL (v100/v103/v109 rule).  Every BYTE-EXACT function must agree by
construction, so the tool checks them too and FAILS LOUDLY if any disagrees — that would mean
the terminal-ret reader itself is broken, not that the source is.  A clean run therefore
proves the method before you read the findings.  Validated at v120: with the Layout fix
reverted it reports `0x004176f0 orig ret 0xc / ours ret 0x8`; with it applied, 0 mismatches.

Usage:  python3 tools/aritycheck.py
⚠ COMPILES (it shares bytediff.paired) — do not run during another sweep.
"""
import os, sys, glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match, bytediff

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTENT = {}
for _l in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
    _p = _l.split()
    if len(_p) == 2:
        EXTENT[int(_p[0], 16)] = int(_p[1])


# bytes that plausibly precede a terminal ret: pop ebx/ebp/esi/edi, leave
_EPI_PREV = {0x5B, 0x5D, 0x5E, 0x5F, 0xC9}


def _preceded_by_epilogue(buf, i):
    """buf[i] starts a ret — does a plausible epilogue sit immediately before it?"""
    if i <= 0:
        return False
    if buf[i - 1] in _EPI_PREV:
        return True
    # add esp, imm8   (83 C4 xx)
    return i >= 3 and buf[i - 3] == 0x83 and buf[i - 2] == 0xC4


def terminal_ret(buf, window=24):
    """Argument byte count from the function's terminal `ret`, or None if unreadable.

    ⚠ Do NOT just read the last 1/3 bytes. Ghidra's extents are not always exact: they can
    over-run the real `ret` into a trailing jump TABLE (`Layout` 0x4176f0 ends
    `... c2 0c 00 | 37 7a 41`, three bytes of a table entry past its `ret 0xc`), which is the
    same v117 artifact family that made the raw length census manufacture targets. The first
    draft of this tool read `buf[-3]`, found no ret, silently returned None, and so reported
    "0 arity mismatches" WITH the known Layout bug in the tree — the positive control below is
    what caught it. Scan BACKWARD instead, and require a plausible epilogue in front so a
    stray 0xC2/0xC3 inside table data cannot pose as a return.
    """
    n = len(buf)
    # exact tail first — unambiguous when the buffer really ends at the ret
    if n >= 1 and buf[-1] == 0xC3:
        return 0
    if n >= 3 and buf[-3] == 0xC2:
        imm = buf[-2] | (buf[-1] << 8)
        if imm % 4 == 0 and imm <= 0x40:
            return imm
    # otherwise the extent over-runs the ret (trailing jump table) — scan back, and require a
    # plausible epilogue so table data cannot pose as a return
    for i in range(n - 1, max(-1, n - 1 - window), -1):
        if buf[i] == 0xC3 and _preceded_by_epilogue(buf, i):
            return 0
        if buf[i] == 0xC2 and i + 2 < n:
            imm = buf[i + 1] | (buf[i + 2] << 8)
            if imm and imm % 4 == 0 and imm <= 0x40 and _preceded_by_epilogue(buf, i):
                return imm
    return None


def main():
    rows, checked, skipped, ctrl_bad = [], 0, 0, []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in bytediff.paired(cpp):
            L = match.trim_pad(code)
            ext = EXTENT.get(va)
            if ext is None or ext <= 1:
                continue
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = bytediff.EXE[foff:foff + ext]
            ro, rn = terminal_ret(orig), terminal_ret(bytes(code[:L]))
            if ro is None or rn is None:
                skipped += 1
                continue
            checked += 1
            if ro == rn:
                continue
            cm, om = match.mask(code, relocs, L), match.mask(orig[:L], relocs, L)
            exact = L == ext and cm[:L] == om[:L]
            (ctrl_bad if exact else rows).append(
                (os.path.basename(cpp), va, ro, rn, name))

    print("positive control: %d function(s) had a readable terminal ret on both sides "
          "(%d skipped as unreadable)" % (checked, skipped))
    if ctrl_bad:
        print("\n!! TOOL BUG: %d BYTE-EXACT function(s) disagree — a byte-exact function cannot\n"
              "   have the wrong arity, so the terminal-ret reader is wrong. Fix it before\n"
              "   believing anything below (v100/v101)." % len(ctrl_bad))
        for tu, va, ro, rn, name in ctrl_bad:
            print("   %-22s 0x%08x  orig ret %#x / ours ret %#x  %s" % (tu, va, ro, rn, name))
        return 2

    if not rows:
        print("\n0 arity mismatches — every comparable function cleans the same number of\n"
              "argument bytes as the original.")
        return 0
    print("\nARITY MISMATCH — the original cleans a different number of argument bytes:")
    print("%-22s %-11s %9s %9s  %s" % ("TU", "addr", "orig ret", "ours ret", "name"))
    for tu, va, ro, rn, name in sorted(rows, key=lambda r: -abs(r[2] - r[3])):
        print("%-22s 0x%08x %9s %9s  %s   <-- %+d dword arg(s)"
              % (tu, va, hex(ro), hex(rn), name[:44], (ro - rn) // 4))
    return 1


if __name__ == "__main__":
    sys.exit(main())
