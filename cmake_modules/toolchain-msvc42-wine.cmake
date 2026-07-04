# CMake toolchain: Microsoft Visual C++ 4.2 (cl 10.20 / link 3.10) via CrossOver wine.
# Usage:
#   cmake -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain-msvc42-wine.cmake
#
# NOTE: cl 10.20 predates CMake's MSVC auto-detection heuristics, so we hand-set the compiler id
# and skip the compile/link probe (which would try modern MSVC flags). Validate + iterate once
# toolchain/vc42/ is populated (see toolchain/README.md). For the very first bytematches, driving
# toolchain/bin/{cl,link} directly (no CMake) is the simplest path — CMake is for scaling later.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

get_filename_component(_YODA_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_WRAP "${_YODA_ROOT}/toolchain/bin")

set(CMAKE_C_COMPILER   "${_WRAP}/cl")
set(CMAKE_CXX_COMPILER "${_WRAP}/cl")
set(CMAKE_LINKER       "${_WRAP}/link")

# cl 10.20 == _MSC_VER 1020. Tell CMake what it is so it stops probing.
set(CMAKE_C_COMPILER_ID   MSVC)
set(CMAKE_CXX_COMPILER_ID MSVC)
set(CMAKE_C_COMPILER_VERSION   10.20)
set(CMAKE_CXX_COMPILER_VERSION 10.20)
set(CMAKE_C_COMPILER_WORKS   TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)

# Don't try to link a test executable during configure (needs full MFC/CRT env + wine).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# The locked flag set from the Phase 2 decomp.me match (CONFIRM/UPDATE in CLAUDE.md).
set(_YODA_FLAGS "/nologo /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS")
set(CMAKE_C_FLAGS_INIT   "${_YODA_FLAGS} /MT")
set(CMAKE_CXX_FLAGS_INIT "${_YODA_FLAGS} /MT")
set(CMAKE_EXE_LINKER_FLAGS_INIT "/INCREMENTAL:NO /NOLOGO")

# Find libs/headers only inside the VC++ tree we ship.
set(CMAKE_FIND_ROOT_PATH "${_YODA_ROOT}/toolchain/vc42")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
