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
  4. byte_diff             reloc-masked byte diff: positional when the lengths
                           match (the classic DIFF(n) count), otherwise summed
                           over the instruction alignment; also catches wrong
                           immediates/displacements.

With weights 1000 / 100 / 10 / 1 the permuter first makes the instruction stream
structurally identical, then makes the register mapping a clean bijection, then
nudges that bijection onto the original's registers, then closes the last bytes.
Three extra gradient tiers where there used to be a plateau.

This is the technique decomp.me / simonlindholm's decomp-permuter use to make
randomized search converge; here it is tailored to MSVC 4.2 x86-32 output.

EH-FUNCLET SAFETY (the 2026-07-06 fix). The old scorer masked the ORIGINAL's
bytes at the CANDIDATE's reloc offsets before disassembling: once any length
shifted, that zeroed opcode bytes mid-stream, capstone decoded garbage, and the
align tier turned into noise (the savers' phantom align=1368). Now each side is
disassembled RAW (both encodings are always valid) and relocation masking happens
per-instruction per-side: the candidate's COFF reloc fields are zeroed inside its
own instruction (and at the same intra-instruction offsets in the aligned original
when the encodings line up), and rel8/rel32 branch targets are zeroed on each side
independently (length shifts move them; the align tier still checks the branch
structurally). No pre-decode masking, no positional assumptions. A COMDAT is
copied verbatim by the linker, so main body + EH funclets + jump tables are
directly comparable in stream order — no body/funclet split is needed for the
gradient (differing funclets are real signal: a missing CString temp = a missing
funclet). Jump-TABLE data at the tail of switch functions still decodes as
garbage on both sides; that residual noise is roughly symmetric but do not trust
per-insn identity inside table bytes.

Standalone:  tools/asmscore.py <src.cpp> 0xADDR [--len N]  (compile + score one function;
             --len overrides the original's extent when the candidate's length is off)
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
    __slots__ = ("mnem", "opkinds", "regs", "off", "size", "raw",
                 "is_rel_branch", "imm_off", "imm_size", "reloc_offs")

    def __init__(self, cs_insn):
        self.mnem = cs_insn.mnemonic
        self.off = cs_insn.address
        self.size = cs_insn.size
        self.raw = bytes(cs_insn.bytes)
        self.is_rel_branch = capstone.CS_GRP_BRANCH_RELATIVE in cs_insn.groups
        self.imm_off = cs_insn.imm_offset if cs_insn.imm_size else None
        self.imm_size = cs_insn.imm_size
        self.reloc_offs = ()          # intra-insn offsets of COFF reloc fields (candidate side)
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

    def masked(self, extra_offs=()):
        """Encoding with this side's reloc fields + rel-branch target zeroed,
        plus any `extra_offs` (the OTHER side's reloc fields, same intra-insn
        offsets — valid when the two encodings line up structurally)."""
        b = bytearray(self.raw)
        zero = set()
        for o in list(self.reloc_offs) + list(extra_offs):
            zero.update(range(o, o + 4))
        if self.is_rel_branch and self.imm_off is not None:
            zero.update(range(self.imm_off, self.imm_off + self.imm_size))
        for o in zero:
            if 0 <= o < len(b):
                b[o] = 0
        return bytes(b)


def _decode(buf, relocs=()):
    """Disassemble RAW bytes (no pre-masking — encodings are always valid on both
    sides). `relocs` are function-relative offsets of 4-byte COFF reloc fields
    (candidate side); each is attached to the instruction containing it. Trailing
    int3/nop padding instructions are dropped."""
    insns = [Insn(i) for i in _md.disasm(bytes(buf), 0)]
    # cut at the FIRST int3: YodaDemo pads exclusively with 0xCC and no function
    # body contains one, so everything after it is padding + the next function
    # (an over-long orig slice must not charge gap costs for foreign code).
    for k, ins in enumerate(insns):
        if ins.mnem == "int3":
            insns = insns[:k]
            break
    if relocs:
        rel = sorted(relocs)
        for ins in insns:
            mine = tuple(o - ins.off for o in rel if ins.off <= o < ins.off + ins.size)
            if mine:
                ins.reloc_offs = mine
    while insns and insns[-1].mnem == "nop":
        insns.pop()
    return insns


def _mask(buf, offs, n):
    b = bytearray(buf[:n])
    for o in offs:
        for k in range(4):
            if o + k < n:
                b[o + k] = 0
    return bytes(b)


def _pair_bytediff(ai, bi):
    """Byte difference of one aligned pair, reloc/branch-masked per side.
    The candidate's reloc field offsets are also applied to the original when the
    encodings are the same length (same field layout once the structure matches)."""
    extra = bi.reloc_offs if ai.size == bi.size else ()
    a = ai.masked(extra)
    b = bi.masked()
    if len(a) != len(b):
        common = sum(1 for x, y in zip(a, b) if x != y)
        return common + abs(len(a) - len(b))
    return sum(1 for x, y in zip(a, b) if x != y)


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

    `relocs` are byte offsets (within the function) of 4-byte reloc fields in
    the CANDIDATE (`mine`). `exact_len` (the original's trimmed length) is used
    for the exact-match test; defaults to len(orig). `orig` may be sliced LONGER
    than exact_len (margin for EH-funclet tails) — trailing int3 padding is
    dropped at the instruction level.

    Both sides are disassembled RAW; masking is per-instruction per-side (see
    module docstring — never pre-mask the original at candidate reloc offsets).
    """
    tgt = exact_len if exact_len is not None else len(orig)
    equal = len(mine) == tgt and len(orig) >= tgt
    a = _decode(orig[:tgt] if equal else orig)
    b = _decode(mine, relocs)
    align, pairs = _align(a, b)

    # Positional byte comparison is only meaningful while byte offsets coincide —
    # which the CLI's equal slice lengths do NOT guarantee (an internal +N/-N shift
    # keeps the totals equal). Trust it only when the alignment says the streams
    # never slipped: no gaps, no size-changed pairs.
    slipped = any(ia is None or ib is None for ia, ib in pairs) or \
        any(a[ia].size != b[ib].size for ia, ib in pairs if ia is not None and ib is not None)
    if equal and not slipped:
        # the classic positional reloc-masked count (verify.py's DIFF(n) number)
        om = _mask(orig, relocs, tgt)
        cm = _mask(mine, relocs, tgt)
        byte_diff = sum(1 for i in range(tgt) if om[i] != cm[i])
    else:
        # streams slipped: sum byte differences over the instruction alignment
        # (per-side reloc/branch masking), plus the size of unpaired instructions.
        byte_diff = 0
        for ia, ib in pairs:
            if ia is not None and ib is not None:
                byte_diff += _pair_bytediff(a[ia], b[ib])
            elif ia is not None:
                byte_diff += a[ia].size
            else:
                byte_diff += b[ib].size

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
    res.exact = (len(mine) == tgt and byte_diff == 0)
    res.total = W_ALIGN * align + W_REGPEN * reg_pen + W_IDENTITY * identity_miss + W_BYTES * byte_diff
    return res


def dump_diff(orig, mine, relocs, out=None):
    """Print the instruction alignment (orig vs candidate), marking non-identical
    pairs — the human-readable view of what the align/reg tiers are charging."""
    out = out or sys.stdout
    a = _decode(orig)
    b = _decode(mine, relocs)
    _, pairs = _align(a, b)
    txt_a = {i.off: i for i in a}
    txt_b = {i.off: i for i in b}
    md2 = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    dis_a = {i.address: "%s %s" % (i.mnemonic, i.op_str) for i in md2.disasm(bytes(orig), 0)}
    dis_b = {i.address: "%s %s" % (i.mnemonic, i.op_str) for i in md2.disasm(bytes(mine), 0)}
    for ia, ib in pairs:
        ai = a[ia] if ia is not None else None
        bi = b[ib] if ib is not None else None
        sa = dis_a.get(ai.off, "?") if ai else "-"
        sb = dis_b.get(bi.off, "?") if bi else "-"
        if ai and bi and ai.struct_key() == bi.struct_key():
            mark = " " if ai.regs == bi.regs else "r"   # r = register-only difference
            if mark == " ":
                continue
        elif ai and bi:
            mark = "S"                                   # structural substitution
        else:
            mark = "-" if bi is None else "+"            # deleted / inserted vs orig
        print("%s  %-42s | %s" % (mark, ("%04x %s" % (ai.off, sa)) if ai else "",
                                  ("%04x %s" % (bi.off, sb)) if bi else ""), file=out)


# --------------------------------------------------------------------------- CLI
def _cli():
    import re
    import subprocess
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import match

    src = sys.argv[1]
    addr = int(sys.argv[2], 0)
    orig_len = int(sys.argv[sys.argv.index("--len") + 1], 0) if "--len" in sys.argv else None
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
        # --len gives the ORIGINAL's true extent (from Ghidra) — vital when the
        # candidate's length is off (EH funclets shift everything downstream).
        tgt = orig_len if orig_len is not None else L
        orig = exe[foff:foff + tgt]
        res = score(orig, code[:L], relocs, exact_len=tgt)
        cand = (res.total, name, res)
        if best is None or cand[0] < best[0]:
            best = cand
    print("%#x  best-fit=%s  (want=%s)" % (addr, best[1], want))
    print("  ", best[2])
    if "--dump" in sys.argv:
        for name, code, relocs in match.coff_functions(obj):
            if name != best[1]:
                continue
            L = match.trim_pad(code)
            tgt = orig_len if orig_len is not None else L
            dump_diff(exe[foff:foff + tgt], code[:L], relocs)


if __name__ == "__main__":
    _cli()
