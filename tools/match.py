#!/usr/bin/env python3
"""Match harness: compile a .cpp with the VC++ 4.2 toolchain, then byte-compare
each annotated function against the original in YodaDemo.exe (relocations masked).

Reads `// FUNCTION: YODA 0xADDR` markers from the source to learn each function's
original address (in source order), pairs them positionally with the .obj's COMDAT
function sections (MSVC emits one .text COMDAT per C++ function), trims trailing
int3/nop padding to get each function's true length, and diffs against the exe.

Usage:  tools/match.py src/World/World.cpp [--exe YodaDemo/YodaDemo.exe]
        (expects the compiled .obj next to the .cpp, same basename)
"""
import re, struct, sys, os, subprocess

TEXT_VA, TEXT_RAW = 0x401000, 0x400   # YodaDemo .text: VA 0x401000 -> file 0x400
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def coff_functions(objpath):
    d = open(objpath, "rb").read()
    nsec = struct.unpack_from("<H", d, 2)[0]
    symoff = struct.unpack_from("<I", d, 8)[0]
    nsym = struct.unpack_from("<I", d, 12)[0]
    opt = struct.unpack_from("<H", d, 16)[0]
    sh = 20 + opt
    strtab = symoff + nsym * 18
    secs = []
    for i in range(nsec):
        off = sh + i * 40
        name = d[off:off + 8].rstrip(b"\0").decode("latin1")
        vsz, va, rawsz, rawptr, relptr, lnp, nrel, nln, flags = struct.unpack_from("<IIIIIIHHI", d, off + 8)
        secs.append((name, rawptr, rawsz, relptr, nrel))

    def symname(rec):
        if rec[:4] == b"\0\0\0\0":
            o = struct.unpack_from("<I", rec, 4)[0]
            e = d.index(b"\0", strtab + o)
            return d[strtab + o:e].decode("latin1")
        return rec.rstrip(b"\0").decode("latin1")

    funcs = []  # (name, code_bytes, reloc_offsets)
    i = 0
    while i < nsym:
        rec = d[symoff + i * 18:symoff + i * 18 + 18]
        val, secn, typ, scl, naux = struct.unpack_from("<IhHBB", rec, 8)
        if typ == 0x20 and secn > 0:                 # function symbol
            nm, rawptr, rawsz, relptr, nrel = symname(rec), *secs[secn - 1][1:]
            code = d[rawptr:rawptr + rawsz]
            relocs = [struct.unpack_from("<IIH", d, relptr + r * 10)[0] for r in range(nrel)]
            funcs.append((nm, code, relocs))
        i += 1 + naux
    return funcs


def trim_pad(code):
    n = len(code)
    while n > 0 and code[n - 1] in (0xCC, 0x90):
        n -= 1
    return n


def mask(buf, offs, n):
    b = bytearray(buf[:n])
    for o in offs:
        for k in range(4):
            if o + k < n:
                b[o + k] = 0
    return bytes(b)


# --------------------------------------------------------------------------- name-based pairing
# Best-fit pairing mis-assigns COMDATs that are byte-identical after relocation masking (e.g. two
# `mov eax,imm; ret` GetMessageMap stubs, tiny `ret`-only virtuals). Pair each `// FUNCTION` marker
# to the COMDAT for the SAME source function instead, by deriving the mangled-name substring the
# marker's following declaration (or macro / parenthetical hint) implies.
_MARKER_RE = re.compile(r"//\s*FUNCTION:\s*YODA\s+0x([0-9a-fA-F]+)([^\n]*)")


def _want_key(hint, seg):
    """The mangled-name substring the function after a marker should contain, or None.
    `hint` is the rest of the marker's own line (for `(GetMessageMap)`-style annotations);
    `seg` is the source text following the marker."""
    # an explicit mangled fragment in the marker's own hint wins (e.g. "(??_GCTheApp scalar dtor)"
    # or "(?Meth@Cls)") — the escape hatch for compiler-generated COMDATs with no source line.
    m = re.search(r"(\?\?_[A-Z]\w+@?@?|\?\?[0-9]\w+@?@?|\?[A-Za-z_]\w*@\w+)", hint)
    if m:
        return m.group(1)

    # skip blank / comment-only lines to the first real declaration or macro
    lines = seg.split("\n")
    i = 0
    while i < len(lines) and (not lines[i].strip() or lines[i].lstrip().startswith("//")):
        i += 1
    decl = "\n".join(lines[i:i + 4])

    m = re.match(r"\s*BEGIN_MESSAGE_MAP\s*\(\s*(\w+)", decl)
    if m:
        return "?GetMessageMap@%s@@" % m.group(1)
    m = re.match(r"\s*IMPLEMENT_DYNCREATE\s*\(\s*(\w+)", decl)
    if m:
        cls = m.group(1)
        if "GetRuntimeClass" in hint:
            return "?GetRuntimeClass@%s@@" % cls
        return "?CreateObject@%s@@" % cls        # first of the two; CreateObject marker comes first
    m = re.match(r"\s*(\w+)::~\1\s*\(", decl)     # destructor
    if m:
        cls = m.group(1)
        return ("??_G%s@@" % cls) if ("??_G" in hint or "scalar" in hint.lower()) else ("??1%s@@" % cls)
    m = re.match(r"\s*(\w+)::\1\s*\(", decl)      # constructor
    if m:
        return "??0%s@@" % m.group(1)
    m = re.search(r"\b([A-Za-z_]\w*)::(~?[A-Za-z_]\w*)\s*\(", decl)   # Class::Method
    if m:
        return "?%s@%s@@" % (m.group(2), m.group(1))
    m = re.search(r"\b([A-Za-z_]\w*)\s*\(", decl)                     # free function
    if m and m.group(1) not in ("if", "for", "while", "switch", "return"):
        return "?%s@@" % m.group(1)               # extern-"C" names (?name missing) fall back below
    return None


def pair_by_name(text, funcs):
    """Pair (addr) markers in `text` to COMDATs in `funcs` [(name, code, relocs)] by derived
    mangled name; each COMDAT used once. Returns [(addr, name, code, relocs)] in marker order.
    Any marker whose key can't be derived, or matches no unused COMDAT, is paired positionally
    against the next unused COMDAT (so this is a strict superset of the old behaviour)."""
    markers = [(int(a, 16), h) for a, h in _MARKER_RE.findall(text)]
    positions = [m.start() for m in _MARKER_RE.finditer(text)]
    ends = [m.end() for m in _MARKER_RE.finditer(text)]
    by_name = {n: (n, c, r) for n, c, r in funcs}
    used = set()
    out = []
    for idx, (addr, hint) in enumerate(markers):
        # span to EOF (not the next marker): _want_key skips comment/marker lines to the real
        # declaration, so stacked markers (ctor + its ??_G, or the two IMPLEMENT_DYNCREATE funcs)
        # each reach the shared decl; the explicit-hint precedence disambiguates them.
        want = _want_key(hint, text[ends[idx]:])
        chosen = None
        if want:
            for n, (nm, c, r) in by_name.items():
                if n in used:
                    continue
                if want in nm or (want.startswith("?") and want[1:] in nm):
                    chosen = (nm, c, r)
                    break
        if chosen is None:                        # positional fallback: next unused COMDAT
            for nm, c, r in funcs:
                if nm not in used:
                    chosen = (nm, c, r)
                    break
        if chosen is None:
            continue
        used.add(chosen[0])
        out.append((addr, chosen[0], chosen[1], chosen[2]))
    return out


def main():
    src = sys.argv[1]
    exe = sys.argv[sys.argv.index("--exe") + 1] if "--exe" in sys.argv else os.path.join(ROOT, "YodaDemo/YodaDemo.exe")
    obj = os.path.join(ROOT, "build", os.path.splitext(os.path.basename(src))[0] + ".obj")
    if not os.path.exists(obj):
        print("obj not found; compile first:", obj); return 2

    addrs = [int(m, 16) for m in re.findall(r"//\s*FUNCTION:\s*YODA\s+0x([0-9a-fA-F]+)", open(src).read())]
    funcs = coff_functions(obj)
    if len(funcs) != len(addrs):
        print(f"WARN: {len(addrs)} markers but {len(funcs)} obj functions; matching min()")
    exe_bytes = open(exe, "rb").read()

    def diff_at(code, relocs, va):
        L = trim_pad(code)
        foff = (va - TEXT_VA) + TEXT_RAW
        orig = exe_bytes[foff:foff + L]
        cm, om = mask(code, relocs, L), mask(orig, relocs, L)
        diffs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
        return L, orig, diffs

    # best-fit assignment: pair each obj function with the address it matches best
    ok = 0
    used = set()
    for name, code, relocs in funcs:
        best = None
        for va in addrs:
            if va in used:
                continue
            L, orig, diffs = diff_at(code, relocs, va)
            score = (0 if (not diffs and len(orig) == L) else 1, len(diffs))
            if best is None or score < best[0]:
                best = (score, va, L, orig, diffs)
        _, va, L, orig, diffs = best
        used.add(va)
        status = "MATCH" if (not diffs and len(orig) == L) else f"DIFF({len(diffs)})"
        print(f"  {va:#08x}  {status:>9}  len={L} relocs={len(relocs)}  {name}")
        if status != "MATCH":
            for o in diffs[:6]:
                print(f"       @{o:#05x} orig={orig[o]:02x} ours={code[o]:02x}")
        ok += status == "MATCH"
    print(f"\n{ok}/{len(funcs)} matched")
    return 0 if ok == len(funcs) else 1


if __name__ == "__main__":
    sys.exit(main())
