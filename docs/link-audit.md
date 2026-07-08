# Phase G0 — "link-to-complete" audit (whole-image completeness oracle)

**Goal:** compile every `src/**/*.cpp` with the locked VC++ 4.2 toolchain and attempt a full
static-MFC **link** into one EXE. The link is not (yet) byte-identical — it is a *completeness
oracle*. The linker enumerates, precisely and always-currently:
- **unresolved externals** — a function/datum that is called but not defined *under that exact
  mangled name* → either a true gap (never transcribed) or cross-TU name/signature/linkage drift;
- **duplicate symbols** — a non-COMDAT collision (a stub body colliding with the real one).

This turns the Phase-F/G "is it feature-complete?" question and the manual FUN_* sweep into a
concrete checklist. It *indirectly* did Phase F's job: it found `GameView::RemoveItem` (0x429150),
a real 214-byte app function the "zero FUN_*" sweep missed because it sits one function past the
recorded Worldgen TU end.

## Harness

`tools/link_exe.sh` (run from repo root). Compiles all TUs, links with `NAFXCW.LIB` + `LIBCMT.LIB`
+ the Win32 import libs, and prints a categorized unresolved summary. Artifacts + `link.log` +
`unresolved.txt` land under `$CLAUDE_JOB_DIR/tmp/build-link/`. Re-run any session; the unresolved
count should trend to 0.

## Results (2026-07-07, v32) — ✅ LINKS + RUNNABLE

`tools/link_exe.sh` now reports **0 unresolved, 0 duplicates, link exit 0** and produces a
**runnable `yoda.exe`** (446 KB, incl. the copied resource section). All 34 v31 unresolved were
closed: 10 WAVMIX imports via the authentic import lib, 24 code/data symbols by reconciling the
caller stubs to the real definitions (categories B/C/D below, each marked ✅ RESOLVED). The net
byte-match cost of the signature reconciliations was ~-0.7 % exact (Worldgen ParseZaux/ParseZax2/
SetCurrentToIntroZone + one WorldDoc fn flipped to PHASE-DISPLACED as the shared-header dial
rotated toward the TRUE signatures) — expected dial breathing, recoverable in G1. Coverage 99.09 %
holds. The reconciled signatures are now CORRECT, which is what G1's fixed-point pass needs anyway.

### Prior state (2026-07-07, v31)

- **duplicate symbols: 0** — no stub-vs-real body collisions. The stub headers emit declarations
  only; every function body is defined at most once. Good hygiene going into G2.
- **unresolved externals: 34 unique** — categorized below. The codebase is essentially
  **feature-complete**: after closing the one true gap (RemoveItem), *every* remaining code symbol
  is a function that IS transcribed but is *referenced under a drifted name/signature*. The rest
  are external imports, data tables, and DYNCREATE runtime-class objects.

### A. External DLL imports (10) — ✅ RESOLVED via an in-house non-copyrighted stub
`WaveMixActivate/CloseChannel/CloseSession/FlushChannel/FreeWave/Init/OpenChannel/OpenWave/Play/Pump`
— WAVMIX32.DLL (the shipped `wavemix` sound mixer). Resolved without redistributing Microsoft's
lib: we author `toolchain/wavmix32/wavmix32.def` (the 10 export names — an interface list) +
`wavmix32_stub.c` (no-op `WINAPI` stubs), and `link_exe.sh` builds a stdcall-decorated import lib
+ a no-op stub DLL from them. The implib provides our `_WaveMix*@N` symbols and imports the
UNDECORATED names from WAVMIX32.DLL — same import table as the original, so the stub DLL runs the
game silently and a real WAVMIX32.DLL drops in for sound. (LIB /DEF alone gives undecorated
symbols; the `@N` decoration must come from compiling stdcall stubs — hence the stub `.c`.)

### B. Cross-TU name / signature / class drift (24) — ✅ RESOLVED (callers reconciled to real defs)
Each was defined in some TU but the *caller's* stub declared a different name/class/return/arity →
a different mangled name. Pure renames are byte-neutral (call sites are masked relocations); the
signature changes (marked ⚑) rotated the TU-phase dial and were re-verified.

| was (caller wanted) | reconciled to | fix |
|---|---|---|
| `World::FindTile(Tile*)` | `World::FindTile(void*)` @0x403aa0 | Worldgen.h decl → `void*` |
| `World::GetExitDirections():int` | `…():unsigned char` @0x4032c0 | ⚑ Worldgen.h decl → `unsigned char` |
| `World::SaveZoneRecursive(CFile*,short)` | `…(CFile*,short,int bFull)` @0x4033b0 | ⚑ +3rd arg `pCell->flagSolved` at OnSaveWorld's 3 calls (raw-disasm-proven 3 pushes) |
| `World::EnterZone(Zone*)` | `World::GetZoneIndex(Zone*)` @0x423dc0 | rename WorldStub.h + Iact.cpp |
| `World::Populate():void` | `World::Populate():int` @0x425e30 | ⚑ WorldStub.h decl → `int` |
| `Zone::DamageEntityAt(…,int,…)` | `…(…,short,…)` @0x405710 | removed the stray `int`-param overload dup in RecordClasses.h (kept the real `short`) |
| `World::LoadWorldMaybe/GetGridOrderMaybe/FindSpecialZoneMaybe` | `LoadWorld/GetZoneGridOrder/SetCurrentToIntroZone` @0x421fd0/0x421e50/0x423d20 | rename WorldStub.h + GameData.cpp (FindSpecial… proven = SetCurrentToIntroZone via StartGame disasm) |
| `GameView::PlayerMove(int)/PlayerCheckWalkable(short,short)` | `PlaySound(int)/DrawZoneCell(short,short)` @0x409060/0x409460 | Records.h stubs were MISNAMED — the real funcs are PlaySound/DrawZoneCell (GameView.h) |
| `FrameView::ConfirmExit()/DrawText(CDC*)/UpdateDragCursor(int)` | `GameView::…` @0x416030/0x40f060/0x412cc0 | renamed the `FrameView` stub class → `GameView` in Frame.{h,cpp} (DrawText→DrawTextA via windows.h) |
| `CTheApp::LogWrite(char*)` | member `CTheApp::LogWrite` @0x419cb0 | changed the App.cpp DEFINITION from free `__stdcall Log_Write` to the member (body ignores `this` ⇒ byte-identical; matches the caller's thiscall ECX setup) |

### C. DYNCREATE runtime-class objects (3) — ✅ RESOLVED
`rtcDeskcppDoc/rtcMainFrame/rtcDeskcppView` replaced in App.cpp with `RUNTIME_CLASS(World)/
(CMainFrame)/(GameView)` — forward-declared each class with just its `static const CRuntimeClass
classX;` member so the macro resolves to the real `IMPLEMENT_DYNCREATE`-emitted symbol in the
other TU. Byte-neutral (masked data reloc; InitInstance held DIFF330).

### D. Data globals (5) — ✅ RESOLVED (linkage unified + real tables extracted from the binary)
| symbol | fix |
|---|---|
| `App_bCpuHasMMX` | defined once `extern "C"` in App.cpp; Canvas.cpp extern → `extern "C"` (unified C linkage) |
| `YodaMasterPalette` | extracted the real 256-RGBQUAD table (`.data 0x456230`, 1024 B) → defined `extern "C"` in WorldDoc.cpp |
| `gWorldgenGridOrderTable[100]` | extracted (`.data 0x456630`, the 5→1 grid-order ring) → defined in Worldgen.cpp |
| `gNeedleTable` | extracted (`.data 0x456938`, actually **25 ints**; the `[50-nLo]` index reaches [25] at nLo=25 — an original one-past-the-end read of the adjacent "UPAU" tag). Defined `[26]` with a 26th = 16 (geometric) so our build is well-defined |
| `Iact_szCmdTextBuf` | defined `char[2048]` in IactScript.cpp (`.bss 0x459558`; exact reservation is a G2 detail) |

## Resources — ✅ DONE (runnable image)

`tools/extract_res.py` parses `YodaDemo.exe`'s `.rsrc` directory and emits a linkable `.res`
(byte-faithful copy, not reconstructed `.rc`): 127 resources — 15 dialogs, 1 menu, 17 string
tables, 16 bitmaps, 11 icons + 11 group-icons, 32 cursors + 21 group-cursors, 1 accelerator, 1
version. `link_exe.sh` links it in (link.exe 3.10 accepts `.res` directly). The output EXE's
`.rsrc` matches the original's virtual size (0xd674). So the image RUNS with the original UI.
A real `WAVMIX32.DLL` must be present at load (as for the original demo).

## Definition of done — ✅ MET (for a linkable + runnable image)

`tools/link_exe.sh` links with **0 unresolved + 0 duplicates + resources** → a runnable image.
Remaining: G1 recovers the ~0.7 % dial-displaced exacts; G2 drives toward the byte-identical
layout (COMDAT geography, .data/.rsrc placement, PE timestamp/checksum).
