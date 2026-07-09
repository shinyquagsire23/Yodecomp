#!/usr/bin/env python3
"""bugscan.py -- static correctness-bug oracle for the YodaDemo decompilation.

The committed, reusable form of v34's ephemeral "vtscan" (see CLAUDE.md lessons
#24/#25). For every transcribed function (`// FUNCTION: YODA` markers) it
Needleman-Wunsch aligns our compiled instruction stream against the original
(relocations masked) and flags aligned instruction pairs that share a mnemonic +
operand shape + base register + index but differ only in a memory-operand
DISPLACEMENT. That signature is the fingerprint of the two correctness-bug
classes a raw byte-% shrugs off:

  #24 wrong virtual method   -- `call dword ptr [reg+disp]` at the WRONG vtable
                                slot (adjacent Set/GetXxx CDC/CObArray pairs);
                                the running-EXE oracle catches these.
  #25 wrong inlined accessor -- `mov reg,[reg+disp]` reading the WRONG struct
                                field (AfxGetInstanceHandle +0x8 vs
                                AfxGetResourceHandle +0xc); NOT runtime-observable
                                in a static-MFC EXE, so ONLY this static scan
                                catches them.

Noise control: reg-alloc / scheduling drift changes WHICH register holds a
pointer, so requiring the SAME base register on both sides filters most reg-alloc
noise. Findings in structurally-identical functions (NW align cost 0 -- the whole
allocation agrees, only names may be bijected) are HIGH confidence; findings in
drifted functions are LOW confidence (a matching register NAME may hold a
different pointer -- e.g. StartGame [esi+0x4b4]/[esi+0x4b0] both target the same
absolute grid) and are flagged for manual review only.

Absolute `[disp32]` operands (base=None) are relocation-masked globals/imports and
are skipped. Stack-relative operands ([ebp/esp+disp]) are reported in a separate
low-interest bucket (frame-layout, not field/slot bugs).

Usage:
  python3 tools/bugscan.py [src/Worldgen.cpp ...]      # default: all src/**/*.cpp
  python3 tools/bugscan.py --all                    # include LOW-confidence bucket
Requires build/<TU>.obj to exist (compile first, exactly like verify.py/match.py).
"""
import os
import re
import sys
import glob

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capstone
import match
import asmscore

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
_STACK = {"ebp", "esp"}

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_md.detail = True


def _memops(raw):
    """[(base, index, scale, disp)] for each memory operand of the single
    instruction encoded by `raw`, in operand order (regs lower-cased)."""
    for ins in _md.disasm(bytes(raw), 0):
        out = []
        for op in ins.operands:
            if op.type == capstone.x86.X86_OP_MEM:
                m = op.mem
                base = ins.reg_name(m.base).lower() if m.base else None
                index = ins.reg_name(m.index).lower() if m.index else None
                out.append((base, index, m.scale, m.disp))
        return out
    return []


def scan_func(addr, code, relocs):
    """Return (align_cost, [finding]) for one function. A finding is a dict with
    the aligned instruction pair that shares base+index but differs in disp."""
    L = match.trim_pad(code)
    foff = (addr - match.TEXT_VA) + match.TEXT_RAW
    # +16 slack: asmscore._decode trims at the first 0xCC pad, so an over-long
    # slice self-truncates at the real function end (16-byte align guarantees pad).
    a = asmscore._decode(EXE[foff:foff + L + 16])
    b = asmscore._decode(code[:L], relocs)
    align_cost, pairs = asmscore._align(a, b)
    findings = []
    for ia, ib in pairs:
        if ia is None or ib is None:
            continue
        ai, bi = a[ia], b[ib]
        if ai.struct_key() != bi.struct_key():
            continue
        ma, mb = _memops(ai.raw), _memops(bi.raw)
        if len(ma) != len(mb):
            continue
        for (baseA, idxA, scA, dispA), (baseB, idxB, scB, dispB) in zip(ma, mb):
            if baseA is None or baseB is None:       # absolute [disp32] -> reloc/global
                continue
            if baseA != baseB or idxA != idxB or scA != scB:
                continue                             # different pointer reg -> reg-alloc noise
            if dispA == dispB:
                continue
            findings.append({
                "off": ai.off, "mnem": ai.mnem, "base": baseA,
                "dispA": dispA, "dispB": dispB,
                "stack": baseA in _STACK,
                "call": ai.mnem in ("call", "jmp"),
            })
    return align_cost, findings


def _disp(v):
    return ("-0x%x" % -v) if v < 0 else ("0x%x" % v)


def main():
    argv = [x for x in sys.argv[1:] if not x.startswith("--")]
    show_low = "--all" in sys.argv
    srcs = argv or sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True))

    hi, low, frame = [], [], []
    for src in srcs:
        base = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(ROOT, "build", base + ".obj")
        if not os.path.exists(obj):
            print("!! skip %s (no %s -- compile first)" % (os.path.relpath(src, ROOT),
                                                           os.path.relpath(obj, ROOT)))
            continue
        text = open(src).read()
        funcs = match.coff_functions(obj)
        for addr, name, code, relocs in match.pair_by_name(text, funcs):
            align_cost, finds = scan_func(addr, code, relocs)
            for f in finds:
                f["addr"], f["name"], f["src"] = addr, name, os.path.relpath(src, ROOT)
                f["align"] = align_cost
                if f["stack"]:
                    frame.append(f)
                elif align_cost == 0:
                    hi.append(f)
                else:
                    low.append(f)

    def dump(title, rows):
        print("\n=== %s (%d) ===" % (title, len(rows)))
        for f in rows:
            kind = "CALL[slot]" if f["call"] else "mem[field]"
            print("  %s  %#010x %-38s @+%#05x  %-11s %s base=%s orig=%s ours=%s"
                  % (f["src"], f["addr"], f["name"][:38], f["off"],
                     f["mnem"], kind, f["base"], _disp(f["dispA"]), _disp(f["dispB"])))

    # SYSTEMATIC patterns: the same (function, mnem, base, dispA, dispB) tuple repeated
    # is the fingerprint of a real semantic bug an inlined accessor/vtable-slot mistake
    # produces at every call site (the v34 AfxGetResourceHandle bug fired 14x as
    # `mov eax,[eax+0xc]`->`[eax+0x8]`), as opposed to scattered reg-alloc noise where
    # a walking pointer's disps all differ. Group across HIGH+LOW non-stack findings.
    groups = {}
    for f in hi + low:
        k = (f["addr"], f["name"], f["mnem"], f["base"], f["dispA"], f["dispB"])
        groups.setdefault(k, []).append(f)
    # A one-directional shift (every site orig=X ours=Y, never the reverse) is the
    # fingerprint of a genuine wrong-field/wrong-slot bug (v34 Afx: orig=0xc ours=0x8
    # x14, no reverse). A BIDIRECTIONAL swap (orig=X ours=Y AND orig=Y ours=X in
    # comparable numbers) is instead two independent writes/reads REORDERED by the
    # scheduler -- NW pairs them crosswise (e.g. ShowWinMessage's field30 if/else arm
    # crossing). Rank one-directional shifts first and label swaps so they don't read
    # as bugs.
    def _rev_count(addr, name, mnem, base, da, db):
        return len(groups.get((addr, name, mnem, base, db, da), []))

    systematic = []
    for k, v in groups.items():
        if len(v) < 3:
            continue
        rev = _rev_count(*k)
        systematic.append([k, v, "SWAP(sched)" if rev >= len(v) // 2 else "SHIFT(bug?)"])
    systematic.sort(key=lambda kv: (kv[2].startswith("SWAP"), -len(kv[1])))

    def dump(title, rows):
        print("\n=== %s (%d) ===" % (title, len(rows)))
        for f in rows:
            kind = "CALL[slot]" if f["call"] else "mem[field]"
            print("  %s  %#010x %-38s @+%#05x  %-11s %s base=%s orig=%s ours=%s"
                  % (f["src"], f["addr"], f["name"][:38], f["off"],
                     f["mnem"], kind, f["base"], _disp(f["dispA"]), _disp(f["dispB"])))

    bugs = [s for s in systematic if s[2].startswith("SHIFT")]
    print("\n=== SYSTEMATIC -- identical (mnem,base,disp) mismatch >=3x in one func"
          " (%d; SHIFT=likely #24/#25 bug, SWAP=scheduling) ===" % len(systematic))
    for k, v, tag in systematic:
        addr, name, mnem, base, da, db = k
        print("  %-11s %#010x %-38s %2dx  %-6s [%s] orig=%s ours=%s%s"
              % (tag, addr, name[:38], len(v), mnem, base,
                 _disp(da), _disp(db), "  <CALL-slot!>" if v[0]["call"] else ""))

    dump("HIGH confidence -- drift-free field/slot disp mismatch (likely #24/#25 bug)", hi)
    if show_low:
        dump("LOW confidence -- drifted func, same reg NAME (manual review)", low)
        dump("FRAME -- stack-relative disp mismatch (frame layout, usually noise)", frame)
    else:
        print("\n(%d low-confidence + %d frame findings hidden; --all to show)"
              % (len(low), len(frame)))
    print("\n%d HIGH-confidence finding(s); %d SHIFT (likely bug), %d SWAP (scheduling)."
          % (len(hi), len(bugs), len(systematic) - len(bugs)))
    return 1 if (hi or bugs) else 0


if __name__ == "__main__":
    sys.exit(main())
