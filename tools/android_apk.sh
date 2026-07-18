#!/usr/bin/env bash
# ============================================================================================
# android_apk.sh — the worker behind the `apk` CMake target (cmake/Android.cmake).
#
# Assembles a build COPY of packaging/android (never touching the committed template), stages
# the cross-compiled libmain.so per ABI + the baked game assets + the version-matched SDL Java +
# launcher icons into it, then drives gradlew to a debug .apk. Building sibling ABIs (beyond the
# one the primary CMake tree produced) happens here too, so the whole thing is one CMake target.
#
# Invoked with --args by cmake/Android.cmake; also `--install-only` to adb-install + launch.
# ============================================================================================
set -euo pipefail

# ── args ──────────────────────────────────────────────────────────────────────────────────────
SOURCE= TEMPLATE= WORK= SDL_SRC= PRIMARY_ABI= PRIMARY_SO= EXTRA_ABIS= ANDROID_PLATFORM=
NDK= GAME= VARIANT= APP_NAME= PACKAGE= RUN_DIR= PYTHON= TOOLS= ICON_EXE= ICON_INDY= OUT=
INSTALL_ONLY=0
while [ $# -gt 0 ]; do
    case "$1" in
        --source) SOURCE=$2; shift 2;;
        --template) TEMPLATE=$2; shift 2;;
        --work) WORK=$2; shift 2;;
        --sdl-src) SDL_SRC=$2; shift 2;;
        --primary-abi) PRIMARY_ABI=$2; shift 2;;
        --primary-so) PRIMARY_SO=$2; shift 2;;
        --extra-abis) EXTRA_ABIS=$2; shift 2;;
        --android-platform) ANDROID_PLATFORM=$2; shift 2;;
        --ndk) NDK=$2; shift 2;;
        --game) GAME=$2; shift 2;;
        --variant) VARIANT=$2; shift 2;;
        --app-name) APP_NAME=$2; shift 2;;
        --package) PACKAGE=$2; shift 2;;
        --run-dir) RUN_DIR=$2; shift 2;;
        --python) PYTHON=$2; shift 2;;
        --tools) TOOLS=$2; shift 2;;
        --icon-exe) ICON_EXE=$2; shift 2;;
        --indy) ICON_INDY=$2; shift 2;;
        --out) OUT=$2; shift 2;;
        --install-only) INSTALL_ONLY=1; shift;;
        *) echo "android_apk.sh: unknown arg $1" >&2; exit 2;;
    esac
done

# ── adb install + launch (apk-install target) ─────────────────────────────────────────────────
if [ "$INSTALL_ONLY" = 1 ]; then
    ADB="${ANDROID_HOME:-$HOME/Library/Android/sdk}/platform-tools/adb"
    echo "== adb install $OUT =="
    "$ADB" install -r "$OUT"
    "$ADB" shell monkey -p "$PACKAGE" -c android.intent.category.LAUNCHER 1 >/dev/null
    echo "launched $PACKAGE"
    exit 0
fi

echo "== android_apk: $APP_NAME  ($GAME/$VARIANT)  package=$PACKAGE =="

# ── generator for the sibling-ABI native builds ───────────────────────────────────────────────
GEN=(-G "Unix Makefiles")
if command -v ninja >/dev/null 2>&1; then GEN=(-G Ninja); fi

# All ABIs to package: primary (already built by the parent tree) + extras. (No bash arrays —
# macOS ships bash 3.2, where "${arr[@]}" on an empty array trips `set -u`.)
ABIS="$PRIMARY_ABI"
for a in $(printf '%s' "${EXTRA_ABIS:-}" | tr ';' ' '); do
    [ -n "$a" ] && ABIS="$ABIS $a"
done
echo "   ABIs: $ABIS   (primary=$PRIMARY_ABI)"

# so_for_abi <abi> -> path to that ABI's libmain.so (build it if it isn't the primary).
so_for_abi() {
    local abi=$1
    if [ "$abi" = "$PRIMARY_ABI" ]; then echo "$PRIMARY_SO"; return; fi
    local nb="$WORK/native/$abi"
    echo ">> building sibling ABI $abi" >&2
    cmake "${GEN[@]}" -S "$SOURCE" -B "$nb" \
        -DYODA_PLATFORM=SDL -DYODA_GAME="$GAME" -DYODA_VARIANT="$VARIANT" \
        -DCMAKE_BUILD_TYPE=Release \
        --toolchain "$NDK/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
        -DFETCHCONTENT_SOURCE_DIR_SDL3="$SDL_SRC" \
        -DYODA_ANDROID_EXTRA_ABIS="" >&2
    cmake --build "$nb" --target main >&2
    local so; so=$(find "$nb" -name libmain.so | head -1)
    [ -n "$so" ] || { echo "android_apk: libmain.so not found for $abi" >&2; exit 1; }
    echo "$so"
}

# ── stage a clean build copy of the Gradle template ───────────────────────────────────────────
GP="$WORK/gradle"
rm -rf "$GP"
mkdir -p "$GP"
cp -R "$TEMPLATE"/. "$GP"/
chmod +x "$GP/gradlew"
MAIN="$GP/app/src/main"

# SDL Java (version-matched to the SDL3 we static-linked — copied from the fetched source).
SDL_JAVA="$SDL_SRC/android-project/app/src/main/java/org/libsdl"
[ -d "$SDL_JAVA" ] || { echo "android_apk: SDL Java not found at $SDL_JAVA" >&2; exit 1; }
mkdir -p "$MAIN/java/org/libsdl"
cp -R "$SDL_JAVA"/. "$MAIN/java/org/libsdl"/

# jniLibs: one libmain.so per ABI.
for abi in $ABIS; do
    so=$(so_for_abi "$abi")
    mkdir -p "$MAIN/jniLibs/$abi"
    cp "$so" "$MAIN/jniLibs/$abi/libmain.so"
    echo "   staged $abi/libmain.so  ($(du -h "$so" | cut -f1))"
done

# ── assets: bake the game data (DTA/DAW + sfx + starter yoda.INI) + a manifest ────────────────
AS="$MAIN/assets"
rm -rf "$AS"; mkdir -p "$AS"
case "$GAME/$VARIANT" in
    INDY/*)
        cp "$RUN_DIR/DESKTOP.DAW" "$AS/DESKTOP.DAW"
        # Indy sfx are loose *.WAV alongside the data file.
        find "$RUN_DIR" -maxdepth 1 -iname '*.wav' -exec cp {} "$AS/" \; ;;
    */FULL)
        cp "$RUN_DIR/YODESK.DTA" "$AS/YODESK.DTA"
        [ -d "$RUN_DIR/sfx" ] && cp -R "$RUN_DIR/sfx" "$AS/sfx" ;;
    *)
        cp "$RUN_DIR/YodaDemo.dta" "$AS/YODADEMO.DTA"      # engine opens the uppercase name
        [ -d "$RUN_DIR/sfx" ] && cp -R "$RUN_DIR/sfx" "$AS/sfx" ;;
esac
# starter INI (the game opens "yoda.INI"). Reuse the run folder's if present; guarantee a valid
# Terrain (Terrain=-1 would spin worldgen forever — CLAUDE.md).
if [ -f "$RUN_DIR/yoda.INI" ]; then
    sed 's/Terrain=-1/Terrain=1/' "$RUN_DIR/yoda.INI" > "$AS/yoda.INI"
else
    printf '[OPTIONS]\nTerrain=1\nMIDILoad=1\nLCount=1\n' > "$AS/yoda.INI"
fi
# manifest: every asset (relative path), read by the SDL3 backend to extract into internal storage.
( cd "$AS" && find . -type f ! -name manifest.txt | sed 's|^\./||' | sort ) > "$AS/manifest.txt"
echo "   assets: $(wc -l < "$AS/manifest.txt" | tr -d ' ') files baked"

# ── launcher icons (from the game's own app icon; falls back to a solid tile) ──────────────────
"$PYTHON" "$TOOLS/android_icons.py" "$ICON_EXE" "$MAIN/res" \
    ${ICON_INDY:+--indy "$ICON_INDY"} || echo "   (icon generation skipped)"

# ── app identity: name + local.properties ─────────────────────────────────────────────────────
# Double-quote the label so an apostrophe (Indiana Jones') survives Android string parsing.
cat > "$MAIN/res/values/strings.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="app_name">"$APP_NAME"</string>
</resources>
EOF
{
    echo "sdk.dir=${ANDROID_HOME:-$HOME/Library/Android/sdk}"
    echo "ndk.dir=$NDK"
} > "$GP/local.properties"

# ── build the APK ─────────────────────────────────────────────────────────────────────────────
echo "== gradlew assembleDebug =="
NDK_VER=$(basename "$NDK")            # e.g. 25.2.9519653 — match AGP's strip NDK to the build NDK
( cd "$GP" && ./gradlew --no-daemon -PyodaAppId="$PACKAGE" -PyodaNdkVersion="$NDK_VER" assembleDebug )

APK=$(find "$GP/app/build/outputs/apk/debug" -name '*.apk' | head -1)
[ -n "$APK" ] || { echo "android_apk: no APK produced" >&2; exit 1; }
mkdir -p "$(dirname "$OUT")"
cp "$APK" "$OUT"
echo "== APK ready: $OUT  ($(du -h "$OUT" | cut -f1)) =="
