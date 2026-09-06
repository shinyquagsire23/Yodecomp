#!/usr/bin/env python3
"""Census: the LOCAL FRAME SIZE of every residual, ORIGINAL vs OURS.

⭐ v128.  A function's prologue reserves its spill/locals area with `sub esp,N` (or, under an
EH frame, `push ebp; mov ebp,esp; ...; sub esp,N`).  N is a one-number summary of how many
values cl 10.20 could NOT keep in registers — and unlike the callee-save set (savescan.py,
lesson #42/#43) or `this` residency (thisscan.py, lesson #55), NOTHING in this project ever
compared it against the original.  It is the cheapest read there is on a whole class of
residual that no register dial can reach:

  * OURS LARGER  ⇒ we HOMED a local the original keeps in a register.  The usual cause is a
    live range we started too early, so it overlaps a value cl wanted to re-use.  This is what
    v128's WorldgenPlaceItemForLockChainMaybe 0x41d0c0 was: `int nOk = 0; if (c) nOk = f();`
    starts nOk BEFORE item1a's last use (`push ebx`), so nOk cannot inherit item1a's dead
    register and gets a frame slot instead — `sub esp,8` against the original's `sub esp,4`.
    Assigning nOk in BOTH ARMS of an if/else let cl sink `mov ebx,0` into the argument setup
    and re-use EBX, exactly as the original does.  117 B -> 33 B, length onto the extent.
  * OURS SMALLER ⇒ the original homed something we do not have at all — a local we merged
    away, or (lesson #50) a member reload we suppressed with a cached alias.

⚠ A hit is a CANDIDATE, not a defect.  The frame also carries EH state and compiler temps, so
a difference can be downstream of any other lever; confirm with tools/sbs.py before investing.

Positive control (it FAILS LOUDLY if this breaks): no BYTE-EXACT function may report a frame
mismatch — such a function's prologue bytes are identical to the original's by construction,
so a hit there would mean the prologue walk is lying (the v110 savescan / v120 aritycheck trap).

⚠ COMPILES the tree.  Do not run it while another sweep or progress.py is in flight.

Usage:  python3 tools/framescan.py [--exact <file>]   (cached set from tools/exactset.py)
"""
import os, sys, glob
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, residuals

MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
EXE = residuals.EXE


def extents():
    out = {}
    for ln in open(os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")):
        p = ln.split()
        if len(p) >= 2:
            try:
                out[int(p[0], 16)] = int(p[1])
            except ValueError:
                pass
    return out


def frame(code, va):
    """(bytes reserved for locals, has_ebp_frame) read from the PROLOGUE only.

    Stop at the first call/branch — a linear sweep of a whole body desyncs on an embedded
    jump table or EH data and will silently report nonsense (v110's savescan trap).
    """
    total, fp = 0, False
    for i in MD.disasm(bytes(code), va):
        m, op = i.mnemonic, i.op_str
        if m in ("call", "jmp") or m.startswith("j"):
            break
        if m == "mov" and op == "ebp, esp":
            fp = True
        elif m == "sub" and op.startswith("esp, "):
            try:
                total += int(op.split(", ")[1], 0)
            except ValueError:
                return None, fp
        elif m == "add" and op.startswith("esp, "):
            return total, fp          # already unwinding: a frameless leaf
        elif m == "ret":
            break
    return total, fp


def main():
    cached = None
    if "--exact" in sys.argv:
        cached = set()
        for ln in open(sys.argv[sys.argv.index("--exact") + 1]):
            f = ln.split()
            if f:
                cached.add(int(f[0], 16))
    EXT = extents()
    rows, control, nseen = [], 0, 0
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            exact = (cm[:L] == om[:L])
            if cached is not None:
                exact = va in cached
            nseen += 1
            of, ofp = frame(EXE[foff:foff + min(L, 64)], va)
            sf, sfp = frame(code[:min(L, 64)], va)
            if of is None or sf is None or of == sf:
                continue
            if exact:
                control += 1
                sys.stderr.write(
                    "CONTROL FAILURE: byte-exact %s %#x reports frame %d vs %d\n"
                    % (name, va, of, sf))
                continue
            ext = EXT.get(va)
            rows.append((sf - of, va, os.path.basename(cpp), of, sf,
                         L, ext, ofp or sfp, name))
    if control:
        sys.stderr.write("\n%d control failure(s) — the prologue walk is lying; "
                         "fix that before reading anything below.\n" % control)
        sys.exit(1)
    rows.sort(key=lambda r: (-abs(r[0]), r[1]))
    print("# LOCAL FRAME SIZE, original vs ours (v128).  delta > 0 = OURS RESERVES MORE = we")
    print("# homed a local the original enregisters (the 0x41d0c0 case); delta < 0 = the")
    print("# original homed something we do not have.  A hit is a CANDIDATE — confirm with sbs.py.")
    print("%-9s %-22s %5s %5s %6s %6s %6s %4s  %s"
          % ("addr", "TU", "orig", "ours", "delta", "ourlen", "extent", "ebp", "name"))
    for d, va, tu, of, sf, L, ext, fp, name in rows:
        print("%#09x %-22s %5d %5d %+6d %6d %6s %4s  %s"
              % (va, tu, of, sf, d, L, ext if ext else "-", "y" if fp else "", name[:52]))
    print("\n%d frame mismatch(es) over %d markers; positive control CLEAN "
          "(0 byte-exact functions report one)." % (len(rows), nseen))


if __name__ == "__main__":
    main()
