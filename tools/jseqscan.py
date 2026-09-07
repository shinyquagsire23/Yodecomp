#!/usr/bin/env python3
"""THE CONDITIONAL-JUMP SEQUENCE CENSUS — arm order, localised, without alignment (v134).

`mixscan.py` (lesson #60) told us that ShowWinMessage 0x40f4b0 emitted `jne -5 / je +5`:
five conditional branches whose POLARITY is inverted relative to the original.  That is a
count, not a location, and in a 2025-byte residual differing in 82 % of its bytes there was
no way to find them — `sbs.py` prints 880 lines and `armscan.py` structurally cannot help,
because it compares jcc's at the same ALIGNED instruction boundary and nothing after the
first differing byte is aligned any more.

The fix is to stop aligning by ADDRESS and align the two SEQUENCES instead.  Decode both
sides, keep only the conditional jumps, and diff the two mnemonic lists with an LCS
(difflib).  Control flow is the one property that survives a schedule shift.  On 0x40f4b0
that reduced "five flips somewhere" to five exact call sites, all of them the same
`field30 == 1` construct, and the fix (lesson #47's arm order, `if (c != 1) B; else A;`)
took the function from ext+36 to ext-9.

⚠ THE LCS IS LOAD-BEARING, NOT A REFINEMENT — the obvious positional version LIES.  Compared
by ORDINAL, `LoadWorld` 0x421fd0 reports FOURTEEN polarity flips and looks like the richest
target in the tree.  It has none: a SINGLE `je` moves from index 41 to index 77 (the
loop-exit cleanup block, which the original places mid-ladder and we place at the end — a
difference that function's note has recorded since v129), and every "flip" after it is the
positional shift talking.  A moved block rotates the whole tail of the sequence, so
alignment must be by LCS or the tool manufactures a dozen targets out of one known fact.

Read the output in three classes — the class is what tells you whether to invest:

  * POLARITY  (je<->jne, jl<->jge, jg<->jle, ja<->jbe, jb<->jae, js<->jns)
      The two arms are in the opposite ORDER to the original's.  This is lesson #47 and it
      is SOURCE-STEERABLE: cl emits the *then* arm as the fallthrough, except that it
      INVERTS when it tail-merges the two arms, so a merged if/else reads backwards.  Swap
      the arms in the source and re-measure.
  * MIRROR    (jl<->jg, jle<->jge, jb<->ja, jbe<->jae)
      Same branch, compare operands exchanged.  That is lesson #54's compare-encoding
      peephole: DIAL-BOUND, not source-bound.  ⛔ Park it; a spelling sweep there is
      guaranteed waste (v123 spent ~35 compiles proving exactly that).
  * OTHER     anything else — a genuinely different test; read it with sbs.py.

  * SHAPE     a branch present on one side and not the other, at that point in the stream
      Either the control-flow shape genuinely differs (a missing early-out, an un-rotated
      loop, a cross-jump we merged and the original did not — lessons #46/#52/#60), or a
      BLOCK HAS MOVED, which shows up as one delete plus one insert of the same mnemonic.
      Check the pairing before reading a delete as a missing branch.

POSITIVE CONTROL: every BYTE-EXACT function whose length equals its extent must report an
identical sequence.  The tool checks this itself and exits 1 if one does not (aritycheck /
framescan / mixscan discipline — a tool must agree with the anchor at zero perturbation).

⚠ A hit is a CANDIDATE, not a defect.  A polarity flip can be a SYMPTOM that follows the
body rather than the defect itself (the v126 DrawWeaponBox trap, lesson #41) — and on
0x40f4b0 the three `GetAt(GetSize() - 1)` selectors are NOT flipped while the five
tail-merged ones are, i.e. the polarity is PER SITE and a file-wide rewrite is wrong.

⚠ Decode traps, both inherited from mixscan.py and both of which fabricated targets before:
decode the ORIGINAL from raw EXE bytes at its OWN Ghidra extent (never through a buffer cut
to our length — the v121 desync), and EXCLUDE functions carrying a jump TABLE, whose
reloc-zeroed entries decode as instructions on our side only.

Usage:  python3 tools/jseqscan.py [--shape-only]
        --shape-only  only the SHAPE class (a branch present on one side only)

Unlike mixscan/residuals --lenmis there is no length filter: control flow is comparable
whether or not the emitted length matches, and several of the hits below sit at length +-0.

⚠ COMPILES (it needs our own COMDATs) — do not run it while a vartest/declorder sweep is in
flight; they fight over build/*.obj.
"""
import sys, os, glob, difflib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import residuals, match, capstone

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# cl only ever emits the signed/unsigned/equality/sign families here; jecxz/loop are not
# conditional jumps in this sense and never appear in cl 10.20 output for this image.
INVERSE = {"je": "jne", "jne": "je", "jl": "jge", "jge": "jl", "jg": "jle", "jle": "jg",
           "jb": "jae", "jae": "jb", "ja": "jbe", "jbe": "ja", "js": "jns", "jns": "js",
           "jo": "jno", "jno": "jo", "jp": "jnp", "jnp": "jp"}
MIRROR = {"jl": "jg", "jg": "jl", "jle": "jge", "jge": "jle",
          "jb": "ja", "ja": "jb", "jbe": "jae", "jae": "jbe"}


def jseq(buf, va):
    """[(offset, mnemonic)] for every CONDITIONAL jump, in stream order."""
    out = []
    for ins in _md.disasm(bytes(buf), va):
        m = ins.mnemonic
        if m.startswith("j") and m != "jmp" and m in INVERSE:
            out.append((ins.address - va, m))
    return out


def classify(o, u):
    if INVERSE.get(o) == u:
        return "POLARITY"
    if MIRROR.get(o) == u:
        return "MIRROR"
    return "OTHER"


def main():
    counts_only = "--shape-only" in sys.argv

    rows, shapes, control, n_stub, n_table = [], [], 0, 0, 0
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in residuals.paired(cpp):
            L = match.trim_pad(code)
            ext = residuals.EXTENT.get(va)
            if ext is None:
                n_stub += 1
                continue
            if residuals.has_jumptable(code[:L], va):
                n_table += 1
                continue
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            cm = match.mask(code, relocs, L)
            om = match.mask(residuals.EXE[foff:foff + L], relocs, L)
            ndiff = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])

            a = jseq(residuals.EXE[foff:foff + ext], va)
            b = jseq(code[:L], va)
            am = [m for _, m in a]
            bm = [m for _, m in b]

            if ndiff == 0 and L == ext:                        # positive control
                control += 1
                if am != bm:
                    sys.stderr.write("CONTROL FAILED: byte-exact 0x%08x %s jcc sequence "
                                     "differs (%d vs %d)\n" % (va, name, len(a), len(b)))
                    return 1
                continue
            if ndiff == 0:
                continue

            tu = os.path.basename(cpp)
            subs, only_o, only_u = [], [], []
            for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(
                    a=am, b=bm, autojunk=False).get_opcodes():
                if tag == "equal":
                    continue
                if tag == "replace" and (i2 - i1) == (j2 - j1):
                    subs += [(i1 + k, a[i1 + k], b[j1 + k]) for k in range(i2 - i1)]
                else:
                    only_o += [(i1 + k, a[i1 + k]) for k in range(i2 - i1)]
                    only_u += [(j1 + k, b[j1 + k]) for k in range(j2 - j1)]
            if only_o or only_u:
                shapes.append((va, tu, name, L - ext, len(a), len(b), only_o, only_u))
            if subs:
                rows.append((len(subs), va, tu, name, L - ext, len(a), subs))

    print("# CONDITIONAL-JUMP SEQUENCE census (v134): the two ordered lists of jcc mnemonics,")
    print("# aligned by LCS so a schedule shift cannot desync them (and a MOVED BLOCK reads as")
    print("# one delete + one insert instead of a dozen phantom flips -- see the docstring).")
    print("# POLARITY = arm order, lesson #47, SOURCE-STEERABLE.")
    print("# MIRROR   = compare-operand exchange, lesson #54, DIAL-BOUND -> park on sight.")
    print()
    print("=== SHAPE — a branch on one side only (strongest class; check for a MOVED block) ===")
    print("%-11s %-22s %-42s %6s %5s %5s  %s"
          % ("addr", "TU", "name", "len", "orig", "ours", "orig-only / ours-only"))
    for va, tu, name, d, na, nb, oo, ou in sorted(shapes, key=lambda r: -(len(r[6]) + len(r[7]))):
        fmt = lambda L: ", ".join("#%d %s@+%x" % (i, m, off) for i, (off, m) in L[:4]) or "-"
        print("0x%08x %-22s %-42s %+6d %5d %5d  %s  |  %s"
              % (va, tu, name[:42], d, na, nb, fmt(oo), fmt(ou)))
    print("(%d function(s))" % len(shapes))

    if counts_only:
        return 0

    print()
    print("=== SUBSTITUTIONS — same branch, different mnemonic ===")
    print("%-11s %-22s %-42s %6s %4s  %s"
          % ("addr", "TU", "name", "len", "n", "sites (idx orig@off -> ours@off) [class]"))
    for nbad, va, tu, name, d, ntot, subs in sorted(rows, key=lambda r: -r[0]):
        sites = "; ".join("%d %s@+%x->%s@+%x [%s]"
                          % (k, ao[1], ao[0], bo[1], bo[0], classify(ao[1], bo[1]))
                          for k, ao, bo in subs[:6])
        if len(subs) > 6:
            sites += "; ... (%d more)" % (len(subs) - 6)
        print("0x%08x %-22s %-42s %+6d %4d  %s" % (va, tu, name[:42], d, ntot, sites))
    print("(%d function(s))" % len(rows))

    print()
    print("positive control CLEAN over %d byte-exact function(s); %d marker(s) have no usable\n"
          "Ghidra extent and %d carry a jump TABLE — both EXCLUDED rather than mis-reported."
          % (control, n_stub, n_table))
    return 0


if __name__ == "__main__":
    sys.exit(main())
