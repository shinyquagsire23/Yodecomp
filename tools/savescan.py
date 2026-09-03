#!/usr/bin/env python3
"""Scan every non-exact residual for a CALLEE-SAVE SET MISMATCH against the original.

⭐ WHY (lesson #42, v110). The set of callee-saved registers a function preserves is a
one-line summary of how many long-lived values its body needs. When ours differs from the
original's, some value is materialised on one side and not the other — and because the
prologue sits at the TOP of the function, a single extra `push ebx` shifts every later byte,
so a handful of real decisions can present as hundreds of differing bytes. Fix the save set
first; most of the residual is usually an artifact of the shift.

Read the two directions differently:
  * ours saves MORE  -> we materialised something the original recomputes. Usual culprit is
    a NAMED TEMP for a partly-loop-invariant subexpression (lesson #42: SaveStoryHistory*'s
    `int idx = k + lineNo*10;` let cl hoist lineNo*10 into ebx), or a pointer-chain call in
    GLOBAL form letting cl keep the base alive across a call as a CSE (DrawTextA, lesson #35).
  * ours saves FEWER -> the ORIGINAL kept a value alive across a call that we spell as an
    argument expression. Give it a name and assign it BEFORE the neighbouring call
    (CheckCheat: `int x = pWorld->playerX * 7 + 18;` ahead of the `str = "..."` assignment,
    372 B -> 0).
  * the original saves NOTHING at all -> suspect a thunk/forwarder and CHECK THE EXTENT before
    touching the body (OnAppExit was a 5-byte `jmp ConfirmExit`, scored against our 220-byte
    duplicate of the twin's body).

⚠ MEASURE THE PROLOGUE, NOT THE EPILOGUE. The obvious implementation — "the set of registers
`pop`'d in the body" — is WRONG and silently so: a linear sweep desyncs on an embedded jump
table or EH data, so large functions report an empty or truncated set. It flagged six
functions here whose originals visibly DO push ebx/esi/edi in their first 0x20 bytes. This is
the v100 "the instrument itself can lie" failure mode; the positive control below is the
guard. Reading only up to the first call/branch keeps the decode inside real prologue bytes.

Shares progress.py's compile + COMDAT filter/pairing block verbatim (the v100/v101 rule).

Usage:  tools/savescan.py [--all]      (--all: list matching functions too)
"""
import os, sys, re, glob

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import capstone
import match
import verify
import progress as prog

EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
SAVED = ("ebx", "esi", "edi")


def prologue_saves(buf):
    """Callee-saved pushes before the first call/branch — i.e. inside the prologue."""
    out = []
    for i in _md.disasm(bytes(buf[:64]), 0):
        if i.mnemonic == "call" or i.mnemonic[0] == "j" or i.mnemonic == "ret":
            break
        if i.mnemonic == "push" and i.op_str in SAVED:
            out.append(i.op_str)
    return tuple(sorted(out))


def main():
    show_all = "--all" in sys.argv
    rows, n_res = [], 0
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        obj = prog.compile_obj(cpp)
        if not obj:
            continue
        text = open(cpp).read()
        hinted = set(re.findall(
            r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
        funcs = [f for f in match.coff_functions(obj)
                 if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                     or any(h in f[0] for h in hinted))
                 and not f[0].lstrip("?").startswith(("_$E", "$E"))]
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            nd = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            if nd == 0:
                continue
            n_res += 1
            a, b = prologue_saves(orig), prologue_saves(code[:L])
            if a != b or show_all:
                rows.append((a != b, nd, os.path.basename(cpp), va, name[:46], a, b))

    print("positive control: %d non-exact residuals scanned" % n_res)
    print("%d have a callee-save MISMATCH\n" % sum(1 for r in rows if r[0]))
    for bad, nd, f, va, nm, a, b in sorted(rows, reverse=True):
        print("%s %-22s %#010x ndiff=%-6d orig=%-18s ours=%-18s %s"
              % ("!" if bad else " ", f, va, nd, "+".join(a) or "-", "+".join(b) or "-", nm))


if __name__ == "__main__":
    main()
