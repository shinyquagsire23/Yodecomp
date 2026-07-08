#!/usr/bin/env python3
"""vtcheck — .rdata vtable-content oracle (v47; auto-locating rewrite v48).

Validates that every modeled class's EMITTED vtable (in build/<TU>.obj) overrides the same slots,
with the same class methods, as the ORIGINAL vtable in YodaDemo.exe. The data-side complement to
bugscan.py (which checks .text call sites): a MISSING override here = a virtual we forgot to declare
(the base runs instead — a runtime bug), exactly the World::IsModified gap closed in v46.

Fully automatic (v48): for each `??_7<Class>@@6B@` data COMDAT we emit, read its relocations → per-slot
target symbol; look up the original ADDRESS of each override target from our own `// FUNCTION: YODA`
markers (via match.pair_by_name); then LOCATE the original vtable in the exe by scanning .rdata for
those addresses at consistent relative offsets. Classes with <2 locatable overrides are SKIPPED
(can't anchor the base) — pure MFC-base-copy vtables (CObject/CBitmap/…) have 0 app overrides and are
skipped automatically.

⚠ ICF caveat: the linker folds identical COMDATs, so a trivial BASE method (CObject::Serialize, a CView
no-op, …) can fold to an app-region address and look like an override. FOLDED_BASE lists those (verified
via Ghidra); they are treated as base. Run from repo root (needs build/*.obj compiled)."""
import sys, os, struct, glob
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match as M

EXE = 'YodaDemo/YodaDemo.exe'
APP_LO, APP_HI = 0x401000, 0x429150
# App-region addresses that are FOLDED library/base methods (ICF), NOT class overrides (see docs/g2-layout.md).
FOLDED_BASE = {0x401060, 0x401070, 0x401080, 0x401090, 0x4010a0, 0x40e3f0}


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    sh = pe + 24 + opt
    secs = []
    for i in range(nsec):
        o = sh + i * 40
        vs, vaddr, rs, rp = struct.unpack_from('<IIII', data, o + 8)
        secs.append((vaddr, max(vs, rs), rp))
    return secs


def va_to_off(secs, va):
    r = va - 0x400000
    for vaddr, size, rp in secs:
        if vaddr <= r < vaddr + size:
            return r - vaddr + rp
    return None


def our_vtable(objpath):
    """{vtable_symbol: {slot_off: target_symbol}} for every ??_7*@@6B@ COMDAT defined in the obj."""
    d = open(objpath, 'rb').read()
    nsec = struct.unpack_from('<H', d, 2)[0]; symoff = struct.unpack_from('<I', d, 8)[0]
    nsym = struct.unpack_from('<I', d, 12)[0]; opt = struct.unpack_from('<H', d, 16)[0]
    sh = 20 + opt; strtab = symoff + nsym * 18
    secrel = []
    for i in range(nsec):
        o = sh + i * 40
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, fl = struct.unpack_from('<IIIIIIHHI', d, o + 8)
        secrel.append((relptr, nrel))

    def nm(rec):
        if rec[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', rec, 4)[0]; e = d.index(b'\0', strtab + off)
            return d[strtab + off:e].decode('latin1')
        return rec.rstrip(b'\0').decode('latin1')
    syms = {}; vt_secs = {}; i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from('<IhHBB', rec, 8)
        n = nm(rec); syms[i] = n
        if n.startswith('??_7') and n.endswith('@6B@') and secn > 0:
            vt_secs[n] = secn - 1
        i += 1 + naux
    out = {}
    for vtname, secn in vt_secs.items():
        relptr, nrel = secrel[secn]
        slots = {}
        for r in range(nrel):
            voff, symidx, typ = struct.unpack_from('<IIH', d, relptr + r * 10)
            slots[voff] = syms.get(symidx, '?')
        out[vtname] = slots
    return out


def build_name2addr():
    n2a = {}
    for cpp in glob.glob('src/*/*.cpp'):
        obj = 'build/' + os.path.basename(cpp).replace('.cpp', '.obj')
        if not os.path.exists(obj):
            continue
        text = open(cpp).read()
        for a, n, c, r in M.pair_by_name(text, M.coff_functions(obj)):
            n2a.setdefault(n, a)
    return n2a


def find_base(data, secs, known):
    """known = [(slot_off, orig_addr)]. Return the vtable base VA consistent across all, or None."""
    off0, addr0 = known[0]
    le = struct.pack('<I', addr0)
    cands = []
    start = 0
    while True:
        p = data.find(le, start)
        if p < 0:
            break
        start = p + 1
        # VA of this file position
        va = None
        for vaddr, size, rp in secs:
            if rp <= p < rp + size:
                va = 0x400000 + vaddr + (p - rp); break
        if va is None:
            continue
        base = va - off0
        boff = va_to_off(secs, base)
        if boff is None:
            continue
        ok = all(struct.unpack_from('<I', data, boff + o)[0] == a for o, a in known)
        if ok:
            cands.append(base)
    return cands[0] if len(set(cands)) == 1 else (cands[0] if cands else None)


def main():
    data = open(EXE, 'rb').read()
    secs = sections(data)
    n2a = build_name2addr()
    total_bad = 0; checked = 0
    rows = []; skipped = []
    # MFC library base-class vtable copies our objs emit (they reference lib functions, not our
    # overrides) — expected to be un-anchorable; not a coverage concern.
    MFC_BASE = {'CObject', 'CScrollBar', 'CEdit', 'CButton', 'CBitmap', 'CBitmapButton', 'CGdiObject',
                'CBrush', 'CPen', 'CPalette', 'CException', 'CFileException', 'CProgressCtrl'}
    for obj in sorted(glob.glob('build/*.obj')):
        for vtname, slots in our_vtable(obj).items():
            cls = vtname[len('??_7'):vtname.index('@@')]
            our_ov = {o: n for o, n in slots.items() if ('@' + cls + '@@') in ('@' + n) or (cls + '@@') in n}
            # override addresses we know from our markers (anchors for locating the original vtable)
            known = [(o, n2a[n]) for o, n in our_ov.items() if n in n2a]
            if len(known) < 2:
                if cls not in MFC_BASE:
                    skipped.append((cls, len(our_ov)))
                continue
            base = find_base(data, secs, sorted(known))
            if base is None:
                rows.append((cls, obj, None, 0, len(our_ov), 'NOT-LOCATED')); continue
            checked += 1
            boff = va_to_off(secs, base)
            nslots = max(slots) // 4 + 1
            orig = [struct.unpack_from('<I', data, boff + s * 4)[0] for s in range(nslots)]
            orig_ov = {s * 4: a for s, a in enumerate(orig) if APP_LO <= a < APP_HI and a not in FOLDED_BASE}
            bad = []
            for o in sorted(set(orig_ov) | set(our_ov)):
                if (o in orig_ov) != (o in our_ov):
                    who = 'MISSING override (orig has, we base)' if o in orig_ov else 'EXTRA (we override, orig base)'
                    bad.append((o, who, orig_ov.get(o), our_ov.get(o)))
            total_bad += len(bad)
            rows.append((cls, obj, base, len(orig_ov), len(our_ov), 'OK' if not bad else f'{len(bad)} MISMATCH', bad))
    for row in rows:
        cls, obj, base, no, nu, status = row[:6]
        b = f"@{base:#08x}" if base else "(not located)"
        print(f"{cls:16} {os.path.basename(obj):14} {b:12} orig_ov={no:2} our_ov={nu:2}  {status}")
        if len(row) > 6:
            for o, who, oa, un in row[6]:
                print(f"      +{o:#05x}  {who}  orig={oa and hex(oa)}  ours={un or '(base)'}")
    if skipped:
        # modeled classes we couldn't anchor (<2 known override addresses) — mostly single-dtor-override
        # data classes (Zone/Tile/IactScript/… override only ??_E). Low-risk; listed for transparency.
        s = ', '.join(f"{c}({n})" for c, n in sorted(set(skipped)))
        print(f"\nskipped (unanchorable, <2 override addrs): {s}")
    print(f"\nchecked {checked} classes — {'CLEAN' if not total_bad else f'{total_bad} real mismatch(es)'}")
    return 1 if total_bad else 0


if __name__ == '__main__':
    sys.exit(main())
