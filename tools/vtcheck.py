#!/usr/bin/env python3
"""vtcheck — .rdata vtable-content oracle (v47).

Validates that each modeled class's EMITTED vtable (in build/<TU>.obj) overrides the same slots,
with the same class methods, as the ORIGINAL vtable in YodaDemo.exe. The data-side complement to
bugscan.py (which checks .text call sites): a MISSING override here = a virtual we forgot to declare
(the base runs instead — a runtime bug), e.g. World::IsModified before v46. A correct override
self-places in the right slot, so this mainly catches OMISSIONS.

Method: read the original vtable from the exe (VA->file offset via the section table) and our vtable
from the obj (the `??_7<Class>@@6B@` data COMDAT + its relocations -> per-slot target symbol). For each
slot, OURS is an override iff its target symbol is a `<Class>@@` method; ORIG is an override iff its
target lands in the app region [0x401000,0x429150) AND is not a KNOWN ICF-FOLDED BASE address.

⚠ ICF caveat: the linker folds identical COMDATs, so a trivial BASE method (CObject::Serialize, a CView
no-op, …) can fold to an app-region address and look like an override. The FOLDED_BASE set below lists
those (verified via Ghidra); they are treated as base, not overrides. Run from repo root.
"""
import sys, os, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

EXE = 'YodaDemo/YodaDemo.exe'
APP_LO, APP_HI = 0x401000, 0x429150

# App-region addresses that are FOLDED library/base methods (ICF), NOT class overrides.
# 0x401060/70/80 = CObject::Serialize/AssertValid/Dump (folded into the first app TU, referenced by
# every vtable). 0x401090/a0 = DisableSelfWindow/Enable — 12-byte bodies that trivial CView virtuals
# fold onto. 0x40e3f0 = the folded CView no-op default (Ghidra-commented). See docs/g2-layout.md.
FOLDED_BASE = {0x401060, 0x401070, 0x401080, 0x401090, 0x4010a0, 0x40e3f0}

# class -> (obj, vtable mangled-symbol prefix, original vtable base VA)
CLASSES = {
    'World':    ('build/WorldDoc.obj', '??_7World@@6B',    0x44c438),
    'GameView': ('build/GameView.obj', '??_7GameView@@6B', 0x44b638),
}


def va_to_off(data, va):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    nsec = struct.unpack_from('<H', data, pe + 6)[0]
    opt = struct.unpack_from('<H', data, pe + 20)[0]
    sh = pe + 24 + opt
    for i in range(nsec):
        o = sh + i * 40
        vs, vaddr, rs, rp = struct.unpack_from('<IIII', data, o + 8)
        if vaddr <= (va - 0x400000) < vaddr + max(vs, rs):
            return (va - 0x400000) - vaddr + rp
    return None


def our_vtable(obj, prefix):
    d = open(obj, 'rb').read()
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
    syms = {}; i = 0
    tsec = None
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from('<IhHBB', rec, 8)
        syms[i] = nm(rec)
        if tsec is None and syms[i].startswith(prefix) and secn > 0:
            tsec = secn - 1
        i += 1 + naux
    rawptr, rawsz, relptr, nrel = secs[tsec]
    slots = {}
    for r in range(nrel):
        voff, symidx, typ = struct.unpack_from('<IIH', d, relptr + r * 10)
        slots[voff] = syms.get(symidx, '?')
    return slots


def main():
    data = open(EXE, 'rb').read()
    total_bad = 0
    for cls, (obj, prefix, base) in CLASSES.items():
        if not os.path.exists(obj):
            print(f"{cls}: SKIP ({obj} not built)"); continue
        off = va_to_off(data, base)
        ours = our_vtable(obj, prefix)
        # bound the comparison to OUR vtable's real extent — our COMDAT has one reloc per slot, so the
        # highest slot offset + 4 = the vtable size. Reading past it walks into adjacent .rdata (embedded
        # sub-vtables / folded dtors) and mis-reports (v47: World +0xbc, GameView +0x114/+0x12c).
        nslots = max(ours) // 4 + 1
        orig = [struct.unpack_from('<I', data, off + s * 4)[0] for s in range(nslots)]
        orig_ov = {s * 4: a for s, a in enumerate(orig)
                   if APP_LO <= a < APP_HI and a not in FOLDED_BASE}
        our_ov = {o: n for o, n in ours.items() if (cls + '@@') in n}
        bad = []
        for o in sorted(set(orig_ov) | set(our_ov)):
            if (o in orig_ov) != (o in our_ov):
                who = 'orig-only (MISSING override)' if o in orig_ov else 'ours-only (EXTRA)'
                bad.append((o, who, orig_ov.get(o), our_ov.get(o)))
        status = 'OK' if not bad else f'{len(bad)} MISMATCH'
        print(f"{cls:10} orig-overrides={len(orig_ov):2}  our-overrides={len(our_ov):2}  -> {status}")
        for o, who, oa, un in bad:
            print(f"    +{o:#05x}  {who}  orig={oa and hex(oa)}  ours={un or '(base)'}")
        total_bad += len(bad)
    print(f"\n{'CLEAN — all vtable override patterns match' if not total_bad else f'{total_bad} real mismatch(es)'}")
    return 1 if total_bad else 0


if __name__ == '__main__':
    sys.exit(main())
