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

## Results (2026-07-07, v31)

- **duplicate symbols: 0** — no stub-vs-real body collisions. The stub headers emit declarations
  only; every function body is defined at most once. Good hygiene going into G2.
- **unresolved externals: 34 unique** — categorized below. The codebase is essentially
  **feature-complete**: after closing the one true gap (RemoveItem), *every* remaining code symbol
  is a function that IS transcribed but is *referenced under a drifted name/signature*. The rest
  are external imports, data tables, and DYNCREATE runtime-class objects.

### A. External DLL imports — need an import lib/stub (10)
`WaveMixActivate/CloseChannel/CloseSession/FlushChannel/FreeWave/Init/OpenChannel/OpenWave/Play/Pump`
— WAVMIX32.DLL (the shipped `wavemix` sound mixer). The original links `WAVMIX32.LIB`; we don't
have it. Fix: synthesize a `wavmix32.lib` import stub (a `.def` → `lib /DEF`, or an empty stub obj
exporting these `@N` stdcall names) so the link resolves. Not our source code.

### B. Cross-TU name / signature / class drift (transcribed — reconcile the stub to the canonical decl)
Each is defined in some TU but the *caller's* stub header declares it with a different name, class,
return type, or arity → a different mangled name. **Reconciling a pure rename is byte-neutral**
(call sites are masked relocations — the code bytes don't change), so these can be fixed without
disturbing existing byte-matches. A *signature* change (return width / param type / arity) can
change the caller's marshalling — re-verify that caller after.

| unresolved (caller wants) | canonical definition | drift |
|---|---|---|
| `World::FindTile(Tile*)` | `World::FindTile(void*)` @0x403aa0 | param type (`Tile*` vs `void*`) |
| `World::GetExitDirections():int` | `…():unsigned char` @0x4032c0 | return width (H vs E) |
| `World::SaveZoneRecursive(CFile*,short)` | `…(CFile*,short,int bFull)` @0x4033b0 | arity |
| `World::EnterZone(Zone*)` | `World::GetZoneIndex(Zone*)` @0x423dc0 | method name |
| `World::Populate()` / `Zone::DamageEntityAt(...)` | defined (Worldgen/Records) | sig drift |
| `World::LoadWorldMaybe()` / `GetGridOrderMaybe(int,int)` / `FindSpecialZoneMaybe()` | defined @0x421fd0/0x421e50/… (Worldgen) | GameData `WorldStub.h` decl vs real |
| `GameView::PlayerMove(int)` / `PlayerCheckWalkable(short,short)` | @0x409060/0x409460 | `Records.h` stub class vs GameView |
| `FrameView::ConfirmExit()` / `DrawTextA(CDC*)` / `UpdateDragCursor(int)` | `GameView::…` @0x416030/0x40f060/0x412cc0 | stub class name (`FrameView` = `GameView`) |
| `CTheApp::LogWrite(char*)` | free `void __stdcall Log_Write(char*)` @0x419cb0 | member-vs-free + calling convention |

### C. DYNCREATE runtime-class objects (3)
`rtcDeskcppDoc`, `rtcDeskcppView`, `rtcMainFrame` — the `CDocTemplate`/`AddDocTemplate` construction
references these `CRuntimeClass` objects by stub names, but `IMPLEMENT_DYNCREATE(World/GameView/
CMainFrame)` emits them as `World::classWorld` etc. Reconcile the doc-template call to the real
`RUNTIME_CLASS(World)` names (or vice-versa).

### D. Data globals — linkage / name drift or genuinely-undefined tables (5)
| symbol | note |
|---|---|
| `_YodaMasterPalette` | defined `YodaMasterPalette` in WorldDoc.cpp (C++); referenced C-linkage → wrap `extern "C"` |
| `App_bCpuHasMMX` (both `_App_bCpuHasMMX` and `?App_bCpuHasMMX@@3HA`) | same datum referenced C and C++ linkage — unify |
| `Iact_szCmdTextBuf` | IactScript.cpp; extern-vs-static / name drift |
| `gNeedleTable`, `gWorldgenGridOrderTable` | referenced by Worldgen but not defined there — likely real data tables still to place (candidate true gaps) |

## Resources (for a *runnable* EXE, not for the link)

The link does NOT need resources — code references resource **IDs** (integers), not symbols. But to
produce a *runnable* image for behavioral comparison against the original, extract `YodaDemo.exe`'s
resources (RT_DIALOG, RT_MENU, RT_BITMAP, RT_STRING, RT_GROUP_ICON, RT_RCDATA — the game's `.WLD`
seed etc.) with `wrestool -x` (icoutils) or a PE resource parser, regenerate a `.rc`/`.res`, and
add it to the link. Track under this phase but after the symbol reconciliation.

## Definition of done

`tools/link_exe.sh` links with 0 unresolved (WAVMIX stubbed) and 0 duplicates → a linkable image;
then + resources → a runnable image; then G2 drives it toward the byte-identical layout.
