#!/usr/bin/env python3
"""⭐ v127 — the OPERAND-WIDTH census: where the ORIGINAL touches a stack slot as a full
DWORD and WE sign/zero-extend a WORD at the same aligned instruction (or vice versa).

WHY IT IS A TARGET LIST — it reads a DECLARED TYPE straight off the machine code.
`aritycheck.py` (v120) compares a callee's terminal `ret N` with our compiled one, so it
catches a wrong parameter COUNT. It is blind to a wrong parameter TYPE: `short nOrder` and
`int nOrder` occupy the same 4-byte stack slot and clean the same number of argument bytes,
so the arity oracle, the linker, bugscan, vtcheck and msgcheck all pass. The only trace is
the access WIDTH:

  * `mov eax, dword ptr [esp+N]`      ⇒ the slot holds a 32-bit value (int/long/pointer)
  * `movsx eax, word ptr [esp+N]`     ⇒ a 16-bit SIGNED value widened for an int context
  * `movzx eax, word ptr [esp+N]`     ⇒ a 16-bit UNSIGNED value widened
  * `mov ax, word ptr [esp+N]`        ⇒ used at 16 bits throughout

A caller passing a `short` to an `int` parameter MUST widen it; one passing an `int` can copy
the raw slot. So an ORIG-d32 / OURS-sx16 pair at the same argument slot says our declaration
is `short` where the original's is `int` — and each such site costs 2 bytes plus whatever
register pressure the extra temp creates.

Read the two directions as:
  * ORIG wide, ours narrow  ⇒ we declared a `short` the original declared `int`.
  * ORIG narrow, ours wide  ⇒ we declared an `int` the original declared `short`.

⚠ A HIT IS A CANDIDATE, NOT A DEFECT. The same slot can be a LOCAL rather than a parameter,
and cl will also widen a genuinely-short value that flows into an int expression. Confirm by
reading the CALLEE's own accesses to the slot (a `__thiscall` callee's parameter base is
computable from its prologue) before changing any signature — a parameter type lives in a
shared header, so changing it re-rolls every TU's codegen dial (the v120 `TextDialog::Layout`
precedent, where a two-sided proof was worth a deliberate re-baseline).

POSITIVE CONTROL (the v103 rule — an empty or small result needs one as much as a finding):
asserts that NO byte-exact function reports a hit, and prints how many functions aligned. A
hit inside a byte-exact function would mean the instruction alignment is lying.

READ-ONLY apart from the compile of each TU — it recompiles src/*.cpp like residuals.py, so
do NOT run it while a vartest/formsweep sweep is in flight (they share build/*.obj).

Usage:  python3 tools/widthscan.py [--exact <exactset.txt>] [--all]
"""
import os, re, sys, glob
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, residuals, asmscore

EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# `<mnemonic> <dst>, <size> ptr [<esp|ebp> <+|-> <disp>]`
_STACK = re.compile(
    r"^(mov|movsx|movzx)\s+(\S+),\s*(byte|word|dword) ptr \[(esp|ebp)\s*([+-])\s*(0x[0-9a-f]+|\d+)\]$")
_R32 = re.compile(r"^e(?:ax|bx|cx|dx|si|di|bp)$")


def _slot(dis):
    """(base, signed_disp, width_tag) for a stack-slot LOAD, else None.

    width_tag: 'd32' full dword | 'sx16'/'zx16' widened word | 'm16' 16-bit register load.
    Only loads INTO a register count — a store's width is fixed by the destination's type
    and carries the same information, but mixing the two directions muddies the alignment.
    """
    m = _STACK.match(dis)
    if not m:
        return None
    mn, dst, size, base, sign, disp = m.groups()
    d = int(disp, 16) if disp.startswith("0x") else int(disp)
    if sign == "-":
        d = -d
    if mn == "mov" and size == "dword" and _R32.match(dst):
        return (base, d, "d32")
    if mn == "movsx" and size == "word":
        return (base, d, "sx16")
    if mn == "movzx" and size == "word":
        return (base, d, "zx16")
    if mn == "mov" and size == "word":
        return (base, d, "m16")
    return None


def _wide(tag):
    return tag == "d32"


def _dis(buf):
    return {i.address: "%s %s" % (i.mnemonic, i.op_str) for i in _md.disasm(bytes(buf), 0)}


def _census(d):
    """{'wide': n, 'narrow': n} over every stack-slot LOAD in one function's disassembly.

    'narrow' counts the WIDENING loads (movsx/movzx word) — the ones a `short` declaration
    forces and an `int` declaration does not. 'wide' counts full-dword slot loads.
    """
    out = {"wide": 0, "narrow": 0}
    for dis in d.values():
        k = _slot(dis)
        if not k:
            continue
        if k[2] == "d32":
            out["wide"] += 1
        elif k[2] in ("sx16", "zx16"):
            out["narrow"] += 1
    return out


def scan():
    rows, n, seen = [], 0, set()
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            if va in seen:
                continue
            seen.add(va)
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            if len(orig) != L:
                continue
            mine = code[:L]
            try:
                a = asmscore._decode(orig)
                b = asmscore._decode(mine, relocs)
                _, pairs = asmscore._align(a, b)
            except Exception:
                continue
            n += 1
            da, db = _dis(orig), _dis(mine)

            # (1) STRUCTURAL census — count the widening loads on each side independently.
            # This is the primary signal (lesson #56: census our own output, do not infer it
            # from the diff's "other column"). It survives the schedule differences that make
            # positional alignment unreliable in exactly the functions worth looking at.
            ca, cb = _census(da), _census(db)
            dw = cb["narrow"] - ca["narrow"]

            # (2) POSITIONAL evidence — an aligned pair where exactly one side is a full
            # dword names the offending slot outright. Strong when present, often absent.
            sites = []
            for ia, ib in pairs:
                if ia is None or ib is None:
                    continue
                sa, sb = da.get(a[ia].off, ""), db.get(b[ib].off, "")
                ka, kb = _slot(sa), _slot(sb)
                if not ka or not kb:
                    continue
                if ka[0] == kb[0] and _wide(ka[2]) != _wide(kb[2]):
                    sites.append((a[ia].off, sa, sb, "wide" if _wide(ka[2]) else "narrow"))

            if dw or sites:
                rows.append((va, os.path.basename(cpp), name, L,
                             residuals.EXTENT.get(va), dw, ca, cb, sites))
    return rows, n


def main():
    exact = set()
    if "--exact" in sys.argv:
        for ln in open(sys.argv[sys.argv.index("--exact") + 1]):
            exact.add(int(ln.split()[0], 16))

    rows, n = scan()
    print("# scanned %d paired functions; %d differ in stack-slot ACCESS WIDTH" % (n, len(rows)))
    if exact:
        bad = [r for r in rows if r[0] in exact]
        if bad:
            raise SystemExit("!! POSITIVE CONTROL FAILED: byte-exact function(s) report a hit: %s\n"
                             "   The instruction alignment is lying — fix it before trusting a row."
                             % [hex(r[0]) for r in bad])
        print("# POSITIVE CONTROL ok: 0 of the %d byte-exact functions report a hit" % len(exact))
    print("# ORIG-wide   = we declared a `short` where the original declared `int`")
    print("# ORIG-narrow = we declared an `int` where the original declared `short`\n")
    for va, tu, name, L, ext, dw, ca, cb, sites in sorted(
            rows, key=lambda r: (-abs(r[5]), -len(r[8]))):
        d = ("%+d" % (L - ext)) if ext else " n/a"
        print("0x%08x  %-22s len=%-5d ext%s  widen orig=%d ours=%d (%+d)  %s"
              % (va, tu, L, d, ca["narrow"], cb["narrow"], dw, name[:40]))
        for off, sa, sb, kind in sites[:8]:
            print("        +0x%03x  ORIG-%-6s orig `%s`   ours `%s`" % (off, kind, sa, sb))


if __name__ == "__main__":
    main()
