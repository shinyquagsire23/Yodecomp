# Toolchain — Visual C++ 4.2 under CrossOver wine (macOS / Apple Silicon)

Goal: run the original **Microsoft Visual C++ 4.2** `cl.exe` (10.20) / `link.exe` (3.10) under
wine so we can compile matching C and byte-compare against `YodaDemo.exe`. See `../CLAUDE.md` for the
full phased plan; this file is the concrete setup.

## Host status (verified 2026-07-04)
- **wine: CrossOver (`/Applications/Wine Crossover.app`)** — its `wine32on64` runs **32-bit** Windows
  console PEs on Apple Silicon. Verified by running the 32-bit `SysWOW64\cmd.exe` → prints the Windows
  version. **No wine upgrade needed.** (mainline `wine-stable 11.0` is available via brew as a fallback.)
- Prefix `~/.wine` is `win64` with `SysWOW64` present (32-bit subsystem works).
- **Disk is tight (~19 GB free)** → use the *minimal extracted* toolchain, NOT Docker, NOT a kept ISO.

## Acquire VC++ 4.2 (the one manual step)
VC++ 4.2 is abandonware. Get the CD image (e.g. WinWorld "Microsoft Visual C++ 4.2", or an MSDN disc
on archive.org). **Do NOT run SETUP** (16-bit ACME installer; fights wine). Instead extract/copy the
tree into `toolchain/vc42/` so it looks like:

```
toolchain/vc42/
  BIN/      CL.EXE C1.DLL C1XX.DLL C2.DLL LINK.EXE MSPDB*.DLL NMAKE.EXE ML.EXE ...
  INCLUDE/  stdio.h windows.h ...
  LIB/      LIBCMT.LIB KERNEL32.LIB USER32.LIB ...
  MFC/
    INCLUDE/  afxwin.h ...
    LIB/      NAFXCW.LIB ...
    SRC/      (MFC source — matches message-map/vtable code nearly for free)
```
Mount an ISO on macOS with `hdiutil attach VC42.iso` then `cp -R /Volumes/.../MSDEV/VC/{BIN,INCLUDE,LIB,MFC} toolchain/vc42/`,
or extract with `7z x VC42.iso`. Keep `BIN/` intact — `CL.EXE` needs its sibling DLLs (C1/C1XX/C2/MSPDB).
Path must contain **no spaces** (this repo path is fine).

## Use it
`bin/cl` and `bin/link` are thin wrappers that set `INCLUDE`/`LIB` and dispatch through wine.
Quick smoke test after populating `vc42/`:
```
toolchain/bin/cl /nologo /c /O2 toolchain/test/hello.c && echo OK
```
Then build with CMake using the toolchain file:
```
cmake -B build -G "Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=cmake_modules/toolchain-msvc42-wine.cmake
```
Locked flag set (from the Phase 2 decomp.me match — CONFIRM/UPDATE): `/nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS`, link `/INCREMENTAL:NO`.
