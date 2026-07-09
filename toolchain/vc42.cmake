# toolchain/vc42.cmake — CMake toolchain file for the wine-wrapped Microsoft Visual C++ 4.2
# (cl 10.20 / link 3.10-era) build used by the Yoda Stories decompilation (Phase H).
#
#   cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake
#   cmake --build build-cmake
#
# WHY custom-command based (read this before "fixing" it to use CMake's MSVC ruleset):
#   cl 10.20 (_MSC_VER 1020) long predates CMake's MSVC auto-detection, and the compiler is a
#   32-bit Windows PE run under wine through the thin wrappers in toolchain/bin/{cl,link,lib}.
#   Making CMake's built-in MSVC compile/link rules drive that (compiler-id probe, /showIncludes
#   dependency scanning, modern link-flag injection, mt.exe manifest embedding) is fragile and
#   would risk perturbing the byte-exact anchor. So the project is declared LANGUAGES NONE and the
#   actual compile/link steps are add_custom_command()s in CMakeLists.txt that invoke the SAME
#   wrappers, with the SAME invocation shape, as tools/link_exe.sh — the proven byte-match build.
#   This toolchain file's job is therefore only to (a) declare the Windows/x86 target and (b) hand
#   CMakeLists.txt the wrapper paths + VC tree location. See CLAUDE.md "PHASE H — H1".

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

# Repo root = parent of this toolchain/ directory.
get_filename_component(YODA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# The wine wrappers (they set INCLUDE/LIB from the VC tree and translate paths for wine).
set(YODA_CL   "${YODA_ROOT}/toolchain/bin/cl"   CACHE FILEPATH "VC++ 4.2 cl.exe wine wrapper")
set(YODA_LINK "${YODA_ROOT}/toolchain/bin/link" CACHE FILEPATH "VC++ 4.2 link.exe wine wrapper")
set(YODA_LIB  "${YODA_ROOT}/toolchain/bin/lib"  CACHE FILEPATH "VC++ 4.2 lib.exe wine wrapper")

# The VC++ 4.2 tree the wrappers use. The wrappers honor a VCDIR env override at build time
# (export VCDIR=... before `cmake --build`) for A/B-testing an alternate compiler; this cache
# entry is informational / for future toolchain files that want to point elsewhere.
set(YODA_VC_DIR "${YODA_ROOT}/toolchain/vc42" CACHE PATH "VC++ 4.2 install tree (BIN/INCLUDE/LIB/MFC)")

# Executables produced by this target are Windows PEs.
set(CMAKE_EXECUTABLE_SUFFIX ".exe")
