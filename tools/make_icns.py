#!/usr/bin/env python3
# make_icns.py <game_exe> <out.icns> [--indy <DESKADV.EXE>]
#
# Build a macOS .icns from the game's own application icon (GROUP_ICON id 2 == IDR_MAINFRAME):
#   • Yoda  (PE32 YodaDemo.exe / Yodesk.exe): read the icon straight out of the .rsrc.
#   • Indy  (16-bit NE DESKADV.EXE, via --indy): reuse make_res.indy_icon().
# The source is a 32x32 DIB icon (genuine pixel art). We decode it to RGBA ourselves and upscale
# with NEAREST-NEIGHBOUR to the iconset sizes so the pixels stay CRISP — sips' default interpolation
# smears a mostly-white icon into a faint "ghost". macOS-only for the final pack (needs iconutil).
import sys, os, struct, subprocess, tempfile, shutil, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import reslib


def decode_dib_icon(img):
    """Decode an RT_ICON DIB (BITMAPINFOHEADER + palette + XOR bits + 1bpp AND mask, bottom-up)
    into (w, h, rows) where rows is a top-to-bottom list of bytearray(w*4) RGBA scanlines."""
    hdr = struct.unpack_from('<IiiHHIIiiII', img, 0)
    w, h2, planes, bpp = hdr[1], hdr[2], hdr[3], hdr[4]
    h = h2 // 2                                     # height is doubled (colour rows + mask rows)
    pal_n = hdr[9] if hdr[9] else (1 << bpp if bpp <= 8 else 0)
    po = 40
    palette = [struct.unpack_from('<BBBB', img, po + i * 4) for i in range(pal_n)]  # B,G,R,x
    xor_stride = ((w * bpp + 31) // 32) * 4
    and_stride = ((w + 31) // 32) * 4
    xo = po + pal_n * 4
    ao = xo + xor_stride * h
    rows = []
    for y in range(h):
        sy = h - 1 - y                              # DIBs are bottom-up
        xrow = xo + sy * xor_stride
        arow = ao + sy * and_stride
        out = bytearray(w * 4)
        for x in range(w):
            if bpp == 8:
                idx = img[xrow + x]; b, g, r, _ = palette[idx]
            elif bpp == 4:
                byte = img[xrow + (x >> 1)]
                idx = (byte >> 4) if (x & 1) == 0 else (byte & 0xf)
                b, g, r, _ = palette[idx]
            elif bpp == 1:
                byte = img[xrow + (x >> 3)]
                idx = (byte >> (7 - (x & 7))) & 1
                b, g, r, _ = palette[idx]
            elif bpp == 24:
                b, g, r = img[xrow + x * 3], img[xrow + x * 3 + 1], img[xrow + x * 3 + 2]
            elif bpp == 32:
                b, g, r = img[xrow + x * 4], img[xrow + x * 4 + 1], img[xrow + x * 4 + 2]
            else:
                raise SystemExit(f"unsupported icon bpp {bpp}")
            transparent = (img[arow + (x >> 3)] >> (7 - (x & 7))) & 1
            o = x * 4
            out[o], out[o + 1], out[o + 2], out[o + 3] = r, g, b, (0 if transparent else 255)
        rows.append(out)
    return w, h, rows


def scale_nn(rows, sw, sh, dw, dh):
    """Nearest-neighbour resample (keeps pixel-art edges crisp at any size)."""
    out = []
    for dy in range(dh):
        srow = rows[dy * sh // dh]
        drow = bytearray(dw * 4)
        for dx in range(dw):
            sx = dx * sw // dw
            drow[dx * 4:dx * 4 + 4] = srow[sx * 4:sx * 4 + 4]
        out.append(drow)
    return out


def write_png(path, w, h, rows):
    def chunk(typ, data):
        return struct.pack('>I', len(data)) + typ + data + \
               struct.pack('>I', zlib.crc32(typ + data) & 0xffffffff)
    raw = bytearray()
    for row in rows:
        raw.append(0)                               # filter type 0 (none)
        raw += row
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))  # 8-bit RGBA
    png += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    png += chunk(b'IEND', b'')
    open(path, 'wb').write(png)


def game_icon_dib(exe, indy_exe):
    if indy_exe:
        import make_res
        _grp, image = make_res.indy_icon(indy_exe)
    else:
        grp = reslib.pe_leaf(exe, reslib.RT_GROUP_ICON, 2)
        assert grp is not None, "GROUP_ICON id 2 (IDR_MAINFRAME) not found in " + exe
        member = struct.unpack_from('<H', grp, 6 + 12)[0]   # first GRPICONDIRENTRY tail = RT_ICON id
        image = reslib.pe_leaf(exe, reslib.RT_ICON, member)
        assert image is not None, f"RT_ICON id {member} not found"
    return image


# name -> pixel size for the .iconset (Retina @2x variants are the next size up)
ICONSET = [
    ("icon_16x16.png", 16), ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32), ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128), ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256), ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512), ("icon_512x512@2x.png", 1024),
]


def main():
    exe, out = sys.argv[1], sys.argv[2]
    indy_exe = None
    a = sys.argv[3:]
    while a:
        if a[0] == '--indy':
            indy_exe = a[1]; a = a[2:]
        else:
            raise SystemExit(f"unknown arg {a[0]}")

    w, h, rows = decode_dib_icon(game_icon_dib(exe, indy_exe))
    tmp = tempfile.mkdtemp(prefix="mkicns_")
    try:
        iconset = os.path.join(tmp, "app.iconset")
        os.mkdir(iconset)
        for name, size in ICONSET:
            write_png(os.path.join(iconset, name), size, size, scale_nn(rows, w, h, size, size))
        subprocess.run(["iconutil", "-c", "icns", iconset, "-o", out], check=True)
        print(f"wrote {out}  (crisp {w}x{h} nearest-neighbour)")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
