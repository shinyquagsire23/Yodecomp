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

Usage:  tools/residuals.py [--top N] [--tie-only] [--csv out.csv]
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

# a jcc and its operand-swapped mirror encode the SAME predicate once the cmp is reversed
MIRROR = {"jl": "jg", "jg": "jl", "jle": "jge", "jge": "jle",
          "jb": "ja", "ja": "jb", "jbe": "jae", "jae": "jbe"}
TIE_KINDS = {"cmp-swap", "test-swap", "jcc-mirror", "lea-sib-swap", "inc/add",
             "add-swap", "imul-swap", "and-swap", "or-swap", "xor-swap"}


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
            for off in offs:
                a, b = co.get(va + off), cs.get(va + off)
                if a and b and (a.address, a.size) == (b.address, b.size) \
                   and a.mnemonic == b.mnemonic and a.op_str == b.op_str:
                    continue
                kinds.add(classify(a, b) or "UNCLASSIFIED")
            kinds.discard("IDENT")
            lenmis = len(orig) != L
            rows.append(dict(cpp=os.path.relpath(cpp, ROOT), va=va, name=name, L=L,
                             ndiff=len(offs), span=(offs[-1] - offs[0] + 1) if offs else 0,
                             lenmis=lenmis, kinds=",".join(sorted(kinds)),
                             tie=bool(kinds) and kinds <= TIE_KINDS and not lenmis))
    rows.sort(key=lambda r: (r["ndiff"], r["L"]))
    return rows


def main():
    def opt(flag, d):
        return sys.argv[sys.argv.index(flag) + 1] if flag in sys.argv else d
    top = int(opt("--top", "40"))
    rows = scan()
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


main()
