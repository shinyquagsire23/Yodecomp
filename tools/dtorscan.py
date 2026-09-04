#!/usr/bin/env python3
"""DESTRUCTOR-POSITION target list (v116) — the seam v110 opened and never automated.

⭐ What it looks for. A C++ object's destructor runs at the closing brace of the scope that
declares it. cl 10.20 marks each such scope with an EH-state store `mov dword ptr [ebp-4], imm`
followed by the dtor `call`. So the POSITION of that store inside a loop body is a direct
readout of how the 1997 author SCOPED the object:

    orig:  ... mov [ebp-4],-1 ; call ~CBrush ; add y,0x20 ; inc slot ; inc i ; <backedge>
    ours:  ... add y,0x20 ; inc slot ; inc i ; mov [ebp-4],-1 ; call ~CBrush ; <backedge>

The original's dtor runs BEFORE the increments, which an object declared directly in the
do-body CANNOT produce — its dtor must run at the body's closing brace, i.e. after them.
The fix is an explicit nested block around the drawing work. That is exactly what took
DrawTextA from 24 B to 2 B at v110; this scan finds the rest.

⚠ READ A HIT AS A CANDIDATE, NOT A DEFECT. Loop bodies are matched by ORDINAL (both streams
sorted by backedge address), so a function whose loops do not correspond 1:1 can produce a
spurious row. Confirm with `tools/bytediff.py` and try the nested block with `vartest.py`.

⚠ THE FILTERS THAT MATTER (all three are v110 warnings, encoded here):
  - `[ebp-4]` holding a REGISTER is a spill or an EH-state-zero reuse, NOT a dtor tell. Only
    `mov [ebp-4], IMMEDIATE` counts.
  - "a call appears in the diff" is NOT a filter — it matches almost everything. The signal is
    the store's POSITION relative to the loop's induction increments.
  - `add esp,imm` is stack cleanup and `add ebp,imm` is frame work; neither is an induction
    increment. Both are excluded.

⭐ Baseline rule (v100/v101/v103/v109): prints a positive control before any finding — two
synthetic loop bodies, one that MUST be flagged and one that must not, plus the paired count.
An empty result from a scan is a bug hypothesis, not a finding.

⚠ COMPILES (it needs our bytes), so it touches build/*.obj — do NOT run it while a vartest.py
sweep is in flight.

Usage:
    python3 tools/dtorscan.py                 # every TU
    python3 tools/dtorscan.py src/Canvas.cpp  # one TU
    python3 tools/dtorscan.py --selftest
"""
import os, sys, glob, capstone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match
import bytediff

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True
X86 = capstone.x86


def is_eh_store(i):
    """`mov dword ptr [ebp-4], <immediate>` — the scope/EH-state marker. A REGISTER source
    is a spill, not a scope marker (v110)."""
    if i.mnemonic != "mov" or len(i.operands) != 2:
        return False
    d, s = i.operands
    return (d.type == X86.X86_OP_MEM and d.mem.base == X86.X86_REG_EBP
            and d.mem.index == 0 and d.mem.disp == -4 and s.type == X86.X86_OP_IMM)


def is_increment(i):
    """An induction increment: `inc reg` or `add reg, imm`. esp/ebp are stack/frame work."""
    if not i.operands or i.operands[0].type != X86.X86_OP_REG:
        return False
    if i.operands[0].reg in (X86.X86_REG_ESP, X86.X86_REG_EBP):
        return False
    if i.mnemonic == "inc" and len(i.operands) == 1:
        return True
    return (i.mnemonic == "add" and len(i.operands) == 2
            and i.operands[1].type == X86.X86_OP_IMM)


def loops(ins, base, end):
    """[(body_start, backedge_addr)] for every backward branch, sorted by backedge."""
    out = []
    for i in ins:
        if not i.mnemonic.startswith("j") or len(i.operands) != 1:
            continue
        if i.operands[0].type != X86.X86_OP_IMM:
            continue
        t = i.operands[0].imm
        if base <= t < i.address:
            out.append((t, i.address))
    return sorted(out, key=lambda r: r[1])


def profile(code, base):
    """[(body_start, backedge, incs_after_last_EH_store)] — None where the body has no EH store."""
    ins = list(MD.disasm(bytes(code), base))
    by = sorted(ins, key=lambda i: i.address)
    out = []
    for start, back in loops(ins, base, base + len(code)):
        body = [i for i in by if start <= i.address <= back]
        last = None
        for i in body:
            if is_eh_store(i):
                last = i.address
        if last is None:
            out.append((start, back, None))
            continue
        out.append((start, back, sum(1 for i in body
                                     if i.address > last and is_increment(i))))
    return out


def scan_function(code, relocs, va):
    L = match.trim_pad(code)
    foff = (va - match.TEXT_VA) + match.TEXT_RAW
    orig = bytediff.EXE[foff:foff + L]
    cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
    if not [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]:
        return []
    po, pc = profile(orig[:L], va), profile(bytes(code[:L]), va)
    rows = []
    for n, (o, c) in enumerate(zip(po, pc)):
        oi, ci = o[2], c[2]
        if oi is None or ci is None or oi == ci:
            continue
        # orig dtor runs BEFORE the increments and ours does not => our scope is too WIDE
        if oi > 0 and ci == 0:
            rows.append((n, o[1] - va, oi, ci, "WIDE  (add a nested block)"))
        elif ci > 0 and oi == 0:
            rows.append((n, o[1] - va, oi, ci, "NARROW (remove a nested block)"))
    return rows


def selftest():
    # NOTE: each control's back-jump must land on offset 0 EXACTLY. A displacement that lands
    # mid-instruction makes the loop body start after the EH store, which reads as "no EH store"
    # -- the first draft of this selftest did precisely that and the control caught it.
    # loop body: mov [ebp-4],-1 ; call ; inc esi ; jmp back      -> dtor BEFORE the increment
    a = bytes.fromhex("c745fcffffffff") + b"\xe8\x00\x00\x00\x00" + b"\x46" + b"\xeb\xf1"
    # loop body: inc esi ; mov [ebp-4],-1 ; call ; jmp back      -> dtor AFTER the increment
    b = b"\x46" + bytes.fromhex("c745fcffffffff") + b"\xe8\x00\x00\x00\x00" + b"\xeb\xf1"
    pa, pb = profile(a, 0x401000), profile(b, 0x401000)
    assert pa and pb, "control produced no loop"
    assert pa[0][2] == 1, "dtor-before-increment control: expected 1 inc after the EH store, got %r" % (pa[0][2],)
    assert pb[0][2] == 0, "dtor-after-increment control: expected 0, got %r" % (pb[0][2],)
    # a REGISTER store to [ebp-4] must NOT count as a scope marker
    r = b"\x89\x75\xfc" + b"\x46" + b"\xeb\xfa"
    assert profile(r, 0x401000)[0][2] is None, "register spill to [ebp-4] was read as an EH store"
    # esp/ebp adjustments must not count as induction increments
    e = bytes.fromhex("c745fcffffffff") + b"\x83\xc4\x0c" + b"\xeb\xf4"
    assert profile(e, 0x401000)[0][2] == 0, "`add esp,12` was read as an induction increment"
    print("positive control: dtor-before-inc profiles as 1 and dtor-after-inc as 0; a register "
          "spill to [ebp-4] is not an EH store; `add esp,imm` is not an increment")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    selftest()
    if "--selftest" in sys.argv:
        return 0
    cpps = [a if os.path.exists(a) else os.path.join(ROOT, a) for a in args] or \
        sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp")))
    rows, npaired = [], 0
    for cpp in cpps:
        for va, name, code, relocs in bytediff.paired(cpp):
            npaired += 1
            hits = scan_function(code, relocs, va)
            if hits:
                rows.append((va, os.path.basename(cpp), name, hits))
    print("positive control: %d functions paired across %d TU(s)" % (npaired, len(cpps)))
    print("\nEH-state store sits on the OTHER side of the loop increments (dtor position)")
    print("%-12s %-20s %-32s %s" % ("addr", "tu", "name", "loops"))
    for va, tu, name, hits in sorted(rows, key=lambda r: -len(r[3])):
        print("0x%08x   %-20s %-32s %d" % (va, tu, name[:32], len(hits)))
        for n, boff, oi, ci, verdict in hits:
            print("        loop #%-2d backedge @+0x%03x  orig %d inc(s) after the EH store, "
                  "ours %d  -> our scope looks too %s" % (n, boff, oi, ci, verdict))
    print("\n%d function(s) with a dtor-position mismatch, of %d paired" % (len(rows), npaired))
    return 0


if __name__ == "__main__":
    sys.exit(main())
