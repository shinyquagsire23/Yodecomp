#!/usr/bin/env python3
"""Census of EVERY non-exact residual, ranked by how close it is, on the ANCHOR's definition.

WHY THIS EXISTS (v101). idiomscan classifies by DISASSEMBLY distance, which the v100 lesson
showed can lie (an embedded jump table decodes as instructions -> phantom diffs). This tool
ranks by the anchor's own oracle — the reloc-masked BYTE compare from progress.py — so the
"how close am I" number is the same number the anchor scores. Sort by ndiff to find the cheap
seams; the classifier then says whether a residual is a pure COMMUTATIVE/SELECTION TIE-BREAK
(operand order: cmp swap + jcc mirror, lea base/index swap, inc<->add of a CSE'd 1) or real
structural work.

⭐ v101 RESULT: only 5 of 144 residuals are pure tie-breaks, and all 5 are 2-byte and proven
INERT to source operand order (probed: ParseTilesMaybe, GetZoneIndex, GetFrameTile,
LoadWorldStateFile/Serialize). So that family is bounded at +5 and is NOT a productive seam —
the other 139 need real source/structure work.

Classification is done ONLY on the instructions covering the differing bytes: outside the diff
both streams are byte-identical, so a jump table elsewhere in the function cannot desync us
(the whole-function-alignment approach silently mis-classified LoadStoryHistoryNevada).

⭐ v117 — THE LENGTH COLUMN WAS VACUOUS, AND FIXING IT OPENS A NEW TARGET LIST. `lenmis` used
to be `len(orig) != L` where `orig` is EXE sliced to OUR OWN trimmed length, so it could never
be True and every residual ever published read lenmis=False. It now compares our length to
GHIDRA'S EXTENT (toolchain/test/app_funcs.txt), which is independent. That matters because
lesson #46/#48 make LENGTH the stronger structural signal: a function whose length already
matches has a register/schedule problem, while one whose length is off by N has a real
STRUCTURAL defect (a missing/extra construct), and no amount of decl-order sweeping will fix
it. `--lenmis` ranks the residuals by |our length - extent| to give that seam directly.

Usage:  tools/residuals.py [--top N] [--tie-only] [--lenmis] [--csv out.csv]
"""
import os, sys, re, glob, csv as _csv
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capstone
import match
import verify
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = prog.EXE
_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

# ⭐ Ghidra's own extent per function — the ONLY honest length oracle (v117).
# `orig` below is EXE[foff:foff+L] sliced to OUR trimmed length L, so the old
# `lenmis = len(orig) != L` could never be True except at the very end of .text:
# a column that cannot disagree is not a measurement (the v100/v109/v111 family).
# Lesson #46/#48 want our length vs the ORIGINAL's extent, which is independent.
# ⚠ 18 of the 410 entries are Ghidra STUBS (extent == 1 where the real body is tens or
# hundreds of bytes — e.g. 0x415a50 OnKeyUp reads 1 but truly ends at 0x415ab8, 104 B,
# which is exactly our length), and a few extents OVERLAP their successor's address.
# Trusting those verbatim manufactures "structural defects" that do not exist — the same
# fabricated-target failure as v100/v110. Drop any extent that is 1 or that runs past the
# next function's start; EXTENT_BAD keeps them for reporting so the gap is never silent.
EXTENT, EXTENT_BAD = {}, {}
_raw = []
for _l in open(os.path.join(ROOT, "toolchain/test/app_funcs.txt")):
    _p = _l.split()
    if len(_p) == 2:
        _raw.append((int(_p[0], 16), int(_p[1])))
_raw.sort()
for _i, (_a, _n) in enumerate(_raw):
    _gap = _raw[_i + 1][0] - _a if _i + 1 < len(_raw) else None
    if _n <= 1 or (_gap is not None and _n > _gap):
        EXTENT_BAD[_a] = _n
    else:
        EXTENT[_a] = _n

# a jcc and its operand-swapped mirror encode the SAME predicate once the cmp is reversed
MIRROR = {"jl": "jg", "jg": "jl", "jle": "jge", "jge": "jle",
          "jb": "ja", "ja": "jb", "jbe": "jae", "jae": "jbe"}
TIE_KINDS = {"cmp-swap", "test-swap", "jcc-mirror", "lea-sib-swap", "inc/add",
             "add-swap", "imul-swap", "and-swap", "or-swap", "xor-swap",
             "operand-reassoc"}


def _ops(i):
    return [o.strip() for o in i.op_str.split(",")]


def classify(a, b):
    """Kind of difference between original insn `a` and ours `b` (None = unclassified)."""
    if a is None or b is None:
        return None
    if a.mnemonic != b.mnemonic:
        if MIRROR.get(a.mnemonic) == b.mnemonic:
            return "jcc-mirror"
        if {a.mnemonic, b.mnemonic} == {"inc", "add"}:
            return "inc/add"          # orig reused a CSE'd register 1; ours picked inc
        if {a.mnemonic, b.mnemonic} == {"mov", "lea"}:
            return "mov/lea"
        return None
    oa, ob = _ops(a), _ops(b)
    if oa == ob:
        return "IDENT"
    if a.mnemonic in ("cmp", "test") and len(oa) == 2 and oa == ob[::-1]:
        return a.mnemonic + "-swap"
    if a.mnemonic == "lea":
        key = lambda s: sorted(re.findall(r"[a-z]{2,3}\b|0x[0-9a-f]+|\d+", s))
        if key(oa[1]) == key(ob[1]):
            return "lea-sib-swap"     # SIB base/index exchanged: same address, same length
    if a.mnemonic in ("add", "imul", "and", "or", "xor") and sorted(oa) == sorted(ob):
        return a.mnemonic + "-swap"
    if a.mnemonic == "mov":
        return "mov-operand"
    return None


def reassoc_addrs(pairs):
    """Addresses taking part in a CROSS-INSTRUCTION operand exchange (v125, lesson #54's
    reassociation guise).  `classify` compares ONE instruction against its counterpart, so it
    structurally cannot see an algebraic normalisation that moves an operand BETWEEN two
    neighbouring instructions:

        orig   sub eax,[i]      ; cmp eax,[nScroll]
        ours   sub eax,[nScroll]; cmp eax,[i]

    Both compute `(A - i) != nScroll`; cl 10.20 simply picks which operand lands in the `sub`
    and which in the `cmp`, at identical length and identical registers.  That reported as
    UNCLASSIFIED on DrawTextA 0x40f060 (the project's last 2-byte residual) and so escaped the
    lesson-#54 triage rule.  `pairs` is [(orig_insn, our_insn)] over the DIFFERING instructions,
    in address order.
    """
    out = set()
    for (a1, b1), (a2, b2) in zip(pairs, pairs[1:]):
        if not all((a1, b1, a2, b2)):
            continue
        if a1.address + a1.size != a2.address:      # must be adjacent in the original
            continue
        if a1.mnemonic != b1.mnemonic or a2.mnemonic != b2.mnemonic:
            continue
        oa1, ob1, oa2, ob2 = _ops(a1), _ops(b1), _ops(a2), _ops(b2)
        if not all(len(o) == 2 for o in (oa1, ob1, oa2, ob2)):
            continue
        if oa1[0] != ob1[0] or oa2[0] != ob2[0]:    # destination/left operand unchanged
            continue
        if oa1[1] == ob2[1] and oa2[1] == ob1[1] and oa1[1] != ob1[1]:
            out.add(a1.address)
            out.add(a2.address)
    return out


def has_jumptable(code, va):
    """True if this function embeds a switch JUMP TABLE.

    ⚠ Ghidra's extents in app_funcs.txt run to the function's last RET; a switch's
    trailing jump table (plus its alignment NOP) sits AFTER that and is inside OUR
    COMDAT length. So for such functions `our_len - extent` measures the table, not a
    structural defect — e.g. TriggerHotspotsMaybe 0x40ec30 reads +36 purely because of
    a 5-entry table at +316. Verified by hand against the disassembly.
    """
    for i in _md.disasm(bytes(code), va):
        if i.mnemonic == "jmp" and "[" in i.op_str and "*4" in i.op_str:
            return True
    return False


def _cover(insns):
    m = {}
    for i in insns:
        for k in range(i.size):
            m[i.address + k] = i
    return m


def paired(cpp):
    """(va, name, code, relocs) per marker — IDENTICAL filtering/pairing to progress.py."""
    obj = prog.compile_obj(cpp)
    if not obj:
        sys.stderr.write("COMPILE FAILED: %s\n" % cpp)
        return []
    text = open(cpp).read()
    hinted = set(re.findall(
        r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
    funcs = [f for f in match.coff_functions(obj)
             if (verify.owner_of(f[0]) not in verify.LIB_OWNERS
                 or any(h in f[0] for h in hinted))
             and not f[0].lstrip("?").startswith(("_$E", "$E"))]
    return match.pair_by_name(text, funcs)


def scan():
    rows = []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in paired(cpp):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            offs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
            if not offs and len(orig) == L:
                continue
            co, cs = _cover(_md.disasm(orig, va)), _cover(_md.disasm(bytes(code[:L]), va))
            kinds = set()
            seen, dpairs = set(), []          # differing instruction pairs, in address order
            for off in offs:
                a, b = co.get(va + off), cs.get(va + off)
                if a and b and (a.address, a.size) == (b.address, b.size) \
                   and a.mnemonic == b.mnemonic and a.op_str == b.op_str:
                    continue
                key = a.address if a else va + off
                if key not in seen:
                    seen.add(key)
                    dpairs.append((a, b))
            reassoc = reassoc_addrs(dpairs)
            for off in offs:
                a, b = co.get(va + off), cs.get(va + off)
                if a and b and (a.address, a.size) == (b.address, b.size) \
                   and a.mnemonic == b.mnemonic and a.op_str == b.op_str:
                    continue
                if a is not None and a.address in reassoc:
                    kinds.add("operand-reassoc")
                    continue
                kinds.add(classify(a, b) or "UNCLASSIFIED")
            kinds.discard("IDENT")
            ext = EXTENT.get(va)
            if ext is not None and has_jumptable(code[:L], va) and L > ext:
                ext = None          # length not comparable: trailing jump table
            ldelta = None if ext is None else L - ext
            lenmis = bool(ldelta)
            rows.append(dict(cpp=os.path.relpath(cpp, ROOT), va=va, name=name, L=L,
                             ndiff=len(offs), span=(offs[-1] - offs[0] + 1) if offs else 0,
                             lenmis=lenmis, ext=ext, ldelta=ldelta,
                             kinds=",".join(sorted(kinds)),
                             tie=bool(kinds) and kinds <= TIE_KINDS and not lenmis))
    rows.sort(key=lambda r: (r["ndiff"], r["L"]))
    return rows


def main():
    def opt(flag, d):
        return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else d
    top = int(opt("--top", "40"))
    rows = scan()
    if "--lenmis" in sys.argv:
        sel = sorted([r for r in rows if r["ldelta"]],
                     key=lambda r: (-abs(r["ldelta"]), r["ndiff"]))
        print("# residuals whose LENGTH disagrees with Ghidra's extent = STRUCTURAL defects\n"
              "# (a length-matched residual is a register/schedule problem; this list is not)")
        print("%-24s %-10s %6s %6s %7s %6s  %s"
              % ("TU", "addr", "ours", "extent", "delta", "ndiff", "name"))
        for r in sel[:top]:
            print("%-24s %#010x %6d %6d %+7d %6d  %s"
                  % (os.path.basename(r["cpp"]), r["va"], r["L"], r["ext"],
                     r["ldelta"], r["ndiff"], r["name"][:44]))
        tot = sum(abs(r["ldelta"]) for r in sel)
        noext = [r for r in rows if r["ext"] is None]
        print("\n%d of %d residuals have a LENGTH mismatch (%d bytes of structural error "
              "total); %d are length-EXACT and so are schedule/allocation work only"
              % (len(sel), len(rows) - len(noext), tot,
                 len(rows) - len(noext) - len(sel)))
        print("%d residual(s) have NO comparable extent (Ghidra stub, overlapping entry, or "
              "a trailing switch JUMP TABLE inside our COMDAT but outside the extent) and are "
              "excluded, not scored:\n    %s"
              % (len(noext), ", ".join("%#x" % r["va"] for r in noext) or "none"))
        return
    sel = [r for r in rows if r["tie"]] if "--tie-only" in sys.argv else rows
    print("%-24s %-10s %5s %5s %5s  %-28s %s"
          % ("TU", "addr", "len", "ndif", "span", "kinds", "name"))
    for r in sel[:top]:
        print("%-24s %#010x %5d %5d %5d  %-28s %s"
              % (os.path.basename(r["cpp"]), r["va"], r["L"], r["ndiff"], r["span"],
                 r["kinds"][:28], r["name"][:40]))
    tie = [r for r in rows if r["tie"]]
    print("\n%d non-exact residuals; %d are PURE commutative/selection tie-breaks "
          "(bounded upside, source-inert — see module docstring)" % (len(rows), len(tie)))
    c = Counter()
    for r in rows:
        c.update(k for k in r["kinds"].split(",") if k)
    print("diff-site kinds: " + ", ".join("%s=%d" % kv for kv in c.most_common()))
    path = opt("--csv", None)
    if path:
        with open(path, "w", newline="") as fh:
            w = _csv.DictWriter(fh, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)
        print("wrote %s" % path)


if __name__ == "__main__":
    main()
