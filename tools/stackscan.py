#!/usr/bin/env python3
"""THE STACK-RESIDENCY CENSUS — how much of each side's data lives in the FRAME (v139).

The prologue instruments say how many long-lived values a body needs (`savescan.py` = the
callee-save SET), how much frame it reserves (`framescan.py` = `sub esp,N`) and where `this`
went (`thisscan.py`). None of them can say WHICH ordinary values ended up in memory, and
`mixscan.py` structurally cannot either: `mov eax,[ebp-0x1c]` and `mov eax,edi` are both
one `mov`, so a spill/enregister contest is INVISIBLE to a mnemonic census.

This counts, on BOTH sides, every instruction operand that touches the stack frame — a
memory operand based on ESP or EBP — split into READS, WRITES and LEAs. That is the direct
measure of the lesson #43/#59/#68/#69 family:

  * OURS reads the frame MORE  => we spilled something the original keeps in a register.
    `IactProbeMove` 0x406550 before v139 is the model case: `found` homed (four 8-byte
    `mov [mem],1` and five 5-byte `cmp [mem],0`) and `dx` re-read from its argument slot at
    three sites, where the original held both in registers. Levers: lesson #69 (start the
    live range later / let it coalesce), #59 (assign in both arms), #43 (name the local).
  * OURS reads the frame LESS  => the ORIGINAL homed something we enregister, and we are
    usually LENGTH-SHORT because each of its reloads is 3-6 bytes we never emit.
    `DrawHealthDial` 0x427490 (-16) is the model case: the original feeds nine Chord pushes
    out of four frame slots while we push two coords straight from EDI/EBX.

⭐ THE KEY MUST BE REGISTER-BLIND, and getting there took a second draft. It records only
THAT a stack slot was touched, never which register the value moved to or from, so whether
the allocator picked esi or edi cannot move a column.

⛔ THE FIRST DRAFT WAS NOT, AND ITS POSITIVE CONTROL COULD NOT CATCH IT. It treated any
EBP-based memory operand as frame traffic. But cl 10.20 only makes EBP a frame pointer when
the function needs one (an EH frame, `push ebp; mov ebp,esp`); in an ESP-framed function EBP
is an ordinary callee-saved register, so `lea eax,[ebp+ecx*2]` is ARITHMETIC. That draft
ranked `DrawTileAt` 0x40a3a0 at `lea+3 rd+1` purely because the original parks `this` in EBP
and we park `pCell` there — a pure register swap its own source note had recorded for the
life of the project. A byte-exact control can never see this: identical bytes means identical
registers. ⇒ EBP counts as a frame base ONLY when the prologue establishes an EBP frame,
which is decided per side, from the prologue, below. Generalise the shape, not the fix: ask
of every census column "would this move if only the register allocator changed its mind?" —
and note that a positive control proves a tool right about the NULL case only.

⚠ A hit is a CANDIDATE, not a defect — a differing stack-traffic count can equally be the
downstream shadow of a control-flow difference. Cross it with `mixscan.py` first: if mixscan
reports PURE-REG the streams already agree instruction-for-instruction and this tool will
agree too, so a NONZERO delta here on a PURE-REG function would mean one of the two is lying.

⚠ Same two decode traps as mixscan, handled the same way: decode the ORIGINAL from raw EXE
bytes at its OWN Ghidra extent (never through a buffer sliced to our length), and EXCLUDE
functions carrying a jump table (their reloc-zeroed entries decode as instructions on our
side only).

POSITIVE CONTROL: every BYTE-EXACT function whose length equals its extent must report a
zero delta in all three columns. The tool checks this itself and exits 1 if one does not.
The verdict is printed BOTH before and after the table — v138 lost a control failure to a
`tail -30`.

Usage:  python3 tools/stackscan.py [--all] [--min N]
        --all   include length-EXACT residuals too (default: only length mismatches)
        --min N only show functions whose total |delta| is at least N (default 1;
                pass --min 0 to see the length-mismatched residuals that AGREE)

⚠ COMPILES (it needs our own COMDATs) — do not run it while a vartest/declorder sweep is in
flight; they fight over build/*.obj.
"""
import sys, os, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import residuals, match, capstone
from capstone import x86

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_md.detail = True



def has_ebp_frame(buf, va):
    """True iff the prologue establishes EBP as a FRAME POINTER (`push ebp; mov ebp,esp`).

    ⚠ Load-bearing, not a nicety — see the ⛔ note above. Read it from the PROLOGUE and stop
    at the first call/branch, the same discipline savescan.py/thisscan.py needed: a linear
    sweep of a whole body desyncs on embedded EH data or a jump table.
    """
    prev = None
    for ins in _md.disasm(bytes(buf[:64]), va):
        if ins.mnemonic in ("call", "jmp", "ret") or ins.mnemonic.startswith("j"):
            break
        if (ins.mnemonic == "mov" and ins.op_str.replace(" ", "") == "ebp,esp"
                and prev == "push ebp"):
            return True
        prev = ins.mnemonic + " " + ins.op_str
    return False


def frame_traffic(buf, va, ebp_frame):
    """(reads, writes, leas) over stack-based memory operands. Register-BLIND.

    `ebp_frame` gates whether EBP-based operands count at all; without it EBP is just a
    general callee-saved register and its addressing is arithmetic, not frame traffic.
    """
    stack = (x86.X86_REG_ESP, x86.X86_REG_SP)
    if ebp_frame:
        stack += (x86.X86_REG_EBP, x86.X86_REG_BP)
    r = w = l = 0
    for ins in _md.disasm(bytes(buf), va):
        try:
            ops = ins.operands
        except capstone.CsError:
            continue
        for op in ops:
            if op.type != x86.X86_OP_MEM:
                continue
            if op.mem.base not in stack and op.mem.index not in stack:
                continue
            if ins.mnemonic == "lea":
                l += 1
            elif op.access & capstone.CS_AC_WRITE:
                w += 1
            else:
                r += 1
        # push/pop of a callee-saved register is frame traffic in the trivial sense and is
        # already pinned by savescan.py; it carries no residency information, so the implicit
        # [esp] operand of push/pop/call/ret is deliberately NOT counted (capstone does not
        # emit it as an operand, which is what we want).
    return r, w, l


def main():
    show_all = "--all" in sys.argv
    # default 1, not 0: a "hit" means a NONZERO delta. At --min 0 the table also lists
    # the length-mismatched residuals whose residency already AGREES, which is 17 rows of
    # noise and made the documented green state ("9 hits") disagree with the output.
    floor = int(sys.argv[sys.argv.index("--min") + 1]) if "--min" in sys.argv else 1

    rows, control, failures, mixed = [], 0, [], []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            L = match.trim_pad(code)
            ext = residuals.EXTENT.get(va)
            if ext is None or residuals.has_jumptable(code[:L], va):
                continue
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            cm = match.mask(code, relocs, L)
            om = match.mask(residuals.EXE[foff:foff + L], relocs, L)
            ndiff = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])

            oe = has_ebp_frame(residuals.EXE[foff:foff + ext], va)
            ue = has_ebp_frame(code[:L], va)
            if oe != ue:                 # not comparable: different frame KIND, and that is
                mixed.append((va, name, oe, ue))   # a finding in its own right
                continue
            o = frame_traffic(residuals.EXE[foff:foff + ext], va, oe)
            u = frame_traffic(code[:L], va, ue)
            d = tuple(u[i] - o[i] for i in range(3))

            if ndiff == 0 and L == ext:                      # positive control
                control += 1
                if any(d):
                    failures.append("0x%08x %s reports %r" % (va, name, d))
                continue
            if ndiff == 0 or (not show_all and L == ext):
                continue
            tot = sum(abs(x) for x in d)
            if tot < floor:
                continue
            rows.append((tot, L - ext, va, os.path.relpath(cpp, ROOT), name, o, u, d, ndiff))

    if failures:
        for f in failures:
            sys.stderr.write("!! CONTROL FAIL: byte-exact %s\n" % f)
        sys.stderr.write("!! %d byte-exact function(s) disagree -- output is NOT trustworthy\n"
                         % len(failures))
        return 1
    print("# positive control: %d byte-exact functions, ALL report a zero delta." % control)
    print("# STACK-RESIDENCY delta per residual: OURS minus ORIGINAL (v139).")
    print("# rd>0 = we read the frame more => WE spilled something the original enregisters")
    print("#        (levers: lesson #69 live-range start, #59 both-arms, #43 named local)")
    print("# rd<0 = the ORIGINAL homed something we enregister; we are usually length-SHORT")
    print("%-11s %-22s %-40s %6s %5s %14s %14s   %s"
          % ("addr", "TU", "name", "len", "diff", "orig r/w/lea", "ours r/w/lea", "delta"))
    for tot, dl, va, cpp, name, o, u, d, ndiff in sorted(rows, key=lambda r: -r[0]):
        print("0x%08x %-22s %-40s %+6d %5d %14s %14s   rd%+d w%+d lea%+d"
              % (va, cpp, name[:40], dl, ndiff,
                 "%d/%d/%d" % o, "%d/%d/%d" % u, d[0], d[1], d[2]))
    for va, name, oe, ue in mixed:
        print("!! FRAME-KIND MISMATCH 0x%08x %s: orig ebp-frame=%s ours=%s (not scored)"
              % (va, name, oe, ue))
    print("\n%d residual(s) listed. positive control: %d byte-exact functions, ALL CLEAN."
          % (len(rows), control))
    return 0


if __name__ == "__main__":
    sys.exit(main())
