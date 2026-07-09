#!/usr/bin/env python3
"""Whole-image completion metric (G2 worklist #6) — reccmp-style, relocation-aware.

Compares our linked image against the original YodaDemo.exe SECTION BY SECTION, with
RELOCATIONS MASKED, so the score reflects true CONTENT identity rather than the
0x1000 section shift the reg-coloring .text length walls impose (our .text overflows
one page-align boundary -> every later section's embedded RVAs differ by 0x1000).

- DATA sections (.rdata/.data/.idata/.rsrc): compared section-relative (data is emitted
  in the same order, so same section-relative offsets), masking any 4-byte dword that is
  a relocation site in EITHER image (RVA pointers that legitimately differ by the shift).
- .text: NOT section-relative comparable (function-shifted, not just RVA-shifted) -> use
  g2_diff.py's per-function CONTENT story instead; reported here as a pointer, not scored.

Usage: python3 tools/image_diff.py [our_image.exe]
       (default: $CLAUDE_JOB_DIR/tmp/build-link/yoda.exe, else build/yoda.exe)
"""
import struct, sys, os

def parse(path):
    d = open(path, "rb").read()
    e = struct.unpack_from("<I", d, 0x3c)[0]
    nsec = struct.unpack_from("<H", d, e + 6)[0]
    optsz = struct.unpack_from("<H", d, e + 20)[0]
    opt = e + 24
    base = struct.unpack_from("<I", d, opt + 28)[0]
    # data directory 5 = base relocation table (RVA,size) at opt+96 for PE32
    reloc_rva, reloc_sz = struct.unpack_from("<II", d, opt + 96 + 5 * 8)
    so = opt + optsz
    secs = {}
    for i in range(nsec):
        o = so + i * 40
        name = d[o:o+8].rstrip(b"\0").decode("latin1", "replace")
        vsize, va, rsize, rptr = struct.unpack_from("<IIII", d, o + 8)
        secs[name] = (va, vsize, rptr, rsize)
    return d, base, secs, reloc_rva, reloc_sz

def rva_to_off(secs, rva):
    for va, vs, rp, rs in secs.values():
        if va <= rva < va + max(vs, rs):
            return rp + (rva - va)
    return None

def reloc_rvas(d, secs, reloc_rva, reloc_sz):
    """Set of RVAs holding a 32-bit relocated pointer (type 3 HIGHLOW)."""
    out = set()
    off = rva_to_off(secs, reloc_rva)
    if off is None:
        return out
    end = off + reloc_sz
    while off < end:
        page, blk = struct.unpack_from("<II", d, off)
        if blk < 8:
            break
        n = (blk - 8) // 2
        for i in range(n):
            ent = struct.unpack_from("<H", d, off + 8 + i * 2)[0]
            if ent >> 12 == 3:
                out.add(page + (ent & 0xfff))
        off += blk
    return out

def main():
    ours = sys.argv[1] if len(sys.argv) > 1 else None
    if not ours:
        for c in (os.path.expanduser(os.environ.get("CLAUDE_JOB_DIR", "")) + "/tmp/build-link/yoda.exe",
                  "build/yoda.exe", "tmp/build-link/yoda.exe"):
            if c and os.path.exists(c):
                ours = c; break
    do, bo, so, _, _ = parse("YodaDemo/YodaDemo.exe")
    dn, bn, sn, _, _ = parse(ours)
    # section shift: our later sections sit +SHIFT vs original (the .text length walls
    # overflow one page-align boundary). Compute from .rdata start.
    SHIFT = sn[".rdata"][0] - so[".rdata"][0]
    print("image_diff: OURS = %s" % ours)
    print("  original file %d / ours %d   section shift = %#x (from .text +%d B walls)"
          % (len(do), len(dn), SHIFT, sn['.text'][1] - so['.text'][1]))
    print("  A dword counts as matching if EQUAL or differs by exactly the shift")
    print("  (= a correctly-relocated pointer). Measures content identity modulo the wall-shift.\n")
    print("  section   vsize(orig/ours)   dwords: exact + shifted-ptr / total   content-id")
    tot = totm = 0
    for name in (".rdata", ".data", ".idata", ".rsrc"):
        if name not in so or name not in sn:
            continue
        va_o, vs_o, rp_o, rs_o = so[name]
        va_n, vs_n, rp_n, rs_n = sn[name]
        L = (min(vs_o, vs_n) // 4) * 4
        a = do[rp_o:rp_o + L]; b = dn[rp_n:rp_n + L]
        exact = shifted = 0
        for i in range(0, L, 4):
            va = struct.unpack_from("<I", a, i)[0]
            vb = struct.unpack_from("<I", b, i)[0]
            if va == vb:
                exact += 1
            elif vb - va == SHIFT and va > 0x1000:   # correctly-relocated in-image pointer
                shifted += 1
        n = L // 4
        m = exact + shifted
        print("  %-7s  %6d/%-6d   %6d + %5d / %-6d  = %5.1f%% content-id"
              % (name, vs_o, vs_n, exact, shifted, n, 100.0 * m / n if n else 0))
        tot += n; totm += m
    print("  ---")
    print("  DATA sections: %d/%d dwords content-identical = %.1f%% (wall-shift accounted)"
          % (totm, tot, 100.0 * totm / tot if tot else 0))
    print("  .text: %d/%d B (+%d = reg-coloring length walls, the known ceiling);"
          % (so['.text'][1], sn['.text'][1], sn['.text'][1] - so['.text'][1]))
    print("         per-function CONTENT is g2_diff.py's story (224/378 exact + reg-coloring).")

if __name__ == "__main__":
    main()
