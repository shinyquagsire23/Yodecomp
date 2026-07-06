#!/usr/bin/env python3
"""Completion dashboard: how much of YodaDemo.exe's app region is byte-matched.

Compiles every src/**/*.cpp with the VC++ 4.2 toolchain, byte-matches each
annotated function against the original (relocations masked), and reports
matched-bytes / total-app-function-bytes as a percentage.

Denominator (128158 bytes, 534 funcs in 0x401000-0x429000) is from Ghidra:
sum of function body sizes. Update TOTAL if the app-region function set changes.

Usage:  tools/progress.py
"""
import os, sys, glob, subprocess
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import match   # reuse coff_functions / trim_pad / mask / diff logic
import verify  # reuse owner_of / LIB_OWNERS (MFC base-class COMDAT filter)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOTAL_APP_BYTES = 128158        # Ghidra: sum of app-region (0x401000-0x429000) function body sizes
TOTAL_APP_FUNCS = 534
CL = os.path.join(ROOT, "toolchain/bin/cl")
FLAGS = "/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS".split()
EXE = open(os.path.join(ROOT, "YodaDemo/YodaDemo.exe"), "rb").read()
import re


def compile_obj(cpp):
    obj = os.path.splitext(cpp)[0] + ".obj"
    env = dict(os.environ, WINEDEBUG="-all")
    # MFC TUs need /D_MBCS to compile afxwin.h — detect it through local includes too
    # (e.g. Iact.cpp -> RecordClasses.h -> <afxwin.h>), not just a direct <afx> include.
    txt = open(cpp).read()
    afx = bool(re.search(r"#\s*include\s*<afx", txt))
    for inc in re.findall(r'#\s*include\s*"([^"]+)"', txt):
        p = os.path.join(os.path.dirname(cpp), inc)
        if os.path.exists(p) and re.search(r"#\s*include\s*<afx", open(p).read()):
            afx = True
            break
    flags = FLAGS + (["/D", "_MBCS"] if afx else [])
    r = subprocess.run([CL] + flags + [os.path.basename(cpp)],
                       cwd=os.path.dirname(cpp), env=env, capture_output=True)
    return obj if r.returncode == 0 and os.path.exists(obj) else None


def main():
    matched_bytes = 0
    matched_funcs = 0
    rows = []
    for cpp in sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True)):
        obj = compile_obj(cpp)
        rel = os.path.relpath(cpp, ROOT)
        if not obj:
            rows.append((rel, "COMPILE FAILED", 0, 0)); continue
        text = open(cpp).read()
        # drop MFC base-class library COMDATs (CObject::~CObject, Serialize, ??_GCObject, ...)
        # and CRT dynamic-init thunks (_$E123 etc. from a file-scope object with a ctor/dtor):
        # they byte-match but carry no // FUNCTION marker, so best-fit would mis-pair them.
        funcs = [f for f in match.coff_functions(obj)
                 if verify.owner_of(f[0]) not in verify.LIB_OWNERS
                 and not f[0].lstrip("?").startswith(("_$E", "$E"))]
        # pair each marker to its SAME-named COMDAT (best-fit mis-assigns reloc-masked-identical
        # stubs — two GetMessageMaps become byte-identical once their one imm reloc is masked).
        mb = mf = 0
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            if diffs == 0 and len(orig) == L:
                mb += L; mf += 1
        matched_bytes += mb; matched_funcs += mf
        rows.append((rel, "%d/%d funcs" % (mf, len(funcs)), mb, len(funcs)))

    print("=" * 56)
    print(" Yodecomp completion — app region (0x401000-0x429000)")
    print("=" * 56)
    for rel, note, mb, nf in rows:
        print("  %-26s %-14s %6d B" % (rel, note, mb))
    print("-" * 56)
    pct = 100.0 * matched_bytes / TOTAL_APP_BYTES
    print("  MATCHED  %d / %d bytes   (%d / %d funcs)" % (matched_bytes, TOTAL_APP_BYTES, matched_funcs, TOTAL_APP_FUNCS))
    print("  >>> %.2f%% of app-region code byte-matched <<<" % pct)


if __name__ == "__main__":
    main()
