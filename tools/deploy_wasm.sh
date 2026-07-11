#!/bin/sh -e
# GOAL 4 — build + deploy the GitHub Pages wasm targets (user layout, 2026-07-11):
#
#   yodecomp/index.html        chooser (microfx/web/chooser.html): pick a folder, detect WHICH
#                              game it is, stash files in IndexedDB, redirect to the build
#   yodecomp/full/             YODA_WASM_PRELOAD=OFF  full Yoda   (user assets)
#   yodecomp/demo/             YODA_WASM_PRELOAD=OFF  demo        (user assets)
#   yodecomp/indy/             YODA_WASM_PRELOAD=OFF  Indy        (user assets; MIDI silent —
#                                                                  no soft-synth yet)
#   yodecompdemo/index.html    YODA_WASM_PRELOAD=ON   demo, assets baked in (the demo is the
#                                                     freely distributable one — instant play)
#
# Usage: tools/deploy_wasm.sh [dest]     (default dest: ~/workspace/shinyquagsire23.github.io)
# Builds land in build-wasm-deploy-*/ (separate from build-wasm, which stays the YODA_DEBUG
# automation tree). Commit + push of the pages repo is left to the user.

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-$HOME/workspace/shinyquagsire23.github.io}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 8)"

[ -d "$DEST" ] || { echo "deploy_wasm: dest not found: $DEST" >&2; exit 1; }

build() {  # build <tree> <extra cmake args...>
  tree="$1"; shift
  emcmake cmake -B "$ROOT/$tree" -S "$ROOT" -DYODA_PLATFORM=SDL "$@" >/dev/null
  cmake --build "$ROOT/$tree" --target yoda -j"$JOBS" | tail -1
}

deploy() {  # deploy <tree> <destdir> (renames yoda.html -> index.html; .data only if present)
  tree="$1"; destdir="$2"
  mkdir -p "$destdir"
  cp "$ROOT/$tree/yoda.html" "$destdir/index.html"
  cp "$ROOT/$tree/yoda.js" "$ROOT/$tree/yoda.wasm" "$destdir/"
  [ -f "$ROOT/$tree/yoda.data" ] && cp "$ROOT/$tree/yoda.data" "$destdir/" || true
}

echo "== picker builds (no baked assets) =="
build build-wasm-deploy-full -DYODA_VARIANT=FULL -DYODA_WASM_PRELOAD=OFF
build build-wasm-deploy-demo -DYODA_VARIANT=DEMO -DYODA_WASM_PRELOAD=OFF
build build-wasm-deploy-indy -DYODA_GAME=INDY   -DYODA_WASM_PRELOAD=OFF

echo "== demo build (assets baked) =="
build build-wasm-deploy-demodata -DYODA_VARIANT=DEMO -DYODA_WASM_PRELOAD=ON

echo "== deploy -> $DEST =="
cp "$ROOT/microfx/web/chooser.html" "$DEST/yodecomp/index.html" 2>/dev/null || {
  mkdir -p "$DEST/yodecomp"; cp "$ROOT/microfx/web/chooser.html" "$DEST/yodecomp/index.html"; }
deploy build-wasm-deploy-full     "$DEST/yodecomp/full"
deploy build-wasm-deploy-demo     "$DEST/yodecomp/demo"
deploy build-wasm-deploy-indy     "$DEST/yodecomp/indy"
deploy build-wasm-deploy-demodata "$DEST/yodecompdemo"

echo "done — review + commit in $DEST yourself:"
echo "  yodecomp/      chooser + full/ demo/ indy/ (user-asset builds)"
echo "  yodecompdemo/  instant-play demo (assets baked)"
