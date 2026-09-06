#!/usr/bin/env python3
"""Side-by-side disassembly of ORIGINAL vs OURS for one function (v127 method note).

Usage: python3 tools/sbs.py <src.cpp> <0xADDR> [start_off] [end_off]

Unlike asmscore --dump (which prints only differing lines, unreadable when the
schedule shifts) this prints EVERY instruction in the range on both sides,
marking the mismatches.  Reads the compiled COMDAT via residuals.paired(), i.e.
through progress.py's own filtering + pairing block.
"""
import sys
import capstone
import residuals
import match

def main():
    cpp = sys.argv[1]
    va = int(sys.argv[2], 16)
    lo = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0
    hi = int(sys.argv[4], 0) if len(sys.argv) > 4 else 1 << 30

    pairs = residuals.paired(cpp)
    for mva, _name, code, _rel in pairs:
        if mva != va:
            continue
        L = match.trim_pad(code)
        code = code[:L]
        foff = (va - match.TEXT_VA) + match.TEXT_RAW
        orig = residuals.EXE[foff:foff + L + 64]
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        A = list(md.disasm(bytes(orig), va))
        B = list(md.disasm(bytes(code), va))
        ai = {i.address - va: i for i in A}
        bi = {i.address - va: i for i in B}
        offs = sorted(set(ai) | set(bi))
        print("%-8s %-40s | %-40s" % ("off", "ORIGINAL", "OURS"))
        for o in offs:
            if not (lo <= o <= hi):
                continue
            a = ai.get(o)
            b = bi.get(o)
            fa = "%s %s" % (a.mnemonic, a.op_str) if a else ""
            fb = "%s %s" % (b.mnemonic, b.op_str) if b else ""
            ba = a.bytes.hex() if a else ""
            bb = b.bytes.hex() if b else ""
            mark = " " if (fa == fb and ba == bb) else "*"
            print("%s+%-6x %-40s | %-40s   %s | %s" % (mark, o, fa, fb, ba, bb))
        return
    print("no marker %#x in %s" % (va, cpp))

if __name__ == "__main__":
    main()
