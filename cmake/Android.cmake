# ============================================================================================
# Phase H / GOAL 5 — the Android target (YODA_PLATFORM=SDL + the NDK toolchain).
#
# Design goal (an explicit improvement over the OpenJKDF2 two-phase shell dance): ONE native
# artifact and ONE CMake target.
#   • ONE artifact  — SDL3 (+SDL3_mixer) are static-linked (YODA_SDL_FETCH) into a single
#                     libmain.so; GameActivity.getLibraries() returns just {"main"}. No loose
#                     libSDL3.so / libSDL3_mixer.so to copy around.
#   • ONE target    — `cmake --build build-android --target apk` cross-compiles libmain.so (this
#                     tree = the primary ABI), builds any sibling ABIs, stages libs + baked assets
#                     + the version-matched SDL Java into a build copy of packaging/android, and
#                     drives gradlew to a .apk — all from the single custom target. Gradle is a
#                     dumb packager here (it does NOT re-invoke CMake via externalNativeBuild).
#
#   emcmake-style configure:
#     cmake -B build-android -DYODA_PLATFORM=SDL \
#           --toolchain $ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
#           -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28
#     cmake --build build-android --target apk        # -> build-android/<AppName>.apk
#
# The CMakePresets.json `android-*` presets wrap the configure line. See BUILDING.md "Android".
# ============================================================================================

if(NOT DEFINED sdl3_SOURCE_DIR)
  message(FATAL_ERROR "Android build expects SDL3 fetched from source (YODA_SDL_FETCH=ON is "
                      "forced for ANDROID). Did the FetchContent step run?")
endif()

# ── the game as the single shared library SDLActivity dlopen()s ───────────────────────────────
# Force-including <SDL3/SDL_main.h> renames the harness's main() → SDL_main, which SDL's Android
# JNI entry (built into the static SDL3 we link) resolves out of libmain.so and runs on the SDL
# thread. yoda_game → microfx → SDL3::SDL3 (static), so SDL + SDL_mixer + the game are all inside
# this one .so; the platform libs SDL needs come through SDL3::SDL3's interface deps.
add_library(main SHARED "${_mfx}/harness/yoda_main.cpp")
target_include_directories(main PRIVATE "${_src}")
target_link_libraries(main PRIVATE yoda_game)
mfx_force_load(main yoda_game)
target_compile_options(main PRIVATE "SHELL:-include SDL3/SDL_main.h")
target_link_libraries(main PRIVATE android log)          # always-needed Android system libs
target_link_options(main PRIVATE "-u" "SDL_main")        # keep the JNI entry under --gc-sections

# ── per-game identity + baked-asset sources (mirrors the macOS .app block) ────────────────────
if(YODA_GAME STREQUAL "INDY")
  set(_app_name "Indiana Jones' Desktop Adventures")
  set(_app_pkg  "org.yodecomp.indy")
  set(_run_dir  "${CMAKE_CURRENT_SOURCE_DIR}/YodaIndy")
  set(_app_icon_exe "${CMAKE_CURRENT_SOURCE_DIR}/INDYDESK/DESKADV.EXE")
  set(_app_icon_args --indy "${_app_icon_exe}")
elseif(YODA_VARIANT STREQUAL "FULL")
  set(_app_name "Yoda Stories")
  set(_app_pkg  "org.yodecomp.yoda")
  set(_run_dir  "${CMAKE_CURRENT_SOURCE_DIR}/YodaFull")
  set(_app_icon_exe "${_orig_exe}")
  set(_app_icon_args "")
else()
  set(_app_name "Yoda Stories (Demo)")
  set(_app_pkg  "org.yodecomp.yodademo")
  set(_run_dir  "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo")
  set(_app_icon_exe "${_orig_exe}")
  set(_app_icon_args "")
endif()

# ── the ONE target: build sibling ABIs, stage everything, run gradle, drop the .apk here ──────
set(_apk_out "${CMAKE_CURRENT_BINARY_DIR}/${_app_name}.apk")
add_custom_target(apk
  DEPENDS main
  COMMAND "${CMAKE_COMMAND}" -E echo "== Assembling Android APK: ${_app_name} =="
  COMMAND bash "${_tools}/android_apk.sh"
      --source      "${CMAKE_CURRENT_SOURCE_DIR}"
      --template    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/android"
      --work        "${CMAKE_CURRENT_BINARY_DIR}/android-proj"
      --sdl-src     "${sdl3_SOURCE_DIR}"
      --primary-abi "${ANDROID_ABI}"
      --primary-so  "$<TARGET_FILE:main>"
      --extra-abis  "${YODA_ANDROID_EXTRA_ABIS}"
      --android-platform "${ANDROID_PLATFORM}"
      --ndk         "${CMAKE_ANDROID_NDK}"
      --game        "${YODA_GAME}"
      --variant     "${YODA_VARIANT}"
      --app-name    "${_app_name}"
      --package     "${_app_pkg}"
      --run-dir     "${_run_dir}"
      --python      "${Python3_EXECUTABLE}"
      --tools       "${_tools}"
      --icon-exe    "${_app_icon_exe}" ${_app_icon_args}
      --out         "${_apk_out}"
  USES_TERMINAL VERBATIM
  COMMENT "Android: stage libs+assets+SDL-Java and run gradlew -> ${_apk_out}")

# ── convenience: `--target apk-install` also adb-installs and launches on a device/emulator ───
add_custom_target(apk-install
  DEPENDS apk
  COMMAND bash "${_tools}/android_apk.sh" --install-only
      --out "${_apk_out}" --package "${_app_pkg}"
  USES_TERMINAL VERBATIM
  COMMENT "adb install + launch ${_app_name}")

message(STATUS "yoda: Android build — libmain.so (primary ABI ${ANDROID_ABI}, extra "
               "'${YODA_ANDROID_EXTRA_ABIS}'); `--target apk` -> ${_apk_out}")
