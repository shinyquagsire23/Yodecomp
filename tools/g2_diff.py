#!/usr/bin/env python3
"""g2_diff.py — Phase G2 whole-image layout diff.

Reads the /MAP from tools/g2_link.sh and, for every `// FUNCTION: YODA 0xADDR` marker across
src/**, reports whether our LINKED image placed that function at its exact ORIGINAL address
(layout match) and whether the bytes are identical (content match, reloc-masked).

Two orthogonal signals:
  LAYOUT  = linked Rva+Base == original marker address   (the G2 link-reproduction goal)
  CONTENT = reloc-masked bytes identical                 (the 212 per-TU exact story)

Usage:  python3 tools/g2_diff.py            (uses $CLAUDE_JOB_DIR/tmp/g2/yoda.map)
        python3 tools/g2_diff.py --map PATH --show-misplaced
"""
import os, re, sys, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match  # coff_functions, pair_by_name, mask, trim_pad

MAP = os.path.join(os.environ.get("CLAUDE_JOB_DIR", ROOT), "tmp", "g2", "yoda.map")

def parse_map(path):
    """mangled name -> linked Rva+Base (int). Publics-by-Value section."""
    d = {}
    started = False
    for ln in open(path, errors="replace"):
        if "Publics by Value" in ln:
            started = True; continue
        if not started:
            continue
        # " 0001:00000000       ?OnTimer@AppWnd@@IAEXI@Z   00401000 f AppData.obj"
        m = re.match(r"\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s+(\S+)", ln)
        if m:
            name, rva, obj = m.group(1), int(m.group(2), 16), m.group(3)
            # first occurrence wins (COMDAT folds list all aliases at one addr; keep the first)
            d.setdefault(name, (rva, obj))
    return d

def main():
    mappath = MAP
    show_misplaced = "--show-misplaced" in sys.argv
    if "--map" in sys.argv:
        mappath = sys.argv[sys.argv.index("--map") + 1]
    if not os.path.exists(mappath):
        print("map not found:", mappath, "\n  run tools/g2_link.sh first"); return 2

    linked = parse_map(mappath)
    exe = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
    IMGBASE = 0x400000
    # PE .text file-offset mapping: read section headers to translate VA->file offset.
    import struct
    pe = exe.find(b"PE\0\0")
    nsec = struct.unpack_from("<H", exe, pe + 6)[0]
    opt = struct.unpack_from("<H", exe, pe + 20)[0]
    sh = pe + 24 + opt
    secs = []
    for i in range(nsec):
        o = sh + i * 40
        va = struct.unpack_from("<I", exe, o + 12)[0]
        vsz = struct.unpack_from("<I", exe, o + 8)[0]
        raw = struct.unpack_from("<I", exe, o + 20)[0]
        secs.append((va, vsz, raw))
    def va_to_off(va):
        rva = va - IMGBASE
        for vstart, vsz, raw in secs:
            if vstart <= rva < vstart + vsz:
                return raw + (rva - vstart)
        return None

    tot = layout_ok = content_ok = both_ok = missing = 0
    misplaced = []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        text = open(cpp).read()
        funcs = match.coff_functions(os.path.join(ROOT, "build",
                    os.path.splitext(os.path.basename(cpp))[0] + ".obj"))
        for addr, name, code, relocs in match.pair_by_name(text, funcs):
            tot += 1
            info = linked.get(name)
            if info is None:
                missing += 1
                continue
            laddr = info[0]
            lay = (laddr == addr)
            # content: masked our code vs original bytes at marker addr
            off = va_to_off(addr)
            con = False
            if off is not None:
                orig = exe[off:off + len(code)]
                if len(orig) == len(code):
                    con = match.mask(orig, relocs, len(code)) == match.mask(code, relocs, len(code))
            layout_ok += lay
            content_ok += con
            both_ok += (lay and con)
            if not lay and show_misplaced:
                misplaced.append((addr, laddr, laddr - addr, name))

    print(f"total markers paired to map: {tot - missing}/{tot}  (missing in map: {missing})")
    print(f"  LAYOUT match  (linked addr == orig addr): {layout_ok}/{tot}")
    print(f"  CONTENT match (reloc-masked bytes equal):  {content_ok}/{tot}")
    print(f"  BOTH  (byte-identical AT the right place):  {both_ok}/{tot}")
    if show_misplaced:
        misplaced.sort(key=lambda r: r[0])
        print("\n-- misplaced (orig -> linked, delta) --")
        for a, l, d, n in misplaced[:80]:
            print(f"  0x{a:06x} -> 0x{l:06x}  {d:+#8x}  {n[:48]}")
        if len(misplaced) > 80:
            print(f"  ... +{len(misplaced)-80} more")

if __name__ == "__main__":
    main()
