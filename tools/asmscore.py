#!/usr/bin/env python3
"""Register-rename-aware structural scorer for the Yodecomp permuter.

The raw byte-diff oracle is a FLAT gradient for register-allocation differences:
"same code, swapped registers" changes many bytes at once, so no single source
mutation lowers the count until the one that fixes the allocation drops it to 0.
The hill-climb has nothing to follow (2 -> 2 -> 2 -> 0).

This module disassembles both the candidate and the original (relocations masked
to 0 so encodings stay valid and immediate/reloc values are ignored), aligns the
two instruction streams with Needleman-Wunsch on (mnemonic, operand-kinds), and
grades the residual as a weighted sum of four tiers (lower = closer):

  1. align_cost            structural distance: wrong / inserted / deleted insns,
                           or an operand that changed kind (reg<->mem<->imm).
                           Register identity is IGNORED here (normalized).
  2. reg_penalty           after alignment, is the register difference explained
                           by ONE consistent bijection?  A clean rename -> 0;
                           an inconsistent map (orig ESI -> mine EDI here, EBX
                           there) -> >0.  This is the "is it even a pure
                           allocation difference" signal.
  3. reg_identity_miss     how many aligned register slots differ from the
                           ORIGINAL's exact register.  Drives a consistent
                           rename toward the original's actual allocation.
  4. byte_diff             the original raw reloc-masked byte diff, as the finest
                           tie-break; also catches wrong immediates/displacements.

With weights 1000 / 100 / 10 / 1 the permuter first makes the instruction stream
structurally identical, then makes the register mapping a clean bijection, then
nudges that bijection onto the original's registers, then closes the last bytes.
Three extra gradient tiers where there used to be a plateau.

This is the technique decomp.me / simonlindholm's decomp-permuter use to make
randomized search converge; here it is tailored to MSVC 4.2 x86-32 output.

Standalone:  tools/asmscore.py <src.cpp> 0xADDR   (compiles + scores one function)
As a library: score(orig_bytes, mine_bytes, reloc_offsets) -> ScoreResult
"""
import os
import sys
import functools
from collections import Counter, defaultdict

import capstone

print = functools.partial(print, flush=True)

W_ALIGN = 1000
W_REGPEN = 100
W_IDENTITY = 10
W_BYTES = 1

# substitution / gap costs inside the alignment (structural tier only)
SUB_MNEM = 10       # different mnemonic
SUB_OPKIND = 4      # same mnemonic, an operand changed kind (reg<->mem<->imm)
GAP = 6             # an instruction present on only one side

_md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
_md.detail = True

# canonical sub-register -> 32-bit parent, so AL/AX/EAX are one allocation slot
_REG_PARENT = {}
for _base, _parts in {
    "eax": ("al", "ah", "ax", "eax"), "ebx": ("bl", "bh", "bx", "ebx"),
    "ecx": ("cl", "ch", "cx", "ecx"), "edx": ("dl", "dh", "dx", "edx"),
    "esi": ("sil", "si", "esi"), "edi": ("dil", "di", "edi"),
    "ebp": ("bpl", "bp", "ebp"), "esp": ("spl", "sp", "esp"),
}.items():
    for _p in _parts:
        _REG_PARENT[_p] = _base


def _canon_reg(name):
    return _REG_PARENT.get(name.lower(), name.lower())


class Insn:
    """A decoded instruction reduced to what the scorer compares."""
    __slots__ = ("mnem", "opkinds", "regs")

    def __init__(self, cs_insn):
        self.mnem = cs_insn.mnemonic
        kinds = []
        regs = []                     # ordered allocation slots (reg ops + mem base/index)
        for op in cs_insn.operands:
            if op.type == capstone.x86.X86_OP_REG:
                kinds.append(("r", op.size))
                regs.append(_canon_reg(cs_insn.reg_name(op.reg)))
            elif op.type == capstone.x86.X86_OP_MEM:
                m = op.mem
                base = cs_insn.reg_name(m.base) if m.base else None
                index = cs_insn.reg_name(m.index) if m.index else None
                kinds.append(("m", bool(base), bool(index), m.scale))
                if base:
                    regs.append(_canon_reg(base))
                if index:
                    regs.append(_canon_reg(index))
            elif op.type == capstone.x86.X86_OP_IMM:
                kinds.append(("i",))
            else:
                kinds.append(("?",))
        self.opkinds = tuple(kinds)
        self.regs = regs

    def sub_cost(self, other):
        """Structural substitution cost vs `other` (registers normalized out)."""
        if self.mnem != other.mnem:
            return SUB_MNEM
        if self.opkinds != other.opkinds:
            return SUB_OPKIND
        return 0

    def struct_key(self):
        return (self.mnem, self.opkinds)


def _decode(buf):
    return [Insn(i) for i in _md.disasm(bytes(buf), 0x1000)]


def _mask(buf, offs, n):
    b = bytearray(buf[:n])
    for o in offs:
        for k in range(4):
            if o + k < n:
                b[o + k] = 0
    return bytes(b)


def _align(a, b):
    """Needleman-Wunsch over instruction lists a (orig) and b (mine).
    Returns (align_cost, pairs) where pairs is a list of (ia|None, ib|None)."""
    na, nb = len(a), len(b)
    # cost[i][j] = min cost to align a[i:] with b[j:]
    cost = [[0] * (nb + 1) for _ in range(na + 1)]
    for i in range(na - 1, -1, -1):
        cost[i][nb] = cost[i + 1][nb] + GAP
    for j in range(nb - 1, -1, -1):
        cost[na][j] = cost[na][j + 1] + GAP
    for i in range(na - 1, -1, -1):
        ai = a[i]
        row, nrow = cost[i], cost[i + 1]
        for j in range(nb - 1, -1, -1):
            sub = nrow[j + 1] + ai.sub_cost(b[j])
            delete = nrow[j] + GAP
            insert = row[j + 1] + GAP
            row[j] = sub if sub <= delete and sub <= insert else (delete if delete <= insert else insert)
    # traceback
    pairs = []
    i = j = 0
    while i < na or j < nb:
        if i < na and j < nb and cost[i][j] == cost[i + 1][j + 1] + a[i].sub_cost(b[j]):
            pairs.append((i, j))
            i += 1
            j += 1
        elif i < na and cost[i][j] == cost[i + 1][j] + GAP:
            pairs.append((i, None))
            i += 1
        else:
            pairs.append((None, j))
            j += 1
    return cost[0][0], pairs


class ScoreResult:
    __slots__ = ("total", "exact", "align", "reg_pen", "identity_miss", "byte_diff",
                 "n_orig", "n_mine")

    def __repr__(self):
        return ("Score(total=%d exact=%s | align=%d reg_pen=%d identity_miss=%d byte_diff=%d"
                " | insns %d/%d)" % (self.total, self.exact, self.align, self.reg_pen,
                                     self.identity_miss, self.byte_diff, self.n_mine, self.n_orig))


def score(orig, mine, relocs, exact_len=None):
    """Grade `mine` against `orig` (both raw bytes), relocations masked.

    `relocs` are byte offsets (within the function) of 4-byte reloc fields.
    `exact_len` (the original's trimmed length) is used for the exact-match test;
    defaults to len(orig).
    """
    n = min(len(orig), len(mine))
    om = _mask(orig, relocs, len(orig))
    cm = _mask(mine, relocs, len(mine))
    byte_diff = sum(1 for i in range(n) if om[i] != cm[i]) + abs(len(orig) - len(mine))

    a = _decode(om)
    b = _decode(cm)
    align, pairs = _align(a, b)

    corr = defaultdict(Counter)     # orig_reg -> Counter(mine_reg)
    total_obs = 0
    identity_miss = 0
    for ia, ib in pairs:
        if ia is None or ib is None:
            continue
        ai, bi = a[ia], b[ib]
        if ai.struct_key() != bi.struct_key():
            continue                # only compare registers of structurally identical insns
        for ra, rb in zip(ai.regs, bi.regs):
            corr[ra][rb] += 1
            total_obs += 1
            if ra != rb:
                identity_miss += 1

    mapping = {ra: c.most_common(1)[0][0] for ra, c in corr.items()}
    consistent = sum(c[mapping[ra]] for ra, c in corr.items())
    violations = total_obs - consistent
    used = Counter(mapping.values())
    noninjective = sum(v - 1 for v in used.values() if v > 1)
    reg_pen = violations + 2 * noninjective

    res = ScoreResult()
    res.align = align
    res.reg_pen = reg_pen
    res.identity_miss = identity_miss
    res.byte_diff = byte_diff
    res.n_orig = len(a)
    res.n_mine = len(b)
    tgt_len = exact_len if exact_len is not None else len(orig)
    res.exact = (byte_diff == 0 and len(mine) == tgt_len)
    res.total = W_ALIGN * align + W_REGPEN * reg_pen + W_IDENTITY * identity_miss + W_BYTES * byte_diff
    return res


# --------------------------------------------------------------------------- CLI
def _cli():
    import re
    import subprocess
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import match

    src = sys.argv[1]
    addr = int(sys.argv[2], 0)
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cl = os.path.join(root, "toolchain/bin/cl")
    flags = "/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS".split()
    _t = open(src).read()
    _afx = bool(re.search(r"#\s*include\s*<afx", _t))
    if not _afx:  # look through local includes (e.g. Records.cpp -> Records.h -> <afxwin.h>)
        for _inc in re.findall(r'#\s*include\s*"([^"]+)"', _t):
            _p = os.path.join(os.path.dirname(os.path.abspath(src)), _inc)
            if os.path.exists(_p) and re.search(r"#\s*include\s*<afx", open(_p).read()):
                _afx = True
                break
    if _afx:  # MFC TU -> needs _MBCS to compile afxwin.h
        flags += ["/D", "_MBCS"]
    exe = open(os.path.join(root, "YodaDemo/YodaDemo.exe"), "rb").read()
    workdir = os.path.dirname(os.path.abspath(src))
    base = os.path.splitext(os.path.basename(src))[0]
    obj = os.path.join(workdir, base + ".obj")
    if os.path.exists(obj):
        os.remove(obj)
    r = subprocess.run([cl] + flags + [base + ".cpp"], cwd=workdir,
                       env=dict(os.environ, WINEDEBUG="-all"), capture_output=True)
    if not os.path.exists(obj):
        print(r.stdout.decode("latin1", "replace"))
        raise SystemExit("compile failed")

    text = open(src).read()
    # find the C++ function name declared just after this address' marker, so we
    # pair by name (not global-min diff, which a tiny function would win).
    mk = re.search(r"//\s*FUNCTION:\s*YODA\s+0x0*%x\b" % addr, text, re.I)
    seg = text[mk.end():] if mk else text
    dm = re.search(r"\b(?:[A-Za-z_]\w*[\s\*]+)+([A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\(", seg)
    want = dm.group(2) if dm else None
    foff = (addr - match.TEXT_VA) + match.TEXT_RAW
    best = None
    for name, code, relocs in match.coff_functions(obj):
        if want and ("?%s@" % want) not in name:
            continue
        L = match.trim_pad(code)
        orig = exe[foff:foff + L]
        res = score(orig, code[:L], relocs, exact_len=L)
        cand = (res.total, name, res)
        if best is None or cand[0] < best[0]:
            best = cand
    print("%#x  best-fit=%s  (want=%s)" % (addr, best[1], want))
    print("  ", best[2])


if __name__ == "__main__":
    _cli()
