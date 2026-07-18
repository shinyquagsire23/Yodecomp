# ============================================================================================
# Phase H4 — the portable SDL build (YODA_PLATFORM=SDL).  Design: docs/phase-h4-sdl.md
#
# Governing idea ("microfx"): the game TUs in src/ compile UNMODIFIED with the HOST compiler;
# microfx/include/ is first on the include path, so the TUs' own `#include <afxwin.h>` etc.
# resolve to our drop-in MFC/Win32-subset headers implemented over SDL2. The byte-match anchor
# never sees any of this (this file is only included when YODA_PLATFORM=SDL).
#
# Milestones (docs/phase-h4-sdl.md): M0 core+logic TUs+worldgen smoke test (no SDL needed),
# M1 Canvas->SDL_Surface, M2 event pump/input, M3 SDL2_mixer audio, M4 resources/UI.
# ============================================================================================

set(_mfx "${CMAKE_CURRENT_SOURCE_DIR}/microfx")
set(_src "${CMAKE_CURRENT_SOURCE_DIR}/src")

# additive config macros (same policy as the win32 path) + the platform macro
add_compile_definitions(YODA_PORTABLE)
if(YODA_GAME STREQUAL "INDY")
  add_compile_definitions(GAME_INDY)
endif()
if(YODA_VARIANT STREQUAL "FULL")
  add_compile_definitions(YODA_FULL)
endif()
option(YODA_DEBUG "Enable debug logging (src/DebugLog.h)" OFF)
if(YODA_DEBUG)
  add_compile_definitions(YODA_DEBUG)
endif()
# The SDL config is never byte-matched — engine crash-bug fixes default ON (docs/engine-bugs.md).
option(YODA_BUGFIX "Fix original engine crash bugs (logs hits to yoda_bugfix.log)" ON)
if(YODA_BUGFIX)
  add_compile_definitions(YODA_BUGFIX)
endif()

# ── Android (GOAL 5): the NDK toolchain sets ANDROID/CMAKE_SYSTEM_NAME=Android. Force SDL to be
# built static-from-source (there is no system SDL for the phone; it links into a single
# libmain.so). __ANDROID__ is defined automatically by the NDK clang, so the microfx/game code
# needs no extra define. The `apk` target (cmake/Android.cmake) can also build sibling ABIs.
if(ANDROID)
  set(YODA_SDL_FETCH ON CACHE BOOL "" FORCE)
  set(YODA_ANDROID_EXTRA_ABIS "x86_64" CACHE STRING
      "Additional ABIs the `apk` target also builds (semicolon list; primary = ANDROID_ABI)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1997 MSVC-era C++ under a modern compiler: quiet the era mismatches that are NOT bugs.
# Grow this list instead of editing game TUs (any TU edit needs an anchor-oracle re-run).
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(
    -Wno-writable-strings          # "..." passed as char*
    -Wno-write-strings
    -Wno-deprecated-declarations
    -Wno-parentheses
    -Wno-dangling-else
    -Wno-undefined-bool-conversion # MFC's `if (this == NULL)` GetSafe* idiom
    -fno-delete-null-pointer-checks # ...and keep the optimizer honoring those checks
  )
endif()

# Native MSVC (Visual Studio) portability flags for the microfx MFC subset:
#   /vmg  — MFC-style message maps reinterpret-cast between unrelated pointer-to-member types
#           (e.g. `void (AppWnd::*)(UINT)` -> AFX_PMSG = `void (CCmdTarget::*)(void)`). MSVC's
#           default /vmb (inference-based) representation rejects that cast with C2440; /vmg
#           forces the general representation so the cast is valid — the same pointer-to-member
#           model real MFC is built with. Applied to every microfx + game TU for a consistent ABI.
if(MSVC)
  add_compile_options(/vmg)
  # The 1997 game code calls the classic CRT (strcpy/sprintf/_splitpath/…); silence ucrt's
  # C4996 "this function may be unsafe" / POSIX-name deprecation so the port build isn't buried
  # in warnings. (Off-Windows compilers don't have these; see the Clang|GNU block above.)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
endif()

# Force-load a whole static archive into a target (so every game<->microfx symbol is link-checked,
# and message-map/DYNCREATE self-registration objects that nothing references directly survive).
# The flag is compiler/linker-specific: ld64 (Apple), MSVC link.exe, and GNU-style ld/lld differ.
function(mfx_force_load tgt archive)
  if(APPLE)
    target_link_options(${tgt} PRIVATE "SHELL:-Wl,-force_load,$<TARGET_FILE:${archive}>")
  elseif(MSVC)
    target_link_options(${tgt} PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:${archive}>")
  else()  # GNU ld / lld / emscripten wasm-ld
    target_link_options(${tgt} PRIVATE
      "SHELL:-Wl,--whole-archive $<TARGET_FILE:${archive}> -Wl,--no-whole-archive")
  endif()
endfunction()

# ── resources: the SAME .res blob the WIN32 link consumes, embedded as a C array ─────────────
# (M4: microfx/src/res/mfxres.cpp parses it at runtime — icons/cursors/strings/bitmaps.)
set(_tools "${CMAKE_CURRENT_SOURCE_DIR}/tools")
set(_res_bin "${CMAKE_CURRENT_BINARY_DIR}/yoda.res")
set(_orig_exe "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo/YodaDemo.exe")
if(NOT EXISTS "${_orig_exe}")
  message(FATAL_ERROR "Cannot find the original binary at ${_orig_exe} (resource base).")
endif()
if(YODA_GAME STREQUAL "INDY")
  set(_indy_exe "${CMAKE_CURRENT_SOURCE_DIR}/INDYDESK/DESKADV.EXE")
  add_custom_command(
    OUTPUT  "${_res_bin}"
    COMMAND "${Python3_EXECUTABLE}" "${_tools}/make_res.py" "${_orig_exe}" "${_res_bin}" --indy "${_indy_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_indy_exe}"
    COMMENT "build Indy resources (Yoda base + Indy icon/title/About -> yoda.res)" VERBATIM)
elseif(YODA_VARIANT STREQUAL "FULL")
  set(_full_exe "${CMAKE_CURRENT_SOURCE_DIR}/Yoda Stories/Yodesk.exe")
  add_custom_command(
    OUTPUT  "${_res_bin}"
    COMMAND "${Python3_EXECUTABLE}" "${_tools}/make_res.py" "${_orig_exe}" "${_res_bin}" --full "${_full_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_full_exe}"
    COMMENT "build full-Yoda resources (Yoda base + retail About -> yoda.res)" VERBATIM)
else()
  add_custom_command(
    OUTPUT  "${_res_bin}"
    COMMAND "${Python3_EXECUTABLE}" "${_tools}/extract_res.py" "${_orig_exe}" "${_res_bin}"
    DEPENDS "${_tools}/extract_res.py" "${_orig_exe}"
    COMMENT "extract resources (.rsrc -> yoda.res)" VERBATIM)
endif()
set(_res_c "${CMAKE_CURRENT_BINARY_DIR}/mfxres_blob.c")
add_custom_command(
  OUTPUT  "${_res_c}"
  COMMAND "${Python3_EXECUTABLE}" "${_tools}/bin2c.py" "${_res_bin}" "${_res_c}" g_mfxResBlob
  DEPENDS "${_tools}/bin2c.py" "${_res_bin}"
  COMMENT "embed yoda.res -> mfxres_blob.c" VERBATIM)

# ── microfx: the MFC/Win32 subset library ────────────────────────────────────────────────────
# Platform backends (microfx/src/platform/, contract in microfx/include/mfxplat.h): the GLOB
# skips that directory and EXACTLY ONE video/input TU + ONE audio TU are appended below.
# Defaults auto-select from what find_package sees (SDL3 preferred, then SDL2, else null);
# a port overrides with -DYODA_MFX_VIDEO_BACKEND=<name> / -DYODA_MFX_AUDIO_BACKEND=<name>
# (compiling microfx/src/platform/mfxplat_<name>.cpp / mfxsnd_<name>.cpp).
# ⚠ SDL2 and SDL3 export the SAME symbol names — one binary must never link both runtimes,
# so the audio default follows the video choice and mixed pairings are rejected below.
set(YODA_MFX_VIDEO_BACKEND "" CACHE STRING "microfx video/input backend (default: sdl3 > sdl2 > null)")
set(YODA_MFX_AUDIO_BACKEND "" CACHE STRING "microfx audio backend (default: pairs with the video backend)")

file(GLOB_RECURSE _mfx_srcs "${_mfx}/src/*.cpp" "${_mfx}/src/*.c")
list(FILTER _mfx_srcs EXCLUDE REGEX "/src/platform/")

# ── GOAL 4: the WASM corner (emcmake cmake -B build-wasm -DYODA_PLATFORM=SDL ...) ────────────
# Under Emscripten, SDL3 comes from emscripten's own port system (--use-port=sdl3), never from
# find_package (the toolchain's find-root excludes host libs by design). There is no SDL3_mixer
# port, so audio defaults to null (the game sees WaveMixInit==0 and disables sound — the same
# graceful path as a machine with no sound card). The blocking-modal-loop problem (CWinThread::
# Run / GetMessageA / DoModal own the thread) is solved with ASYNCIFY: every blocking wait
# already funnels through MfxPlatDelay(), which the sdl3 backend routes to emscripten_sleep()
# under __EMSCRIPTEN__ — the browser gets control at every wait with NO game-code restructuring.
# ── SDL source: system (find_package) vs. built-from-source-static (FetchContent) ────────────
# Default = find_package: fast dev iteration against a system SDL (e.g. Homebrew's dylibs).
# YODA_SDL_FETCH=ON builds SDL3 (+SDL3_mixer) STATICALLY from source, so the resulting binary
# links no external dylib — the requirement for a self-contained, redistributable macOS .app
# (Homebrew ships SDL3 dylib-only, so its build would bake /opt/homebrew paths that break on any
# machine without Homebrew). The macos-app flow below turns this ON; see BUILDING.md.
option(YODA_SDL_FETCH "Build SDL3 (+SDL3_mixer) statically from source (FetchContent)" OFF)
if(NOT EMSCRIPTEN AND YODA_SDL_FETCH)
  include(FetchContent)
  # Pin the EXACT versions microfx was developed against (Homebrew: SDL3 3.4.12 / mixer 3.2.4).
  # SDL3: static only — SDL3::SDL3 then aliases the static lib and carries the required Apple
  # frameworks (Cocoa/Metal/CoreAudio/…) as usage requirements, so linking it is self-contained.
  set(SDL_SHARED     OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC     ON  CACHE BOOL "" FORCE)
  set(SDL_TEST       OFF CACHE BOOL "" FORCE)
  set(SDL_INSTALL    OFF CACHE BOOL "" FORCE)
  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)   # forces SDL3_mixer static too
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.12 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3)
  # SDL3_mixer: VENDORED so decoders build from bundled source (never link Homebrew codec
  # dylibs → otool-clean). We only need WAV (always built in) + MIDI (Indy); disable the heavy
  # optional codecs to keep the archive lean. MIDI uses the vendored timidity synth, not
  # FluidSynth (which would pull a system dylib).
  set(SDLMIXER_VENDORED         ON  CACHE BOOL "" FORCE)
  set(SDLMIXER_SAMPLES          OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_INSTALL          OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_FLAC             OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_GME              OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MOD              OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MP3              OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_OPUS             OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_VORBIS           OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_WAVPACK          OFF CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI             ON  CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI_TIMIDITY    ON  CACHE BOOL "" FORCE)
  set(SDLMIXER_MIDI_FLUIDSYNTH  OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG release-3.2.4 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3_mixer)
  set(SDL3_FOUND TRUE)
  set(SDL3_mixer_FOUND TRUE)
  message(STATUS "microfx: SDL3 3.4.12 + SDL3_mixer 3.2.4 built STATIC from source (FetchContent)")
elseif(NOT EMSCRIPTEN)
  find_package(SDL3 QUIET CONFIG)
  find_package(SDL3_mixer QUIET CONFIG)
  find_package(SDL2 QUIET)
  find_package(SDL2_mixer QUIET)
endif()

set(_mfx_video "${YODA_MFX_VIDEO_BACKEND}")
if(_mfx_video STREQUAL "")
  if(EMSCRIPTEN)
    set(_mfx_video "sdl3")
  elseif(SDL3_FOUND)
    set(_mfx_video "sdl3")
  elseif(SDL2_FOUND)
    set(_mfx_video "sdl2")
  else()
    set(_mfx_video "null")
  endif()
endif()
set(_mfx_audio "${YODA_MFX_AUDIO_BACKEND}")
if(_mfx_audio STREQUAL "")
  if(EMSCRIPTEN)
    set(_mfx_audio "webaudio")       # AudioBufferSourceNodes — immediate start, audio-thread
                                     # rendering (the ScriptProcessor path lags ~1s on Firefox);
                                     # override with sdl3stream/null if ever needed
  elseif(_mfx_video STREQUAL "sdl3" AND SDL3_mixer_FOUND)
    set(_mfx_audio "sdl3mixer")
  elseif(_mfx_video STREQUAL "sdl3")
    set(_mfx_audio "sdl3stream")     # SDL3 without SDL3_mixer: SFX-only core-audio backend
  elseif(_mfx_video STREQUAL "sdl2" AND SDL2_mixer_FOUND)
    set(_mfx_audio "sdl2mixer")
  else()
    set(_mfx_audio "null")
  endif()
endif()
if((_mfx_video STREQUAL "sdl3" AND _mfx_audio STREQUAL "sdl2mixer") OR
   (_mfx_video STREQUAL "sdl2" AND (_mfx_audio STREQUAL "sdl3mixer" OR
                                    _mfx_audio STREQUAL "sdl3stream")))
  message(FATAL_ERROR "microfx: cannot mix SDL2 and SDL3 runtimes in one binary "
                      "(video=${_mfx_video} audio=${_mfx_audio})")
endif()
if(_mfx_audio STREQUAL "webaudio" AND NOT EMSCRIPTEN)
  message(FATAL_ERROR "microfx: the webaudio backend is wasm-only (EM_JS)")
endif()
foreach(_backend "mfxplat_${_mfx_video}" "mfxsnd_${_mfx_audio}")
  if(NOT EXISTS "${_mfx}/src/platform/${_backend}.cpp")
    message(FATAL_ERROR "microfx backend TU not found: ${_mfx}/src/platform/${_backend}.cpp")
  endif()
  list(APPEND _mfx_srcs "${_mfx}/src/platform/${_backend}.cpp")
endforeach()

add_library(microfx STATIC ${_mfx_srcs} "${_res_c}")
target_include_directories(microfx PUBLIC "${_mfx}/include")
if(EMSCRIPTEN)
  # PUBLIC: every consumer (yoda + the node harnesses) needs the SDL3 port headers at compile
  # and the port library at link. microfx throws/catches real C++ exceptions (CFileException —
  # the v85 CFile fix), and emscripten disables EH by default → -fexceptions everywhere
  # (JS-based EH; the combination known to work with ASYNCIFY, unlike -fwasm-exceptions).
  target_compile_options(microfx PUBLIC "--use-port=sdl3" -fexceptions)
  target_link_options(microfx PUBLIC "--use-port=sdl3" -fexceptions)
elseif(_mfx_video STREQUAL "sdl3" OR _mfx_audio STREQUAL "sdl3mixer"
       OR _mfx_audio STREQUAL "sdl3stream")
  target_link_libraries(microfx PUBLIC SDL3::SDL3)
endif()
if(_mfx_video STREQUAL "sdl2")
  target_link_libraries(microfx PUBLIC SDL2::SDL2)
  # MICROFX_HAS_SDL now only gates harness extras (zone_view --show, SDL2 API) — microfx
  # sources themselves are SDL-free outside the platform/ backend TUs.
  target_compile_definitions(microfx PUBLIC MICROFX_HAS_SDL)
endif()
if(_mfx_audio STREQUAL "sdl2mixer")
  target_link_libraries(microfx PUBLIC SDL2_mixer::SDL2_mixer)
endif()
if(_mfx_audio STREQUAL "sdl3mixer")
  target_link_libraries(microfx PUBLIC SDL3_mixer::SDL3_mixer)
endif()

# ── game TUs (UNMODIFIED src/*.cpp), ported incrementally ────────────────────────────────────
# Grow this list TU-by-TU toward the full 13 (canonical order: GameTypes Score WorldgenHelpers
# GameObjects Iact Canvas DeskcppView IactScript TextDialog MainFrm Deskcpp DeskcppDoc Worldgen).
set(YODA_PORTABLE_TUS
  GameTypes Score WorldgenHelpers GameObjects Iact Canvas DeskcppView
  IactScript TextDialog MainFrm Deskcpp DeskcppDoc Worldgen
)
set(_game_srcs "")
foreach(_tu ${YODA_PORTABLE_TUS})
  list(APPEND _game_srcs "${_src}/${_tu}.cpp")
endforeach()

add_library(yoda_game STATIC ${_game_srcs})
target_include_directories(yoda_game PRIVATE "${_src}")
target_link_libraries(yoda_game PUBLIC microfx)   # propagates microfx/include (shadows <afxwin.h>)

# ── Android: one static libmain.so + the single `apk` CMake target ────────────────────────────
# All the desktop/wasm target machinery below is host/browser-specific; the phone gets its own
# self-contained module (builds `main` as the shared lib SDLActivity dlopen()s, and defines the
# one-command `apk` target that stages libs+assets+SDL-Java into the Gradle template and runs it).
if(ANDROID)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Android.cmake")
  return()
endif()

# ── M0 oracle: native worldgen smoke harness (no window, no SDL) ─────────────────────────────
# Force-load the whole game archive so EVERY game↔microfx symbol reference is link-checked,
# not just the ones the harness happens to pull.
add_executable(worldgen_smoke "${_mfx}/harness/worldgen_smoke.cpp")
target_include_directories(worldgen_smoke PRIVATE "${_src}")
target_link_libraries(worldgen_smoke PRIVATE yoda_game)
mfx_force_load(worldgen_smoke yoda_game)

# ── INDYSAV44 save/load round-trip oracle (GAME_INDY) ──
add_executable(save_smoke "${_mfx}/harness/save_smoke.cpp")
target_include_directories(save_smoke PRIVATE "${_src}")
target_link_libraries(save_smoke PRIVATE yoda_game)
mfx_force_load(save_smoke yoda_game)

# ── M5 tail oracle: CFileDialog scan/build-rows/resolve unit test (no game bootstrap needed) ──
add_executable(dlg_smoke "${_mfx}/harness/dlg_smoke.cpp")
target_link_libraries(dlg_smoke PRIVATE microfx)

# ── M1 oracle: render a zone through the real Canvas/gdi path (BMP dump; --show = SDL window) ─
add_executable(zone_view "${_mfx}/harness/zone_view.cpp")
target_include_directories(zone_view PRIVATE "${_src}")
target_link_libraries(zone_view PRIVATE yoda_game)
mfx_force_load(zone_view yoda_game)

# ── M2 oracle: headless walk harness (drives the dispatch/timer layer, no window) ───────────
add_executable(game_walk "${_mfx}/harness/game_walk.cpp")
target_include_directories(game_walk PRIVATE "${_src}")
target_link_libraries(game_walk PRIVATE yoda_game)
mfx_force_load(game_walk yoda_game)

# ── M2: the game itself — real entry point + the platform pump (needs a video backend) ──────
if(NOT _mfx_video STREQUAL "null")
  add_executable(yoda "${_mfx}/harness/yoda_main.cpp")
  target_include_directories(yoda PRIVATE "${_src}")
  target_link_libraries(yoda PRIVATE yoda_game)
  mfx_force_load(yoda yoda_game)

  # Native Windows: link as a GUI-subsystem app so no empty console window pops up alongside the
  # game (the default console subsystem allocates one; the SDL build prints nothing to it). The
  # harness keeps its plain main(), so point the entry at mainCRTStartup (the CRT bootstrap that
  # calls main) instead of WinMainCRTStartup. YODA_DEBUG builds keep the console so stderr/asserts
  # stay visible.
  if(MSVC AND NOT YODA_DEBUG)
    target_link_options(yoda PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
  endif()

  # Native Windows: embed real PE resources so Explorer / the taskbar show the game's app icon.
  # yoda.res is the same linkable Win32 .res the WIN32/MFC build consumes (extract_res.py, or
  # make_res.py with the icon overridden per game), passed straight to link.exe as an external
  # object. The runtime still reads resources from the C-array blob (mfxres.cpp) — this is purely
  # the on-disk exe icon. (Not on WASM: yoda.html has its own favicon path.)
  if(MSVC)
    set_source_files_properties("${_res_bin}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_sources(yoda PRIVATE "${_res_bin}")
  endif()

  # ── convenience: `cmake --build build-sdl --target run` (native, mirrors run_sdl.sh) ─────────
  # The engine derives its data directory from GetModuleFileName (the binary's own folder), so we
  # stage the freshly built `yoda` into the run folder that holds this config's game data and
  # launch it there. The run folder (game data — DTA/DAW + sfx/) is user-supplied; see BUILDING.md.
  if(NOT EMSCRIPTEN)
    if(YODA_GAME STREQUAL "INDY")
      set(_run_dir "${CMAKE_CURRENT_SOURCE_DIR}/YodaIndy")
    elseif(YODA_VARIANT STREQUAL "FULL")
      set(_run_dir "${CMAKE_CURRENT_SOURCE_DIR}/YodaFull")
    else()
      set(_run_dir "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo")
    endif()
    add_custom_target(run
      DEPENDS yoda
      # Preserve the platform executable suffix (yoda.exe on Windows) — the game reads its data
      # from the binary's own folder, so copy it into the run folder next to the DTA/sfx.
      COMMAND "${CMAKE_COMMAND}" -E copy
              "$<TARGET_FILE:yoda>" "${_run_dir}/$<TARGET_FILE_NAME:yoda>"
      COMMAND "${_run_dir}/$<TARGET_FILE_NAME:yoda>"
      WORKING_DIRECTORY "${_run_dir}"
      USES_TERMINAL
      COMMENT "run the native SDL yoda from ${_run_dir}")
  endif()

  # ── macOS: a self-contained .app bundle (statically linked + otool-verified) ─────────────────
  # `cmake --build build-macos-app --target app` -> "<AppName>.app" in the build tree. Requires the
  # static-SDL build (YODA_SDL_FETCH=ON): the assembly script's otool gate FAILS the build if the
  # binary links any non-system library. Game data is baked into the bundle for now (user-owned
  # assets); a later InstallHelper/XDG pass will move writable state out of the .app (CLAUDE.md).
  if(APPLE AND NOT EMSCRIPTEN)
    if(YODA_GAME STREQUAL "INDY")
      set(YODA_APP_NAME "Indiana Jones' Desktop Adventures")
      set(YODA_APP_BUNDLE_ID "org.yodecomp.indy")
      set(_app_icon_exe "${CMAKE_CURRENT_SOURCE_DIR}/INDYDESK/DESKADV.EXE")
      set(_app_icon_args --indy "${_app_icon_exe}")
      set(_app_data "${_run_dir}/DESKTOP.DAW")
      set(_app_dataname "DESKTOP.DAW")
      set(_app_sfx "-")
      set(_app_wavdir "${_run_dir}")            # Indy sfx are loose *.WAV in the data folder
    else()
      set(YODA_APP_NAME "Yoda Stories")
      set(YODA_APP_BUNDLE_ID "org.yodecomp.yoda")
      set(_app_icon_exe "${_orig_exe}")         # YodaDemo.exe holds Yoda's app icon (demo + full)
      set(_app_icon_args "")
      if(YODA_VARIANT STREQUAL "FULL")
        set(_app_data "${_run_dir}/YODESK.DTA")
        set(_app_dataname "YODESK.DTA")
      else()
        set(_app_data "${_run_dir}/YodaDemo.dta")
        set(_app_dataname "YODADEMO.DTA")       # engine opens the uppercase name
      endif()
      set(_app_sfx "${_run_dir}/sfx")
      set(_app_wavdir "-")
    endif()
    set(YODA_APP_EXE "yoda")
    set(YODA_APP_VERSION "1.0")
    set(_app_bundle "${CMAKE_CURRENT_BINARY_DIR}/${YODA_APP_NAME}.app")
    set(_app_plist  "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
    set(_app_icns   "${CMAKE_CURRENT_BINARY_DIR}/AppIcon.icns")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist.in" "${_app_plist}" @ONLY)

    add_custom_command(OUTPUT "${_app_icns}"
      COMMAND "${Python3_EXECUTABLE}" "${_tools}/make_icns.py" "${_app_icon_exe}" "${_app_icns}" ${_app_icon_args}
      DEPENDS "${_tools}/make_icns.py" "${_tools}/reslib.py" "${_tools}/make_res.py" "${_app_icon_exe}"
      COMMENT "make AppIcon.icns from ${_app_icon_exe}" VERBATIM)

    add_custom_target(app
      DEPENDS yoda "${_app_icns}"
      COMMAND bash "${_tools}/make_macos_app.sh"
              --app "${_app_bundle}" --bin "$<TARGET_FILE:yoda>"
              --plist "${_app_plist}" --icns "${_app_icns}"
              --data "${_app_data}" --dataname "${_app_dataname}"
              --sfx "${_app_sfx}" --wavdir "${_app_wavdir}"
      USES_TERMINAL VERBATIM
      COMMENT "assemble the self-contained macOS .app -> ${_app_bundle}")
  endif()
elseif(NOT EMSCRIPTEN)
  # No SDL was found, so the playable `yoda` executable is NOT created (the null backend has no
  # window) — only the headless test harnesses build. In Visual Studio this shows up as "yoda has
  # no Startup Item". Install SDL3 and point CMake at it to get a runnable target:
  #   • vcpkg:  vcpkg install sdl3   +   -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
  #   • or:     -DCMAKE_PREFIX_PATH=<your SDL3 install>   (add SDL3_mixer for MIDI/full audio)
  message(WARNING
    "microfx: no SDL found -> video backend is 'null', so the playable 'yoda' target is NOT "
    "built (only the test harnesses). Install SDL3 and set CMAKE_PREFIX_PATH (or use the vcpkg "
    "toolchain) to get a runnable 'yoda' — see BUILDING.md 'CMake presets (Visual Studio ...)'.")
endif()

# ── WASM tail: per-target emscripten link config ─────────────────────────────────────────────
if(EMSCRIPTEN)
  # Harnesses run under NODE against the real filesystem (NODERAWFS) — worldgen_smoke is the
  # wasm logic-parity oracle (same seed, diff the log vs a native run). EXIT_RUNTIME so node
  # actually exits when main returns.
  foreach(_t worldgen_smoke dlg_smoke zone_view game_walk)
    target_link_options(${_t} PRIVATE
      "-sNODERAWFS=1" "-sALLOW_MEMORY_GROWTH=1" "-sEXIT_RUNTIME=1" "-sSTACK_SIZE=2097152")
  endforeach()

  # The game itself → yoda.html. ASYNCIFY makes the blocking loops legal in a browser (see the
  # header note); the generous ASYNCIFY_STACK_SIZE covers unwinding a deep game stack (modal
  # loop inside a WM handler inside dispatch inside Run). Assets ship in the preloaded MEMFS
  # at "/" — GetModuleFileNameA reports /yoda under wasm, so the game derives its data dir as
  # "/" and its INI as /yoda.INI (preload names are case-SENSITIVE, unlike macOS).
  if(TARGET yoda)
    if(YODA_GAME STREQUAL "INDY")
      set(_wasm_dta "${CMAKE_CURRENT_SOURCE_DIR}/YodaIndy/DESKTOP.DAW")
      set(_wasm_dta_name "DESKTOP.DAW")
    elseif(YODA_VARIANT STREQUAL "FULL")
      set(_wasm_dta "${CMAKE_CURRENT_SOURCE_DIR}/YodaFull/YODESK.DTA")
      set(_wasm_dta_name "YODESK.DTA")
    else()
      set(_wasm_dta "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo/YodaDemo.dta")
      set(_wasm_dta_name "YODADEMO.DTA")
    endif()
    set_target_properties(yoda PROPERTIES SUFFIX ".html")
    target_link_options(yoda PRIVATE
      "-sASYNCIFY=1" "-sASYNCIFY_STACK_SIZE=1048576"
      "-sALLOW_MEMORY_GROWTH=1" "-sSTACK_SIZE=2097152" "-sEXIT_RUNTIME=1"
      "SHELL:--shell-file ${_mfx}/web/mfx_shell.html")   # no branding / console textarea
    # emcc bakes the shell + pre-js into yoda.html/js at LINK time — make edits relink
    set_property(TARGET yoda APPEND PROPERTY LINK_DEPENDS
      "${_mfx}/web/mfx_shell.html" "${_mfx}/web/mfx_asset_picker.pre.js")
    # Two asset modes (user-set 2026-07-11):
    #   YODA_WASM_PRELOAD=ON  (default) — DTA/INI/sfx baked into yoda.data: self-contained page
    #                         for automation (tools/wasm_boottest.js) and local testing.
    #   YODA_WASM_PRELOAD=OFF — SHIPPABLE page: no game data in the bundle; a --pre-js picker
    #                         (microfx/web/mfx_asset_picker.pre.js) asks the user for their
    #                         game folder and copies it into MEMFS before main() runs.
    option(YODA_WASM_PRELOAD "wasm: bake game assets into the page (OFF = user picks a folder)" ON)
    if(YODA_WASM_PRELOAD)
      if(NOT EXISTS "${_wasm_dta}")
        message(FATAL_ERROR "wasm: game data not found: ${_wasm_dta}")
      endif()
      # minimal INI: a valid Terrain (worldgen retries forever on -1); the doc ctor re-picks the
      # planet each run and writes it back (MEMFS is writable; persistence across reloads is a
      # later milestone — IDBFS).
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wasm_yoda.INI"
           "[OPTIONS]\nTerrain=1\nMIDILoad=1\nLCount=1\n")
      target_link_options(yoda PRIVATE
        "SHELL:--preload-file ${_wasm_dta}@/${_wasm_dta_name}"
        "SHELL:--preload-file ${CMAKE_CURRENT_BINARY_DIR}/wasm_yoda.INI@/yoda.INI")
      # SFX live in a sfx/ folder next to the DTA (wave paths arrive "sfx\NAME.WAV" relative to
      # the cwd, which is "/" in MEMFS). Ship it when present.
      get_filename_component(_wasm_dta_dir "${_wasm_dta}" DIRECTORY)
      if(EXISTS "${_wasm_dta_dir}/sfx")
        target_link_options(yoda PRIVATE "SHELL:--preload-file ${_wasm_dta_dir}/sfx@/sfx")
      endif()
    else()
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wasm_picker_cfg.pre.js"
           "var MFX_DATA_NAME = \"${_wasm_dta_name}\";\n")
      target_link_options(yoda PRIVATE
        "SHELL:--pre-js ${CMAKE_CURRENT_BINARY_DIR}/wasm_picker_cfg.pre.js"
        "SHELL:--pre-js ${_mfx}/web/mfx_asset_picker.pre.js")
    endif()
  endif()
endif()

message(STATUS "yoda: portable SDL build — microfx + TUs: ${YODA_PORTABLE_TUS} "
               "(SDL2 ${SDL2_FOUND}; backends: video=${_mfx_video} audio=${_mfx_audio})")
