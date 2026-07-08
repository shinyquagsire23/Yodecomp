#!/usr/bin/env python3
"""Completion dashboard: how much of YodaDemo.exe's app region is byte-matched.

Compiles every src/**/*.cpp with the VC++ 4.2 toolchain, byte-matches each
annotated function against the original (relocations masked), and reports two
tiers against the total app-function bytes:

  EXACT    byte-identical (reloc-masked) — done.
  PARTIAL  transcribed (has a // FUNCTION marker + a compiled COMDAT) but not
           byte-exact — the C is written; only byte-matching polish remains.

Everything else still needs DECOMPILING. Functions carried by more than one TU
(e.g. the retired src/Dta duplicates of Worldgen parsers) are deduped by
address, an exact copy in any TU winning. Partial byte counts use the compiled
COMDAT's trimmed length as a proxy for the original function size (close, but
can drift a few bytes on length-shifted bodies — dashboard precision only).

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
    build = os.path.join(ROOT, "build"); os.makedirs(build, exist_ok=True)
    obj = os.path.join(build, os.path.splitext(os.path.basename(cpp))[0] + ".obj")
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
    fo = "/Fo" + os.path.relpath(obj, os.path.dirname(cpp))
    r = subprocess.run([CL] + flags + [fo, os.path.basename(cpp)],
                       cwd=os.path.dirname(cpp), env=env, capture_output=True)
    return obj if r.returncode == 0 and os.path.exists(obj) else None


def main():
    # addr -> (exact, nbytes): dedupe functions carried by more than one TU
    # (exact beats partial; among equals keep the first seen).
    by_addr = {}
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
        mb = mf = pf = 0
        for va, name, code, relocs in match.pair_by_name(text, funcs):
            L = match.trim_pad(code)
            foff = (va - match.TEXT_VA) + match.TEXT_RAW
            orig = EXE[foff:foff + L]
            cm, om = match.mask(code, relocs, L), match.mask(orig, relocs, L)
            diffs = sum(1 for i in range(min(len(cm), len(om))) if cm[i] != om[i])
            exact = diffs == 0 and len(orig) == L
            if exact:
                mb += L; mf += 1
            else:
                pf += 1
            prev = by_addr.get(va)
            if prev is None or (exact and not prev[0]):
                by_addr[va] = (exact, L)
        rows.append((rel, "%d+%d/%d" % (mf, pf, len(funcs)), mb, len(funcs)))

    exact_bytes = sum(L for e, L in by_addr.values() if e)
    exact_funcs = sum(1 for e, L in by_addr.values() if e)
    partial_bytes = sum(L for e, L in by_addr.values() if not e)
    partial_funcs = sum(1 for e, L in by_addr.values() if not e)

    print("=" * 60)
    print(" Yodecomp completion — app region (0x401000-0x429000)")
    print("=" * 60)
    print("  %-26s %-12s %8s" % ("", "exact+part", "exact B"))
    for rel, note, mb, nf in rows:
        print("  %-26s %-12s %6d B" % (rel, note, mb))
    print("-" * 60)
    e_pct = 100.0 * exact_bytes / TOTAL_APP_BYTES
    p_pct = 100.0 * partial_bytes / TOTAL_APP_BYTES
    t_pct = e_pct + p_pct
    print("  EXACT    %6d bytes  (%d funcs)  — byte-matched" % (exact_bytes, exact_funcs))
    print("  PARTIAL  %6d bytes  (%d funcs)  — transcribed, needs byte-matching"
          % (partial_bytes, partial_funcs))
    print("  TOTAL    %6d bytes  (%d funcs in app region)" % (TOTAL_APP_BYTES, TOTAL_APP_FUNCS))
    print("  >>> %.2f%% exact + %.2f%% partial = %.2f%% transcribed; "
          "%.2f%% left to decompile <<<" % (e_pct, p_pct, t_pct, 100.0 - t_pct))
    # HONEST transcription coverage (v23): the PARTIAL sum above uses OUR COMDAT
    # lengths (EH funclets + jump tables included), which overcounts against the
    # 128158 denominator. Measure marker coverage against TRUE Ghidra extents
    # (toolchain/test/app_funcs.txt, regenerate via the run_script_inline dump).
    ext_path = os.path.join(ROOT, "toolchain", "test", "app_funcs.txt")
    if os.path.exists(ext_path):
        table = {}
        for ln in open(ext_path):
            p = ln.split()
            if len(p) == 2:
                table[int(p[0], 16)] = int(p[1])
        marked = set()
        for cpp in glob.glob(os.path.join(ROOT, "src", "**", "*.cpp"), recursive=True):
            for m in re.finditer(r"FUNCTION:\s*YODA\s+0x0*([0-9a-fA-F]+)",
                                 open(cpp).read()):
                marked.add(int(m.group(1), 16))
        tot = sum(table.values())
        cov = sum(sz for va, sz in table.items() if va in marked)
        un = sorted((sz, va) for va, sz in table.items() if va not in marked)
        print("  --- Ghidra-extent basis (%d funcs, %d bytes incl. funclets/tables) ---"
              % (len(table), tot))
        print("  >>> marker coverage %.2f%% (%d bytes); largest unclaimed:" 
              % (100.0 * cov / tot, cov))
        for sz, va in un[-6:][::-1]:
            print("      %#x  %d bytes" % (va, sz))


if __name__ == "__main__":
    main()
