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

# ── microfx: the MFC/Win32 subset library ────────────────────────────────────────────────────
file(GLOB_RECURSE _mfx_srcs "${_mfx}/src/*.cpp")
add_library(microfx STATIC ${_mfx_srcs})
target_include_directories(microfx PUBLIC "${_mfx}/include")
# SDL2 becomes REQUIRED at M1 (gdi/) — keep M0 (core/) buildable without it.
find_package(SDL2 QUIET)
if(SDL2_FOUND)
  target_link_libraries(microfx PUBLIC SDL2::SDL2)
  target_compile_definitions(microfx PUBLIC MICROFX_HAS_SDL)
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

# ── M2: the game itself — real entry point + SDL event pump (needs SDL2) ────────────────────
if(SDL2_FOUND)
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
               "(SDL2 ${SDL2_FOUND})")
