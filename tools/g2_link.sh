#!/bin/bash
# g2_link.sh — Phase G2 whole-image link experiment.
#
# Unlike link_exe.sh (the completeness ORACLE, which globs objs alphabetically), this links the
# app .objs in ADDRESS ORDER (the v40-derived link order) and emits a /MAP so tools/g2_diff.py can
# report where every function LANDED vs its known original address. The goal is a byte-identical
# whole image; this script + the diff are the G2 driver/measurement.
#
# Prereq: build/<TU>.obj already compiled (run link_exe.sh or the per-TU compile loop first).
# Output: $CLAUDE_JOB_DIR/tmp/g2/{yoda.exe,yoda.map,link.log}
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
BD="${CLAUDE_JOB_DIR:-$ROOT}/tmp/g2"
mkdir -p "$BD"; rm -f "$BD"/*.obj "$BD"/yoda.* "$BD"/link.log 2>/dev/null

# App TUs in first-function-address order (v40).
ORDER="AppData World GameData Records Iact Canvas GameView IactScript Dlg Frame App WorldDoc Worldgen"
OBJS=""
for tu in $ORDER; do
  [ -f "build/$tu.obj" ] || { echo "missing build/$tu.obj — compile first"; exit 1; }
  cp "build/$tu.obj" "$BD/"
  OBJS="$OBJS $tu.obj"
done

# Resources + wavmix stub (reuse if link_exe.sh already produced them).
python3 "$ROOT/tools/extract_res.py" "$ROOT/YodaDemo/YodaDemo.exe" "$BD/yoda.res" >/dev/null 2>&1
WM="$ROOT/toolchain/wavmix32"
[ -f "$WM/wavmix32.lib" ] || { echo "missing wavmix32.lib — run link_exe.sh once first"; exit 1; }
cp "$WM/wavmix32.lib" "$BD/wavmix32.lib"

# /OPT:NOREF: keep ALL COMDATs (do not eliminate unreferenced ones). Our partial image doesn't
# reference every function the way the complete original does, so the default /OPT:REF would drop
# ~19 markers (AppWnd::Disable/Enable/GetMessageMap, …) and make the layout diff undercount. NOREF
# pairs all 378 markers so g2_diff.py can measure true layout order. (v41 finding.)
( cd "$BD" && "$ROOT/toolchain/bin/link" /NOLOGO /SUBSYSTEM:WINDOWS /INCREMENTAL:NO /OPT:NOREF /MAP:yoda.map /OUT:yoda.exe \
    $OBJS yoda.res \
    nafxcw.lib libcmt.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib \
    advapi32.lib shell32.lib comctl32.lib wavmix32.lib > link.log 2>&1 )
rc=$?
echo "link rc=$rc"
echo "yoda.exe: $(ls -la "$BD/yoda.exe" 2>/dev/null | awk '{print $5}') bytes ; map lines: $(wc -l < "$BD/yoda.map" 2>/dev/null)"
[ "$rc" = 0 ] || { echo "--- link.log tail ---"; tail -15 "$BD/link.log"; }
echo "artifacts in $BD"
