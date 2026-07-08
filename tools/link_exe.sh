#!/bin/bash
# link_exe.sh — Phase G0 "link-to-complete" completeness audit.
#
# Compiles every src/**/*.cpp with the locked VC++ 4.2 toolchain, then attempts a full static-MFC
# link into a single EXE. The point is NOT (yet) a byte-identical image — it is a COMPLETENESS
# ORACLE: the linker enumerates every unresolved external (a missing/renamed function or datum) and
# every duplicate symbol (a stub-vs-real collision), turning the Phase-F/G manual sweep into a
# concrete, always-current checklist. Re-run it any session; the unresolved count should trend to 0.
#
# Usage:  tools/link_exe.sh            (from repo root)
# Output: build artifacts + link.log under $CLAUDE_JOB_DIR/tmp/build (or ./build-link if unset),
#         and a categorized summary to stdout.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BD="${CLAUDE_JOB_DIR:-$ROOT}/tmp/build-link"
mkdir -p "$BD"; rm -f "$BD"/*.obj "$BD"/link.log 2>/dev/null
FLAGS="/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS"

echo "=== compiling all TUs ==="
mkdir -p "$ROOT/build"
fail=0
for cpp in $(find src -name "*.cpp" | sort); do
  d=$(dirname "$cpp"); b=$(basename "$cpp" .cpp)
  # objs go in the repo-level build/ dir (not alongside the .cpp)
  (cd "$d" && "$ROOT/toolchain/bin/cl" $FLAGS "/Fo$ROOT/build/$b.obj" "$b.cpp" >/dev/null 2>&1)
  if [ -f "$ROOT/build/$b.obj" ]; then cp "$ROOT/build/$b.obj" "$BD/$b.obj"; else echo "  COMPILE FAIL: $cpp"; fail=1; fi
done
[ "$fail" = 1 ] && { echo "abort: a TU failed to compile"; exit 1; }

# Resources: copy the original's .rsrc tree verbatim into a linkable .res so the image RUNS
# functionally like the original (dialogs/menus/bitmaps/strings/icons/accelerators/version).
echo "=== extracting resources (.rsrc -> .res) ==="
python3 "$ROOT/tools/extract_res.py" "$ROOT/YodaDemo/YodaDemo.exe" "$BD/yoda.res"

echo "=== linking (static MFC: NAFXCW + LIBCMT + resources) ==="
# Win32 import libs the demo pulls in. WAVMIX32 is resolved by the authentic Microsoft
# wavmix32.lib import library (toolchain/wavmix32/); the EXE imports WAVMIX32.DLL by name
# just like the original, so a real (or stub) WAVMIX32.DLL provides sound at runtime.
# Build the WAVMIX32 import lib from our own non-copyrighted stub source if it isn't built yet
# (wavmix32.def + wavmix32_stub.c -> wavmix32.lib + a no-op wavmix32.dll). The implib carries the
# stdcall-decorated _WaveMix*@N thunks our objects need and imports the undecorated names, so a
# real WAVMIX32.DLL drops in for sound.
WM="$ROOT/toolchain/wavmix32"
if [ ! -f "$WM/wavmix32.lib" ]; then
  echo "=== building wavmix32 import lib (from stub source) ==="
  ( cd "$WM" && "$ROOT/toolchain/bin/cl" /nologo /c /MT /W3 /O2 /D WIN32 /D NDEBUG /D _WINDOWS wavmix32_stub.c >/dev/null 2>&1 \
      && "$ROOT/toolchain/bin/link" /NOLOGO /DLL /NOENTRY /DEF:wavmix32.def /OUT:wavmix32.dll /IMPLIB:wavmix32.lib wavmix32_stub.obj kernel32.lib >/dev/null 2>&1 )
fi
[ -f "$WM/wavmix32.lib" ] || { echo "  FAILED to build wavmix32.lib (see toolchain/wavmix32/README.md)"; exit 1; }
cp "$WM/wavmix32.lib" "$BD/wavmix32.lib"
( cd "$BD" && "$ROOT/toolchain/bin/link" /NOLOGO /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OUT:yoda.exe \
    *.obj yoda.res \
    nafxcw.lib libcmt.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
    advapi32.lib shell32.lib comctl32.lib wavmix32.lib > link.log 2>&1 )
rc=$?

echo
echo "=== AUDIT SUMMARY (link.log: $BD/link.log) ==="
echo "link exit code: $rc  (0 = fully linked)"
dup=$(grep -c "already defined" "$BD/link.log"); echo "duplicate symbols (stub-vs-real): $dup"
echo
echo "--- unique unresolved externals ---"
grep "unresolved external symbol" "$BD/link.log" \
  | sed -E 's/^[^:]+: error LNK2001: unresolved external symbol //' | sort -u \
  | tee "$BD/unresolved.txt"
echo
tot=$(wc -l < "$BD/unresolved.txt" | tr -d ' ')
wav=$(grep -c "WaveMix" "$BD/unresolved.txt")
echo "total unique unresolved: $tot  (of which WAVMIX32 external imports: $wav)"
