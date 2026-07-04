#!/usr/bin/env python3
"""Relocation-aware byte-match harness for the Yodecomp decomp.

Compare a function we compiled (a COFF .obj from VC++ 4.2) against the original
bytes in YodaDemo.exe at a given virtual address. Relocation sites (absolute
operands to globals/strings/imports, and rel32 call targets) are masked in BOTH
sides, so a "match" means the code is identical modulo link-time fixups.

Usage:
    tools/bytematch.py --va 0x401490 --obj toolchain/test/func_401490.obj
    tools/bytematch.py --va 0x401490 --obj build/foo.obj --section .text --exe YodaDemo/YodaDemo.exe

Assumes the .obj's chosen section holds exactly the one function (single-function
translation unit), which is how we scale matching one function at a time.
"""
import argparse, struct, sys

# YodaDemo.exe .text: VA 0x401000 maps to file offset 0x400.
TEXT_VA = 0x401000
TEXT_RAW = 0x400


def read_obj_section(path, want):
    d = open(path, "rb").read()
    nsec = struct.unpack_from("<H", d, 2)[0]
    opt = struct.unpack_from("<H", d, 16)[0]
    sh = 20 + opt
    for i in range(nsec):
        off = sh + i * 40
        name = d[off:off + 8].rstrip(b"\0").decode("latin1")
        vsz, vaddr, rawsz, rawptr, relptr, lnptr, nrel, nln, flags = struct.unpack_from(
            "<IIIIIIHHI", d, off + 8)
        if name.startswith(want):
            code = d[rawptr:rawptr + rawsz]
            relocs = [struct.unpack_from("<IIH", d, relptr + r * 10)[0] for r in range(nrel)]
            return code, relocs
    raise SystemExit(f"section {want!r} not found in {path}")


def mask(buf, offs, width=4):
    b = bytearray(buf)
    for o in offs:
        for k in range(width):
            if o + k < len(b):
                b[o + k] = 0
    return bytes(b)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--va", required=True, help="function virtual address, e.g. 0x401490")
    ap.add_argument("--obj", required=True, help="our compiled COFF .obj")
    ap.add_argument("--section", default=".text", help="section holding the function")
    ap.add_argument("--exe", default="YodaDemo/YodaDemo.exe")
    ap.add_argument("--len", default=None, help="override length (default: obj section size)")
    args = ap.parse_args()

    va = int(args.va, 0)
    code, relocs = read_obj_section(args.obj, args.section)
    n = int(args.len, 0) if args.len else len(code)

    exe = open(args.exe, "rb").read()
    foff = (va - TEXT_VA) + TEXT_RAW
    orig = exe[foff:foff + n]

    if len(code) != n:
        code = code[:n]
    om, cm = mask(orig, relocs), mask(code, relocs)
    diffs = [i for i in range(min(len(om), len(cm))) if om[i] != cm[i]]

    print(f"va={va:#x} len={n} obj_relocs={len(relocs)} "
          f"lenmatch={len(code)==len(orig)}")
    if not diffs and len(code) == len(orig):
        print("*** BYTE-IDENTICAL (relocations masked) — 100% CODE MATCH ***")
        return 0
    print(f"masked byte diffs: {len(diffs)}")
    for o in diffs[:16]:
        print(f"  @{o:#05x} orig={orig[o]:02x} ours={code[o]:02x}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
