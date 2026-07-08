#!/usr/bin/env python3
"""msgcheck — .rdata MESSAGE-MAP content oracle (v49; the vtcheck sibling).

Validates that each class's EMITTED AFX_MSGMAP_ENTRY array (in build/<TU>.obj) matches the ORIGINAL
in YodaDemo.exe entry-for-entry: the fixed fields (nMessage, nCode, nID, nLastID, nSig) AND the handler
identity (which function each entry dispatches to). A wrong/missing entry = a menu command or window
message silently mis-dispatched or ignored — a functional bug the vtable check does not cover. v45
reconstructed World's map by hand; this checks EVERY class's map automatically.

Original side: `?GetMessageMap@Cls@@` (addr from our // FUNCTION markers) is `mov eax, &messageMap; ret`;
messageMap = {pBaseMap, lpEntries}; lpEntries → a 24-byte AFX_MSGMAP_ENTRY array terminated by a
zero nMessage. Our side: read the `?_messageEntries@Cls@@` data COMDAT from the obj (fixed fields direct;
each entry's pfn is a relocation → resolve to a symbol → its original address via the markers).
Run from repo root (needs build/*.obj compiled)."""
import sys, os, struct, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vtcheck as V   # reuse sections / va_to_off / build_name2addr

EXE = 'YodaDemo/YodaDemo.exe'
ENT = 24  # sizeof(AFX_MSGMAP_ENTRY)


def orig_entries(data, secs, gm_va):
    """[(nMessage,nCode,nID,nLastID,nSig,pfn)] for the class's OWN entries (before base chaining)."""
    o = V.va_to_off(secs, gm_va)
    body = data[o:o + 6]
    if body[0] != 0xB8 or body[5] != 0xC3:      # mov eax, imm32 ; ret
        return None
    mm = struct.unpack_from('<I', body, 1)[0]
    mo = V.va_to_off(secs, mm)
    pbase, lpent = struct.unpack_from('<II', data, mo)
    eo = V.va_to_off(secs, lpent)
    out = []
    while True:
        f = struct.unpack_from('<6I', data, eo + len(out) * ENT)
        if f[0] == 0:                            # zero nMessage = terminator
            break
        out.append(f)
    return out


def our_entries(objpath, cls):
    """[(nMessage,nCode,nID,nLastID,nSig,handler_symbol_or_None)] from the obj's _messageEntries COMDAT."""
    d = open(objpath, 'rb').read()
    nsec = struct.unpack_from('<H', d, 2)[0]; symoff = struct.unpack_from('<I', d, 8)[0]
    nsym = struct.unpack_from('<I', d, 12)[0]; opt = struct.unpack_from('<H', d, 16)[0]
    sh = 20 + opt; strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        off = sh + i * 40
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, fl = struct.unpack_from('<IIIIIIHHI', d, off + 8)
        secs.append((rawptr, rawsz, relptr, nrel))

    def nm(rec):
        if rec[:4] == b'\0\0\0\0':
            o = struct.unpack_from('<I', rec, 4)[0]; e = d.index(b'\0', strtab + o)
            return d[strtab + o:e].decode('latin1')
        return rec.rstrip(b'\0').decode('latin1')
    syms = {}; ent_sec = None; ent_off = None; i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from('<IhHBB', rec, 8)
        n = nm(rec); syms[i] = n
        if n.startswith('?_messageEntries@' + cls + '@@') and secn > 0:
            ent_sec = secn - 1; ent_off = val
        i += 1 + naux
    if ent_sec is None:
        return None
    rawptr, rawsz, relptr, nrel = secs[ent_sec]
    # map (section offset -> symbol name) for relocations in this section
    rel = {}
    for r in range(nrel):
        voff, symidx, typ = struct.unpack_from('<IIH', d, relptr + r * 10)
        rel[voff] = syms.get(symidx)
    out = []
    while True:
        base = rawptr + ent_off + len(out) * ENT
        f = struct.unpack_from('<6I', d, base)
        if f[0] == 0:
            break
        pfn_sym = rel.get(ent_off + len(out) * ENT + 0x14)   # pfn field
        out.append((f[0], f[1], f[2], f[3], f[4], pfn_sym))
    return out


def main():
    data = open(EXE, 'rb').read()
    secs = V.sections(data)
    n2a = V.build_name2addr()
    # class -> obj (from the ?GetMessageMap@Cls@@ markers we own)
    gms = {k[len('?GetMessageMap@'):k.index('@@')]: k for k in n2a if k.startswith('?GetMessageMap@')}
    obj_of = {}
    for obj in glob.glob('build/*.obj'):
        d = open(obj, 'rb').read()
        for cls in gms:
            if ('?_messageEntries@' + cls + '@@').encode() in d:
                obj_of[cls] = obj
    total_bad = 0; checked = 0
    for cls in sorted(gms):
        obj = obj_of.get(cls)
        if not obj:
            print(f"{cls:12} — no _messageEntries in any obj (empty map / not emitted)"); continue
        orig = orig_entries(data, secs, n2a[gms[cls]])
        ours = our_entries(obj, cls)
        if orig is None or ours is None:
            print(f"{cls:12} — could not read ({'orig' if orig is None else 'ours'})"); continue
        checked += 1
        bad = []
        if len(orig) != len(ours):
            bad.append(f"entry count orig={len(orig)} ours={len(ours)}")
        for i in range(min(len(orig), len(ours))):
            of, uf = orig[i], ours[i]
            if of[:5] != uf[:5]:
                bad.append(f"#{i} fields orig={[hex(x) for x in of[:5]]} ours={[hex(x) for x in uf[:5]]}")
            # handler identity: orig pfn addr vs our reloc symbol's original addr
            ua = n2a.get(uf[5]) if uf[5] else None
            if ua is not None and ua != of[5]:
                bad.append(f"#{i} handler orig={hex(of[5])} ours={uf[5]}@{hex(ua)}")
        status = 'OK' if not bad else f'{len(bad)} MISMATCH'
        print(f"{cls:12} {os.path.basename(obj):14} entries={len(ours):2}  {status}")
        for b in bad:
            print(f"      {b}")
        total_bad += len(bad)
    print(f"\nchecked {checked} maps — {'CLEAN' if not total_bad else f'{total_bad} mismatch(es)'}")
    return 1 if total_bad else 0


if __name__ == '__main__':
    sys.exit(main())
