#!/usr/bin/env python3
# reslib.py — shared helpers for building the extended-config .res files (Phase H2/H3 resources).
#
# The byte-match anchor build uses tools/extract_res.py (verbatim YodaDemo .rsrc). The extended
# configs (YODA_FULL, GAME_INDY) need a few resources swapped for the correct game/variant; this
# module has the PE/NE resource readers, the MFC string-table patcher, the 32-bit DLGTEMPLATE
# parser/builder (round-trip-validated), and the .res emitter (byte-compatible with extract_res.py).
import struct

RT_ICON, RT_GROUP_ICON, RT_STRING, RT_DIALOG = 3, 14, 6, 5


# ── PE (.rsrc) reader ───────────────────────────────────────────────────────────────────────────
def pe_leaves(path):
    """Return all resource leaves as mutable [type_key, name_key, lang_key, data] lists.
    A key is ('I', int) for an integer id or ('S', utf16_bytes) for a string name."""
    f = open(path, 'rb').read()
    e_lfanew = struct.unpack_from('<I', f, 0x3c)[0]
    coff = e_lfanew + 4
    nsec, = struct.unpack_from('<H', f, coff + 2)
    optsz, = struct.unpack_from('<H', f, coff + 16)
    sect = coff + 20 + optsz
    rva = raw = None
    for i in range(nsec):
        off = sect + i * 40
        if f[off:off + 8].rstrip(b'\0') == b'.rsrc':
            _, rva, _, raw = struct.unpack_from('<IIII', f, off + 8)
    assert rva is not None, "no .rsrc"

    def at(r):
        return raw + (r - rva)

    leaves = []
    def walk(d, path):
        n_named, = struct.unpack_from('<H', f, d + 12)
        n_id, = struct.unpack_from('<H', f, d + 14)
        for k in range(n_named + n_id):
            name_id, off_to = struct.unpack_from('<II', f, d + 16 + k * 8)
            if name_id & 0x80000000:
                so = raw + (name_id & 0x7fffffff)
                sl, = struct.unpack_from('<H', f, so)
                key = ('S', f[so + 2: so + 2 + sl * 2])
            else:
                key = ('I', name_id & 0xffff)
            if off_to & 0x80000000:
                walk(raw + (off_to & 0x7fffffff), path + [key])
            else:
                de = raw + off_to
                drva, size, _, _ = struct.unpack_from('<IIII', f, de)
                leaves.append([path[0], path[1], key, f[at(drva): at(drva) + size]])
    walk(raw, [])
    return leaves


def pe_leaf(path, rtype, rid):
    """Return the data bytes of one integer-id'd resource, or None."""
    for (t, n, l, d) in pe_leaves(path):
        if t == ('I', rtype) and n == ('I', rid):
            return d
    return None


# ── NE (16-bit) resource reader ─────────────────────────────────────────────────────────────────
def ne_resources(path):
    """Return {(type_int, name_int): data} for integer-typed/named NE resources."""
    f = open(path, 'rb').read()
    ne = struct.unpack_from('<I', f, 0x3c)[0]
    assert f[ne:ne + 2] == b'NE', "not NE"
    rt = ne + struct.unpack_from('<H', f, ne + 0x24)[0]
    shift = struct.unpack_from('<H', f, rt)[0]
    p = rt + 2
    out = {}
    while struct.unpack_from('<H', f, p)[0] != 0:
        tid = struct.unpack_from('<H', f, p)[0]
        cnt = struct.unpack_from('<H', f, p + 2)[0]
        p += 8
        for _ in range(cnt):
            off, length, fl, rid, h, u = struct.unpack_from('<HHHHHH', f, p)
            p += 12
            if (tid & 0x8000) and (rid & 0x8000):
                out[(tid & 0x7fff, rid & 0x7fff)] = f[off << shift: (off << shift) + (length << shift)]
    return out


# ── MFC string table: replace substring `index` (0..15) in a 16-string block ─────────────────────
def patch_stringtable(data, index, new_text):
    strs = []
    q = 0
    for _ in range(16):
        if q >= len(data):
            strs.append('')
            continue
        ln, = struct.unpack_from('<H', data, q)
        q += 2
        strs.append(data[q: q + ln * 2].decode('utf-16le'))
        q += ln * 2
    strs[index] = new_text
    out = bytearray()
    for s in strs:
        out += struct.pack('<H', len(s)) + s.encode('utf-16le')
    return bytes(out)


# ── 32-bit DLGTEMPLATE parse/build (for substituting About-dialog text) ──────────────────────────
def _rd_sz(d, p):
    s = b''
    while struct.unpack_from('<H', d, p)[0] != 0:
        s += d[p:p + 2]
        p += 2
    return s.decode('utf-16le'), p + 2


def _rd_name(d, p):
    """A dialog menu/class/text field: 0x0000 (empty), 0xFFFF+WORD ordinal, or a UTF-16 sz."""
    w, = struct.unpack_from('<H', d, p)
    if w == 0:
        return ('empty', 0), p + 2
    if w == 0xffff:
        return ('ord', struct.unpack_from('<H', d, p + 2)[0]), p + 4
    s, p = _rd_sz(d, p)
    return ('sz', s), p


def _wr_name(field):
    kind, val = field
    if kind == 'empty':
        return b'\x00\x00'
    if kind == 'ord':
        return struct.pack('<HH', 0xffff, val)
    return val.encode('utf-16le') + b'\x00\x00'


def parse_dlg32(d):
    """Parse a classic 32-bit DLGTEMPLATE into (header, controls). header/controls carry the raw
    fields so build_dlg32 round-trips byte-for-byte; edit .caption / a control's .text then rebuild."""
    style, ext, cdit = struct.unpack_from('<IIH', d, 0)
    p = 10
    x, y, cx, cy = struct.unpack_from('<hhhh', d, p)
    p += 8
    menu, p = _rd_name(d, p)
    wclass, p = _rd_name(d, p)
    caption, p = _rd_sz(d, p)
    font = None
    if style & 0x40:  # DS_SETFONT
        pt, = struct.unpack_from('<H', d, p)
        p += 2
        fn, p = _rd_sz(d, p)
        font = (pt, fn)
    header = dict(style=style, ext=ext, rect=(x, y, cx, cy), menu=menu, wclass=wclass,
                  caption=caption, font=font)
    controls = []
    for _ in range(cdit):
        p = (p + 3) & ~3
        cst, cex = struct.unpack_from('<II', d, p)
        p += 8
        ix, iy, icx, icy, iid = struct.unpack_from('<hhhhH', d, p)
        p += 10
        ccls, p = _rd_name(d, p)
        ctext, p = _rd_name(d, p)
        extra, = struct.unpack_from('<H', d, p)
        p += 2
        cdata = d[p:p + extra]
        p += extra
        controls.append(dict(style=cst, ext=cex, rect=(ix, iy, icx, icy), id=iid,
                             wclass=ccls, text=ctext, data=cdata))
    return header, controls


def build_dlg32(header, controls):
    out = bytearray()
    x, y, cx, cy = header['rect']
    out += struct.pack('<IIH', header['style'], header['ext'], len(controls))
    out += struct.pack('<hhhh', x, y, cx, cy)
    out += _wr_name(header['menu'])
    out += _wr_name(header['wclass'])
    out += header['caption'].encode('utf-16le') + b'\x00\x00'
    if header['font'] is not None:
        pt, fn = header['font']
        out += struct.pack('<H', pt) + fn.encode('utf-16le') + b'\x00\x00'
    for c in controls:
        out += b'\x00' * ((-len(out)) & 3)
        ix, iy, icx, icy = c['rect']
        out += struct.pack('<II', c['style'], c['ext'])
        out += struct.pack('<hhhhH', ix, iy, icx, icy, c['id'])
        out += _wr_name(c['wclass'])
        out += _wr_name(c['text'])
        out += struct.pack('<H', len(c['data'])) + c['data']
    return bytes(out)


# ── .res emitter (byte-compatible with tools/extract_res.py) ─────────────────────────────────────
def emit_res(leaves, res_path):
    def enc_field(key):
        kind, val = key
        if kind == 'I':
            return struct.pack('<HH', 0xffff, val)
        return val + b'\x00\x00'

    def align4(b):
        return b + b'\x00' * ((-len(b)) & 3)

    out = bytearray()
    out += struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0)
    for (rtype, rname, rlang, data) in leaves:
        tn = align4(enc_field(rtype) + enc_field(rname))
        lang = rlang[1] if rlang[0] == 'I' else 0
        trailer = struct.pack('<IHHII', 0, 0x1030, lang, 0, 0)
        header_size = 8 + len(tn) + len(trailer)
        out += struct.pack('<II', len(data), header_size)
        out += tn + trailer
        out += align4(data)
    open(res_path, 'wb').write(out)


if __name__ == '__main__':   # self-test: DLGTEMPLATE round-trips byte-for-byte
    import sys
    d = pe_leaf(sys.argv[1] if len(sys.argv) > 1 else 'YodaDemo/YodaDemo.exe', RT_DIALOG, 100)
    hdr, ctrls = parse_dlg32(d)
    rebuilt = build_dlg32(hdr, ctrls)
    print("caption:", repr(hdr['caption']), "controls:", len(ctrls))
    print("round-trip:", "OK" if rebuilt == d else f"MISMATCH ({len(rebuilt)} vs {len(d)})")
