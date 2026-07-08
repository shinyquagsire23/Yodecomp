#!/usr/bin/env python3
# extract_res.py — convert a PE's .rsrc section into a linkable Win32 .res file.
#
# Phase G0 (runnable image): the linked yoda.exe references resources by integer ID; to make it
# RUN like the original we copy the original's resource tree verbatim (dialogs, menus, bitmaps,
# strings, icons, accelerators, rcdata) into a .res that link.exe consumes. This is byte-faithful
# to the original resources (not reconstructed .rc text), so behaviour matches.
#
# Usage: python3 tools/extract_res.py YodaDemo/YodaDemo.exe build-link/yoda.res
import struct, sys

def main(exe_path, res_path):
    f = open(exe_path, 'rb').read()
    e_lfanew = struct.unpack_from('<I', f, 0x3c)[0]
    coff = e_lfanew + 4
    nsec, = struct.unpack_from('<H', f, coff + 2)
    optsz, = struct.unpack_from('<H', f, coff + 16)
    sect = coff + 20 + optsz
    rsrc_va = rsrc_raw = rsrc_size = None
    for i in range(nsec):
        off = sect + i * 40
        name = f[off:off + 8].rstrip(b'\0')
        vsz, va, rawsz, raw = struct.unpack_from('<IIII', f, off + 8)
        if name == b'.rsrc':
            rsrc_va, rsrc_raw, rsrc_size = va, raw, vsz
    assert rsrc_va is not None, "no .rsrc section"

    def at(rva):                       # rva (image) -> file offset, only valid inside .rsrc
        return rsrc_raw + (rva - rsrc_va)

    # Walk the 3-level resource directory. Each leaf yields (type, name, lang, data_bytes).
    leaves = []
    def walk_dir(dir_off, level, path):
        # IMAGE_RESOURCE_DIRECTORY: +12 named count, +14 id count
        n_named, = struct.unpack_from('<H', f, dir_off + 12)
        n_id,    = struct.unpack_from('<H', f, dir_off + 14)
        entries = dir_off + 16
        for k in range(n_named + n_id):
            eoff = entries + k * 8
            name_id, off_to_data = struct.unpack_from('<II', f, eoff)
            if name_id & 0x80000000:   # string name
                soff = rsrc_raw + (name_id & 0x7fffffff)
                slen, = struct.unpack_from('<H', f, soff)
                key = f[soff + 2: soff + 2 + slen * 2]      # utf-16 bytes (no length)
                key = ('S', key)
            else:
                key = ('I', name_id & 0xffff)
            if off_to_data & 0x80000000:  # subdirectory
                walk_dir(rsrc_raw + (off_to_data & 0x7fffffff), level + 1, path + [key])
            else:                         # data leaf
                de = rsrc_raw + off_to_data
                data_rva, size, codepage, _ = struct.unpack_from('<IIII', f, de)
                data = f[at(data_rva): at(data_rva) + size]
                leaves.append((path[0], path[1], key, data))
    walk_dir(rsrc_raw, 0, [])

    # Emit .res. Each entry: RESOURCEHEADER + data (DWORD-padded).
    def enc_field(key):
        kind, val = key
        if kind == 'I':
            return struct.pack('<HH', 0xffff, val)
        return val + b'\x00\x00'         # utf-16 null-terminated
    def align4(b):
        return b + b'\x00' * ((-len(b)) & 3)

    out = bytearray()
    # Mandatory empty first entry (type 0, name 0, HeaderSize 32).
    out += struct.pack('<IIHHHHIHHII', 0, 32, 0xffff, 0, 0xffff, 0, 0, 0, 0, 0, 0)

    for (rtype, rname, rlang, data) in leaves:
        tn = align4(enc_field(rtype) + enc_field(rname))
        # RESOURCEHEADER trailer: DataVersion, MemoryFlags, LanguageId, Version, Characteristics
        lang = rlang[1] if rlang[0] == 'I' else 0
        trailer = struct.pack('<IHHII', 0, 0x1030, lang, 0, 0)
        header_size = 8 + len(tn) + len(trailer)
        out += struct.pack('<II', len(data), header_size)
        out += tn + trailer
        out += align4(data)

    open(res_path, 'wb').write(out)
    print(f"wrote {res_path}: {len(leaves)} resources, {len(out)} bytes")
    # quick type histogram
    from collections import Counter
    c = Counter(t[1] if t[0]=='I' else 'str' for (t,_,_,_) in leaves)
    print("resource types (ordinal->count):", dict(c))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
