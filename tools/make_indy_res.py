#!/usr/bin/env python3
# make_indy_res.py — build the GAME_INDY .res: Yoda's resource tree with the app IDENTITY
# (icon + title string) swapped for Indiana Jones' Desktop Adventures.
#
# Phase H3 milestone 5 (resources). The WIN32 image references dialogs/menus/bitmaps/strings by
# integer ID that match our decompiled code (YodaDemo.exe's layout), so we CANNOT wholesale swap
# in Indy's resource set — the dialog templates and control IDs would not match. Instead we keep
# Yoda's .rsrc as the base and override only the two user-visible identity resources:
#   * the app icon   — IDR_MAINFRAME GROUP_ICON id 2 (→ its member RT_ICON), replaced with Indy's.
#   * the app title  — the doc-template string (id 2) + AFX_IDS_APP_TITLE (0xE000) set to Indy's
#                      "Desktop Adventures" (the authentic title the real DESKADV.EXE shows).
# This retires the temporary runtime SetWindowText() override + the Yoda-icon placeholder.
#
# Yoda base is a 32-bit PE (.rsrc dir); Indy's DESKADV.EXE is a 16-bit NE (NE resource table).
# Icon image bytes (BITMAPINFOHEADER + XOR + AND) and the GROUP_ICON directory format are
# identical across NE/PE, so the icon copies verbatim (only its RT_ICON ordinal is remapped to
# avoid colliding with Yoda's ICON ids 1..11).
#
# Usage: python3 tools/make_indy_res.py YodaDemo/YodaDemo.exe INDYDESK/DESKADV.EXE out.res
import struct, sys

IDR_MAINFRAME   = 2          # menu/icon/doc-template string id (this MFC app: IDR_MAINFRAME == 2)
AFX_IDS_APP_TITLE = 0xE000   # 57344
INDY_ICON_ID    = 901        # a free RT_ICON ordinal for the injected Indy icon (Yoda uses 1..11)
RT_ICON, RT_GROUP_ICON, RT_STRING = 3, 14, 6


# ── PE (.rsrc) reader: yields leaves (type_key, name_key, lang_key, data) ───────────────────────
def pe_leaves(path):
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


# ── NE resource reader: return {(type_int, name): data} for integer-typed/named resources ───────
def ne_resources(path):
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


# ── STRING table: replace substring `index` (0..15) in an MFC 16-string block ────────────────────
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
        b = s.encode('utf-16le')
        out += struct.pack('<H', len(s)) + b
    return bytes(out)


# ── .res emitter (matches tools/extract_res.py byte-for-byte) ────────────────────────────────────
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


def main(yoda_exe, indy_exe, res_path):
    leaves = pe_leaves(yoda_exe)
    indy = ne_resources(indy_exe)

    # Indy app icon: GROUP_ICON id 2 -> its member RT_ICON, remapped to a free ordinal.
    grp = bytearray(indy[(RT_GROUP_ICON, IDR_MAINFRAME)])
    cnt, = struct.unpack_from('<H', grp, 4)
    assert cnt >= 1, "Indy GROUP_ICON has no members"
    member_icon_id = struct.unpack_from('<H', grp, 6 + 12)[0]        # nID of entry 0
    icon_data = indy[(RT_ICON, member_icon_id)]
    struct.pack_into('<H', grp, 6 + 12, INDY_ICON_ID)               # repoint the dir entry
    # dwBytesInRes (entry+8) already matches the icon image length in the source binary.

    title = "Desktop Adventures"          # authentic DESKADV.EXE title (str 2 / AFX_IDS_APP_TITLE)
    n_icon = n_grp = n_str = 0
    kept = []
    for lv in leaves:
        (t, n, l, d) = lv
        # Drop Yoda's app icon group (id 2) + its member ICON (id 11) — replaced by Indy's.
        if t == ('I', RT_GROUP_ICON) and n == ('I', IDR_MAINFRAME):
            continue
        if t == ('I', RT_ICON) and n == ('I', 11):
            continue
        # Override the doc-template title string block (id 1 covers string ids 0..15 -> index 2).
        if t == ('I', RT_STRING) and n == ('I', 1):
            lv = [t, n, l, patch_stringtable(d, IDR_MAINFRAME, title)]
            n_str += 1
        # Override AFX_IDS_APP_TITLE (0xE000). String block id = (0xE000/16)+1.
        elif t == ('I', RT_STRING) and n == ('I', AFX_IDS_APP_TITLE // 16 + 1):
            lv = [t, n, l, patch_stringtable(d, AFX_IDS_APP_TITLE % 16, title)]
            n_str += 1
        kept.append(lv)

    lang = ('I', 1033)
    kept.append([('I', RT_GROUP_ICON), ('I', IDR_MAINFRAME), lang, bytes(grp)])
    kept.append([('I', RT_ICON), ('I', INDY_ICON_ID), lang, icon_data])
    emit_res(kept, res_path)
    print(f"wrote {res_path}: base={len(leaves)} leaves, Indy icon #{member_icon_id}->{INDY_ICON_ID} "
          f"({len(icon_data)} B), title={title!r} ({n_str} string blocks patched)")


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])
