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
fail=0
for cpp in $(find src -name "*.cpp" | sort); do
  d=$(dirname "$cpp"); b=$(basename "$cpp" .cpp)
  (cd "$d" && rm -f "$b.obj" && "$ROOT/toolchain/bin/cl" $FLAGS "$b.cpp" >/dev/null 2>&1)
  if [ -f "$d/$b.obj" ]; then cp "$d/$b.obj" "$BD/$b.obj"; else echo "  COMPILE FAIL: $cpp"; fail=1; fi
done
[ "$fail" = 1 ] && { echo "abort: a TU failed to compile"; exit 1; }

echo "=== linking (static MFC: NAFXCW + LIBCMT) ==="
# Win32 import libs the demo pulls in (WAVMIX32 has no import lib here -> its 11 imports stay
# unresolved by design until a wavmix32.lib/stub is added; they are external, not our source).
( cd "$BD" && "$ROOT/toolchain/bin/link" /NOLOGO /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OUT:yoda.exe \
    *.obj \
    nafxcw.lib libcmt.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
    advapi32.lib shell32.lib comctl32.lib > link.log 2>&1 )
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
