#!/usr/bin/env python3
"""READ-BOTH-SIDES census: where does each image cache an IMPORT ADDRESS in a callee-saved
register (`mov <cs-reg>,[__imp__X]` + `call <reg>`) instead of repeating the 6-byte
`call dword ptr [__imp__X]`?

⭐ v125.  This tool exists because of the error it found.  The v124 note on DrawHealthDial
0x427490 named "the original CSEs the GetSysColor import address in EBX and WE DO NOT" as
THE mechanism behind its -16 residual, and the v124 pickup promoted that to the #1 item for
the next session ("worth 3 functions").  It is FALSE: we make the identical CSE, just in EDI
(`8b 3d <imp>` + four `ff d7`) where the original uses EBX (`8b 1d <imp>` + four `ff d3`).
The claim was never measured on OUR side — it was inferred from the byte diff, where the two
`mov`s land at different offsets and the `call ebx`/`call edi` bytes differ, so the construct
LOOKS absent.  Worse, the same note's own v123 register ledger three lines above already read
`ours edi=CSE then x1`, i.e. the note contradicted itself and the contradiction survived into
the pickup.

⇒ THE GENERAL LESSON (the reason this is a tool and not a scratch script): a park note of the
form "the original does X and we don't" is a claim about BOTH BINARIES, and a byte diff only
ever shows you one of them clearly.  Census the feature on OUR side too before believing it.
The project has a long list of harnesses that lied; this is the first case of the SOURCE NOTES
lying, and notes are read far more often than tools are re-run.

Positive control (asserted, not assumed): the three functions that carry this construct AND are
byte-exact — 0x417dc0, 0x4192d0, 0x408590 — must be reported on BOTH sides.  If a byte-exact
function disagrees between the two columns, this tool is broken; it says so and exits 1.

Reading the output:
  * `orig` / `ours` columns list (register, import, #calls-through-register).
  * A `memrep` entry is the OTHER form: the same import called 2+ times through memory,
    uncached.  Both forms occur in both images, so the CSE is a real compiler CHOICE, not a
    property of the toolchain — e.g. byte-exact Canvas::~Canvas 0x407eb0 repeats
    `call [DeleteObject]` FOUR times and caches nothing, in both images.
  * A row where the register DIFFERS but the construct is present on both sides is a plain
    allocation rotation; do NOT read it as a missing construct.

Usage:  python3 tools/impcse.py            (both sides; compiles every TU)
        python3 tools/impcse.py --orig     (original only; READ-ONLY, safe during a sweep)
"""
import os, sys, re, glob, struct, collections
import capstone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import match, verify
import progress as prog

EXE = open(os.path.join(ROOT, "YodaDemo", "YodaDemo.exe"), "rb").read()
MD = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
CS = ("ebx", "esi", "edi", "ebp")
LOAD_RE = re.compile(r"^(ebx|esi|edi|ebp), dword ptr \[0x[0-9a-f]+\]$")
LOAD_RE_OBJ = re.compile(r"^(ebx|esi|edi|ebp), dword ptr \[0x?0?\]$")
CONTROL = (0x417dc0, 0x4192d0, 0x408590)


def extents():
    out = {}
    for ln in open(os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")):
        p = ln.split()
        if len(p) >= 2:
            out[int(p[0], 16)] = int(p[1])
    return out


# ---------------------------------------------------------------- the ORIGINAL
def iat_names():
    """{va: 'DllFunc'} for every IAT slot, walked from the PE import directory."""
    pe = struct.unpack_from("<I", EXE, 0x3c)[0]
    nsec = struct.unpack_from("<H", EXE, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", EXE, opt + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + struct.unpack_from("<H", EXE, pe + 20)[0] + i * 40
        vsz, va, rawsz, rawptr = struct.unpack_from("<IIII", EXE, o + 8)
        secs.append((va + base, max(vsz, rawsz), rawptr))

    def raw(va):
        for sva, sz, rp in secs:
            if sva <= va < sva + sz:
                return rp + (va - sva)
        return None

    impdir = struct.unpack_from("<I", EXE, opt + 96 + 8)[0]
    out, o = {}, raw(impdir + base)
    while True:
        olt, tds, fc, nm, ft = struct.unpack_from("<IIIII", EXE, o)
        if olt == 0 and ft == 0:
            break
        dll = EXE[raw(nm + base):].split(b"\0")[0].decode()
        t, slot = raw((olt or ft) + base), ft + base
        while True:
            e = struct.unpack_from("<I", EXE, t)[0]
            if e == 0:
                break
            out[slot] = ("%s#%d" % (dll, e & 0xffff) if e & 0x80000000
                         else EXE[raw((e & 0x7fffffff) + base) + 2:].split(b"\0")[0].decode())
            t, slot = t + 4, slot + 4
        o += 20
    return out


def scan_orig(IAT, ext):
    rows = {}
    for va, L in sorted(ext.items()):
        if L < 8:
            continue
        foff = (va - match.TEXT_VA) + match.TEXT_RAW
        loads, calls, mem = {}, collections.Counter(), collections.Counter()
        for i in MD.disasm(EXE[foff:foff + L], va):
            if i.mnemonic == "mov" and LOAD_RE.match(i.op_str):
                a = int(i.op_str.split("[0x")[1].rstrip("]"), 16)
                if a in IAT:
                    loads[i.op_str.split(",")[0]] = IAT[a]
            elif i.mnemonic == "call":
                if i.op_str in CS and i.op_str in loads:
                    calls[i.op_str] += 1
                elif i.op_str.startswith("dword ptr [0x"):
                    a = int(i.op_str.split("[0x")[1].rstrip("]"), 16)
                    if a in IAT:
                        mem[IAT[a]] += 1
        rows[va] = (sorted((r, loads[r], n) for r, n in calls.items()),
                    sorted((s, n) for s, n in mem.items() if n >= 2))
    return rows


# -------------------------------------------------------------------- OUR side
def coff_with_relocs(objpath):
    """match.coff_functions keeps only reloc OFFSETS; we need the reloc SYMBOL too, because
    an unlinked COMDAT spells every import call as `call dword ptr [0]`."""
    d = open(objpath, "rb").read()
    nsec = struct.unpack_from("<H", d, 2)[0]
    symoff = struct.unpack_from("<I", d, 8)[0]
    nsym = struct.unpack_from("<I", d, 12)[0]
    sh = 20 + struct.unpack_from("<H", d, 16)[0]
    strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        o = sh + i * 40
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, flags = struct.unpack_from("<IIIIIIHHI", d, o + 8)
        secs.append((rawptr, rawsz, relptr, nrel))

    def nm(rec):
        if rec[:4] == b"\0\0\0\0":
            o = struct.unpack_from("<I", rec, 4)[0]
            return d[strtab + o:d.index(b"\0", strtab + o)].decode("latin1")
        return rec.rstrip(b"\0").decode("latin1")

    syms, i = [], 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        syms.append(nm(rec))
        syms.extend([None] * rec[17])
        i += 1 + rec[17]
    out, i = [], 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        if typ == 0x20 and secn > 0:
            rawptr, rawsz, relptr, nrel = secs[secn - 1]
            rl = {}
            for r in range(nrel):
                ro, rsym, rtyp = struct.unpack_from("<IIH", d, relptr + r * 10)
                rl[ro] = syms[rsym] if rsym < len(syms) else None
            out.append((nm(rec), d[rawptr:rawptr + rawsz], rl))
        i += 1 + naux
    return out


def short(sym):
    return sym.replace("__imp__", "").replace("__imp_", "").split("@")[0]


def scan_ours():
    rows = {}
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "*.cpp"))):
        obj = prog.compile_obj(cpp)
        if not obj:
            sys.stderr.write("COMPILE FAILED: %s\n" % cpp)
            continue
        text = open(cpp).read()
        # identical COMDAT filter/pairing block to progress.py — never re-derive it (v100)
        hinted = set(re.findall(
            r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
        allf = coff_with_relocs(obj)
        rmap = {(n, bytes(c)): rl for n, c, rl in allf}
        funcs = [(n, c, sorted(rl)) for n, c, rl in allf
                 if (verify.owner_of(n) not in verify.LIB_OWNERS
                     or any(h in n for h in hinted))
                 and not n.lstrip("?").startswith(("_$E", "$E"))]
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            rl = rmap.get((name, bytes(code)), {})
            L = match.trim_pad(code)
            loads, calls, mem = {}, collections.Counter(), collections.Counter()
            for i in MD.disasm(bytes(code[:L]), 0):
                if i.mnemonic == "mov" and LOAD_RE_OBJ.match(i.op_str):
                    s = rl.get(i.address + 2)
                    if s and "__imp_" in s:
                        loads[i.op_str.split(",")[0]] = short(s)
                elif i.mnemonic == "call":
                    if i.op_str in CS and i.op_str in loads:
                        calls[i.op_str] += 1
                    elif i.op_str.startswith("dword ptr ["):
                        s = rl.get(i.address + 2)
                        if s and "__imp_" in s:
                            mem[short(s)] += 1
            rows[va] = (sorted((r, loads[r], n) for r, n in calls.items()),
                        sorted((s, n) for s, n in mem.items() if n >= 2))
    return rows


def fmt(cse, mem):
    a = ",".join("%s=%s x%d" % (r, s, n) for r, s, n in cse) or "-"
    b = ",".join("%s x%d" % (s, n) for s, n in mem)
    return a + ("   [memrep %s]" % b if b else "")


def main():
    ext = extents()
    IAT = iat_names()
    orig = scan_orig(IAT, ext)
    print("IAT slots parsed: %d  (control: 0x45eb94 = %s)" % (len(IAT), IAT.get(0x45eb94)))
    if sys.argv[1:2] == ["--orig"]:
        for va in sorted(orig):
            c, m = orig[va]
            if c or m:
                print("%#010x  %s" % (va, fmt(c, m)))
        return 0
    ours = scan_ours()

    bad = []
    for va in CONTROL:
        oc = [(r, s, n) for r, s, n in orig.get(va, ([], []))[0]]
        uc = [(r, s, n) for r, s, n in ours.get(va, ([], []))[0]]
        if not oc or not uc:
            bad.append(va)
    if bad:
        sys.stderr.write("POSITIVE CONTROL FAILED — byte-exact %s missing the construct on "
                         "one side; the scan is broken.\n" % [hex(v) for v in bad])
        return 1
    print("positive control OK: %s carry the CSE on BOTH sides\n"
          % ", ".join(hex(v) for v in CONTROL))

    print("%-12s %-46s %s" % ("va", "ORIGINAL", "OURS"))
    ndiff = 0
    for va in sorted(set(orig) | set(ours)):
        oc, om = orig.get(va, ([], []))
        uc, um = ours.get(va, ([], []))
        if not (oc or om or uc or um):
            continue
        o_s = {(s, n) for r, s, n in oc}
        u_s = {(s, n) for r, s, n in uc}
        flag = ""
        if o_s != u_s:
            flag = "  <- CONSTRUCT DIFFERS"
            ndiff += 1
        elif [r for r, s, n in oc] != [r for r, s, n in uc]:
            flag = "  (same construct, different register)"
        print("%#010x  %-46s %s%s" % (va, fmt(oc, om)[:46], fmt(uc, um), flag))
    print("\n%d function(s) where the CSE'd import SET actually differs "
          "(a differing REGISTER is not a differing construct)." % ndiff)
    return 0


if __name__ == "__main__":
    sys.exit(main())
