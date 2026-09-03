#!/usr/bin/env python3
"""Which residuals are explained ENTIRELY by a consistent REGISTER PERMUTATION?

⭐ This is the census that finds targets for the lesson-#37 decl-scope lever (tools/hoisttest.py).
A residual whose every differing instruction becomes the original's under ONE substitution
reg->reg (esi->edi, edi->esi, ...) has identical mnemonics, schedule and operand order — only
register NAMES differ. That is the fingerprint of a register-ALLOCATION difference, which
declaration scope steers; it is NOT a compiler ceiling, despite what several park notes claimed.

v104 result: 10 of 138 residuals are pure permutations, and the lever landed 4 of them.

Shares progress.py's compile + COMDAT-filter + pairing block verbatim (the v100/v101 rule), so
"exact" here is the anchor's own predicate. Compiling all 13 TUs takes a few minutes; --cache
saves/reuses the masked bytes so later runs are instant.

Usage:  tools/regperm.py [--cache <file.pkl>] [--all]
        --all also lists the non-permutation residuals with their verdict.
"""
import os, sys, re, glob, pickle
from collections import Counter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capstone, match, verify
import progress as prog

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE = prog.EXE
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

FAM, SIZE = {}, {}
for fam, names in {"a": ("eax", "ax", "al", "ah"), "b": ("ebx", "bx", "bl", "bh"),
                   "c": ("ecx", "cx", "cl", "ch"), "d": ("edx", "dx", "dl", "dh"),
                   "si": ("esi", "si"), "di": ("edi", "di"),
                   "bp": ("ebp", "bp"), "sp": ("esp", "sp")}.items():
    for n in names:
        FAM[n] = fam
        SIZE[n] = 4 if n.startswith("e") else (1 if n[-1] in "lh" and len(n) == 2 else 2)
RE = re.compile(r"\b(" + "|".join(sorted(FAM, key=len, reverse=True)) + r")\b")


def paired(cpp):
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


def collect():
    out = {}
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        for va, name, code, relocs in paired(cpp):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            out[va] = (os.path.relpath(cpp, ROOT), name,
                       bytes(match.mask(orig, relocs, L)), bytes(match.mask(code, relocs, L)))
        sys.stderr.write("  scanned %s\n" % os.path.basename(cpp))
    return out


def verdict(orig, ours):
    """('PERM', {ourfam: origfam}) | ('len-mismatch'|'unaligned'|'mixed'|'other', None)"""
    if len(orig) != len(ours):
        return "len-mismatch", None
    A = {i.address: (i.mnemonic, i.op_str) for i in md.disasm(orig, 0)}
    B = {i.address: (i.mnemonic, i.op_str) for i in md.disasm(ours, 0)}
    if set(A) != set(B):
        return "unaligned", None
    sub, bad = {}, 0
    for off in sorted(A):
        (ma, oa), (mb, ob) = A[off], B[off]
        if (ma, oa) == (mb, ob):
            continue
        if ma != mb:
            bad += 1; continue
        ta, tb = RE.split(oa), RE.split(ob)
        if len(ta) != len(tb) or ta[::2] != tb[::2]:
            bad += 1; continue
        for k in range(1, len(ta), 2):
            if SIZE[ta[k]] != SIZE[tb[k]] or sub.setdefault(FAM[tb[k]], FAM[ta[k]]) != FAM[ta[k]]:
                bad += 1; break
    perm = {k: v for k, v in sub.items() if k != v}
    if bad == 0 and perm and len(set(perm.values())) == len(perm):
        return "PERM", perm
    return ("mixed" if perm else "other"), None


def main():
    args = sys.argv[1:]
    cache = None
    if "--cache" in args:
        i = args.index("--cache"); cache = args[i + 1]; del args[i:i + 2]
    show_all = "--all" in args

    if cache and os.path.exists(cache):
        data = pickle.load(open(cache, "rb"))
        sys.stderr.write("[loaded %d functions from %s]\n" % (len(data), cache))
    else:
        data = collect()
        if cache:
            pickle.dump(data, open(cache, "wb"))

    rows = []
    for va, (cpp, name, orig, ours) in sorted(data.items()):
        if orig == ours:
            continue
        nd = (sum(1 for i in range(len(orig)) if orig[i] != ours[i])
              if len(orig) == len(ours) else None)
        v, perm = verdict(orig, ours)
        rows.append((0 if v == "PERM" else 1, nd if nd is not None else 9999, cpp, va, name, v, perm))
    rows.sort()
    n_perm = sum(1 for r in rows if r[5] == "PERM")
    print("%d non-exact; %d explained ENTIRELY by a register permutation "
          "(-> tools/hoisttest.py, lesson #37)\n" % (len(rows), n_perm))
    print("%-24s %-12s %5s  %-28s %s" % ("cpp", "va", "ndiff", "permutation", "name"))
    for _, nd, cpp, va, name, v, perm in rows:
        if v != "PERM" and not show_all:
            continue
        tag = (",".join("%s->%s" % (k, x) for k, x in sorted(perm.items()))
               if perm else v)
        print("%-24s 0x%08x %5s  %-28s %s"
              % (os.path.basename(cpp), va, nd, tag, name[:46]))
    print("\nverdicts:", Counter(r[5] for r in rows).most_common())


if __name__ == "__main__":
    main()
