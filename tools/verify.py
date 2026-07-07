#!/usr/bin/env python3
"""Verify byte-matching of a compiled module against YodaDemo.exe — robust to MFC.

Unlike match.py's positional best-fit, this pairs each `// FUNCTION: YODA 0xADDR` marker to the
COMDAT for the SAME source function, skipping the extra library COMDATs the compiler emits for an
MFC-derived class (CObject/CArchive/... base methods + auto vector-dtor helpers), which come from
NAFXCW.LIB at link time and are not ours to match.

Usage:
    # 1. compile the module's .obj (MFC classes need /D_MBCS):
    toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS \\
        src/Zone/Zone.cpp     # run from src/Zone/ ; produces Zone.obj
    # 2. verify:
    python3 tools/verify.py src/Zone/Zone.cpp
Add -v to dump the first differing offsets (orig vs ours byte) for each non-exact function.
"""
import re, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match  # reuse coff_functions / trim_pad / mask

TEXT_VA, TEXT_RAW = 0x401000, 0x400
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# MFC base classes whose COMDATs are library code (linked from NAFXCW.LIB), not ours to match.
LIB_OWNERS = ("CObject", "CArchive", "CDumpContext", "CRuntimeClass", "CException", "CString",
              "AFX_EXCEPTION_LINK",  # out-of-line inline-dtor COMDAT (folded to NAFXCW at link)
              "CGdiObject", "CPalette", "CFileException", "CFile", "CDocument", "CCmdTarget",
              "CWinApp", "CWnd", "CDialog", "CDataExchange", "CSingleDocTemplate",
              "CDocTemplate", "CRuntimeClass")  # MFC classes whose inline members COMDAT out


def owner_of(mangled):
    """The class a mangled MSVC C++ name belongs to.
    Special names put the class right after the operator code: ??0Zone@@ (ctor), ??1CObject@@ (dtor),
    ??_GZone@@ (scalar-deleting dtor), ??_EX@@ (vector dtor). Named methods put it after the first @:
    ?FindObjectAt@Zone@@ . Returns None for free functions."""
    m = re.match(r"\?\?(?:_[A-Za-z]|[0-9])([A-Za-z_]\w*)@@", mangled)      # ctor/dtor/vector-dtor
    if not m:
        m = re.match(r"\?[A-Za-z_]\w*@([A-Za-z_]\w*)@@", mangled)          # NamedMethod@Class@@
    return m.group(1) if m else None


def main():
    if len(sys.argv) < 2:
        print(__doc__); return 2
    src = sys.argv[1]
    verbose = "-v" in sys.argv
    exe = os.path.join(ROOT, "YodaDemo/YodaDemo.exe")
    obj = os.path.splitext(src)[0] + ".obj"
    if not os.path.exists(obj):
        print("obj not found — compile first (see this file's header):", obj); return 2

    text = open(src).read()
    exe_bytes = open(exe, "rb").read()
    # keep only our COMDATs (drop MFC base-class library methods + CRT init thunks), then pair
    # each marker to its SAME-named COMDAT (best-fit mis-assigns reloc-masked-identical stubs).
    # Exception: a marker with an EXPLICIT mangled hint (e.g. "(??0CGdiObject@@ ctor COMDAT)")
    # claims its COMDAT even when the class is lib-owned — some MFC COMDAT copies provably
    # SURVIVE per-TU in the app region (the CException dtor family, GameView's CGdiObject/
    # CBitmap trios) and are ours to match there.
    hinted = set(re.findall(
        r"//\s*FUNCTION:\s*YODA\s+0x[0-9a-fA-F]+[^\n]*?(\?\?(?:_[A-Z]|[0-9])\w+@@)", text))
    funcs = [f for f in match.coff_functions(obj)
             if (owner_of(f[0]) not in LIB_OWNERS
                 or any(h in f[0] for h in hinted))
             and not f[0].lstrip("?").startswith(("_$E", "$E"))]
    paired = match.pair_by_name(text, funcs)

    # pair by name (marker's derived mangled substring), positional fallback inside pair_by_name
    ok = exact_bytes = 0
    rows = []
    for va, name, code, relocs in paired:
        L = match.trim_pad(code)
        orig = exe_bytes[(va - TEXT_VA) + TEXT_RAW:][:L]
        cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
        diffs = [i for i in range(min(len(cm), len(om))) if cm[i] != om[i]]
        rows.append((va, name, L, diffs, orig, code))
    for va, name, L, diffs, orig, code in rows:
        status = "MATCH" if not diffs else f"DIFF({len(diffs)})"
        print(f"  {va:#08x}  {status:>9}  len={L:<4} {name}")
        ok += not diffs
        exact_bytes += L if not diffs else 0
        if verbose and diffs:
            for o in diffs[:8]:
                print(f"       @{o:#05x} orig={orig[o]:02x} ours={code[o]:02x}")
    print(f"\n{ok}/{len(rows)} exact   ({exact_bytes} bytes byte-identical)")
    print("  (non-exact = effective matches: reg-alloc / instruction-selection tie-breaks; see the")
    print("   in-source // EFFECTIVE MATCH annotations. Disasm cross-check: Ghidra @addr vs the .obj.)")
    return 0 if ok == len(rows) else 1


if __name__ == "__main__":
    sys.exit(main())
