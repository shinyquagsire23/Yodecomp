#!/usr/bin/env python3
"""IF/ELSE ARM-ORDER target list (v116, lesson #47).

⭐ What it looks for. cl 10.20 emits the *then* arm as the FALLTHROUGH and branches over
it to the *else* arm, so the source arm order is directly readable off the disassembly.
When the 1997 author wrote the arms in the opposite order to ours, the emitted code differs
by exactly one thing at the top of the diamond: the jcc's POLARITY is inverted (`je` where
we emit `jne`), with the two arm bodies swapped behind it.

That is what took StartGame 0x4037a0 from 79 B to 69 B at v115, where it was found BY EYE.
This is the scan that finds the rest.

⚠ THE SIGNAL IS THE POLARITY FLIP, NOT THE CONDITION'S SPELLING. `!x` vs `x != 0`, `n++`
vs `n = n + 1` are all INERT (measured at v115, both at 79 B). Only swapping the arms in
the source moves anything. Do not "fix" a hit by negating the condition.

⚠ Distinguish from an `if (!x) return;` early-out: that has only ONE arm and no order to
vary. This scan requires BOTH arms to be present in the original — the fallthrough arm must
end in an unconditional `jmp` FORWARD past the jcc's target (the classic if/else diamond) —
and reports one-armed sites separately, as weaker candidates.

⚠ A hit is a CANDIDATE, not a defect. A jcc polarity flip can also fall out of a register
bijection or an upstream schedule shift that happens to land a compare the other way round.
Confirm with `tools/bytediff.py` and try the arm swap with `tools/vartest.py --expect N`.

⚠ Linear-sweep disassembly desyncs on an embedded switch JUMP TABLE (v100's "8 fake class-D
targets", v110's epilogue-pops bug). This scan is resistant by construction: it only ever
compares instructions that BOTH streams decode at the SAME offset with the SAME length, and
everything before the first differing byte is byte-identical, so the alignment at the first
run is guaranteed. Sites deep past the first diff are flagged `~` (alignment not guaranteed).

⭐ Baseline rule (v100/v101/v103/v109): prints a positive control before any finding — a
synthetic je/jne pair the detector MUST flag, a jne/jne pair it must NOT, and the
paired-function count. An empty result from a scan is a bug hypothesis, not a finding.

⚠ Unlike loopform.py/unrotscan.py this tool COMPILES (it needs our bytes to compare
polarity), so it touches build/*.obj — do NOT run it while a vartest.py sweep is in flight.

Usage:
    python3 tools/armscan.py                 # every TU
    python3 tools/armscan.py src/Score.cpp   # one TU
    python3 tools/armscan.py --selftest      # controls only
"""
import os, sys, glob, capstone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match
import bytediff

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
MD.detail = True   # op_str prints small targets WITHOUT "0x" — parse the operand, not the text

# cl 10.20 emits the *then* arm as fallthrough, so a source arm swap shows up as the
# jcc flipping to its exact inverse. Signed/unsigned pairs are kept distinct on purpose:
# a jl<->jae pair is a COMPARE-KIND change, not an arm swap, and must not be flagged.
INVERSE = {
    "je": "jne", "jne": "je", "jz": "jnz", "jnz": "jz",
    "jl": "jge", "jge": "jl", "jle": "jg", "jg": "jle",
    "jb": "jae", "jae": "jb", "jbe": "ja", "ja": "jbe",
    "js": "jns", "jns": "js", "jo": "jno", "jno": "jo",
    "jp": "jnp", "jnp": "jp", "jpe": "jpo", "jpo": "jpe",
}


def decode(buf, base=0):
    """offset -> instruction, from a linear sweep."""
    return {i.address - base: i for i in MD.disasm(buf, base)}


def target(i):
    """Branch target as an absolute address, from the OPERAND (see the MD.detail note)."""
    ops = i.operands
    if len(ops) != 1 or ops[0].type != capstone.x86.X86_OP_IMM:
        return None
    return ops[0].imm


def two_armed(ins, i):
    """True if `i` is the top of an if/else DIAMOND: the fallthrough arm ends in an
    unconditional jmp forward past i's target (so both arms exist and have an order)."""
    t = target(i)
    if t is None or t <= i.address:
        return False
    prev = [j for j in ins.values() if j.address + j.size == t]
    if not prev:
        return False
    p = prev[0]
    if p.mnemonic != "jmp":
        return False
    pt = target(p)
    return pt is not None and pt > t


def scan_function(code, relocs, va, verbose=False):
    """[(off, orig_mn, our_mn, two_armed, aligned)] polarity flips."""
    L = match.trim_pad(code)
    foff = (va - match.TEXT_VA) + match.TEXT_RAW
    orig = bytediff.EXE[foff:foff + L]
    cm = match.mask(code, relocs, L)
    om = match.mask(orig, relocs, L)
    offs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
    if not offs:
        return []
    first = offs[0]
    diffset = set(offs)
    oi, ci = decode(orig[:L]), decode(bytes(code[:L]))
    out = []
    for off, o in sorted(oi.items()):
        c = ci.get(off)
        if c is None or c.size != o.size:
            continue
        if o.mnemonic not in INVERSE or INVERSE[o.mnemonic] != c.mnemonic:
            continue
        # the flip itself must be part of the byte diff, not an incidental decode
        if not (diffset & set(range(off, off + o.size))):
            continue
        out.append((off, o.mnemonic, c.mnemonic, two_armed(oi, o), off <= first))
    return out


def selftest():
    # A synthetic diamond the detector MUST flag: test eax,eax / je +5 / <then> / jmp / <else>
    # ours emits jne at the same offset with the same size.
    o = bytes.fromhex("85c0") + b"\x74\x05" + b"\x40" * 3 + b"\xeb\x02" + b"\x41" * 2 + b"\xc3"
    c = bytes.fromhex("85c0") + b"\x75\x05" + b"\x40" * 3 + b"\xeb\x02" + b"\x41" * 2 + b"\xc3"
    oi, ci = decode(o), decode(c)
    assert oi[2].mnemonic == "je" and ci[2].mnemonic == "jne", "control decode failed"
    assert INVERSE[oi[2].mnemonic] == ci[2].mnemonic, "inverse table failed"
    assert two_armed(oi, oi[2]), "diamond detector failed on a two-armed control"
    # a one-armed early-out must NOT be called two-armed
    o1 = bytes.fromhex("85c0") + b"\x74\x01" + b"\x40" + b"\xc3"
    assert not two_armed(decode(o1), decode(o1)[2]), "diamond detector fired on one arm"
    # jne/jne must not be flagged as a flip
    assert INVERSE["jne"] != "jne"
    # signed/unsigned must stay distinct (jl<->jae is a compare-kind change, not a swap)
    assert INVERSE["jl"] == "jge" and INVERSE["jb"] == "jae"
    print("positive control: synthetic je/jne diamond FLAGGED, one-armed early-out NOT "
          "two-armed, jne/jne not a flip, jl<->jae kept distinct")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    selftest()
    if "--selftest" in sys.argv:
        return 0
    cpps = [os.path.join(ROOT, a) if not os.path.exists(a) else a for a in args] or \
        sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp")))
    rows, npaired, nresid = [], 0, 0
    for cpp in cpps:
        for va, name, code, relocs in bytediff.paired(cpp):
            npaired += 1
            hits = scan_function(code, relocs, va)
            if hits:
                nresid += 1
                rows.append((va, os.path.basename(cpp), name, hits))
    print("positive control: %d functions paired across %d TU(s)" % (npaired, len(cpps)))
    print("\nORIGINAL's jcc is the exact INVERSE of ours at the same offset (lesson #47)")
    print("%-12s %-22s %-34s %s" % ("addr", "tu", "name", "flips (2-armed / total)"))
    def key(r):
        two = sum(1 for h in r[3] if h[3])
        return (-two, -len(r[3]))
    for va, tu, name, hits in sorted(rows, key=key):
        two = [h for h in hits if h[3]]
        print("0x%08x   %-22s %-34s %d / %d" % (va, tu, name[:34], len(two), len(hits)))
        for off, om, cm_, ta, al in hits:
            print("        @+0x%03x  orig %-4s ours %-4s  %s%s" % (
                off, om, cm_, "TWO-ARMED" if ta else "one-armed",
                "" if al else "  ~unaligned (past first diff)"))
    print("\n%d function(s) with a polarity flip, of %d paired" % (nresid, npaired))
    return 0


if __name__ == "__main__":
    sys.exit(main())
