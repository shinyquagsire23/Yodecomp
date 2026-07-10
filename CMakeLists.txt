cmake_minimum_required(VERSION 3.20)

# ============================================================================================
# Yoda Stories decompilation — extended, buildable engine (Phase H).
#
#   cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake
#   cmake --build build-cmake            # -> build-cmake/yoda.exe (runnable under wine)
#
# This build drives the wine-wrapped Visual C++ 4.2 toolchain via custom commands (see
# toolchain/vc42.cmake for the rationale — cl 10.20 predates CMake's MSVC support). It is
# functionally equivalent to tools/link_exe.sh: compile every TU, extract the original .rsrc
# into a .res, build the wavmix32 import stub, and static-MFC link a runnable yoda.exe.
#
# ── GOVERNING PRINCIPLE (the byte-match anchor) ─────────────────────────────────────────────
# The DEFAULT config (YODA_GAME=YODA, YODA_VARIANT=DEMO, YODA_PLATFORM=WIN32) MUST keep emitting
# the exact same compiler flags as tools/link_exe.sh, so its objects stay byte-identical to the
# 211-exact anchor. Every extension axis is ADDITIVE: it only appends a -D define, and only when
# it diverges from the default. The default therefore adds NOTHING and its preprocessed tokens
# are identical to the anchor. The fidelity gate remains tools/{link_exe,verify,progress}.py —
# this CMake build is for the EXTENDED (full-game / Indy / SDL) configs, and its default corner
# is a convenience mirror of the anchor, not a replacement for the byte-match harness.
#
# Macro policy (guards land in H2–H4; write them so the fall-through == the anchor):
#   YODA_GAME=INDY     -> -D GAME_INDY     (yoda    = fall-through, no macro)
#   YODA_VARIANT=FULL  -> -D YODA_FULL     (demo    = fall-through, no macro)
#   YODA_PLATFORM=SDL  -> -D YODA_PORTABLE (win32   = fall-through, no macro)
# ============================================================================================

project(yoda LANGUAGES NONE)

# ── config matrix ───────────────────────────────────────────────────────────────────────────
set(YODA_GAME     "YODA"  CACHE STRING "Game:     YODA (default) | INDY")
set(YODA_VARIANT  "DEMO"  CACHE STRING "Variant:  DEMO (default) | FULL   (Yoda only)")
set(YODA_PLATFORM "WIN32" CACHE STRING "Platform: WIN32 (default, byte-match anchor) | SDL")
set_property(CACHE YODA_GAME     PROPERTY STRINGS YODA INDY)
set_property(CACHE YODA_VARIANT  PROPERTY STRINGS DEMO FULL)
set_property(CACHE YODA_PLATFORM PROPERTY STRINGS WIN32 SDL)

# ── H4 portable target: a completely separate build path (host compiler + SDL2 + microfx) ────
# The SDL config never touches the wine/VC4.2 toolchain: configure WITHOUT
# -DCMAKE_TOOLCHAIN_FILE, e.g.  cmake -B build-sdl -DYODA_PLATFORM=SDL && cmake --build build-sdl
# Game TUs compile UNMODIFIED — microfx/include/ shadows <afxwin.h> et al. (docs/phase-h4-sdl.md).
if(YODA_PLATFORM STREQUAL "SDL")
  if(CMAKE_TOOLCHAIN_FILE MATCHES "vc42")
    message(FATAL_ERROR "YODA_PLATFORM=SDL uses the HOST compiler — configure without "
                        "-DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake.")
  endif()
  enable_language(CXX C)
  include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/PortableSDL.cmake")
  return()
endif()

# ── derive the additive defines (default adds nothing → byte-identical to link_exe.sh) ────────
set(_extra_defs "")
if(YODA_GAME STREQUAL "INDY")
  list(APPEND _extra_defs /D GAME_INDY)
endif()
if(YODA_VARIANT STREQUAL "FULL")
  list(APPEND _extra_defs /D YODA_FULL)
endif()

# Debug logging (writes yoda_debug.log next to the exe). OFF by default so the anchor build is
# untouched; enable for the extended builds with -DYODA_DEBUG=ON.
option(YODA_DEBUG "Enable debug logging (src/DebugLog.h)" OFF)
if(YODA_DEBUG)
  list(APPEND _extra_defs /D YODA_DEBUG)
endif()

if(YODA_GAME STREQUAL "YODA" AND YODA_VARIANT STREQUAL "DEMO" AND YODA_PLATFORM STREQUAL "WIN32")
  set(_is_anchor TRUE)
else()
  set(_is_anchor FALSE)
endif()

# Guarded fixes for ORIGINAL engine bugs (docs/engine-bugs.md; YODA_SIC_FIX/BUGLOG macros).
# Crash/UB/leak sites only — behavior-shaping bugs stay faithful (worldgen parity). Default:
# ON for every non-anchor config, OFF for the byte-match anchor (tokens must stay original).
if(_is_anchor)
  set(_bugfix_default OFF)
else()
  set(_bugfix_default ON)
endif()
option(YODA_BUGFIX "Fix original engine crash bugs (logs hits to yoda_bugfix.log)" ${_bugfix_default})
if(YODA_BUGFIX AND _is_anchor)
  message(FATAL_ERROR "YODA_BUGFIX=ON would perturb the byte-match anchor config — "
                      "use it only with FULL / INDY / SDL builds.")
endif()
if(YODA_BUGFIX)
  list(APPEND _extra_defs /D YODA_BUGFIX)
endif()
if(_is_anchor)
  set(_anchor_note " (byte-match anchor config)")
else()
  set(_anchor_note "")
endif()
message(STATUS "yoda: GAME=${YODA_GAME} VARIANT=${YODA_VARIANT} PLATFORM=${YODA_PLATFORM}${_anchor_note}")
if(_extra_defs)
  message(STATUS "yoda: extra defines: ${_extra_defs}")
endif()

# ── the locked VC++ 4.2 flag set (IDENTICAL to tools/link_exe.sh for the default config) ──────
set(YODA_CFLAGS /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS ${_extra_defs})

# ── paths ─────────────────────────────────────────────────────────────────────────────────────
set(_src   "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(_tools "${CMAKE_CURRENT_SOURCE_DIR}/tools")
set(_wm    "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/wavmix32")
set(_obj   "${CMAKE_CURRENT_BINARY_DIR}/obj")
file(MAKE_DIRECTORY "${_obj}")

# Serialize the wine toolchain steps. Many concurrent wine32on64 cl.exe invocations contend on
# the single wineserver and can livelock (observed: a -j8 build wedged with 7 stuck CL.EXE). A
# depth-1 Ninja job pool runs the compiles/links one at a time — matching tools/link_exe.sh's
# proven serial loop — so `cmake --build` never needs a manual -j1. (Ninja-only; harmless else.)
set_property(GLOBAL PROPERTY JOB_POOLS wine=1)

# Translation units in canonical link (address) order. This is the documented .obj order; the
# functional build doesn't require it, but it keeps intent clear and the map stable.
set(YODA_TUS
  GameTypes Score WorldgenHelpers GameObjects Iact Canvas DeskcppView
  IactScript TextDialog MainFrm Deskcpp DeskcppDoc Worldgen)

# All headers — a crude but correct dependency: touching any header rebuilds every TU (the TUs
# are tightly coupled through the shared headers, so this matches reality without a scanner).
file(GLOB _hdrs "${_src}/*.h")

# ── compile each TU (mirrors link_exe.sh: cwd=src, relative source, absolute /Fo) ─────────────
# Passing the source as a bare basename with WORKING_DIRECTORY=src makes the wrapper translate it
# to the exact same Z:\...\src\<TU>.cpp path link_exe.sh produces — identical provenance, so the
# default config's objects are byte-for-byte those of the anchor build.
set(_objs "")
foreach(_tu ${YODA_TUS})
  set(_o "${_obj}/${_tu}.obj")
  add_custom_command(
    OUTPUT  "${_o}"
    COMMAND "${YODA_CL}" ${YODA_CFLAGS} "/Fo${_o}" "${_tu}.cpp"
    DEPENDS "${_src}/${_tu}.cpp" ${_hdrs}
    WORKING_DIRECTORY "${_src}"
    COMMENT "cl (vc4.2) ${_tu}.cpp"
    JOB_POOL wine
    VERBATIM)
  list(APPEND _objs "${_o}")
endforeach()

# ── wavmix32 import stub (built from committed non-copyrighted source) ─────────────────────────
set(_wmlib "${CMAKE_CURRENT_BINARY_DIR}/wavmix32.lib")
add_custom_command(
  OUTPUT  "${_wmlib}"
  COMMAND "${YODA_CL}" /nologo /c /MT /W3 /O2 /D WIN32 /D NDEBUG /D _WINDOWS
          "/Fo${CMAKE_CURRENT_BINARY_DIR}/wavmix32_stub.obj" "${_wm}/wavmix32_stub.c"
  COMMAND "${YODA_LINK}" /NOLOGO /DLL /NOENTRY "/DEF:${_wm}/wavmix32.def"
          "/OUT:${CMAKE_CURRENT_BINARY_DIR}/wavmix32.dll" "/IMPLIB:${_wmlib}"
          "${CMAKE_CURRENT_BINARY_DIR}/wavmix32_stub.obj" kernel32.lib
  DEPENDS "${_wm}/wavmix32_stub.c" "${_wm}/wavmix32.def"
  COMMENT "build wavmix32 import lib (stub)"
  JOB_POOL wine
  VERBATIM)

# ── resources: copy the original .rsrc tree verbatim into a linkable .res ──────────────────────
# The WIN32 image references dialogs/menus/bitmaps/strings by integer ID; extract_res.py copies
# the original's resource section so the image RUNS like the original. Needs the original binary.
set(_res "${CMAKE_CURRENT_BINARY_DIR}/yoda.res")
set(_orig_exe "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo/YodaDemo.exe")
if(NOT EXISTS "${_orig_exe}")
  message(FATAL_ERROR
    "Cannot find the original binary at ${_orig_exe}\n"
    "The WIN32 build copies its .rsrc for a runnable image. Place YodaDemo.exe there "
    "(it is copyrighted and gitignored — acquire it separately).")
endif()
# Extended configs keep Yoda's .rsrc as the base (our code references YodaDemo's integer resource
# IDs) and override only the resources that reflect the game/variant (tools/make_res.py): GAME_INDY
# swaps the app icon + title + About-dialog credits (from DESKADV.EXE); YODA_FULL swaps the About
# dialog for the retail one (so it reads "Yoda(tm) Stories", not the demo's "…Demo"). The pure demo
# anchor uses extract_res.py (verbatim) — unchanged.
if(YODA_GAME STREQUAL "INDY")
  set(_indy_exe "${CMAKE_CURRENT_SOURCE_DIR}/INDYDESK/DESKADV.EXE")
  if(NOT EXISTS "${_indy_exe}")
    message(FATAL_ERROR
      "The GAME_INDY build sources its icon/title/About from Indy's binary at ${_indy_exe}\n"
      "(copyrighted and gitignored — acquire it separately).")
  endif()
  add_custom_command(
    OUTPUT  "${_res}"
    COMMAND python3 "${_tools}/make_res.py" "${_orig_exe}" "${_res}" --indy "${_indy_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_indy_exe}"
    COMMENT "build Indy resources (Yoda base + Indy icon/title/About -> yoda.res)"
    VERBATIM)
elseif(YODA_VARIANT STREQUAL "FULL")
  set(_full_exe "${CMAKE_CURRENT_SOURCE_DIR}/Yoda Stories/Yodesk.exe")
  if(NOT EXISTS "${_full_exe}")
    message(FATAL_ERROR
      "The YODA_FULL build sources its retail About dialog from ${_full_exe}\n"
      "(copyrighted and gitignored — acquire it separately).")
  endif()
  add_custom_command(
    OUTPUT  "${_res}"
    COMMAND python3 "${_tools}/make_res.py" "${_orig_exe}" "${_res}" --full "${_full_exe}"
    DEPENDS "${_tools}/make_res.py" "${_tools}/reslib.py" "${_orig_exe}" "${_full_exe}"
    COMMENT "build full-Yoda resources (Yoda base + retail About -> yoda.res)"
    VERBATIM)
else()
  add_custom_command(
    OUTPUT  "${_res}"
    COMMAND python3 "${_tools}/extract_res.py" "${_orig_exe}" "${_res}"
    DEPENDS "${_tools}/extract_res.py" "${_orig_exe}"
    COMMENT "extract resources (.rsrc -> yoda.res)"
    VERBATIM)
endif()

# ── link: static MFC (NAFXCW) + static CRT (LIBCMT) + Win32 imports + wavmix32 + resources ─────
# Bare lib names resolve from the wrapper's LIB path (VC + MFC lib dirs). afx.h's #pragma comment
# also pulls nafxcw/libcmt, but we list them for clarity — link dedups.
set(_exe "${CMAKE_CURRENT_BINARY_DIR}/yoda.exe")
add_custom_command(
  OUTPUT  "${_exe}"
  COMMAND "${YODA_LINK}" /NOLOGO /SUBSYSTEM:WINDOWS /INCREMENTAL:NO "/OUT:${_exe}"
          ${_objs} "${_res}"
          nafxcw.lib libcmt.lib kernel32.lib user32.lib gdi32.lib winspool.lib
          comdlg32.lib advapi32.lib shell32.lib comctl32.lib winmm.lib "${_wmlib}"
  DEPENDS ${_objs} "${_res}" "${_wmlib}"
  COMMENT "link yoda.exe (static MFC)"
  JOB_POOL wine
  VERBATIM)

add_custom_target(yoda_win32 ALL DEPENDS "${_exe}")

# ── convenience: `cmake --build build-cmake --target run` (mirrors run.sh) ─────────────────────
# Copies the freshly built exe next to the game data and launches it under CrossOver wine.
set(_cx "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver")
add_custom_target(run
  DEPENDS "${_exe}"
  COMMAND "${CMAKE_COMMAND}" -E copy "${_exe}" "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo/yoda.exe"
  COMMAND "${CMAKE_COMMAND}" -E env
          "PATH=${_cx}/bin:/usr/bin:/bin:/usr/sbin:/sbin"
          "DYLD_LIBRARY_PATH=${_cx}/lib64:${_cx}/lib32on64"
          "${_cx}/bin/wine" --bottle General "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo/yoda.exe"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/YodaDemo"
  USES_TERMINAL
  COMMENT "run yoda.exe under CrossOver wine")
