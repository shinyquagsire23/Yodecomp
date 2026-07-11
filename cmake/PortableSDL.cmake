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
    COMMAND python3 "${_tools}/make_res.py" "${_orig_exe}" "${_res_bin}" --indy "${_indy_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_indy_exe}"
    COMMENT "build Indy resources (Yoda base + Indy icon/title/About -> yoda.res)" VERBATIM)
elseif(YODA_VARIANT STREQUAL "FULL")
  set(_full_exe "${CMAKE_CURRENT_SOURCE_DIR}/Yoda Stories/Yodesk.exe")
  add_custom_command(
    OUTPUT  "${_res_bin}"
    COMMAND python3 "${_tools}/make_res.py" "${_orig_exe}" "${_res_bin}" --full "${_full_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_full_exe}"
    COMMENT "build full-Yoda resources (Yoda base + retail About -> yoda.res)" VERBATIM)
else()
  add_custom_command(
    OUTPUT  "${_res_bin}"
    COMMAND python3 "${_tools}/extract_res.py" "${_orig_exe}" "${_res_bin}"
    DEPENDS "${_tools}/extract_res.py" "${_orig_exe}"
    COMMENT "extract resources (.rsrc -> yoda.res)" VERBATIM)
endif()
set(_res_c "${CMAKE_CURRENT_BINARY_DIR}/mfxres_blob.c")
add_custom_command(
  OUTPUT  "${_res_c}"
  COMMAND python3 "${_tools}/bin2c.py" "${_res_bin}" "${_res_c}" g_mfxResBlob
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

find_package(SDL3 QUIET CONFIG)
find_package(SDL3_mixer QUIET CONFIG)
find_package(SDL2 QUIET)
find_package(SDL2_mixer QUIET)

set(_mfx_video "${YODA_MFX_VIDEO_BACKEND}")
if(_mfx_video STREQUAL "")
  if(SDL3_FOUND)
    set(_mfx_video "sdl3")
  elseif(SDL2_FOUND)
    set(_mfx_video "sdl2")
  else()
    set(_mfx_video "null")
  endif()
endif()
set(_mfx_audio "${YODA_MFX_AUDIO_BACKEND}")
if(_mfx_audio STREQUAL "")
  if(_mfx_video STREQUAL "sdl3" AND SDL3_mixer_FOUND)
    set(_mfx_audio "sdl3mixer")
  elseif(_mfx_video STREQUAL "sdl2" AND SDL2_mixer_FOUND)
    set(_mfx_audio "sdl2mixer")
  else()
    set(_mfx_audio "null")
  endif()
endif()
if((_mfx_video STREQUAL "sdl3" AND _mfx_audio STREQUAL "sdl2mixer") OR
   (_mfx_video STREQUAL "sdl2" AND _mfx_audio STREQUAL "sdl3mixer"))
  message(FATAL_ERROR "microfx: cannot mix SDL2 and SDL3 runtimes in one binary "
                      "(video=${_mfx_video} audio=${_mfx_audio})")
endif()
foreach(_backend "mfxplat_${_mfx_video}" "mfxsnd_${_mfx_audio}")
  if(NOT EXISTS "${_mfx}/src/platform/${_backend}.cpp")
    message(FATAL_ERROR "microfx backend TU not found: ${_mfx}/src/platform/${_backend}.cpp")
  endif()
  list(APPEND _mfx_srcs "${_mfx}/src/platform/${_backend}.cpp")
endforeach()

add_library(microfx STATIC ${_mfx_srcs} "${_res_c}")
target_include_directories(microfx PUBLIC "${_mfx}/include")
if(_mfx_video STREQUAL "sdl3" OR _mfx_audio STREQUAL "sdl3mixer")
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

# ── M0 oracle: native worldgen smoke harness (no window, no SDL) ─────────────────────────────
# Force-load the whole game archive so EVERY game↔microfx symbol reference is link-checked,
# not just the ones the harness happens to pull.
add_executable(worldgen_smoke "${_mfx}/harness/worldgen_smoke.cpp")
target_include_directories(worldgen_smoke PRIVATE "${_src}")
target_link_libraries(worldgen_smoke PRIVATE yoda_game)
if(APPLE)
  target_link_options(worldgen_smoke PRIVATE "SHELL:-Wl,-force_load,$<TARGET_FILE:yoda_game>")
else()
  target_link_options(worldgen_smoke PRIVATE
    "SHELL:-Wl,--whole-archive $<TARGET_FILE:yoda_game> -Wl,--no-whole-archive")
endif()

# ── M5 tail oracle: CFileDialog scan/build-rows/resolve unit test (no game bootstrap needed) ──
add_executable(dlg_smoke "${_mfx}/harness/dlg_smoke.cpp")
target_link_libraries(dlg_smoke PRIVATE microfx)

# ── M1 oracle: render a zone through the real Canvas/gdi path (BMP dump; --show = SDL window) ─
add_executable(zone_view "${_mfx}/harness/zone_view.cpp")
target_include_directories(zone_view PRIVATE "${_src}")
target_link_libraries(zone_view PRIVATE yoda_game)
if(APPLE)
  target_link_options(zone_view PRIVATE "SHELL:-Wl,-force_load,$<TARGET_FILE:yoda_game>")
else()
  target_link_options(zone_view PRIVATE
    "SHELL:-Wl,--whole-archive $<TARGET_FILE:yoda_game> -Wl,--no-whole-archive")
endif()

# ── M2 oracle: headless walk harness (drives the dispatch/timer layer, no window) ───────────
add_executable(game_walk "${_mfx}/harness/game_walk.cpp")
target_include_directories(game_walk PRIVATE "${_src}")
target_link_libraries(game_walk PRIVATE yoda_game)
if(APPLE)
  target_link_options(game_walk PRIVATE "SHELL:-Wl,-force_load,$<TARGET_FILE:yoda_game>")
else()
  target_link_options(game_walk PRIVATE
    "SHELL:-Wl,--whole-archive $<TARGET_FILE:yoda_game> -Wl,--no-whole-archive")
endif()

# ── M2: the game itself — real entry point + the platform pump (needs a video backend) ──────
if(NOT _mfx_video STREQUAL "null")
  add_executable(yoda "${_mfx}/harness/yoda_main.cpp")
  target_include_directories(yoda PRIVATE "${_src}")
  target_link_libraries(yoda PRIVATE yoda_game)
  if(APPLE)
    target_link_options(yoda PRIVATE "SHELL:-Wl,-force_load,$<TARGET_FILE:yoda_game>")
  else()
    target_link_options(yoda PRIVATE
      "SHELL:-Wl,--whole-archive $<TARGET_FILE:yoda_game> -Wl,--no-whole-archive")
  endif()
endif()

message(STATUS "yoda: portable SDL build — microfx + TUs: ${YODA_PORTABLE_TUS} "
               "(SDL2 ${SDL2_FOUND}; backends: video=${_mfx_video} audio=${_mfx_audio})")
