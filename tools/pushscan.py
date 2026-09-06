#!/usr/bin/env python3
"""⭐ v126 — the PUSH-FORM census: where the ORIGINAL pushes an IMMEDIATE and WE push a
REGISTER at the same aligned argument slot (or vice versa).

WHY IT IS A TARGET LIST. A call argument is either materialised as a literal (`push 0`,
2 bytes) or folded into a register that already holds the value (`push ebx`, 1 byte). Which
one cl picks is not a spelling tie-break — it reports on the SOURCE:

  * ORIG pushes a REGISTER, ours an IMMEDIATE  ⇒ the original had a nearby variable whose
    live constant cl could reuse, and our STATEMENT ORDER destroys it (lesson #39, and the
    v119 fold read in the joining direction). v126: DrawWeaponBox 0x428ac0 writes
    `bReleaseDC = 1;` BEFORE the SelectPalette call, so cl must materialise `push 0`; the
    original writes it AFTER and pushes the still-zero EBX. 270 B -> 106, length onto the extent.

  * ORIG pushes an IMMEDIATE, ours a REGISTER  ⇒ WE merged into one call something the
    original spelled as TWO duplicated calls (lesson #52), so our version has to compute the
    argument into a variable while the original's arm knows it is a constant. v126:
    OnUpdate 0x408e70's `::ReleaseDC(hWnd, hdc)` — the original spells ReleaseDC twice, once
    per arm, and pushes a literal 0 for hWnd. 103 B -> 0, BYTE-EXACT.

Both v126 wins came out of this scan, in opposite directions, which is what makes the
two-sided reading above a rule rather than one anecdote.

POSITIVE CONTROL (the v103 rule — an empty or small result needs one as much as a finding):
asserts that NO byte-exact function reports a hit, and prints the number of functions decoded
and aligned. A hit inside a byte-exact function would mean the alignment is lying.

READ-ONLY apart from the compile of each TU — it recompiles src/*.cpp like residuals.py, so
do NOT run it while a vartest/formsweep sweep is in flight (they share build/*.obj).

Usage:  python3 tools/pushscan.py [--exact <exactset.txt>]
"""
import os, re, sys, glob
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, residuals, asmscore

EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_REG = re.compile(r"e(?:ax|bx|cx|dx|si|di|bp|sp)$")
_IMM = re.compile(r"(?:0x)?[0-9a-fA-F]+$")


def _kind(dis):
    """'reg' | 'imm' | None for a push instruction, from its disassembly text."""
    if not dis.startswith("push "):
        return None
    op = dis[5:].strip()
    if _REG.match(op):
        return "reg"
    if _IMM.match(op):
        return "imm"
    return None                      # push [mem] / push offset — not a materialisation choice


def _dis(buf):
    return {i.address: "%s %s" % (i.mnemonic, i.op_str) for i in _md.disasm(bytes(buf), 0)}


def scan():
    rows, n = [], 0
    seen = set()
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
            da, db = _dis(orig), _dis(mine)
            hits = []
            for ia, ib in pairs:
                if ia is None or ib is None:
                    continue
                sa, sb = da.get(a[ia].off, ""), db.get(b[ib].off, "")
                ka, kb = _kind(sa), _kind(sb)
                if ka and kb and ka != kb:
                    hits.append((a[ia].off, sa, sb, ka))
            n += 1
            if hits:
                rows.append((va, os.path.basename(cpp), name, L, residuals.EXTENT.get(va), hits))
    return rows, n


def main():
    exact = set()
    if "--exact" in sys.argv:
        for ln in open(sys.argv[sys.argv.index("--exact") + 1]):
            exact.add(int(ln.split()[0], 16))

    rows, n = scan()
    print("# scanned %d paired functions; %d have a push-form mismatch" % (n, len(rows)))
    if exact:
        bad = [r for r in rows if r[0] in exact]
        if bad:
            raise SystemExit("!! POSITIVE CONTROL FAILED: byte-exact function(s) report a hit: %s\n"
                             "   The instruction alignment is lying — fix it before trusting any row."
                             % [hex(r[0]) for r in bad])
        print("# POSITIVE CONTROL ok: 0 of the %d byte-exact functions report a hit" % len(exact))
    print("# ORIG-reg = a statement-order fold we destroy (lesson #39/#119)")
    print("# ORIG-imm = a call the original DUPLICATED and we merged (lesson #52)\n")
    for va, tu, name, L, ext, hits in sorted(rows, key=lambda r: -len(r[5])):
        d = ("%+d" % (L - ext)) if ext else " n/a"
        print("0x%08x  %-22s len=%-5d ext%s  %d hit(s)  %s" % (va, tu, L, d, len(hits), name[:44]))
        for off, sa, sb, ka in hits[:6]:
            print("        +0x%03x  ORIG-%s  orig `%s`   ours `%s`" % (off, ka, sa, sb))


if __name__ == "__main__":
    main()
