# Toolchain — Visual C++ 4.2 under CrossOver wine (macOS / Apple Silicon)

Goal: run the original **Microsoft Visual C++ 4.2** `cl.exe` (10.20) / `link.exe` under
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

## ⚠️ Wrapper invocation (gotcha, learned 2026-07-05)
`bin/cl` and `bin/link` are **bash scripts** that internally run `wine .../CL.EXE|LINK.EXE`.
Invoke them **directly** — `toolchain/bin/cl <args>` — NOT `wine toolchain/bin/cl` (that hands the
shell script to wine as a PE → `err:process:exec_process ... not supported on this system`, no `.obj`,
and you may then read a *stale* `.obj` from a prior build and think it worked). The wrappers already
handle wine, `INCLUDE`/`LIB`, and `/`→`Z:\` path conversion for existing-file args.

## MFC linkage — static NAFXCW (stood up + validated 2026-07-05)
All pieces are present under `vc42/`: `MFC/LIB/NAFXCW.LIB` (7.5 MB retail static MFC), `MFC/INCLUDE`
(afx.h/afxwin.h/afxcoll.h), `LIB/LIBCMT.LIB` (static CRT), and the Win32 import libs. `bin/link` sets
`LIB="$VC/LIB;$VC/MFC/LIB"` and dispatches to `LINK.EXE`. **You do not list the MFC/CRT libs by hand** —
`afx.h` emits `#pragma comment(lib, "nafxcw.lib")` (+ libcmt), so LINK pulls them in the right order from
the LIB path automatically.

Validated recipe (see `test/mfctest/`, a `CWinApp` + `CDWordArray` app that links clean to a 111 KB
PE32 GUI EXE, 0 unresolved externals):
```
# compile a static-MFC TU (note the MFC defines)
toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS foo.cpp
# link a static-MFC EXE (pragmas supply nafxcw.lib + libcmt.lib)
toolchain/bin/link /nologo /subsystem:windows /INCREMENTAL:NO /out:foo.exe foo.obj
```
This unblocks **per-function matching of MFC-derived app classes** (`World`=`CDeskcppDoc`,
`GameView`=`CDeskcppView`, and any class with `CObArray`/`CDWordArray`/`CString` members): model the
class as `: public CObject`/`CDocument`/etc. with real MFC members, and the compiler emits the correct
member-construction/destruction codegen (EH frame, ctor/dtor call sequence) — the base-class + member
ctor/dtor calls are relocations (masked by the matcher). This is what the Zone `Ctor`/`Dtor` need to
resolve their reg-alloc residuals (the `FID_conflict_CDWordArray()` ×7 member constructions).

**Linker-version flag (endgame only):** our `LINK.EXE` stamps PE linker version **4.20**, but
`YodaDemo.exe`'s header reads **3.10** (verified via `DUMPBIN /HEADERS`). The **compiler** codegen is
correct (it byte-matched `World::CalcCompletionScore`), so per-function matching is unaffected, and the
linker-version field is masked like the timestamp. But a byte-identical **whole-EXE** (the final puzzle)
would need the exact link 3.10 (shipped with VC++ 4.1; VC++ 4.2 ships 4.20). Source that linker before
the whole-EXE endgame; irrelevant until then.
