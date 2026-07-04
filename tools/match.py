#!/usr/bin/env python3
"""Match harness: compile a .cpp with the VC++ 4.2 toolchain, then byte-compare
each annotated function against the original in YodaDemo.exe (relocations masked).

Reads `// FUNCTION: YODA 0xADDR` markers from the source to learn each function's
original address (in source order), pairs them positionally with the .obj's COMDAT
function sections (MSVC emits one .text COMDAT per C++ function), trims trailing
int3/nop padding to get each function's true length, and diffs against the exe.

Usage:  tools/match.py src/World/World.cpp [--exe YodaDemo/YodaDemo.exe]
        (expects the compiled .obj next to the .cpp, same basename)
"""
import re, struct, sys, os, subprocess

TEXT_VA, TEXT_RAW = 0x401000, 0x400   # YodaDemo .text: VA 0x401000 -> file 0x400
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def coff_functions(objpath):
    d = open(objpath, "rb").read()
    nsec = struct.unpack_from("<H", d, 2)[0]
    symoff = struct.unpack_from("<I", d, 8)[0]
    nsym = struct.unpack_from("<I", d, 12)[0]
    opt = struct.unpack_from("<H", d, 16)[0]
    sh = 20 + opt
    strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        off = sh + i * 40
        name = d[off:off + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, flags = struct.unpack_from("<IIIIIIHHI", d, off + 8)
        secs.append((name, rawptr, rawsz, relptr, nrel))

    def symname(rec):
        if rec[:4] == b"\0\0\0\0":
            o = struct.unpack_from("<I", rec, 4)[0]
            e = d.index(b"\0", strtab + o)
            return d[strtab + o:e].decode("latin1")
        return rec.rstrip(b"\0").decode("latin1")

    funcs = []  # (name, code_bytes, reloc_offsets)
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        if typ == 0x20 and secn > 0:                 # function symbol
            nm, rawptr, rawsz, relptr, nrel = symname(rec), *secs[secn - 1][1:]
            code = d[rawptr:rawptr + rawsz]
            relocs = [struct.unpack_from("<IIH", d, relptr + r * 10)[0] for r in range(nrel)]
            funcs.append((nm, code, relocs))
        i += 1 + naux
    return funcs


def trim_pad(code):
    n = len(code)
    while n > 0 and code[n - 1] in (0xCC, 0x90):
        n -= 1
    return n


def mask(buf, offs, n):
    b = bytearray(buf[:n])
    for o in offs:
        for k in range(4):
            if o + k < n:
                b[o + k] = 0
    return bytes(b)


def main():
    src = sys.argv[1]
    exe = sys.argv[sys.argv.index("--exe") + 1] if "--exe" in sys.argv else os.path.join(ROOT, "YodaDemo/YodaDemo.exe")
    obj = os.path.splitext(src)[0] + ".obj"
    if not os.path.exists(obj):
        print("obj not found; compile first:", obj); return 2

    addrs = [int(m, 16) for m in re.findall(r"//\s*FUNCTION:\s*YODA\s+0x([0-9a-fA-F]+)", open(src).read())]
    funcs = coff_functions(obj)
    if len(funcs) != len(addrs):
        print(f"WARN: {len(addrs)} markers but {len(funcs)} obj functions; matching min()")
    exe_bytes = open(exe, "rb").read()

    def diff_at(code, relocs, va):
        L = trim_pad(code)
        foff = (va - TEXT_VA) + TEXT_RAW
        orig = exe_bytes[foff:foff + L]
        cm, om = mask(code, relocs, L), mask(orig, relocs, L)
        diffs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
        return L, orig, diffs

    # best-fit assignment: pair each obj function with the address it matches best
    ok = 0
    used = set()
    for name, code, relocs in funcs:
        best = None
        for va in addrs:
            if va in used:
                continue
            L, orig, diffs = diff_at(code, relocs, va)
            score = (0 if (not diffs and len(orig) == L) else 1, len(diffs))
            if best is None or score < best[0]:
                best = (score, va, L, orig, diffs)
        _, va, L, orig, diffs = best
        used.add(va)
        status = "MATCH" if (not diffs and len(orig) == L) else f"DIFF({len(diffs)})"
        print(f"  {va:#08x}  {status:>9}  len={L} relocs={len(relocs)}  {name}")
        if status != "MATCH":
            for o in diffs[:6]:
                print(f"       @{o:#05x} orig={orig[o]:02x} ours={code[o]:02x}")
        ok += status == "MATCH"
    print(f"\n{ok}/{len(funcs)} matched")
    return 0 if ok == len(funcs) else 1


if __name__ == "__main__":
    sys.exit(main())
