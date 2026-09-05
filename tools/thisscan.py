#!/usr/bin/env python3
"""READ-ONLY census: how does the ORIGINAL treat `this` in every __thiscall function?

⭐ v124, lesson #55.  A `__thiscall` receives `this` in ECX.  cl 10.20 then either parks it
in a callee-saved register (ENREG), stores it to a frame slot and RELOADS it before each use
(SPILL), or leaves it in ECX and never parks it at all (ECX — small leaf bodies).  Which one
it picks is worth 10-60 bytes of residual in a mid-size function, because a SPILL costs a
3-4 byte `mov ecx,[...]` reload before nearly every statement, and those reloads are exactly
the bytes a "-N length" residual is missing (lesson #49).

THE RULE this census measured over the 213 byte-exact __thiscall functions:

  * NO EH frame  ->  ENREG is the default (71 of 78; median `this` uses 6).
    The ONLY exceptions are functions that need FIVE OR MORE long-lived values: all 3 spill
    with the identical shape `sub esp,N` / `mov [esp+k],ecx` / push ebx+esi+edi+ebp, i.e. all
    four callee-saved registers already committed to other values.  0 counterexamples.
  * EH FRAME (/GX + any object with a dtor, or a TRY)  ->  SPILL is the default (68 of 75).
    ebp is the frame pointer, so only 3 registers are available and `this` loses by default
    EVEN WITH REGISTERS TO SPARE — 53 of the 68 spills are not saturated at all (they are
    mostly ctors/dtors saving one register or none).

⚠ The 7 ENREG-under-EH exceptions are NOT explained by `this` use count, and saying so would
be the vacuous-column mistake: `~Zone` 0x4054d0 SPILLS at 21 uses while WorldgenPushZoneEntry
0x41d6b0 ENREGS at 4.  What they do share is nsaved >= 2.  Treat the EH rule as a strong
default with a genuinely open exception set, not as a solved mechanism.

⇒ HOW TO USE IT.  When a residual's length is SHORT of Ghidra's extent and the diff shows the
original reloading `this` where you hold it in a register (or the reverse), the lever is not a
decl permutation — it is the number of long-lived values the body needs, and under an EH frame
it is the EH frame itself.  See the DrawHealthDial 0x427490 note in src/Worldgen.cpp.

Positive control: the exact set must reproduce the anchor's count, and every classified name
must be __thiscall by its mangled decoration.  Pass a cached set from tools/exactset.py.

Usage:  python3 tools/thisscan.py --exact <file>     (cached set from exactset.py)
        python3 tools/thisscan.py --all              (classify every marker with an extent)
"""
import os, sys, re, collections
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match

EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
SAVE_LETTER = {"ebx": "b", "esi": "s", "edi": "d", "ebp": "p"}
THISCALL = re.compile(r"@@[QIPAMUV]{1,2}[A-Z]?AE")


def extents():
    out = {}
    for ln in open(os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")):
        p = ln.split()
        if len(p) >= 2:
            out[int(p[0], 16)] = int(p[1])
    return out


def classify(va, L):
    """Walk the PROLOGUE only (stop at the first call), per savescan.py's v110 lesson: a
    linear sweep of a whole function desyncs on an embedded jump table or EH data."""
    foff = (va - match.TEXT_VA) + match.TEXT_RAW
    ins = list(MD.disasm(EXE[foff:foff + L], va))
    if not ins:
        return None
    saves, eh, fp, home, kind = [], False, False, None, None
    for i in ins[:30]:
        m, op = i.mnemonic, i.op_str
        if m == "mov" and op.startswith("dword ptr fs:[0]"):
            eh = True
        if m == "mov" and op == "ebp, esp":
            fp = True
        if m == "push" and op in SAVE_LETTER and op not in saves:
            saves.append(op)
        if m == "mov" and op.endswith(", ecx") and home is None:
            d = op[:-5]
            if d in SAVE_LETTER:
                home, kind = d, "ENREG"
            elif d.startswith("dword ptr ["):
                home, kind = d[len("dword ptr "):].replace(" ", ""), "SPILL"
        if m == "call":
            break
    if home is None:
        kind, home = "ECX", "ecx"
    # ebp pushed and then used as the frame pointer is NOT holding a value
    val = [s for s in saves if not (s == "ebp" and fp)]
    uses = 0
    if kind == "ENREG":
        pat = re.compile(r"\b%s\b" % home)
        uses = sum(1 for i in ins if i.address != va and pat.search(i.op_str))
    elif kind == "SPILL":
        uses = sum(1 for i in ins if home in i.op_str.replace(" ", ""))
    return dict(kind=kind, home=home, eh=eh, fp=fp, uses=uses,
                saved="".join(SAVE_LETTER[s] for s in val) or "-",
                nsaved=len(val), avail=3 if fp else 4, sat=len(val) == (3 if fp else 4))


def main():
    args = sys.argv[1:]
    EXT = extents()
    if args[:1] == ["--exact"]:
        names = [ln.split(None, 1) for ln in open(args[1])]
    elif args[:1] == ["--all"]:
        names = [("%#010x" % va, "?") for va in sorted(EXT)]
    else:
        print(__doc__)
        return 2
    rows = []
    for va_s, name in names:
        va, name = int(va_s, 16), name.strip()
        if name != "?" and not THISCALL.search(name):
            continue
        L = EXT.get(va)
        if not L or L < 8:
            continue
        r = classify(va, L)
        if r:
            r.update(va=va, name=name)
            rows.append(r)
    if not rows:                      # v103/v111: an empty result is a bug hypothesis
        sys.stderr.write("thisscan: NO rows — check the input set (positive control failed)\n")
        return 1
    print("# %d __thiscall function(s) classified" % len(rows))
    for grp, pred in (("NO EH FRAME  (ebp is a value register; 4 available)", lambda r: not r["fp"]),
                      ("EH FRAME     (ebp is the frame pointer; 3 available)", lambda r: r["fp"])):
        sub = [r for r in rows if pred(r)]
        c = collections.Counter(r["kind"] for r in sub)
        print("\n== %s   n=%d" % (grp, len(sub)))
        print("   ENREG %-4d SPILL %-4d ECX %-4d" % (c["ENREG"], c["SPILL"], c["ECX"]))
        # print the minority class, which is where the mechanism lives
        minority = "SPILL" if c["SPILL"] <= c["ENREG"] else "ENREG"
        print("   minority class (%s) — this is where the mechanism lives:" % minority)
        for r in sorted([r for r in sub if r["kind"] == minority], key=lambda r: -r["uses"]):
            print("     %#010x %-6s home=%-14s saved=%-4s uses=%-3d %s"
                  % (r["va"], r["kind"], r["home"][:14], r["saved"], r["uses"], r["name"][:46]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
