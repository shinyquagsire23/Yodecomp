#!/bin/zsh
# Build the Android APK and install+launch it on a connected device/emulator (adb).
#   ./build_run_android.sh [demo|full|indy]      (default: demo)
# Fast dev loop => a SINGLE ABI (arm64-v8a by default; export ANDROID_ABI=x86_64 for an Intel
# emulator). For a release APK with both ABIs, use the preset directly:
#   cmake --preset android-demo && cmake --build --preset android-demo
# Needs the Android NDK + SDK (see BUILDING.md "Build: Android .apk").
set -e

GAME=${1:-demo}
ABI=${ANDROID_ABI:-arm64-v8a}

# SDK + NDK: honour the environment, else autodetect the newest NDK under the SDK.
export ANDROID_HOME=${ANDROID_HOME:-$HOME/Library/Android/sdk}
if [[ -z "$ANDROID_NDK_HOME" ]]; then
  export ANDROID_NDK_HOME=$(ls -d "$ANDROID_HOME"/ndk/*(/N) 2>/dev/null | sort -V | tail -1)
fi
[[ -n "$ANDROID_NDK_HOME" ]] || { echo "build_run_android: set ANDROID_NDK_HOME (no NDK under $ANDROID_HOME/ndk)"; exit 1; }

echo "== Android: game=$GAME  abi=$ABI  ndk=$ANDROID_NDK_HOME =="
"$ANDROID_HOME/platform-tools/adb" get-state >/dev/null 2>&1 || \
  echo "  (no adb device yet — start an emulator / plug in a phone before the install step)"

# Configure the android-<game> preset (single ABI for a fast build+run), then build the
# `apk-install` target: it assembles the .apk and does `adb install -r` + launch.
cmake --preset "android-$GAME" -DANDROID_ABI="$ABI" -DYODA_ANDROID_EXTRA_ABIS=""
cmake --build "build-android-$GAME" --target apk-install
