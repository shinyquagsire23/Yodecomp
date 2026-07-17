#!/bin/bash
# make_macos_app.sh — assemble a self-contained macOS .app from the built SDL `yoda` binary and
# its game data, then VERIFY the binary links nothing outside the OS (no /opt/homebrew, no bundled
# dylib). Driven by cmake/PortableSDL.cmake's `app` target; see BUILDING.md.
#
# The game derives its data directory from _NSGetExecutablePath (mfxstubs.cpp GetModuleFileNameA),
# which inside a bundle is .../Contents/MacOS/. So the DTA/DAW + sfx + INI are staged NEXT TO the
# binary there, not in Contents/Resources. (A future InstallHelper/XDG pass will move writable
# state out of the bundle — see CLAUDE.md GOAL notes.)
set -euo pipefail

APP="" BIN="" PLIST="" ICNS="" DATA="" DATANAME="" SFX="-" WAVDIR="-" TERRAIN="2"
while [ $# -gt 0 ]; do
  case "$1" in
    --app)      APP="$2";      shift 2 ;;
    --bin)      BIN="$2";      shift 2 ;;
    --plist)    PLIST="$2";    shift 2 ;;
    --icns)     ICNS="$2";     shift 2 ;;
    --data)     DATA="$2";     shift 2 ;;   # the DTA/DAW file
    --dataname) DATANAME="$2"; shift 2 ;;   # canonical name the engine opens (case-sensitive vols)
    --sfx)      SFX="$2";      shift 2 ;;   # dir -> copied as Contents/MacOS/sfx ('-' = none)
    --wavdir)   WAVDIR="$2";   shift 2 ;;   # loose *.WAV (Indy) copied into Contents/MacOS ('-' = none)
    --terrain)  TERRAIN="$2";  shift 2 ;;
    *) echo "make_macos_app.sh: unknown arg $1" >&2; exit 2 ;;
  esac
done
[ -n "$DATANAME" ] || DATANAME="$(basename "$DATA")"
for v in APP BIN PLIST ICNS DATA; do
  eval "val=\$$v"
  [ -n "$val" ] || { echo "make_macos_app.sh: --$(echo "$v" | tr 'A-Z' 'a-z') is required" >&2; exit 2; }
done

MACOS="$APP/Contents/MacOS"
RES="$APP/Contents/Resources"
BINNAME="$(basename "$BIN")"

rm -rf "$APP"
mkdir -p "$MACOS" "$RES"
cp "$PLIST" "$APP/Contents/Info.plist"
cp "$ICNS"  "$RES/AppIcon.icns"
cp "$BIN"   "$MACOS/$BINNAME"
chmod 755   "$MACOS/$BINNAME"

# ── game data, staged next to the binary (= the engine's data dir) ───────────────────────────
cp "$DATA" "$MACOS/$DATANAME"
if [ "$SFX" != "-" ] && [ -d "$SFX" ]; then
  cp -R "$SFX" "$MACOS/sfx"
fi
if [ "$WAVDIR" != "-" ] && [ -d "$WAVDIR" ]; then
  # Indy keeps its sound effects as loose *.WAV in the data folder (no sfx/ subdir).
  find "$WAVDIR" -maxdepth 1 -iname '*.wav' -exec cp {} "$MACOS/" \;
fi
# A minimal INI so a freshly built bundle boots (worldgen loops forever on Terrain<=0). The engine
# reads/writes "yoda.INI" (its exe base is "yoda"); it rewrites this on first run.
printf '[OPTIONS]\nTerrain=%s\nMIDILoad=1\nLCount=1\n' "$TERRAIN" > "$MACOS/yoda.INI"

# ── the footgun gate: fail the build if the binary links anything outside the OS ─────────────
# A self-contained .app must reference only /usr/lib/* and /System/Library/Frameworks/*. Anything
# else (a Homebrew dylib, a stray @rpath) would break on a machine without that exact path.
BAD="$(otool -L "$MACOS/$BINNAME" | tail -n +2 | awk '{print $1}' \
        | grep -vE '^/usr/lib/|^/System/Library/' || true)"
if [ -n "$BAD" ]; then
  echo "" >&2
  echo "ERROR: $BINNAME links non-system libraries — the .app is NOT self-contained:" >&2
  echo "$BAD" | sed 's/^/    /' >&2
  echo "Build with -DYODA_SDL_FETCH=ON so SDL3 is static (see BUILDING.md)." >&2
  exit 1
fi

echo "built $APP  (otool-clean: only /usr/lib + system frameworks)"
