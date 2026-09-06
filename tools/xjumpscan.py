#!/usr/bin/env python3
"""⭐ v126 — the GENERALISED lesson-#52 cross-jump census, measured on BOTH sides.

Lesson #52: a constant argument that the original materialises with a BRANCH is not an
expression — it is TWO duplicated call statements that cl tail-merged down to the pushes that
differ. v121 built a census for it, concluded "6 functions project-wide, essentially mined
out", and closed the seam.

⛔ THAT CENSUS WAS TOO NARROW. It scanned for the literal shape `jcc / push imm / jmp /
push imm`, i.e. a ONE-push arm. `OnDraw` 0x409110's diamond pushes TWO values per arm
(`jne L; push 0; push 0; jmp E; L: push nViewTop; push nViewLeft; E: <one shared call>`) and
was invisible to it. Duplicating that call took OnDraw from 386 B to BYTE-EXACT. Generalising
the arm to "one or more pushes plus argument-setup" takes the census from 6 to 12.

⇒ The transferable lesson is about CENSUS SHAPE, not about this function: a pattern census
that hard-codes an arm SIZE will silently under-report. Match the mechanism (two arms that
differ only in argument setup, joining at one call), not one instance of its output.

⭐ AND IT REPORTS OUR OWN SIDE TOO (lesson #56): a diamond present in both images is
CONFIRMED, not a target. Only "ORIG has one, ours does not" is work. Of the 12, exactly two
are ours to fix: OnBumpTile 0x413df0 and OnKeyDown 0x4150f0 (arms push 3/3 — new at v126).

POSITIVE CONTROL: must re-find all 6 of v121's known diamonds plus OnDraw 0x409110; fails
loudly otherwise.

READ-ONLY on the original; recompiles src/*.cpp for our side (like residuals.py), so do not
run it during a vartest/formsweep sweep.

Usage:  python3 tools/xjumpscan.py [--exact <exactset.txt>]
"""
import os, sys, glob
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, residuals

EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# opcodes that can legitimately appear while EVALUATING call arguments
SETUP = {"push", "mov", "lea", "movsx", "movzx", "xor", "add", "sub", "inc", "dec", "and",
         "or", "shl", "sar", "shr", "neg", "cdq", "imul", "test", "cmp"}
JCC = set("jo jno jb jae je jne jbe ja js jns jp jnp jl jge jle jg".split())


def diamonds(buf, base=0):
    """Cross-jumped call diamonds in `buf`: [(jcc_off, pushes_arm1, pushes_arm2, call_off)]."""
    ins = list(_md.disasm(bytes(buf), base))
    at = {i.address: k for k, i in enumerate(ins)}
    out = []
    for k, i in enumerate(ins):
        if i.mnemonic not in JCC:
            continue
        try:
            t = int(i.op_str, 0)
        except ValueError:
            continue
        if t <= i.address or t not in at:
            continue
        # arm 1 = fallthrough, argument-setup only, ending in an unconditional jmp...
        j = k + 1
        while j < len(ins) and ins[j].address < t and ins[j].mnemonic in SETUP:
            j += 1
        if j >= len(ins) or ins[j].mnemonic != "jmp":
            continue
        # ...that sits immediately before the jcc's target (so arm 2 starts right after it)
        if ins[j].address + ins[j].size != t:
            continue
        try:
            m = int(ins[j].op_str, 0)
        except ValueError:
            continue
        if m not in at or m <= t:
            continue
        arm1, arm2 = ins[k + 1:j], ins[at[t]:at[m]]
        if not arm1 or not arm2 or not all(x.mnemonic in SETUP for x in arm2):
            continue
        p1 = sum(1 for x in arm1 if x.mnemonic == "push")
        p2 = sum(1 for x in arm2 if x.mnemonic == "push")
        if p1 < 1 or p2 < 1:
            continue
        # the merge point must reach a CALL through argument-setup only — that shared call is
        # the whole point: it is the two duplicated source calls after cl tail-merged them.
        q = at[m]
        while q < len(ins) and ins[q].mnemonic in SETUP:
            q += 1
        if q >= len(ins) or ins[q].mnemonic != "call":
            continue
        out.append((i.address - base, p1, p2, ins[q].address - base))
    return out


def main():
    exact = set()
    if "--exact" in sys.argv:
        for ln in open(sys.argv[sys.argv.index("--exact") + 1]):
            exact.add(int(ln.split()[0], 16))

    rows, seen = [], set()
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            if va in seen:
                continue
            seen.add(va)
            L = match.trim_pad(code)
            n = residuals.EXTENT.get(va) or L
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            try:
                o = diamonds(EXE[foff:foff + n])
                u = diamonds(code[:L])
            except Exception:
                continue
            if o:
                rows.append((va, os.path.basename(cpp), name, n, o, u, va in exact))

    found = {r[0] for r in rows}
    ctrl = {0x4193f0, 0x419460, 0x4270f0, 0x412250, 0x411730, 0x409110}
    if not ctrl <= found:
        raise SystemExit("!! POSITIVE CONTROL FAILED: known diamonds not re-found: %s"
                         % [hex(x) for x in sorted(ctrl - found)])
    print("# POSITIVE CONTROL ok: all %d known diamonds re-found" % len(ctrl))
    open_ = [r for r in rows if len(r[5]) < len(r[4])]
    print("# %d functions have a cross-jump diamond in the ORIGINAL; %d byte-exact; "
          "%d where OURS is MISSING one\n" % (len(rows), sum(1 for r in rows if r[6]), len(open_)))
    for va, tu, name, n, o, u, isx in sorted(rows, key=lambda r: (len(r[5]) >= len(r[4]), r[6])):
        tag = "  <-- OURS MISSING %d" % (len(o) - len(u)) if len(u) < len(o) else \
              ("  (byte-exact: confirmed)" if isx else "  (present in ours too)")
        print("0x%08x  %-22s ext=%-5d orig=%d ours=%d  arms=%s%s"
              % (va, tu, n, len(o), len(u), [(a, b) for _, a, b, _ in o], tag))


if __name__ == "__main__":
    main()
