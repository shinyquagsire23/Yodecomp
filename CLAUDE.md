# Yodecomp — decompilation cheat sheet

In general, you can adhere to patterns found in `OpenJKDF2`, located at `~/workspace/OpenJKDF2`. CMake should be used, and the assumed build platform is macOS and Linux. Claude is permitted to modify this file with any useful notes that will aid other/later Claudes. Use `wine` to invoke Windows toolchains and executables.

### External references
- **`~/workspace/DesktopAdventures`** — the user's own engine recreation of the *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures). Invaluable for asset-format and game-logic semantics when naming functions/structs. Notably `scrdoc.txt` = reverse-engineered **script opcode format** (pre-script conditions like `BumpTile`, `CheckEndItem`, `EnemyDead`, `HasItem`, `HealthLs`…), plus `SCRIPTS.md`, `README.md`. Use it to name `.DTA`/zone/script parsing code. The 10×10 grid at World+0x4B4 (stride 0x34/zone) is the map's zone grid.
- `~/workspace/OpenJKDF2` — style/naming conventions (`Module_Function`, loose-Hungarian), CMake layout.

## Naming Convention and Decompiling Tips

In general, variable names should follow a loose-Hungarian Notation, where pointers start with `p` (ie, `pThing`), pointers to arrays are prefixed with `pa` (ie `paIndices`), booleans are prefixed with `b` (ie, 'Main_bMotsCompat'). Name a pointer after the **struct it points to**, not its MFC role: the `World`(`CDeskcppDoc`) pointer is **`pWorld`**, NOT `doc`/`pDoc` (e.g. `GameView.pWorld@0x44`); a `GameView` pointer is `pView`, etc.

**Function naming = C++ `Namespace::Method`.** This is a C++/MFC app, so group
functions by their **class** as a Ghidra namespace and give the method a bare name — **do NOT repeat the
group in the method**: `Canvas::BlitMasked` (not `Canvas::Canvas_BlitMasked` / flat `Canvas_BlitMasked`),
`GameView::RemoveItem`, `World::Load`, `Zone::GetTile`.

⚠️ **CRITICAL: the namespace name MUST equal the struct name — Ghidra derives a `__thiscall` function's
auto-`this` type from its parent namespace by matching it to a same-named `Structure`.** Put a doc method in
a `Dta` namespace (no `Dta` struct) and its `this` silently degrades to `void*` (offsets instead of
`this->field`). So **namespace = the C++ class, not a sub-module.** The whole **doc translation unit**
(`CDeskcppDoc` = our `World` struct: `.dta` load/parse + worldgen + `.wld` save + inventory/UI) lives in the
**`World`** namespace; the **view** (`CDeskcppView` = `GameView`) in **`GameView`**. Sub-modules like "Dta"
or "Worldgen" are a *documentation* concept (docs/compile-units.md), **not** namespaces — an early attempt to
make `Dta::`/`Worldgen::` namespaces broke `this=World*` and was folded back into `World`.

Namespaces in the DB now: **class** namespaces (`World`,`GameView`,`Zone`,`Canvas`,`Tile`,`ZoneObj`,
`MapEntity`,`Puzzle`,`IactScript`,`InvScrollBar`,…) — `this` types correctly; and **CU/module** namespaces
for genuinely separate `.obj`s that aren't a modeled struct (`GameData`,`Iact`,`App`,`Settings`,`Frame`,
`Dlg`,`Log`,`Mfc`,`Render`) — their `__thiscall` members show `void* this` until/unless the class is modeled
as a struct (then move them or `set_function_this_type`). Global leftovers are only `FUN_*` (undiscovered),
`FID_*` (MFC library), and import thunks. Migration was done with idempotent `run_script_inline` loops over
`fm.getFunctions` (longest-prefix→namespace map, collision→append addr; then a residual self-prefix strip).
Loose-Hungarian still applies to the bare method + variables (`Draw*`, `p`/`b`/`n`). When you `set_function_
this_type X*`, Ghidra moves the func into namespace `X` automatically — so typing and namespacing are one act.

**Prefer a descriptive guess over `FUN_<addr>` — mark uncertainty with `Maybe`.** A name derived from what a
function *accesses/calls* (even a vague one) is more useful than an anonymous `FUN_*`, and it refines over
time. When the behaviour is clear but the exact purpose isn't, append **`Maybe`**: e.g. `World::InitUiMaybe`,
`GameView::DrawStatusMaybe`. This is the standing signal that the name is a hypothesis to confirm/sharpen
later (grep `Maybe` to find them). Only leave `FUN_<addr>` when you genuinely can't tell what it touches. 
Still avoid *confidently wrong* names (the `BlitWeaponBox` miss) — read the body first; `Maybe` is for honest
"looks like X" guesses, not unread ones.
For struct fields, an **`Unk`** append like **`Unk0x44`** can be used as a placeholder for fields which have 
not been seen as written or read to in a way that is clearly identifiable, but the size or type is known.
For example, an unidentified state machine enum in a struct may start as an int **`Unk`**.
A **`Related`** append may be used if a function touches several known subsystems, but its actual function is 
unknown. This is an in-between identifier between **`Maybe`** and **`Unk`** in that it gives a signal for what 
the function touches, without solidifying exactly what the function is/does. This may also be used for fields.
For example, a function which accesses Palette related functions might be marked
**`ThisType::FUN_123456_PaletteRelated`**.
In summary: **`Unk`** (for class fields and structs) and **`FUN_`** (for subroutines) can be upgraded to
**`Related`**, **`Related`** can be upgraded to **`Maybe`**, and **`Maybe`** can be upgraded to a certain
identifier. Before bytematching a TU, all functions being decompiled (and referenced by the decompilation) should 
be upgraded from **`Maybe`** with certain, idiomatic names, with documentation to back the naming. Dll referenced 
struct members in the decompilation should also have a certain identifier.

## Decompiling

A decompiler instance can be accessed via `http://localhost:8089`, which is running an instance of https://github.com/bethington/ghidra-mcp. The binary that should be accessed is `YodaDemo.exe`, which can also be found at `YodaDemo/YodaDemo.exe`.

**CRITICAL GOTCHA:** The Ghidra project has *multiple* programs open (JK.EXE, DroidWorks.exe, YodaDemo.exe, …) and the *current* active program may not be YodaDemo. The HTTP endpoints are stateless — `switch_program` does **not** persist across calls. You **must** append `program=YodaDemo.exe` to *every* request, or you may silently be reading a different game.

Examples (note the mandatory `program=` param):
`http://localhost:8089/list_functions?program=YodaDemo.exe&limit=3000` - Lists YodaDemo functions.
`http://localhost:8089/decompile_function?program=YodaDemo.exe&address=0040b270` - Decompiles the function at 0x40b270.
`http://localhost:8089/get_current_program_info` - Sanity check; ignores param and shows the *default* (JK.EXE).

The richer `mcp__ghidra__*` tools are also available and take a `program`/instance argument.

**WRITE GOTCHA (worse than the read one):** `program=` is honored only for **reads**. **Mutations** (rename,
set-comment, set-prototype, etc. — both `mcp__ghidra__*` and the HTTP endpoints) always act on the
**currently-active** program, which is **JK.EXE**, and `switch_program` returns `success` but does NOT
persist (next request is JK.EXE again). Passing `program=YodaDemo.exe` to a write is silently ignored →
you will corrupt JK.EXE instead (verified 2026-07-04; e.g. renaming "YodaDemo 0x401490" actually hit
JK.EXE's `jkGame_SetDefaultSettings` at 0x401480). **To rename/annotate YodaDemo you must make
YodaDemo.exe the ACTIVE program in the Ghidra GUI first** (open/focus it in the CodeBrowser). Confirm with
`list_open_programs` → `current_program` must read `YodaDemo.exe` before any write. Then writes land correctly.

## Binary facts (established 2026-07-04)

`YodaDemo.exe` is the demo of **Yoda Stories** (LucasArts, 1997), a Win32 **MFC** application.

| Property | Value | Source |
|---|---|---|
| Format | PE32, x86 (i386), GUI subsystem 4.0 | PE header |
| ImageBase / SectAlign / FileAlign | 0x400000 / 0x1000 / 0x200 | PE optional header |
| **Linker version** | **3.10** | PE optional header → **VC++ 4.2** |
| Build timestamp | 1997-02-18 19:31:59 UTC | PE `TimeDateStamp` |
| CRT | **static (`/MT`, LIBCMT.LIB)** — CRT funcs (`_memset`,`_malloc`,`_sprintf`) live in `.text`; **no MSVCRT import** | PE import table |
| MFC | statically linked (`NAFXCW.LIB`; no MFC42.DLL import; `Afx*`, `CImageList`, `CToolTipCtrl` symbols) | Ghidra |
| Imported DLLs | KERNEL32, USER32, GDI32, ADVAPI32, WAVMIX32, COMCTL32, WINSPOOL, comdlg32, SHELL32 | PE import table |
| Rich header | **absent** (e_lfanew=0x80) — consistent with pre-VS6 toolchain | PE parse |
| C++ EH | present (`__CxxFrameHandler`, `__CxxThrowException`, `/GX`) | Ghidra |

**Compiler conclusion: Microsoft Visual C++ 4.2 (cl 10.20 / link 3.10).** Evidence: linker 3.10 + MSVCRT40 + no Rich header + 1997 date + MFC 4.2 idioms all converge.

### `.text` layout (0x401000–0x44afff, ~303 KB)

Standard MSVC link order = **app object files first, then statically-linked library objects**:
- **App code: ~0x401000 – ~0x429000** (~529 functions, mostly still `FUN_*`). *This is what we decompile & match.*
- **Library code: ~0x429000 – 0x44afff** (~1181 functions): MFC (`CImageList`, `CProgressCtrl`, `CToolTipCtrl`), WaveMix, C++ EH runtime, and CRT (`_memset`, `_atoi`, `_malloc`, `_sprintf`, `_time`, `__ftol`). *We link against the real libs; we do NOT hand-write these.*
- Boundary is approximate — a few MFC static objects (e.g. `AfxTryCleanup` @ 0x409050) are pulled into the app region by reference order.
- Biggest app function: `FUN_0040b270` (~10.8 KB) — likely the main window proc / game loop.

## Decompilation strategy (phased plan)

The five original requirements are reorganized into a dependency-ordered plan. The full plan can be seen in PLAN_COMPLETED.md.

### ⭐ Prior art — the trail is already blazed (USE THIS)
**LEGO Island (1997)** was built with the *identical* config: **MSVC 4.20 + static MFC + Win32 GUI game + tools under wine.** The **isledecomp** project (github.com/isledecomp) solved exactly our problem. Adopt their approach wholesale:
- **`reccmp`** — their address-anchored, relocation-aware function comparator. Source functions get a marker comment `// FUNCTION: YODA 0x401230`; build the project with cl 4.2 (add **`/Zi`** — debug info does NOT change codegen but gives reccmp the recompiled addresses via PDB); reccmp diffs each function against the original at its recorded address and reports per-function match %. **Comparison is anchored by address, not layout** — so we do NOT need to solve TU boundaries / link order up front.
- **`decomp.me`** hosts MSVC 4.x compilers — use it on **day one** to experiment with matching a function *before* the local toolchain exists.
- Their wiki documents MSVC 4.2 codegen idioms — don't rediscover them.
- **Defer byte-identical whole-`.text` to the endgame.** Match functions individually first; identical layout is a deterministic end-puzzle (TU order + lib link order + masking PE timestamp/checksum).

### Phase 0 — Identify the compiler ✅ DONE
VC++ 4.2 (see table above).

### Phase 1 — Stand up the matching toolchain (unblocks everything)
See PLAN_COMPLETED.md

### Phase 2 — Prove it: first bytematch ✅ CODE-MATCHED (2026-07-04)
See PLAN_COMPLETED.md

### Phase 3 — Map compile units & document (the long grind)
- Comb the **app region** function-by-function. Contiguous runs of functions = same `.obj` (MSVC emits functions in source order per translation unit; `.rdata`/`.data` groupings and string clusters corroborate boundaries).
- **Padding note (tested 2026-07-04):** unlike JK.EXE (which spaces CUs with `0x90` runs), YodaDemo pads with **`0xCC` only**, and *every* function is 16-byte aligned (padding runs are a uniform 1–15 bytes). So **padding-run length does NOT isolate CU boundaries here** — the alignment is per-function (consistent with `/Gy`). Better CU signals for this binary: (a) **shared string/global clusters** — decompile a run of functions and see which reference the same adjacent `.rdata`/`.data` block; (b) source-order heuristics; (c) `.rdata` const-pool groupings. Also watch for **non-16-aligned gaps after padding** (e.g. `0x416301→0x41699e`) — those are un-recovered jump/switch tables or functions Ghidra missed; disassemble and define them.
- **When a compile unit is identified, RENAME every function in that unit with a shared prefix** (OpenJKDF2 convention: `Video_*`, `sithThing_*`, `Main_*`). Pick a prefix from the unit's role (e.g. `Sound_*`, `Tile_*`, `Map_*`, `Palette_*`). Use `mcp__ghidra__rename_function` / `batch_rename_function_components` with `program=YodaDemo.exe`.
- **Provisional CU tagging (do this early, before understanding each function).** Once you know a CU's
  extent from an anchor, bulk-rename its `FUN_*` to `<Prefix>_FUN_<addr>` (keeps the address, clearly
  provisional, reversible). Then *caller* decompilations show module context at a glance. A script over
  (start,end,prefix) ranges does the whole binary in seconds. See the 340-func tagging (Zone_/Iact_/
  Render_/Player_/View_/Dta_/GameData_) and the named CU outline in docs/compile-units.md.
- **Proximity corrects mis-attribution (MSVC never interleaves .objs).** All functions of one `.obj` are
  emitted contiguously, so a function's neighbors reveal its true CU — which can override a name you gave
  by behavior. Example: `Zone_ReadZaux/Zax2-4` sat *between* `Iact_*` functions (0x405ae0–0x4070e0), so
  they're in the **Iact** `.obj`, not the Zone-class `.obj` (0x405150–0x405ae0) — renamed to `Iact_Read*`.
  When a named function is wedged in a different-prefix run, re-prefix it to match its neighbors.
- Document discovered structs & signatures in Ghidra (types) and mirror them into headers under `src/`.
- Naming: loose-Hungarian — `p`=pointer, `pa`=pointer-to-array, `b`=bool (see top of file).

### Phase 4 — Scale matching
- One-by-one, write matching C per compile unit, compile with the locked toolchain, bytematch. Track match % over the app region.

### Compile units identified (progress log)
- **`World_*`** — game-state/score module. Confirmed contiguous cluster **0x401450–0x401ab9**, pinned by
  the dispatcher `World_UpdateScore` (0x401450) which sums four score components into `world+0x70`:
  `World_CalcTimeScore` (0x4019c0), `World_CalcSolvedScore` (0x401780), `World_CalcScoreFromCounter`
  (0x4016d0), `World_CalcCompletionScore` (0x401490, ✅bytematched). Plus accessor `World_GetZoneCell`
  (0x401a80). All operate on the World struct via ECX: 10×10 zone grid @ +0x4B4 (stride 0x34), totalZones
  @ +0x58, time @ +0x78/+0x7c, score @ +0x70. **Note:** +0x4B4 is referenced by ~12 functions spanning
  the whole app region — the grid is a *shared* struct, so shared-offset access is NOT a CU signal; only
  contiguity + the dispatcher's call set delimit the unit. Edges to refine: functions <0x401450 are MFC
  ctor/dtor boilerplate (may be a separate class TU); 0x401ac0+ switches to MFC `CWinApp`/`CString` code.

## 🗺 LONG-TERM ROADMAP (written 2026-07-05; revised 2026-07-06 v15 with Phase E/F/G elaborated — keep this current)

**The unit of completion is the TRANSLATION UNIT, not the function.** Lesson #7 + the Records coupling
matrix prove codegen state flows forward through a TU (and through class decls in its header): functions
match piecemeal only until the TU around them changes. So the plan is TU-by-TU, each TU driven to
"all functions exact-or-annotated-effective", with a single JOINT residual pass per TU at the endgame.

### App-region inventory (~128 KB, 534 funcs) and TU status
| TU / module | range | ~size | state |
|---|---|---|---|
| World scorers (doc-TU fragment) | 0x401450–0x401ab9 | 1.6 KB | ✅ 5/6 exact (07-06: CalcTimeScore matched); CalcSolvedScore x87 park proven permuter-immune |
| **GameData** (2nd doc-TU src file) | 0x401ac0–0x4042b0 | ~10 KB | ✅ DONE 07-06: 11/27 exact (src/GameData/), rest effective/PHASE-DISPLACED; BQP +9B |
| **Records** (6 record classes) | 0x4042b0–0x405ae0 | 5.5 KB | ✅ DONE 25/33 exact + 8 annotated eff. |
| **Iact** (`.obj`: Zone readers + IACT) | 0x405ae0–0x407cf4 | ~9 KB | ✅ TU COMPLETE 07-06: all 10 funcs transcribed, 88% insn-identical; 2 exact + 8 annotated tie-break residuals |
| **Canvas** (DIBSection blitter) | 0x407df0–0x4084e8 | 1.8 KB | ✅ DONE 8/11 + 3 eff. (parked) |
| **IactScript** (3 script record classes) | 0x418700–0x418dd0 | ~1.7 KB | ✅ 11/12 exact (src/IactScript/) |
| **Dlg TU** (CTextDialog) | 0x418dd0–0x419000 | ~0.6 KB | ✅ DONE 07-06 (src/Dlg/): 5/5 EXACT. Implicit-dtor lesson (??_G inline) |
| **Frame TU** (CMainFrame) | 0x419000–0x419720 | ~1.8 KB | ✅ DONE 07-06 (src/Frame/): 14/18 exact + 4 eff. (2 palette sbb, PreCreateWindow, OnActivate). Owns g_strReplayPath |
| ~~Core utils~~ = GameView TU head | 0x408c60–0x40a560 | 6.5 KB | ⚠ mislabel: GameView methods (Dtor/OnDraw/DrawZoneCell/ZoneTransitionStep) + the ~CGdiObject/GDI COMDAT copies — Phase E, not a warm-up |
| **GameView TU** (view/UI/AI monster) | 0x4084f0–0x418700 | ~64 KB | ⭐ PHASE E step 4 ACTIVE (src/GameView/GameView.cpp). v19: 19/31 exact — transcribed through 0x40b160(excl). New EXACT: DrawGameArea, IsUsableTileMaybe (switch tree), 6 GDI COMDATs (CGdiObject/CBitmap ctor/??_G/??1 trios, hint-markers). EFFECTIVE: ZoneTransitionStep + WorldEntryStepMaybe (a parity-CROSSED clone pair), BlitTile/DrawTileAt (reg-role), FireWeaponStep (open flags-test axis). NEXT: DrawEntities 0x40b160, FindEntityAt 0x40b210, Tick 0x40b270 (10.8KB) |
| **App TU** (CTheApp + CAboutDlg + Log_Write) | 0x419720–0x419ed0 | ~2 KB | ✅ DONE 07-06 (src/App/): 15/16 exact + InitInstance eff. (CPUID hand-asm) |
| **WorldDoc TU** (doc main src file) | 0x419ed0–0x41bee0 | ~8 KB | src/WorldDoc/: 7/13 exact incl. the 1441B DTOR + ctor-derived REAL World class; ctor/OnNew/OnOpen/GetLocatorIcon parked (imm-store batching + block-layout opens) — joint-pass fodder |
| **World/doc TU** (dta-load+worldgen+wld+doc) | 0x41bee0–0x429150 | ~54 KB | ✅ TRANSCRIPTION COMPLETE v14/v15: src/Worldgen 90 markers = EVERY function incl. the GameView tail block; 34/90 exact + 56 annotated EFFECTIVE (reg/slot/dial tie-breaks, per-function autopsies in-source); zero FUN_*. Unclaimed in range: 4 exception COMDATs @head, ??_GCPalette 0x41e8b0, EH thunk 0x424f69, jmp 0x424fb0 (all Phase G). Joint pass AFTER Phase E (shared Worldgen.h ⇒ E's GameView decls re-rotate this TU) |

With the doc TU transcribed (v14), the untranscribed remainder is essentially ONE monster: the
GameView TU + its mislabeled head (~63 KB ≈ 26 % of the app region). Its struct is complete and its
runtime behavior documented (docs/game-logic.md) — Phase E is transcription plus the method-decl-set
reconstruction, not research. After E: mop-up minis (F), then joint passes + whole-image (G).

### Struct status board (audited 2026-07-05 — regen with the run_script_inline coverage dump)
Coverage = defined-bytes ÷ sizeof; unk = fields still named unk*/field_*/…Maybe.

| struct | size | cover | unk | state / phase to finish |
|---|---|---|---|---|
| Zone / ZoneObj / Tile / Canvas | 0x848/0x10/0x40c/0x43c | 100 % | 0 | ✅ done (byte-match-proven) |
| MapEntity | 0x64 | 96 % | 5 | ✅ good; unk10/18/20/2c/60 have no readers found (runtime-only scratch?) |
| Puzzle | 0x2c | 95 % | 3 | ✅ good; unk2/unk3/unk14 parse-only (unknown in DA too) |
| Character | 0x4c | 94 % | 3 | ✅ good; unk40/unk44 parse-only, unk48 tail pad |
| CObArray family / CDWordArray / BITMAPINFO256 | 0x14/0x428 | 100 % | 0 | ✅ modeling helpers |
| **World** | **0x33c0** | **~98 %** | few | ✅ v10 full Ghidra↔Worldgen.h mirror + v15 rect-block/tail sync. Worldgen.h IS the emerging real CDeskcppDoc header. Residual unks (unk2e58/unk2e60/unk3378/unk33b8…) are semantics-only — layout is ctor/byte-match-proven |
| **GameView** | **0x310** | **~95 %** | ~10 | ✅ struct COMPLETE, now in src/GameView/GameView.h (v16, promoted from Worldgen.h) + Ghidra synced. METHOD decl set reconstructed v16 (overrides via vtable-diff, afx_msg via msgmap, ~35 helpers). Open: byte-prove "(sig?)" helper widths + AFX_MSG order during step-4 transcription |
| MapZone (10×10 grid cell) | 0x34 | 100 % | few | ✅ v10 vptr-true rebuild (vftable@0, id@4, zoneType@8 as ZoneType enum); save/load semantics from v13 |
| IactScript | 0x30 | 100 % | 0 | ✅ solved 2026-07-05: vtbl@0 (0x44bc68) + 2 inline CObArray (conditions@4, commands@0x18) + doneFlag@0x2c — Zone-pattern. Whole Iact-script TU (0x418700–0x418dd0) renamed Records-style: IactScript/IactCondition/IactCommand ::Ctor/ScalarDtor/Dtor/Read |
| IactCondition / IactCommand | 0x1c/0x20 | 100 % | 0 | ✅ vftable@0 added (0x44bc80/98); opcode@4 + args[5]@8 (+text@0x1c for commands) |
| InvScrollBar | 0x44 | good | 0 | ✅ v14/v15: CScrollBar-derived, scrollMax@0x3c, scrollPos@0x40; Ctor=0x4085c0 (its own mini-TU) |
| TextDialog | 0xc8 | good | 3 | ✅ v14/v15: NOT CDialog-derived (plain class). unk10/unk14/unk54 (ShowTextDialog args a/b/c), strText@0xb8 (CString), pParentView@0xc0, soundSession@0xc4; Ctor 0x416b90 / Run() 0x416c40 / ??1 0x427440 (Run is 0-arg, byte-match-proven) |
| InvItem | 0xc | 100 % | 0 | ✅ v15: CObject-derived (vftable/pTile/name); Ctor 0x4011d0 + CtorTileName 0x401270 (first app TU); element of World.inventory@0xa8 |
| CFile (stub) | 0x40 | 6 % | 0 | intentional — DB stub only pins Read@vtbl+0x3c; real MFC used at compile time |

**Class-modeling status (updated v15):** the old TODO list is largely resolved — `Frame`/`App`/`Dlg`
were matched from real MFC class decls in their src/ TUs (Ghidra structs never needed); "Settings" and
"GameData" were proven to be doc-TU source files (this=World), `Log_Write` a CTheApp member; `Render`
= GameView-TU methods; `TextDialog`/`InvScrollBar`/`InvItem` modeled+synced (v15). Remaining void*-this
in Ghidra is Phase-E incidental (GameView-TU helpers get typed as they're transcribed).
MFC-derived modeling recipe proven in src/Records: real base class + real members ⇒ ctor/dtor codegen free.
**Type-identity findings (2026-07-05, backported to Ghidra + Records.h): Zone.cobArray4/5 are `CWordArray`
(NOT CDWordArray — ReadZaux calls CWordArray::SetAtGrow; identical 0x14 layout so Zone::Ctor still
byte-matches, but the ctor reloc + element width differ — check genCandidateA/B in Phase D). CFile vtable:
Seek = slot +0x30 (ReadIzon seeks past mismatched records), Read = +0x3c. The Iact-script record TU at
0x418700–0x418dd0 is a Records-clone (3 CObject classes, ctor/??_G/dtor/Read each) — likely quick match
in Phase B. ReadIzon uses the same `tag[4]=0` + intrinsic-strcmp idiom as Puzzle::Read.**

### Phase plan (revised 2026-07-06 v15 — A–D done; E is the active phase)
Written to be followable without prior context: each phase lists concrete steps + done-criteria.

- **A — GameData ✅ / B — Iact ✅ / C — warm-ups ✅ / D — World/doc TU ✅ (transcription).**
  History in the PRIOR blocks + git log. What matters going forward: every TU up to and including
  the 54 KB doc TU is transcribed with per-function EFFECTIVE annotations where not exact; the
  World struct and Worldgen.h are essentially the real CDeskcppDoc header; the TU-phase dial
  (standing rules) is the dominant residual mechanism everywhere. D's non-exact functions are
  NOT open work — they re-resolve in G1's joint pass; do not re-litigate them piecemeal.

- **E — GameView TU (0x408c60–0x418700 incl. the mislabeled "Core utils" head; ~63 KB) ⭐ ACTIVE.**
  The last monster. ⚠ Open question to settle early: the head (0x408c60–0x40a560) may be a
  SEPARATE source file of the view class (the doc class had three: WorldDoc/GameData/Worldgen —
  all this=World, distinct TUs for phase purposes). Signals to check: exception-COMDAT clusters
  at candidate boundaries (each TU emits its own copies — the proven boundary marker), shared
  string/global clusters, and whether matching the head under one TU vs. two changes its dial.
  Structure the src/ tree accordingly (src/GameView/ can hold multiple .cpp files, one per TU).
  Do the steps IN THIS ORDER — each unblocks the next:
  1. ✅ **DONE v16 — OnKeyDown body repair.** Was already healed: GameView::OnKeyDown 0x4150f0
     decompiles as one function (body 0x4150f0–0x4156f1, proper return); the former FUN_004156f2
     is gone and 0x4156f2–0x415813 is now just the EH cleanup funclet (destroys the 4 stack
     CStrings at this+0x5c..0x68 + the CDialog) — comes free from C++ EH codegen, not transcribed.
  2. ✅ **DONE v16 — Header promotion.** src/GameView/GameView.h created; InvItem/Canvas/
     InvScrollBar/GameView/TextDialog moved there (Worldgen.h #includes it at the exact old
     position, preserving decl order). CODEGEN-NEUTRAL (proven: identical preprocessed tokens).
     ⚠ NEW LESSON: a token-neutral header split can STILL rotate the dial via #line/blank-line
     provenance — re-including guarded MFC headers dropped 34→33. Fix: GameView.h does NOT
     re-include them (only included after them). Keep the physical byte layout stable across a
     split, not just the tokens. 34/90 holds.
  3. ✅ **DONE v16 (intermediate) — Method-decl-set reconstruction (the dial).** Full GameView
     decl set added to GameView.h in MFC/ClassWizard structure. Overrides pinned by VTABLE DIFF
     (GameView 0x44b638 vs base CView 0x44d4ac → exactly 6 differ: ~GameView/PreCreateWindow/
     OnInitialUpdate/OnActivateView/OnUpdate/OnDraw; GetRuntimeClass/GetMessageMap/CreateObject =
     DYNCREATE+msgmap macros). afx_msg from msgmap @0x44b240 (MFC-standard sigs, //{{AFX_MSG in
     MAP order). ~35 plain helpers from a fable disasm sweep (widths from call-site pushes; the
     "(sig?)"-tagged ones need byte-proof during transcription). Result: 34/90 holds, exact bytes
     6306→6393 (gained IsItemPlaced+Randomize, lost ParseZax2+SetCurrentToIntroZone — dial
     breathing). ⚠ Two OPEN axes for the FIXED point (G1): (a) the plain-helper param widths;
     (b) AFX_MSG-MAP-order vs pure-address-order for handler decls (I used MAP order = ClassWizard
     prior; unverified). Debunked: 0x40e3f0 is the folded CView no-op default (NOT an override,
     commented in Ghidra); 0x40a560/0x411010 are embedded BalloonBitmap/BalloonButton vtables.
     Bonus: 0x417ec0–0x4186e0 = THREE embedded options-dialog classes (GameSpeed ctrl0x67 /
     Sound ctrl0x8f / Difficulty ctrl0x90, each CDialog+OnInitDialog), NOT GameView (plate-
     commented @0x417ec0). Remaining step-3 work: refine "(sig?)" helper widths as they're
     transcribed; don't grind the dial before G1.
  4. **Transcribe in .text order**, exactly like Phase D: head block first (0x408c60–0x40a560:
     Dtor, OnActivateView, OnUpdate, PlaySound, OnDraw, DrawZoneCell 0x409460, DrawZoneCellRect,
     DrawWholeZone, ZoneTransitionStep, DrawGameArea, BlitTile + the GDI COMDAT copies —
     ~CGdiObject 0x40a1a0 etc. come free from MFC usage). Then 0x40a560 onward. The 10.8 KB
     window-proc/game-loop FUN_0040b270 (OnTimer/Tick) and the enemy-AI switch (Character+0x36)
     are documented in docs/game-logic.md — transcription, not research. TextDialog::Ctor/Run
     (0x416b90/0x416c40, ~2.3 KB) and the option dialogs are embedded in this TU. Use
     `// FUNCTION: YODA 0xADDR` markers + verify.py/asmscore.py per function; annotate
     EFFECTIVE with an autopsy instead of grinding reg/slot tie-breaks (standing rule).
  5. Done when: every function in 0x408c60–0x418700 has a marker (exact or annotated), zero
     FUN_* left in the range. Expect ~90 % transcribed globally at that point.

- **F — Mop-up sweep + coverage audit (~1 week of small wins).**
  1. **Audit:** list all 534 app-region functions (Ghidra) minus every `// FUNCTION: YODA`
     marker across src/**. Everything unclaimed is either an unowned mini-TU or a Phase-G
     artifact. Known unclaimed today: the FIRST app TU (0x401000–0x401450: InvItem ctors,
     CObject no-op COMDATs 0x401060/70/80, FUN_00401090…) — likely the app's utility/collection
     source file; the InvScrollBar mini-TU (0x4085c0–0x408710, between Canvas and GameView);
     IactScript's last function if any; anything the audit surfaces.
  2. Transcribe each mini-TU Records-style (real MFC base class + real members = ctor/dtor
     codegen free; `toolchain/vc42/MFC/SRC` for message-map/vtable reference).
  3. Done when: the audit lists only lib-owned/COMDAT/thunk addresses (verify.py LIB_OWNERS).

- **G — Endgame (two sub-phases, IN THIS ORDER).**
  - **G1 — JOINT residual passes (moved here from ad-hoc; run ONLY after E).** Rationale:
    Worldgen.h is shared by the doc TU and (via the E header work) the GameView TU — every decl
    added in E re-rotates the doc TU's tie-breaks, so any joint pass run before the headers are
    final gets invalidated. After E, the decl sets are at their fixed points and the parked
    EFFECTIVE functions (doc TU 56, Records 8, Iact 8, Canvas 3, WorldDoc 6, GameData, scorers…)
    get one systematic pass each: (a) build the parallel-permuter loop (N wine workers around
    tools/permute.py --mode all, asmscore as oracle — the TODOs are listed in the permuter
    section); (b) for dial-suspect functions, the probe is cheap: extract the function into a
    one-function probe TU (see the minimal-TU recipe in tooling) — if its score is identical
    solo, the residual is header-dial, so search over REAL-decl-order variations, not function
    source. (c) UseWeapon's binding flip, the loader/saver clone rotations, and the
    imm-store-batching family are the expected big wins here. Track: effective-match bytes
    converting to exact.
  - **G2 — Whole-image build.** (1) Reproduce the link: app .objs in address order, then
    LIBCMT/NAFXCW (link 3.10-vs-4.20 flag question — notes in toolchain/README). (2) COMDAT
    geography: map which COMDATs FOLD to one copy (CObject::Serialize/AssertValid/Dump →
    first-app-TU 0x401060/70/80, referenced by every vtable) vs SURVIVE per-TU (the
    CException/CFileException dtor family at each TU head) — unresolved WHY; both behaviors are
    proven in-binary. Our TUs currently over-emit (CPen/CBrush/CGdiObject/CObject dtors in
    Worldgen that the original TU lacks) and under-emit (??_GCPalette 0x41e8b0 — find the
    CPalette odr-use between PlaceBlockades and PickItemFromZone in the original source order).
    (3) .rdata/.data layout, vtables, message maps, string pools. (4) Loose ends: 0x424fb0 bare
    jmp thunk, 0x424f69 CxxFrameHandlerThunk, PE timestamp/checksum masking. (5) reccmp-style
    final whole-image diff; progress.py → 100 %.

### Standing rules that make this work
- **Original engine bugs go in `docs/engine-bugs.md`** (verified-against-disasm defects in the
  1997 binary that our byte-exact source must reproduce — script-index clobber, missing bounds
  checks, …). Mark each reproduction site in src/ with a `// sic:` comment pointing there. New
  finds during matching get an entry; do NOT "fix" them.
- **Structs before transcription** (non-Maybe fields + calls) — the user's rule; it held for Records.
- **Cross-TU calls via stub classes** with correct arg widths/convention (see Records.h World/GameView
  stubs). Each phase PROMOTES stubs toward the real shared headers (src/ include tree mirrors the
  original project's headers by the end).
- **Fresh-TU determinism**: identical source ⇒ identical bytes; any unexplained diff means the SOURCE
  differs (shape, type width, decl presence) — hunt the construct, don't blame the compiler. Proven
  levers live in the Records/Canvas annotations (int-vs-short locals, `= -1` placement, nested `int id`
  locals, shared-return nesting, memset(0xff), tag[4]=0+intrinsic strcmp, CFile vcall CSE).
- **⭐ THE TU-PHASE DIAL (2026-07-06, biggest mechanism find yet):** the class's member-decl set in the
  TU's header ROTATES allocator/cmp-direction tie-breaks in EVERY function of the TU — and signature
  SHAPE is load-bearing (adding `int GetZoneCell(int,int)` to WorldStub.h flipped Nevada's loader to
  its jg form; each +decl combo gave different loaders/Place/FindZC/BQP outcomes). **Do NOT chase
  per-function phase with fake decls — only real methods.** The unique all-functions fixed point is
  the ORIGINAL header's complete decl set ⇒ reconstructing the real CDeskcppDoc class declaration
  (all ~200 methods, right order) is a first-class Phase-D goal. Also explains the Records Tile::Ctor
  flip and the original binary's own loader jg/jl/jg drift. A function byte-exact under one dial but
  not the current one = annotate `PHASE-DISPLACED` (source proven correct), not a source miss.
- **Byte-diff numbers lie once lengths diverge** — use verify.py per-function + capstone diffs against
  the TRUE original extent (funclets + EH stubs included); asmscore for reg-vs-structure triage.
- **Verification traps (proven 2026-07-06):** (a) always `rm <TU>.obj` + fresh `toolchain/bin/cl`
  manually before measuring — verify.py can silently read a stale .obj; (b) verify.py/match.py
  positional pairing MIS-PAIRS identical-length clone families (the loader/saver triplets) — use
  per-NAME COMDAT diffs (match.coff_functions, substring the mangled name) as clone ground truth;
  (c) COMDAT trim length INCLUDES EH funclets — slicing exe[addr:addr+L] is garbage for EH functions
  or any length-shifted body; compare main-body-to-main-body (split at first ret) via capstone.
- **Agents for RE sweeps, main thread for matching.** Reader-analysis naming sweeps parallelize well
  (see the MapEntity/Puzzle sweep); matching iterations don't.
- **Milestones** (progress.py %exact + transcribed): 7.02 % after A; 13.52 % mid-D (v7);
  **59.54 % transcribed / 13.78 % exact late-D (2026-07-06 v12)**;
  **73.63 % transcribed / 15.14 % exact, doc TU fully transcribed + Ghidra synced (2026-07-06 v15)**;
  **73.63 % transcribed / 15.21 % exact — Phase E steps 1-3 (GameView.h promoted + full
  method decl set reconstructed) (2026-07-07 v16)**;
  **76.50 % transcribed / 16.03 % exact — Phase E step 4 begun: GameView TU head block
  0x4084f0–0x409460 (10 exact + 5 eff) (2026-07-07 v17)**;
  **76.86 % transcribed / 16.31 % exact — Phase E step 4: DrawZoneCell EXACT +
  DrawZoneCellRect/DrawWholeZone eff. (11/18 GameView markers) (2026-07-07 v18)**;
  **82.07 % transcribed / 16.71 % exact — Phase E step 4: ZoneTransitionStep→FireWeaponStep
  block done, 19/31 GameView markers (2026-07-07 v19)**;
  **89.09 % transcribed / 16.84 % exact — Phase E step 4: DrawEntities EXACT +
  FindEntityAt eff. + the 10.8KB Tick transcribed (align 3588→2202), 22/34 markers
  (2026-07-07 v20)**;
  ~90 % after E, 100 % = G's whole-image build. Track effective-match bytes separately
  (they count for G, not for %).

### 📋 SESSION PROTOCOL (follow this shape every session)
1. **Orient:** read the ⏭ NEXT SESSION PICKUP block below; `cd src/<TU> && rm -f <TU>.obj &&
   ../../toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS <TU>.cpp`
   then `python3 tools/verify.py src/<TU>/<TU>.cpp` from repo root — confirm the recorded
   exact-count reproduces BEFORE changing anything (if not, a header drifted; bisect first).
2. **Ghidra check:** `curl -s localhost:8089/list_open_programs` → `current_program` must be
   `YodaDemo.exe` before ANY write (renames/comments/struct edits hit the ACTIVE program, and
   `program=` is honored only for reads). If not active, queue the writes in the pickup block
   instead of writing.
3. **Work loop per function:** read the ORIGINAL disasm first (Ghidra `disassemble_function`,
   dump to a tmp file); transcribe idiomatically; compile fresh; `tools/asmscore.py <TU>.cpp
   0xADDR [--dump]` (dump columns: LEFT = original, RIGHT = ours). `align>0` ⇒ structural — hunt
   the source construct (the lessons lists). `align=0, reg_pen>0` ⇒ allocator tie-break — do NOT
   grind; annotate `// EFFECTIVE` with a short autopsy and move on (standing rule). Never trust
   a raw byte-diff once lengths diverge.
4. **Triaging a stubborn residual:** (a) probe source knobs CHEAPLY (decl order, operand order,
   guard shapes — one compile each, revert losers); (b) the minimal-TU probe: copy the one
   function + `#include "<TU>.h"` into a probe .cpp, asmscore it — identical score solo means
   the residual is header-dial/intrinsic, different means TU-position; (c) then park with the
   probe results in the annotation. ~30 min per function max before parking.
5. **Session end (do ALL of these):** update the ⏭ pickup block (new findings → instincts,
   done items removed, next steps concrete); demote the old pickup to a condensed ⏮ PRIOR;
   update the TU/struct tables + milestone line if they changed; sync any new struct
   fields/renames to Ghidra if it's ACTIVE (else list them as PENDING in the pickup);
   `save_program`; commit with a descriptive message. Run `python3 tools/progress.py` for the
   milestone number.
6. **Agents:** use them for read-only RE sweeps (naming/xref surveys); keep matching iterations
   in the main thread (they're serial compile-and-look loops).
7. **Escalation:** if running on a smaller model and a function resists after the step-4 triage
   (e.g. a novel codegen mechanism, an EH/switch-layout open, or a suspected new dial axis),
   spawn a `fable`-model agent (Agent tool, `model: "fable"`) with the disasm + current source +
   the relevant lesson numbers rather than burning compiles guessing. The lessons lists (KEY
   codegen 1–14, the per-version crack lists) are the shared vocabulary — cite them by number.

### ⏭ NEXT SESSION PICKUP (2026-07-07 v20 — Phase E step 4: Tick transcribed; 16.84% exact / 89.09% transcribed)
**v20 session results (committed). src/GameView/GameView.cpp = 20 exact + 8 effective +
6 COMDAT / 34 markers, contiguous 0x4084f0–0x40d470(excl).**
- **DrawEntities 0x40b160 EXACT** — countdown recipe (`int i=0; int n=nCount; do{...i++;n--;}
  while(n!=0)` under `if (nCount>=1)` + inner `if (n>0)` DOUBLE guard); the SetTile val arg
  needed `int nFrame = pChar->currentFrame;` between the calls (movsx-before-y/x-loads) with
  `(short)nFrame` at the site. **FindEntityAt 0x40b210 EFFECTIVE** (35/35, align=0, 6B): decl
  order pZone/nCharId/n/i load-bearing (12 perms probed, 3 tie at align=0); result var is a
  16-bit AX-resident `short nCharId = -1` (mov ax,0xffff at head); residual = one 2-reg
  walker/pEnt cycle (removing the pEnt local is WORSE — real local).
- **Tick 0x40b270 (10.8KB) TRANSCRIBED, EFFECTIVE-WIP** (align 3588→2202; ~2350 real insns vs
  2336 — +154 phantom rows are our 12 inner-switch jump tables at the COMDAT end disassembling
  as zeros). Full autopsy in-source. ⭐ NEW LAYOUT MECHANISMS (the session's big find):
  (a) **cl 4.2 defers any block ENDING in an unconditional transfer and prefers fall-through
  continuity**: `if (c) goto L;` with L unemitted ⇒ cl INLINES L's block as the fall-through
  and defers the source fall-through to after the current chain (the dead-entity cleanup =
  deferred fall-through of `if (*pActive != 0) goto ALIVE_ENT;` — reproduces the orig exactly;
  same mechanism as WorldDoc GetLocatorIcon's sunk early-return bodies).
  (b) **An arm that FALLS THROUGH stays inline**: case 11's random-step block is the labeled
  THEN-arm of an `||` if/else falling into the probe loop (backward `goto RANDOM11` from the
  walkable test re-enters it) — align dropped 180+ from this one restructure.
  (c) **Switch comparison-trees survive only with duplicated bodies**: `case -1: case 0:` and
  `default:` need SEPARATE (textually duplicated) reset bodies or cl folds the tree to one
  `cmp 1;jne` (4 probe-result switches; the duplicates then partially cross-jump like the orig).
  (d) Walkable-check geography: case 1's `t != -1 ⇒ zeros` stays INLINE (falls to the retreat
  counter); cases 2/3/6/7/8/9/10 spell `if (t != -1) { nDX=0; nDY=0; break; }` and cross-jump
  into ONE shared block (= case 12's body). (e) **GetTile is SIGNED here**: `short t = GetTile(...)`
  temps give the orig's `cmp ax,0xffff` (the ushort decl in RecordClasses.h made `== -1`
  dead code — cast/temp at every site; v18 DrawZoneCell lesson generalizes).
- **⚠ TWO GLOBAL RESIDUAL FAMILIES (park, G1):** (1) cmp-DIRECTION mirror on ~40
  entity-vs-player compares (`cmp [slot],reg`+inverted jcc vs orig `cmp reg,[slot]`; forces
  re-cmp where the orig reuses flags in sign chains) — correlates with our frame layout being
  +4-shifted (pEnt slot 0x1c vs orig 0x18); one global tie-break. (2) reg-role rotation in the
  bullet/erase blocks (pBY/pBX/pBDY/pBDX = EBX/EDI/EBP/ESI in orig). PLUS the parked FIRE/SHOOT
  block placement (7 source shapes probed incl. full duplication — see the in-source autopsy;
  do not re-grind without a new theory).
- Literal-constant finds: `>= 6` (case-11 abs), `>= 14` (case-1 counter), `<=` in case-10's
  3-arm signs, `pEnt->timer <= 0` (weapon gate) — always mirror cmp-constant+jcc from disasm.

**▶ START HERE (v21): more Tick polish is G1 fodder — move ON. Next in .text order:**
1. **OnTimer/UpdateFrameMaybe 0x40d470** (the actual per-frame driver: Tick×5 + CyclePalette×6
   + DrawGameArea×4 + win/lose via abortFrame; docs/game-logic.md has the frame-mode table),
   then StepDetonatorEffect 0x40e400, ApplyHotspotCamera 0x40e500, TransitionZoneScript
   0x40e750 (byte-prove its "(sig?)" unused arg1), TransitionZoneXWing 0x40e7c0,
   TransitionZoneDoor 0x40e9d0, ReenableHotspotObjects 0x40ebe0, DrawObjects 0x40ec30.
2. Tick leftovers for G1 (not now): fire-block placement, cmp-direction family, case-10 3-arm
   jump-to-then (je vs jne), tail SetTile arg scheduling, loop-increment order, bTurned slot.
3. **Open items (carried):** InvScrollBar ??_G/??1 dtor COMDATs (0x408690/0x4086b0) PARKED;
   options dialogs (0x417ec0–0x4186e0) + TextDialog ctor/Run embedded later in this .cpp;
   World.unk50 → nCurrentZoneIdMaybe rename (Worldgen.h+WorldDoc.h+Ghidra together + 4-TU
   re-verify); Ghidra BlitTile prototype still __fastcall-confused (queue: void __thiscall
   (GameView*, short, short, int, Tile*)); FireWeaponStep's flags-test axis (v19, 4 sites).
4. **G1 dial axes:** ZTS↔WES parity crossing; DrawZoneCellRect/DrawWholeZone rotations;
   FireWeaponStep flags-test + erase-block shapes; Tick's cmp-direction + fire-block + reg
   roles; plain-helper param widths; AFX_MSG map-order. JOINT pass AFTER E.
5. **Re-verify Worldgen after ANY GameView.h/Worldgen.h edit** (one compile + verify.py).

### ⏮ PRIOR (2026-07-07 v19 — ZTS/WES/FireWeaponStep block, condensed)
ZTS 0x409650 eff. (441/441, align=48; no-`x`-local CSE-temp lesson; span IS a local); WES
0x409c10 eff. (349/349, align=8, pure parity crossing vs ZTS — loader-triplet family, G1;
ZTS's GetDC-via-reg + nStep→BX head VINDICATED by WES). Engine bug #13: both step-10 arms
read the IactRun mask uninitialized when skipped. DrawGameArea 0x40a200 EXACT (separate
COLORREF/DWORD locals; redundant ||-term = dev code). IsUsableTileMaybe 0x40a620 EXACT
(66-case range-folded switch). BlitTile/DrawTileAt eff. (reg-role; BlitTile sig byte-proven
`(short y, short x, int nUnused, Tile*)`; sx/sy hoisted locals mandatory). FireWeaponStep
0x40a710 eff. (820/828): int nWeaponTile local + conditions re-mention frames[7]; nStep==0
duplicated into both head conditions; ⚠ flags-test axis PARKED (every spelling narrows
`test eax,0x60000` to `test byte [pT+0x406],6`; orig uses the WIDE form here, narrow in
Detonate). 6 GDI COMDATs (CGdiObject/CBitmap trios) byte-match marker-only; verify.py now
honors explicit mangled hints over LIB_OWNERS. World+0x2e44 = bWeaponHitPendingMaybe.

**Ghidra write recipes (v15-verified, keep):** `run_script_inline` = POST JSON
`{"code": "..."}` built with json.dumps (literal newlines in hand-written JSON break the
parser); NO import statements — fully-qualify every Ghidra class in the Java snippet;
struct field over an existing named field: `replaceAtOffset` works 1-for-1, but growing a
RECT over four ints needs `getComponentAt(off)` + `clearComponent(ordinal)` for each byte
range FIRST (it won't consume neighbors); renames into class namespaces:
`f.getSymbol().setName(...)` + `f.setParentNamespace(symbolTable.getNamespace/createClass)`
— this also auto-retypes `this` when a same-named Structure exists; clear stray params with
`f.replaceParameters(DYNAMIC_STORAGE_ALL_PARAMS, true, USER_DEFINED, new Parameter[0])`;
finish with POST `save_program`. The compile-error noise from old *.java files in
~/ghidra_scripts is expected — those 4 scripts (MergeEhFunclets/ParentGapFunclets/
CreateGapFunctions/FillFunctionHoles) are REAL body-repair tools kept for Phase E step 1.
### ⏮ PRIOR (2026-07-07 v18 — DrawZoneCell trio, condensed)
**DrawZoneCell 0x409460 EXACT (361B)**, sig `void DrawZoneCell(short x, short y)` (Ghidra ABI
confusion corrected in DB). Cracks: (a) hoist `x<<5`/`y<<5` into `short sx,sy` locals (persistent
ESI/EBX residency — lesson #13 inverse); (b) tile id is a SIGNED short (`(short)GetTile(...)` ⇒
movsx; -1 = empty); (c) the bounds-guard inlines `pWorld->currentZone` INSIDE the `||` after the
x<0 term (lazy short-circuit load + CSE across width/height/layer-0; a statement-form pZone hoists
the load above the branch — a STEERABLE lazy-load knob, pairs with duplicated-call-arms). Engine
quirk reproduced: valid x is [0,width) but valid y is [0,height]. DrawZoneCellRect + DrawWholeZone
EFFECTIVE (pure reg-role; DrawWholeZone was exact under the prior dial, PHASE-DISPLACED by
DrawZoneCell's CSE form — G1).

### ⏮ PRIOR (2026-07-07 v17 — Phase E step 4 head block, condensed)
src/GameView/GameView.cpp created; HEAD BLOCK 0x4084f0–0x409460 = 10 exact + 5 eff. SINGLE-TU
settled: NO exception-COMDAT cluster in 0x4084f0–0x418700 + InvScrollBar ctor/dtor interleaved
between GameView's DYNCREATE statics and GameView::GameView ⇒ ONE .obj (head is NOT a separate
source file). EXACT: CreateObject/GetRuntimeClass (IMPLEMENT_DYNCREATE), both GetMessageMap (two
BEGIN_MESSAGE_MAP in MAP order), MusicThreadProc, InvScrollBar::Ctor, ~GameView, ??_GGameView,
OnActivateView, InvScrollBar::OnHScroll. EFFECTIVE: GameView ctor (imm/reg store-scheduling),
OnUpdate (block-layout), OnDraw (const-0 in EDI), PlaySound (EAX/EDX swap), OnVScroll. Cracks:
switch(x) not if/else-if for 0/1/other dispatch (test-0/cmp-1/jmp-default); declaring `int *p=arr`
AFTER a call lets the scheduler fold `mov esi,offset` into that call's pushes; CBitmapButton members
ARE real MFC (CButton+4 CBitmap=0x5c) — "BalloonButton/Bitmap" was over-analysis; strCheatBuffer=
CString, wndDialogText=CEdit (implicit member ctor/dtor free). GameView.cpp globals wired
(g_bStopMusicThread@0x456134, g_hWaveMixEvent@0x459454, g_waveHandles[64]@0x459458, MIXPLAYPARAMS).

### ⏮ PRIOR (2026-07-07 v16 — Phase E steps 1-3, condensed)
GameView.h promoted out of Worldgen.h (InvItem/Canvas/InvScrollBar/GameView/TextDialog); it does
NOT re-include MFC/Records (a token-neutral split still rotates the dial via #line/blank-line
provenance — keep the physical byte layout stable, not just tokens). Full GameView method decl set
reconstructed: overrides pinned by VTABLE DIFF (GameView 0x44b638 vs base CView 0x44d4ac = exactly 6
differ: ~GameView/PreCreateWindow/OnInitialUpdate/OnActivateView/OnUpdate/OnDraw); afx_msg from
msgmap @0x44b240 in MAP order; ~35 plain helpers from a fable disasm sweep (some widths "(sig?)"
unproven). Debunked (all plate-commented in Ghidra): **0x40e3f0 = folded CView no-op DEFAULT, not an
override**; 0x40a560/0x411010 = embedded BalloonBitmap/BalloonButton vtables (slot 73 terminates
GameView's); 0x413be0 = EmptyFrameHookMaybe (real empty method from OnTimer); **0x417ec0–0x4186e0 =
THREE embedded options-dialog classes** (GameSpeed ctrl0x67 / Sound ctrl0x8f / Difficulty ctrl0x90;
each CDialog+OnInitDialog @0x417f50/0x418230/0x418510 + ??_G + msgmap) — model as mini-classes in
step-4/F. OnKeyDown 0x4150f0 healed (0x4156f2+ = its EH cleanup funclet, comes free).

### ⏮ PRIOR (2026-07-06 v14 — Phase D COMPLETE-TRANSCRIBED: 90 markers, condensed)
**src/Worldgen = 90 markers covering 0x41bee0–0x429150 — every function of the doc TU
including the whole GameView tail block (OnInitialUpdate, DrawDirectionArrows,
ShowTextDialog EXACT, ??1TextDialog, DrawHealthDial/Needle, AddHealth, UseWeapon,
DetonateAdjacentTiles, OnCmdMinimize EXACT, DrawWeaponBox/Icon, BlitViewportDither,
PreCreateWindow EXACT, AddItemToInv; plus the v13 World half). GameView (0x310) +
TextDialog (0xc8, NOT CDialog-derived) + InvScrollBar (0x44) + InvItem (0xc) fully
modeled in Worldgen.h; Zone gained DamageEntityAt/HitEntityAt decls (Records TU
re-verified 25/33 ✓; Iact breathed 2→1 exact, its 8 annotated tie-breaks unaffected).**

**v14 net-new cracks (fold into instincts):**
- **Duplicated-call arms are EVERYWHERE in this dev's code**: LoadIcon per arrow arm,
  IactRun + flag set/clear per UseWeapon arm, GetSysColor+GetNearest+Fill per weapon-box
  arm — write the FULL call in each arm; the compiler cross-jumps the common tail leaving
  per-arm constant/coordinate pushes. Value-ternaries on adjacent constants go BRANCHLESS
  (sbb/add) even via pointer-typed locals or if/else — when the orig has branchy push-imm
  arms, the CALL is in the arms, period.
- **VC4.2 jumps TO the then-arm in value-assign if/else** (DrawDirectionArrows needed
  `== 0` disabled-icon-first) but in statement-arm if/else around calls the layout is NOT
  source-steerable (weapon boxes, AddItemToInv scrollbar — both spellings identical).
- **BOOL fall-off = C2561 hard error**: PreCreateWindow matched via `BOOL bRet = base();
  cs.style |= ...; return bRet;` — the result rides EAX across mem-ops for free
  (PlaceItemOnLock family).
- **`AfxGetInstanceHandle();` as a bare statement** = the recurring dead
  AfxGetModuleState call (result load dropped, call kept) — OnInitialUpdate/DrawDirection-
  Arrows both.
- **evaluate-callee-first locals**: `CFrameWnd *pFrame = GetParentFrame();` before
  PostMessage (call-before-pushes = a local, not an inline arg).
- **A guarded `new Canvas(w,h)` shape proves ctor-hood** — Canvas TU's "Init" 0x407df0 is
  really Canvas::Canvas(int,int) (stub decl added; Canvas TU rename pending).
- **Struct-copy RECTs**: `rc = pWorld->rectArrowBox; rc.left -= 4;` (4-dword copy + edits).
- **movsx-immediately int for GetTile results** (Detonate hit align=0 with it) and NEVER
  cast to short at DrawZoneCell call sites — but UseWeapon's DrawZoneCell args ARE
  short-arithmetic ((short)x + sdx*2 with short sdx/sdy locals): read each site.
- **The early-return dtor block lesson holds in the view code too** (DrawHealthNeedle's
  `if (nLo == 0) return;`).

### ⏮ PRIOR (2026-07-06 v13 — condensed; the save/load session, lessons still in force)
**Session result (commits 5ad13c3..04458c4): World-half of the TU finished — LoadWorldState-
File + Serialize (EFFECTIVE DIFF-2 each), DrawLocatorMap + DrawRect (EFFECTIVE), OnNewWorld
(EXACT), OnSaveWorld/OnLoadWorld (EFFECTIVE-WIP, autopsies in-source), ??_GCProgressCtrl
(MATCH via marker only — our TU already emitted it).**
- **.wld save format ("YODASAV44")**: seed/planet/unk33b8; quest-item word lists (the LAST
  element re-seeds nCurrentGoalItem + startItem/startItem2 from Puzzle.itemA/B on load);
  center-2x2 quest cells (mapScratch when unk33b8==0, else mapGrid[44..]); full 10x10
  (mapGrid vs mapGridBackup by the same flag) as 15-field cell dumps; -1,-1-terminated
  SaveZoneRecursive/LoadZoneRecursive streams; inventory as tile ids (re-NEWed InvItems);
  player/weapon(char index + unk48 ammo)/camera/health/difftime-elapsed tail; unk248 saved
  as count+SUM, rebuilt as count copies of the AVERAGE.
- **&field pointer locals are REAL source** (`int *pHealth = &healthLo;` — the lea+spill+
  deref pattern; OnSave/OnLoad cache gameState/nFrameMode/bStartingGame/&pWorld the same
  way; OnInitialUpdate proved plain field writes ALSO produce compiler-made caches — write
  plain first, add the pointer local only when the lea+slot shape demands).
- **Never Read(&i,4) into a live loop counter** — taking its address memory-homes it
  TU-wide and wrecks reg-alloc; the original uses fresh x/y pairs per sentinel loop.
- **OnSaveWorld/OnLoadWorld shapes**: unk33b8 selectors are `!= 0` grid/backup-arm-FIRST
  (all sites); recursive-save blocks materialize `MapZone *pCell` (base-folded [reg+4] id
  reads); DoModal success arm = the if-body fall-through; story-history planet dispatch is
  a SWITCH with per-arm vGoal/pArr temps + one cross-jumped SetAtGrow tail (SelectPuzzle's
  planet dispatch is a LADDER — always read the disasm); Open-fail switches: OnSave 9/7/9,
  OnLoad all-8 in the same three groups.
- **CFileDialog**: needs <afxdlgs.h> (afxwin.h only fwd-declares — silent C2228/C2541 on
  members otherwise); m_ofn.lpstrInitialDir = World.lpszSaveDirMaybe@0x33bc stored BEFORE
  the pDlg null check in BOTH dialogs (sic, #8 family); save flags 0x80006 "wld"/
  "savegame", open flags 0x1006 "*.wld" + Flags &= ~OFN_SHOWHELP(0x10); GetPathName chain:
  `strPath = pDlg->GetPathName().GetBuffer(200);`.
- **inc-vs-add-with-CSE'd-reg**: a 2-byte instruction-selection family (orig
  `add [nDone],ecx` reusing ECX=1 from neighboring =1 stores; ++/+=1/n=n+1 all inert).
- **DrawRect (free __stdcall, bevel)**: per-use strength-reduced IVs = copy-variable named
  locals (`int y2 = y1;`) in source; edge-4 decl order x1,x2,nBottom,y1-LAST aligned the
  pRect reload (align 26->8); explicit `int n = nThickness; do{..n--}while(n)` countdowns.
- **asmscore best-fit trap**: free functions (no `Class::` after the marker) fall back to
  global best-fit and can silently mispair (OnToggleSound stole DrawRect) — score by
  explicit COMDAT name via match.coff_functions, or add `(?FuncName@)`-style marker hints.
- **World vtable base = 0x44c438** (GetFirstViewPosition=+0x68 anchor): +0x58 SetTitle-4?
  ... +0x60 IsModified, +0x64 SetModifiedFlag, +0x68/+0x6c GetFirstViewPosition/GetNextView,
  +0x70/+0x74 OnChangedViewList/DeleteContents, +0x78/+0x7c OnNewDocument/OnOpenDocument
  (app overrides), +0x80 OnSaveDocument, **+0x84 OnCloseDocument** (OnNewWorld's 0-arg
  vcall — plain C++, the "GetFile bug" theory was retracted), +0x88 ReportSaveLoadException,
  +0x8c GetFile (0x441f5d), +0x90 ReleaseFile. RecordDataFileOwner is #ifdef _MAC (absent).

### ⏮ PRIOR (2026-07-06 v10 FINAL — Phase D: 63 markers; 14.86% exact, 48.72% transcribed; Ghidra World struct fully synced)
**State: src/Worldgen = 63 markers (all 9 placers + the 3 gap queries transcribed); global
14.86% exact + 33.86% partial = 48.72% transcribed. Session commits: 8d8402b (gap + queries +
5 placers + enums), then the World.h consolidation commit. Worldgen exact-count breathes with
the dial (29->26 after the scorer decls landed in Worldgen.h — bytes UP 5267->5588); do not
grind, the per-function EFFECTIVE annotations carry the autopsies.**

**▶ START HERE — the hub is now mechanical:** WorldgenPlaceQuestNodeMaybe 0x41f120 (2KB) —
its ENTIRE callee set is transcribed (SelectPuzzle excepted). Then SelectPuzzle 0x41eab0,
CarveQuestPath 0x41d940, PlaceBlockades 0x41e350, Generate 0x41f960 (6.6KB), the save/load
monsters (OnSaveWorld/OnLoadWorld/Serialize/LoadWorldStateFile — CArchive+CATCH_ALL, WorldDoc
OnOpenDocument recipe), then the GameView methods 0x426c40-0x429150.

**✅ GAP 0x41bee0-0x41c340 SOLVED (v10):** the Worldgen TU actually STARTS at 0x41bee0, not
0x41c340. Contents: ??1CException 0x41bee0 + ??_GCException 0x41bf30 (vftable 0x44d064) and
??_GCFileException 0x41c180 + ??1CFileException 0x41c340 (vftable 0x44d2b4; CString
m_strFileName@+0x10 destroyed via ~CString 0x43d4e9, base ~CException) — the LINKED MFC
COMDAT copies (lib code at 0x4294xx calls them too; verify.py LIB_OWNERS filters them, no
source needed). Interleaved with them sit the TU's first 3 source functions, all transcribed:
ZoneHasIzxItemMaybe 0x41bfa0 (bool twin of ZoneFindInIzxList, cobArray4/5 by sel),
ZoneRequiresItemMaybe 0x41c0b0 (genCandidateA/IZAX), PickUnplacedItemMaybe 0x41c200 (random
genCandidateB item not in the dedup set). All renamed + plate-commented in Ghidra.

**v10 placer results:** PlaceUsefulDropChainMaybe 0x41cbe0 was byte-EXACT (then
PHASE-DISPLACED by later decls — source proven); AssignTransitItem 0x41d480 align=12;
LockChain 0x41d0c0 align=56; PlaceUsefulObject 0x41d260 align=80; PopulateGoalZone 0x41c8f0
align=92 (annotated EFFECTIVE-WIP). **New cracks (add to instincts):**
- **In EH functions, early-return guards SHARE ONE dtor+return-0 block** emitted as the FIRST
  guard's fall-through; later `return 0`s cross-jump BACK to it. Write guards as separate
  early returns, NOT nested ifs (AssignTransit 176->12). In non-EH functions each `return 0`
  gets its own epilogue copy (PopulateGoalZone/LockChain) — EXCEPT when the original wrote one
  `if (a || b || (p = ...) == NULL || ...) return 0;` ||-chain with embedded assignments =
  ONE shared return-0 (PlaceUsefulObject 170->80; Ghidra's comma-expr rendering is literal).
- **`if (sel != 0)`-first arm order** (A-arm fall-through) cracked DropChain to EXACT; but
  LockChain needed `== 0`-first — mirror the JE/JNE from disasm per function, never assume.
- **Params used at 2+ sites CSE-spill on their own** — do NOT invent `int nA = iA;` locals
  (PopulateGoalZone 108->92 from deleting them). `int v = wordArray.GetAt(i)` (int, not
  ushort) is what hoists the xor zero-extend out of a loop.
- Engine bug #11 (docs/engine-bugs.md): LockChain's failure path removes item1a TWICE, never
  item2. Plus two always-true `>= 0` guards on zero-extended WORDs (bug-#10 family).

**⭐ ENUMS (new standing rule, user directive — see memory/prefer-enums-over-comments):**
magic field values get NAMED ENUMS, not comments. ZoneType (map_flags roles), ZoneObjType
(OBJ_TYPE), TileFlags now live in src/Records/RecordClasses.h AND the Ghidra DB (fields
Zone.type/ZoneObj.type/Tile.flags typed with them; decompiles now print OBJ_DOOR_IN etc.).
GOTCHA: modify_struct_field_type clobbers the field NAME — restore with modify_struct_field
using field_name="offset:0xN". HTTP writes need JSON bodies (form-POST returns "address is
required"); key is "function_address" for renames, "address" for plate comments.

**✅ GHIDRA WORLD-STRUCT SYNC COMPLETE (v10-FINAL — do not redo):** the Ghidra `World`
(0x33c0) now mirrors Worldgen.h exactly: m_bModified@0x44, the full 0x54-0x7c block
(score/unk74/timeBase/timeOffset...), **tiles/zones/inventory/characters/puzzles as real
CObArrays** (the old exploded tileArray/zoneObjects/puzzles-as-Puzzle** fields are GONE —
decompiles now render `(this->zones).m_pData[i]`, matching our GetAt-inline idiom),
questItemsA/B (Maybe dropped — semantics proven), **uniqueRequiredItemsMaybe@0x234** (named
from AssignTransitItem: one-shot dedup of single-IZAX required items; renamed in Worldgen.h/
WorldDoc.h/Worldgen.cpp too — codegen-neutral, 26/62+4/6 verified), unk248@0x248, worldgen
CObArray lists@0x25c/0x270, apZoneGrid Zone*[100]@0x2d0, apUiTiles Tile*[20]@0x460,
**mapGrid/mapGridBackup/mapScratch as MapZone[100]/[100]/[4] @0x4b0/0x1900/0x2d50** (vptr-TRUE
anchoring — Ghidra's MapZone was rebuilt to the ctor-proven layout: vftable@0, id@4,
zoneType@8 typed with the ZoneType enum), pSysColorTable@0x326c, 3 RECTs@0x3274, and the whole
0x32a4-0x33b0 tail. GameView struct: NOT touched this pass (Phase-E prep as planned).

**⚠ STRUCT-EDIT GOTCHAS learned the hard way (v10-FINAL):**
- **`recreate_struct` force IGNORES field offsets** (packs sequentially from 0) AND its
  naming filter auto-prefixes (id→nId, score→nScore). NEVER use it for offset-precise
  structs. **`remove_struct_field` on packed structs DELETES the bytes and SHIFTS everything
  after** (it silently shrank World 13248→11770 mid-edit).
- **The reliable tool is `run_script_inline`** (POST JSON key `"code"`, JAVA source injected
  into a GhidraScript): `Structure.deleteAll(); growStructure(size)` (deleteAll leaves a
  notional length-1 — re-grow to target), then `replaceAtOffset(off, dt, len, name, comment)`
  per field. Honors offsets and exact names. Old broken *.java files in ~/ghidra_scripts
  produce compile-error NOISE in every run's output — ignore them, check for your println.
- **Force-recreating a struct broke ~24 World-namespace functions' conventions** (this-typing
  degraded to __fastcall(int)); swept back to __thiscall via script over
  fm.getFunctions + setCallingConvention, then cleared leftover spurious EDX params on the
  void methods. CreateObject 0x419ed0 / FUN_00419f50 are genuinely __stdcall DYNCREATE
  statics — do NOT thiscall them. The 4 exception dtors were moved OUT of World into real
  CException/CFileException class namespaces (rename_function_by_address does NOT parse `::`
  — it had made flat "CException::Dtor" names inside World).
- HTTP writes: JSON bodies only; rename key = "function_address", plate key = "address";
  modify_struct_field addresses unnamed fields as field_name="offset:0xN" (its renamer also
  auto-prefixes: probe→nProbe).

**World.h consolidation (user directive, v10):** src/World/World.h DELETED — the scorers TU
(src/World/World.cpp) now includes ../Worldgen/Worldgen.h (shared World facade; score/
timeBase/timeOffset/gameState/etc. fields filled in from WorldDoc.h's ctor-proven names, six
scorer method decls added as cross-TU section). The old local "Zone" 0x34 struct was really
MapZone shifted by 4 (exists==id, field18==flagSolved, field20==flagA, field24==flagB).
Scorers stay 4/6 exact: the indexed `mapGrid[n].field` form keeps the lea anchor at
this+0x4b4 (grid-copy recipe) — CalcCompletionScore is now PHASE-DISPLACED (pure 3-reg
rotation, walker-temp rank not source-steerable), CalcTimeScore matches. Long-term: Worldgen.h
IS the emerging real CDeskcppDoc header — keep growing it with real decls only; GameData/Iact
still use their own stubs (consolidating those = endgame dial re-verification).

### ⏮ PRIOR (2026-07-06 v9 FINAL — condensed; recipes still in force)
**State: src/Worldgen = 54 markers, 27 exact; global 14.10% exact + 31.97% partial = 46.07%
transcribed (progress.py now prints both tiers; PARTIAL = has marker+COMDAT but not byte-exact).
Session commits: 7961680 (3 parsers + dispatchers), ac69ef0 (PlacePuzzle WIP + LogWrite id),
8199d58 (PlacePuzzle effective + WorldgenPlacePuzzles), 36c3781 (4 placers), then progress.py
partial tier + Ghidra sync. All .obj files current; the jump-table probe battery (sw*.cpp)
lived in the job tmp dir — DISPOSABLE, its conclusions are in Load's in-source annotation.**

**▶ START HERE — remaining 5 placers (0x41c8f0-0x41d660), then the hub:**
PopulateGoalZone 0x41c8f0 (752B), PlaceUsefulDropChainMaybe 0x41cbe0 (480B),
PlaceItemForLockChainMaybe 0x41d0c0 (416B), PlaceUsefulObjectMaybe 0x41d260 (544B),
AssignTransitItemMaybe 0x41d480 (480B). **These are exactly PlaceQuestNode's callee set** —
the 2KB hub (0x41f120, 94 blocks, cc=86, already declared in Worldgen.h) calls:
SelectPuzzle(Maybe), IsZoneUsed, AddZoneEntry, PickItemFromZone, AssignTransitItem,
PlaceUsefulObject, PlaceUsefulDropChain, PlaceItemForLockChain, ShuffleList,
PopulateGoalZone, **FUN_0041c0b0 (UNNAMED — identify first!)**. So: finish the 5 placers +
0x41c0b0, then PlaceQuestNode becomes mechanical, then SelectPuzzle 0x41eab0, CarveQuestPath
0x41d940, PlaceBlockades 0x41e350, Generate 0x41f960, save/load monsters, GameView methods.
**⚠ UNCLAIMED GAP 0x41bee0-0x41c340** (between WorldDoc TU end and Worldgen TU start):
0x41bee0 = WorldDoc's own CFileException-dtor COMDAT copy (proof the linker does NOT fold
these across TUs — each TU's copy survives); 0x41c0b0 = the unnamed PlaceQuestNode callee;
map what else is in there before assuming TU boundaries.

**Placer-family recipes (4/9 done — FillQuestItemSpot 0x41c580 / FillSpawn 0x41c730 clone
pair, FillQuestItemSpot2Maybe 0x41cf10, PlaceItemOnLock 0x41cdc0; all EFFECTIVE 20-90B,
pure reg-role/cmp-mirror tie-breaks):** local CWordArray spot-list + rand-pick
`objects.GetAt(paSpots.GetAt(rand() % n))`; DOOR_IN (type 9) recursion tail-loop; guard+
do-while UP-count loops (calls inside kill the countdown transform); `if (zoneId < 0)
return 0;` early form; `short v` 16-bit local for the candidate value; FillQuestItemSpot2:
conditionally-SCOPED CWordArray inside `if (bFound)` + in-condition recursion assignment
`(nResult = recurse(...)) == 1`. ⭐ **PlaceItemOnLock crack: `int nResult = 0` lives in EAX
END-TO-END** — entry xor eax,eax; `nResult = 1` in the success arm; in-condition recursion;
`return nResult` costs ZERO bytes everywhere. VC4.2 hard-errors C2561 (no return at all) and
C2202 (a path falls off) — so a "void-looking" original whose callers test EAX ALWAYS has an
EAX-resident result var. PlaceItemOnLock's sel!=0 arm (cobArray5) is the fall-through.

v9 mid-session delta (PlacePuzzle/WorldgenPlacePuzzles), kept for the recipes:
- **PlacePuzzle refined to EFFECTIVE (39B, insns 255/255)** — the in-source annotation lists
  three NEW STRUCTURE RECIPES that cracked it: (a) hoisting `int nIso = GetSize()` re-keys
  ARRAY frame slots (use-count driven!) and yields mov+test/idiv-reg; (b) the pick is
  sequential-if + `goto cleanup` in the far arm (only shape where the far store cross-jumps
  into the B-arm tail while the log arm falls through dead re-tests); (c) **the delete/scan
  countdown recipe**: `int i = 0; int n = GetSize(); do { ...GetAt(i); i++; n--; } while
  (n != 0);` under a separate `GetSize() > 0` guard — produces the DEC/JNE countdown that
  plain `while (i < n)` NEVER does. Recipe reused successfully twice in WorldgenPlacePuzzles.
- **WorldgenPlacePuzzles (0x421930, 1310B) transcribed, EFFECTIVE-WIP** (annotation has the
  autopsy). More cracks: `int nVal = pEntry->val` kills 66-prefix short loads at dual call
  sites; the flag-if needs the call arm FIRST + `if (bRetry == 0) call; else = nBanned;`
  inner shape; n++ precedes the lastX/lastY stores in all FOUR duplicated accept copies
  (real source duplication, one per worldSize case + first-tele). Residual = the OPEN
  block-sinking family (accept copies + retry sunk past the switch) + slot rotation.
- New fields: worldSize@0x3328 (teleporter min-distance tier), genSkipTeleCheckMaybe@0x2e64.
  New decls: PlaceQuestNode (7-arg, 0x41f120), WorldgenPlacePuzzles, PlacePuzzle.
✅ Ghidra sync DONE (v9 late,
YodaDemo was ACTIVE): World fields added (zoneCountLoadedMaybe@0x54, genSkipTeleCheckMaybe
@0x2e64, bDtaLoadedMaybe@0x32f8), 0x32d4 quad renamed nView* (+ WorldDoc.h/.cpp updated),
EnterZone→GetZoneIndex, 0x41c340→CFileExceptionDtorTUEmitted (+plate), Log_Write→LogWrite
(+CTheApp-member plate). Ghidra's richer names BACKPORTED into Worldgen.h pads (startItem
pair@0x2e38, weaponHit pair@0x2e48, bHidePlayer@0x2e54, arrowBox quad@0x32e4, bWorldReady
@0x32f4, nextCamera pair+pPendingZone@0x3300, healthLo/Hi+difficulty+counter+gameSpeed
@0x3314-0x3324) — codegen-neutral, verified 27/54 + 7/13 unchanged. progress.py now also
reports the PARTIAL tier (transcribed-not-exact): 14.10% exact + 31.97% partial = 46.07%
transcribed. GOTCHA: the MCP add_struct_field auto-prefixes names (bX→nX) — fix with
modify_struct_field after; rename_function_by_address needs strict_mode=off to bypass the
token-collision filter.

### ⏮ PRIOR (2026-07-06 v8 — Phase D: 48 funcs incl. BOTH IFF dispatchers; 13.85%)
**Progress 13.85% byte-exact (was 13.52% at v7).** v8 delta on top of the v7 block below:
- **ParseActn (402B) + ParseHtsp (407B) EXACT on first compile** — the ParseChar TRY/CATCH
  recipe + an inner SetSize/SetAt loop. Mirror details that mattered: Actn tests `id == -1`
  and looks the zone up BEFORE reading the count, SetAt-then-Read; Htsp tests `id < 0`,
  count-read first, Read-then-SetAt, and calls FlagQuestObjects after the loop (also when
  count<=0). Inner loops = explicit guard+do-while, `while (nCount > i)` backedge form.
  Inlined `(Zone *)zones.GetAt(id)` = m_pData indexing via World+0x98 comes free.
- **ParseSnds EFFECTIVE (5B)**: only residual = char-buffer frame-slot ORDER (orig is
  size-ascending ext/fname/name/path; ours swaps name/fname). Probes ALL inert — decl order
  x2, nested strcat(strcpy()), scope splits: array slot keys are compiler-internal. PARKED.
- **LoadWorld (0x421fd0, 1690B) + Load (0x422670, 2245B) transcribed, EFFECTIVE-WIP ~95-97%
  insn-identical** (in-source annotations carry the full residual autopsy). New cracks:
  (a) **CRect built from two CPoints** — `CRect(CPoint(l,t), CPoint(r,b))` computes r,b
  BEFORE l,t (right-to-left arg eval); the flat 4-arg ctor computes l,t first and CSEs the
  sums differently — the CPoint form halved LoadWorld's align score. (b) `AfxGetApp()->
  DoWaitCursor(1)` written directly (BeginWaitCursor() emits an out-of-line lib call).
  (c) MFC inlines verified: AfxGetMainWnd = double AfxGetThread + vcall+0x7c; CProgressCtrl
  SetRange/SetStep/StepIt = raw ::SendMessage PBM_*; CFileException ctor fully inline; and
  a local `CFileException e` makes the TU emit ~CFileException as its FIRST function —
  that's what 0x41c340 really is (docs called it "load/save helper ctor"; fix pending).
  (d) `if (x == 0) x++;` ≠ `x = 1` (load/test/inc/store vs cmp-mem/store-imm).
  (e) planet-pick logic: switch(currentPlanet) x2 (milestone completionCount 5/10/15 vs
  normal), rand()%2 arms, then DEMO HARDCODE currentPlanet=2 overrides it all.
  (f) .dta open failure: CFileException-cause switch → AfxMessageBox(5/6/0xe01e) →
  **AfxAbort()**, then dead-but-emitted cleanup (engine-bugs #7 family).
- **⚠ TWO OPEN codegen problems, both in the dispatchers** (park; joint/endgame or
  decomp.me): (1) loop-exit-cleanup block placement — orig glues it after the FIRST parse
  arm mid-ladder (both dispatchers!), ours after the loop tail; if(nDone==0)-nesting proven
  IL-equivalent. (2) Load's m_cause switch: orig = DIRECT 13-entry table pointing at 3
  MERGED arm blocks; sw*.cpp probe battery proved VC4.2 byte-maps any ≤5-arm switch and
  never merges ≥6 written arms — combo unreachable from source shape alone.
- New World fields: bStartingGameMaybe@0x2e40, completionCount@0x332c, bDtaLoaded@0x32f8,
  zoneCountLoaded@0x54. Worldgen.h now includes afxcmn.h + ../App/App.h (CTheApp.m_str@0xc0
  = the .dta path). Cross-TU decls added: ParseTilesMaybe/CacheUiTilePtrsMaybe (WorldDoc).
- Dial churn this round: Randomize flipped OUT (30B), RemoveZoneEntry2 flipped IN, the
  Zaux/Zax2/Zax3 trio rotated again. Standing rule unchanged: don't grind these.
- **PlacePuzzle semantics** (function now EFFECTIVE, see v9): 3 CPoint* CObArrays
  (isolated / adjacent-to-306 / past-order-cutoff), rand-pick priority isolated>adjacent,
  far only when both empty; `!!!!No Place to put Find Puzzle` log via
  `((CTheApp *)AfxGetApp())->LogWrite(...)` — **Log_Write@0x419cb0 is really a CTheApp
  MEMBER** (call sites set ECX=pApp; body ignores this, so App TU's free-function form
  still matches; member decl in App.h, App TU re-verified 11/12).
  `new CPoint(x,y)` = the raw new(8)+inline-ctor null-check shape.

### ⏮ PRIOR (2026-07-06 v7 — PHASE D underway: 43 doc-TU funcs transcribed; 13.52%)
**Progress 13.52% byte-exact (was 10.01% at v6).** src/Worldgen/ now carries **43 functions,
~26 exact depending on the current dial** (the count breathes as functions are added — see
"dial churn" below). Everything below the fold in v6 still applies; this block is the delta.

**What is DONE in src/Worldgen (exact at least under one dial, all structurally proven):**
leaves+list helpers (IsItemPlaced, Push/Remove/Add ZoneEntry, IsZoneUsed, AddPlacedZoneId,
IsTileInGoalList, GetZoneGridOrder, IsModified/SetModifiedFlag), recursive queries
(CheckZoneItemsAvailable, WorldgenCollectZoneRefs; ZoneProvidesItem/ZoneFindInIzxList are
structurally converged, reg-tie-break parked), WorldgenPickItemFromZone (392B EXACT),
Randomize (204B EXACT), grid copies Backup/RestoreZoneGrid (189B each EXACT),
Backup/RestoreRecords (773B each EXACT — they copy the center 2x2 quest cells
mapGrid[44,45,54,55] <-> mapScratch[0..3] with constant id tags 0x5e/0x5f/0x5d/0x60),
SetupGrid, ReadZone (575B EXACT — demo zone-id whitelist switch), ReadStupCanvas,
SetCurrentToIntroZone, GetZoneIndex (Ghidra name: EnterZone), UpdateCamera (EXACT — writes
the 288x288 view window rect @0x32d4..e0; WorldDoc.h misnames these nHealthDial*, reconcile),
the 4 audio toggles (OnToggle/OnUpdateToggle Sound/Music, all EXACT first compile),
7 chunk parsers (ParseZone/Zaux/Zax3/Zax2/Tnam exact-or-rotating; ParseCaux/Chwp have a
block-layout+reg residual the arm-order knob cannot steer), Populate (DIFF~13, one
per-case store slot), PlaceZone (WIP: reg 2-cycle + EH-state placement residual),
WorldgenShuffleList (structurally complete; reg cascade parked; **engine bug #10** — its
`GetAt(k) != -1` guard never fires, WORD zero-extend vs -1).

**⭐ NEW codegen cracks from this sprint (add to instincts):**
- **Indexed one-array copy anchors the walker at the first-accessed FIELD** — the grid
  copies only matched as `mapGrid[n].f = mapGrid[n+100].f; n++` inside the 10x10 loops
  (two-pointer and single-pointer [100]-displacement forms put the anchor at struct base).
- **Identical if/else arms are REAL dev code**: PickItemFromZone tests the dead a2 flag and
  does the same SetAtGrow either way; the compiler cross-jumps the arms leaving a dead
  `cmp [n],0` wedged between arg pushes and the call. Grep for flags-unused cmps.
- **`x <<= s; x &= m;` as SEPARATE statements never combine** (Randomize matched only in
  this form, keeping even the no-op `& 0xff000000` after `<< 0x18`); a single
  `(x << s) & m` expression canonicalizes to mask-first `and 0xff; shl`.
- **Switch, not if-ladder, for type dispatches**: all-compares-up-front + out-of-line arms
  (CollectZoneRefs/CheckZoneItemsAvailable OBJ_TYPE ladders, ReadZone's 13-case demo zone
  whitelist with range folding, Populate, Randomize).
- **Per-case constant args cross-jump a shared call tail**: Populate ends each switch case
  with `PlaceZoneObjectTiles(CONST);` — per-case `push CONST` + one shared call+jmp; the
  switch default lands AFTER the call (was the tell).
- **`return found;` vs `return 1;`** distinguishable: `mov eax,edi` vs `mov eax,1` at the
  duplicated epilogue (ZoneProvidesItem).
- **Early-return rotation trap**: a `for` with an early `return` does NOT rotate into
  guard+do-while; write `if (n > 0) { do { ... } while (i < n); }` explicitly (IsItemPlaced,
  all the list scans). `break`-form loops DO rotate.
- MFC idioms compile exact for free: GetFirstViewPosition/GetNextView vcalls (+0x68/+0x6c),
  CCmdUI::SetCheck (slot +4), CFile::Seek(x, CFile::current) (+0x30), the branchy
  `n = (n == 0)` cmp/sbb/neg, and CFile-vcall CSE into a register across a loop.
- `m_bModified` is CDocument+0x44 → 0x422f40/50 are IsModified/SetModifiedFlag overrides.

**⚠ DIAL CHURN is the dominant residual now.** Every added function rotates reg-alloc
tie-breaks TU-wide; matches flip in and out (IsItemPlaced, the RemoveZoneEntry pair, the
Zaux/Zax2/Zax3 clone trio rotate phases like the GameData loader triplet). Do NOT grind a
2-20 byte reg/cmp-direction residual mid-build — finish the TU first, then one JOINT pass
(the standing rule). ZoneProvidesItem (found-var in EDI vs stack) and ShuffleList (the
{bAnyEmpty,nMoved,k*2-offset} contest) are the two structured parks with notes in-source.

**NEXT (in order):** (1) ParseChar (17B eff.) + ParsePuz2 done via the ParseTiles TRY/CATCH
recipe (hand-expanded CATCH_ALL + THROW_LAST + dead OOM box; pNew declared WITHOUT `= NULL` —
the null-init emits extra stores the original lacks). Their residual = the "nDone++-arm at
function end" block-layout family (also ParseCaux/Chwp) — arm-order/continue knobs proven
inert, park it. (2) ParseSnds 0x4233f0 (splitpath) / ParseActn 0x423510 / ParseHtsp 0x4236b0. (3) LoadWorld 0x421fd0 + Load 0x422670 (the IFF dispatcher, big switch on
FourCC tags — string cmps like Puzzle::Read). (4) PlacePuzzle 0x421620 + WorldgenPlacePuzzles
0x421930, then the placer family 0x41c580-0x41d660, CarveQuestPath 0x41d940, PlaceBlockades
0x41e350, SelectPuzzle 0x41eab0, PlaceQuestNode 0x41f120, and last Generate 0x41f960 (6.6KB)
+ the save/load monsters (OnSaveWorld/OnLoadWorld/Serialize/LoadWorldStateFile — CArchive+
CATCH_ALL, use the WorldDoc OnOpenDocument recipe). (5) The GameView methods embedded in
this TU (0x426c40-0x429150: OnInitialUpdate, DrawDirectionArrows, ShowTextDialog,
DrawHealthDial/Needle, AddHealth, UseWeapon, DetonateAdjacentTiles, DrawWeaponBox/Icon,
BlitViewportDither, PreCreateWindow, AddItemToInv) need the real GameView layout — do them
after the World:: half, growing the GameView stub the same way.
**src/Dta/ RETIRED (v15)** — its 3 addresses live here (Zaux/Zax2 MATCH; Zax3 = the rotating clone).
**Ghidra renames pending (needs YodaDemo ACTIVE for writes):** EnterZone→GetZoneIndex
(0x423dc0); the 0x32d4 quad nHealthDial*→view-window rect (also fix WorldDoc.h comments).

### ⏮ PHASE-D WORKING NOTES (was the v6 pickup — facts still in force)
- **Build/verify loop (any TU):** `cd src/<TU> && rm -f <TU>.obj && ../../toolchain/bin/cl /nologo /c
  /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS <TU>.cpp`, then from repo root
  `python3 tools/verify.py src/<TU>/<TU>.cpp` and `python3 tools/asmscore.py ... 0xADDR --dump`.
- **The Worldgen facade** (src/Worldgen/Worldgen.h) grows one REAL member/method at a time toward
  WorldDoc.h's ctor-proven layout (grids@0x4b0, MapZone HAS a vptr, apZoneGrid@0x2d0, worldgen lists
  @0x25c/0x270, genScratch@0x3380). DON'T consolidate WorldStub.h→WorldDoc.h yet — whole-image
  endgame step (dial re-verification across GameData/Iact).
- **Cross-TU calls are masked relocs** — declare with correct arg widths (GetZoneById/RefreshZone/
  PlaceZoneObjectTiles live in the GameData TU; Canvas/GameView stubs are local to Worldgen.h;
  rand/time via stdlib.h/time.h).
- **Doc TU fully documented (3-agent sweep):** zero FUN_*, all worldgen/save Maybe functions named +
  plate-commented in Ghidra; algorithm bible = docs/worldgen.md (Generate → CarveQuestPath ×3 →
  PlaceQuestNode hub → Fisher-Yates shuffle → leaf placers; two zone-entry lists {u16 zoneId@4,
  u16 val@6} ctor 0x401390; .wld = FourCC container, VERS==0x200; STUP = 288×288 canvas snapshot).
  Corrections: 0x41c340 = load/save helper ctor (NOT the doc ctor); 0x41eab0 = WorldgenSelectPuzzle.
- **After Phase D:** GameView TU (Phase E: 0x40a560–0x418700 + its head 0x408c60–0x40a560 mislabeled
  "Core utils", ~57 KB, InvScrollBar/option dialogs embedded); parked scorers + joint residual
  passes; Phase G whole-image build.

[v5 sprint, all committed: **name-based COMDAT pairing** in match/verify/progress (mangled-name
pairing; explicit `(??_G...)` marker hints; stacked markers OK) — re-baselined the honest %; trust
per-NAME diffs for clone families. App TU 15/16 exact; Frame TU 14/18; Dlg TU 5/5.]

**⭐ NEW MFC-matching lessons (fold into instincts):**
- **Implicit vs explicit dtor controls the ??_G shape.** An empty `virtual ~T(){}` forces a
  separate ??1 + a THIN ??_G that calls it. The IMPLICIT dtor (declare none) makes MSVC INLINE
  member destruction into ??_G. Match the original's inline-vs-call shape: CTextDialog needed
  implicit (orig ??_G is 188B inlined); CMainFrame needed explicit (orig ??_G is 30B, calls ??1).
- **Message maps/DYNCREATE/ON_WM_* compile byte-exact** — write the real macros; sizeof from the
  CRuntimeClass struct; handler order = the map-entry order (dump `.rdata` map).
- **Log_Write was __stdcall** (the RET 4); a bare free func can be stdcall.
- **CPUID needs `_emit 0x0f, 0xa2`** (VC4.2 predates it, like the Canvas MMX blits).
- **Version-byte compare widens only as `(int)(BYTE)(x>>8)`** (signed movzx) vs staying in AH.
- **`CString s; s = p;` (default-ctor-then-assign) ≠ `CString s = p;` (copy-ctor)** — different shape.
- **OnActivate/OnSysCommand: order branches so the original's fall-through is the primary path**
  (deactivate fall-through; SC_CLOSE's ConfirmExit fall-through). Switch on message codes gives
  the compiler's comparison-tree; write cases in the map order.
- **The `sbb` boolean idiom vs push-1/push-0 branch** (bForceBackground = this!=pFocusWnd) is
  instruction-selection MSVC picks internally — cmp-direction family, not source-steerable.
- **CFrameWnd fields:** sizeof(CFrameWnd)=0xbc, sizeof(CDialog)=0x5c, CWinApp m_hPrevInstance@0x6c/
  m_lpCmdLine@0x70, App+0xc4 frame-delay -> World+0x74. GameView pWorld@0x44/bBusy@0x4c/
  nDragSlot@0x144/bDragActive@0x148/pMusicThread@0x2fc.

### ⏮ PRIOR (2026-07-06 v4 — App TU, condensed)
src/App 15/16 exact + InitInstance effective (992B CPUID/MMX hand-asm). Unique cracks not in the
lessons block: OnAppAbout's parent = `AfxGetApp()->m_pMainWnd` (NOT AfxGetMainWnd()); `short nBpp`
keeps the 16-bit store; the four CRT dynamic-init thunks a global `CTheApp theApp;` emits are
matchable (reloc-masked); GetMessageMap @0x419720 had been mislabeled Frame::.

### ⏮ PRIOR (2026-07-06 v3 — WorldDoc TU, condensed)
**"Settings" was never a TU** — 0x419ed0–0x41bee0 is the doc class's MAIN source file
(src/WorldDoc/): "Settings::Save" was `World::~World` (the 1441B dtor byte-matched on the FIRST
compile, proving WorldDoc.h's ctor-derived member order); "App::LoadSettings" was `World::World`.
7/13 exact incl. IMPLEMENT_DYNCREATE and OnOpenDocument (624B modified MFC DOCCORE copy — the dead
`IsModified()` vcall from the Release-stripped TRACE0 head was the last crack).
Codegen finds: **MFC macros byte-match for free** (write the real macros; `AFX_EXCEPTION_LINK
_afxExceptionLink` is referencable by name for hand-expanded CATCH_ALL). **Unreachable code after
THROW_LAST() is EMITTED** (no DCE — engine-bugs.md #7; reproduce dead statements). **OPEN PROBLEM —
imm-vs-reg store batching** (WorldDoc ctor DIFF~510: our compiles sink `= imm` stores to the end of
reg-store runs; the original interleaves at source positions; repositioning moves SOME imms, braces
inert). **OPEN — block layout** (GetLocatorIcon: orig sinks early-return bodies to function END;
mechanism unmapped; write its case-10 as an explicit case). verify.py LIB_OWNERS filters
inline-emitted MFC COMDATs. WIP there: ctor, OnNewDocument palette block, GetLocatorIcon, DrawPlayer.

### ⏮ PRIOR (2026-07-06 v2 — Iact TU, condensed)
Phase B COMPLETE: src/Iact = all 10 funcs, 88% insn-identical (2 exact + 8 annotated tie-breaks;
the two interpreters carry only reg/cmp/schedule residuals). Interpreter cracks:
- **A duplicated epilogue = two `return` statements in source.** Frame-slot order is
  USAGE-COUNT-driven, not decl-order (decl permutations proven inert twice; changing a var's use
  count re-ranks its slot — the permuter's decl mode cannot crack slot cycles).
- ctype.h `_toupper` is the blind `-0x20` MACRO; the original CALLS the CRT `toupper` function.
- **Never cast an EXPRESSION to short at a short-param call site** (emits 66-prefix `shl ax`);
  route through int temps and push the dword.
- **In-condition assignment forces CSE**: BumpTile's `args[1] != (ty = y + dy)` keeps the add-form
  compare AND reuses ty (the bare sum canonicalizes to sub-form, killing the CSE).
- Engine bug reproduced: COND_CheckCellItems reuses the SCRIPT loop index for its inventory scan —
  visible as init-store + final-value replacement (a fingerprint worth recognizing).
- IACT semantics: events 1=walk 2=BumpTile 3=DragItem 4=enter-zone 5=enter-vehicle; RunCommands
  returns a dirty mask (0x20=tiles 0x800=warp); 0x456104/0x45610c = item-name placeholders;
  World+0x2e48/4c = nWeaponHit{X,Y}Maybe. IactCondOp/IactCmdOp enums in IactScriptClasses.h.

### ⏮ PRIOR (2026-07-05 late — GameData savers + GameView sweep, condensed)
Saver cracks: an inner-scoped `{ CString key = prefix + buf; ...; }` block puts the temp at the
frame BOTTOM (a bare op+ temp) — fixed frame layout; duplicated full-sprintf if/else arms (the
original cross-jumps the common tail); `n >= 0` emits test/jl only when n lands in ESI. PROVEN: the
ORIGINAL binary has TU phase drift (its 3 identical-source loaders emit jg/jl/jg backedges).
GameView RE sweep (saved in Ghidra): ~50 struct fields + ~60 function renames (OnTimer,
ZoneTransitionStep, PlaySound, DrawZoneCell), message map @0x44b240 mapped, sizeof(CView)=0x40.
⚠ **OnKeyDown (0x4150f0) body not fully claimed by Ghidra** (0x41526f–0x415658 orphaned;
FUN_004156f2 = its split EH tail) — needs a body-repair pass before Phase E.

### ⏮ PRIOR (2026-07-05 — MFC linkage, Records TU, references; condensed)
- **Static-MFC linkage stood up** (toolchain/bin/link + NAFXCW.LIB; recipe + linker-3.10-vs-4.20
  endgame flag in toolchain/README.md). GOTCHA: run `bin/cl`/`bin/link` DIRECTLY, never
  `wine bin/cl` (they are bash wrappers calling wine; double-wrapping fails silently → stale .obj).
- Zone byte-matching found a real mis-model: ZoneObj true layout = type@4/state@8/x@a/y@c (fixed).
- **Records TU cracks** (src/Records, ~26/33 exact): `unk38=-1` placed AFTER an arg-consuming store
  → forced immediate; a nested `int id = e->charId` local → one movsx serving two range tests;
  `int` vs `short` of a local decides zero-reg reuse vs imm compares; SetTile's val param is `short`
  (2-byte `push -1` at call sites); a tail-merged `return 1` = nest the drop logic under
  `if (numItems != 0)`. TU-context effects PROVEN live (adding decls flipped matches both ways) —
  residuals are allocator tie-breaks for the JOINT endgame pass, not piecemeal work.
- Runtime engine fully documented: docs/game-logic.md (frame loop `switch(World.nFrameMode)` 1..8 +
  enemy AI switch on Character+0x36), docs/worldgen.md, docs/settings.md.
- **DESKADV.EXE** (Indiana Jones Desktop Adventures, 1995, 16-bit NE/WinG) is open in Ghidra
  (`program=DESKADV.EXE`) as a structure/naming cross-ref for DTA/zone/IACT/worldgen — NOT
  byte-matchable. **Fable** (model `fable`) is available for planning/review/walls.

### Matching progress + tooling (Phase 4 underway)
- **`src/World/World.{h,cpp}`** — first matched module, written as C++ (Yoda Stories is C++/MFC; member
  functions compile `__thiscall`, matching the originals). **4/6 World funcs byte-match 100%**:
  `UpdateScore` (0x401450, 4 call relocs), `CalcCompletionScore` (0x401490), `CalcScoreFromCounter`
  (0x4016d0), `GetZoneCell` (0x401a80). `CalcSolvedScore` (0x401780) ~98% (x87 two-accumulator register
  alloc, 9 bytes) — parked; `CalcTimeScore` (0x4019c0) pending (calls `time`/`__ftol`/ext 0x42a3e0).
- **`src/Canvas/Canvas.{h,cpp}`** — DIBSection CU (0x407df0–0x4084e8): **8/11 exact + 3 effective**
  (`Init` DIFF22 / `Clear` DIFF2 / `BlitMasked` DIFF4 — allocator/scheduler artifacts, annotated
  in-source). `/G` flag axis ruled out (/G3=/G4=/GB=default identical; **/G5 catastrophically worse**
  → binary uses the default scheduler). Init's residual survived EXHAUSTIVE source-shape probes —
  do NOT re-litigate: store order proven source-faithful; memset never decomposes (`rep stosd`);
  abs()/ABS-macro forms worse; `h->`-for-everything worse (the winning form: `BITMAPINFOHEADER *h =
  &biHeader;` CSE'd for biSize + the CreateDIBSection arg only); decl position, multi-use temps,
  `register` hints, dead code, the whole /O axis, PCH compile, unreferenced/inlined helper
  predecessors: all inert or fingerprinted; VC4.2b ruled out by KB docs (patch never touched cl).
  Non-emitted code carries NO TU state (a vanished helper has zero effect). **PARKED (user decision):
  plausibly a separately-built library CU (would explain the hand-crafted MMX + confined residuals);
  do not dig until ~99% completion.**
- **The MMX blits (`BlitFast` 0x408110, `BlitMasked` 0x408240) are HAND-ASM in both branches**
  (VC4.2's assembler predates MMX): reproduce with `__asm { _emit 0xNN }` **one `_emit` per line**,
  each annotated. Gotchas: a write-only-in-C asm-read local (keyq) must be **`volatile`** or DSE
  drops its slot; the `_emit` modrm hard-codes EBP offsets so C locals must land on the original
  slots (get the local COUNT right first — the frame-size byte is the canary); scalar loop labels/
  unroll tails matter. **Statement order (not decl order) colors registers**: `s = src;` before
  declaring `rows` (BlitMasked 34→4), `stride` declared before `cw` (BlitFast 7→0).
- **`tools/match.py`** — compile a `.cpp`, best-fit each COMDAT function section to a `// FUNCTION: YODA
  0xADDR` marker, byte-compare vs the exe with relocations masked. **`tools/progress.py`** — completion
  dashboard: matched-bytes ÷ **128158** total app-function bytes (534 funcs, from Ghidra). Currently
  **7.02%** (2026-07-06). Run: `python3 tools/progress.py`.
- **KEY codegen lessons (MSVC 4.2):**
  1. Each C++ function → its **own `.text` COMDAT** in the `.obj` (function-level linking on for C++).
  2. **Comparisons are emitted literally** — `v >= 0x5b` (`CMP 0x5b;JL`) ≠ `v > 0x5a` (`CMP 0x5a;JLE`).
     Mirror the exact operator/constant from the disassembly, not Ghidra's normalized `<`/`>`.
  3. A result routed through a variable to a **single return** emits `mov reg,VAL; mov eax,reg` per branch
     and keeps the reg; per-branch `return CONST` folds to `mov eax,VAL`. Match the original's shape.
  4. A store before a call that passes `this` is kept (compiler can't prove the callee won't read it) —
     e.g. `mScore=0;` survives before `mScore=CalcTimeScore();`.
  5. x87 stack-slot / register allocation depends on **local declaration order** — reorder locals to match.
  6. **CMP operand order** (`cmp width,x;jg` vs `cmp x,width;jl` for the same `x<width`) is instruction
     selection MSVC 4.2 picks internally — often NOT forceable by flipping the C expression. When a
     function is otherwise identical but a few comparisons have swapped operands + inverted jcc, it's
     permuter territory (like the parked World funcs). Verify a claimed match by **direct disassembly
     diff**, not only `tools/match.py` (its best-fit pairing can occasionally mis-report).
  7. **Register allocation is TU-context-dependent (big one).** Three byte-identical source functions
     (Dta `ParseZaux`/`ParseZax3`/`ParseZax2`) got *different* register allocations in the original
     (ESI/EDI/EBP roles rotated) purely by their position in the full `.obj`. In a partial TU only ONE
     reproduces the original's allocation. **Implication:** context-sensitive functions can't all be
     matched piecemeal — you need to reconstruct the *whole* original translation unit (all its funcs,
     in source/.text order) or use a permuter. This is a strong argument for the asm-first / full-TU
     approach for such modules. Simple leaf/accessor funcs (GetTile, etc.) are context-insensitive and
     match fine piecemeal.
  8. **The TU-phase dial refines #7: the class HEADER alone shifts TU state.** The member-decl set of
     the class (count AND signature shapes — `int f(int,int)` vs `void f()` matter independently)
     rotates allocator/cmp-direction tie-breaks in every function of the TU, even for functions that
     never call the declared methods. Only REAL methods, never fake decls; full write-up + the
     PHASE-DISPLACED annotation convention in "Standing rules" (roadmap section).
  9. **Loop rotation:** a `for` with an early `return` does NOT rotate; write the
     `if (n > 0) { do {...} while (i < n); }` guard+do-while explicitly. `break`-form loops rotate.
     Hoist the count (`int n = arr.GetSize();`) or the strength-reduction to a walking pointer
     fails (GetAt reloads m_pData per iteration).
  10. **`x <<= s; x &= m;` as separate statements never combine** (even a no-op mask after
     `<< 0x18` is emitted); a single `(x << s) & m` expression canonicalizes to mask-first.
  11. **Switch, not if-ladder, for multi-way dispatches:** all compares up front + out-of-line
     arms (type ladders, sparse id whitelists with consecutive-case range folding). Per-case
     constant call args cross-jump a shared call tail (per-case `push CONST` + one call+jmp;
     the switch default lands AFTER the call). Identical if/else arms are REAL dev code — the
     compiler cross-jumps them leaving a dead cmp between arg pushes and a call.
  12. **Indexed one-array copies** (`a[n].f = a[n+100].f; n++` in nested loops) anchor the
     strength-reduced walker at the first-accessed FIELD; two-pointer and single-pointer
     [±100]-displacement forms anchor at the struct base and mismatch. `return found;` (var)
     vs `return 1;` (const) is visible as `mov eax,reg` vs `mov eax,1`. A `pNew = NULL` init
     the original lacks emits extra zero-reg stores — new-expression nullchecks are compiler-
     generated; don't add source inits the flow doesn't need.
  13. **Repeated expressions vs. locals decide slot-vs-register residency (UseWeapon v15).**
     When the original reloads a derived value (`x+dx`, `y+dy`) from a STACK SLOT at every
     use, the source had NO local — the full expression is written at every site and the
     compiler CSEs it into a slot itself. Writing `int tx = x + dx;` instead puts the value
     in a REGISTER (and demotes something else to memory) — provably different code. Corollary
     of the "params used at 2+ sites CSE-spill on their own" lesson, extended to expressions:
     match the original's residency (slot-reload ⇒ repeat the expression; register ⇒ local).
  14. **16-bit call-site arithmetic comes from the CALLEE's short params, not source casts.**
     `DrawZoneCell(x + nAX, nAY + y)` with `DrawZoneCell(short, short)` emits the 16-bit
     forms (`add ax, word ptr [y]`, `imul bx,bx,3`) on PLAIN INT expressions. Explicit
     `(short)` locals/casts create word-slot stores/loads the original lacks. Fingerprints:
     dword-load + 16-bit-add-from-mem = int vars narrowed by the param; word-load = a real
     short local. Site operand order is mirrored: mem-first (`x + dx*2`) folds to a 32-bit
     LEA; mul-first (`dx*3 + x`) goes 16-bit IMUL+ADD.
- **MFC vtable calls** (e.g. `CFile::Read`): VC4.2 rejects the `__thiscall` keyword on free funcs/typedefs.
  Model the class with N dummy `virtual` methods so the real one lands at the observed vtable offset
  (`Read` = slot 15 = `+0x3c`); call it as a normal virtual. Works — see the CFile stub in
  `src/Records/RecordClasses.h` (src/Dta, the original example, was retired in v15). Non-virtual
  `__thiscall` helpers (e.g. record loaders) → model as member functions (implicitly thiscall); the
  call is a masked relocation so the exact target/name is irrelevant to the match.
- **Completion-% endgame** (user goal): the rigorous version is an **asm-first build** — disassemble all,
  assemble+link a byte-identical EXE with link.exe 3.10, then swap asm→C keeping it identical. Heavy infra;
  `tools/progress.py` gives the byte-% now without it.

### Tooling (`tools/`, all Python; run from repo root)
- **`match.py <src.cpp> [--exe ...]`** — compile the `.cpp`'s object (must exist next to it), then for each
  `// FUNCTION: YODA 0xADDR` marker, best-fit the COMDAT function to its address and byte-compare with
  relocations masked. Reports MATCH / DIFF(n) per function. (Best-fit can mis-pair near-identical funcs —
  confirm with a direct disasm diff. It also exposes `coff_functions`/`trim_pad`/`mask` for reuse.)
- **`progress.py`** — completion dashboard: compiles every `src/**/*.cpp`, sums matched bytes ÷ 128158
  total app-region function bytes (534 funcs). One number to track progress.
- **`bytematch.py --va 0x.. --obj ..`** — single-function reloc-masked compare (the original harness).
- **`permute.py <src.cpp> 0xADDR [--iters N] [--mode all|stmt|cmp|decl]`** — the **permuter**. Searches
  source variations of one function (statement order; comparison form; leading local-declaration order →
  register/x87-slot allocation), cl-compiles each, and uses the **graded `asmscore`** (below) as the oracle.
  Stops at 0 diffs, writing `*.matched.cpp`. Only mutates the target function; keeps the rest of the TU as
  context. Use it on the reg-alloc/x87 parked near-matches. (Dedups no-op variants; splits `int y, x;` so
  counters can reorder. Slow — one `cl` per variant; run in background.)
- **`asmscore.py <src.cpp> 0xADDR`** — ⭐ **the register-rename-aware graded scorer (DONE 2026-07-05).**
  Replaces the flat raw-byte-diff oracle inside the permuter. Capstone-disassembles candidate + original
  (relocs masked to 0), Needleman-Wunsch aligns on `(mnemonic, operand-kinds)` with registers normalized,
  and grades the residual in 4 weighted tiers (1000/100/10/1): **`align`** = structural distance (wrong /
  inserted / deleted / kind-changed insns; the *scheduling & instruction-selection* signal) · **`reg_pen`**
  = is the register difference one consistent bijection? (clean rename → 0) · **`identity_miss`** = # reg
  slots differing from the original's exact register · **`byte_diff`** = the old raw count (finest tie-break;
  also catches wrong immediates). Standalone CLI prints the breakdown — **use it as a diagnostic**: `align>0`
  ⇒ scheduling/instr-selection (reach for stmt/cmp reorder or park); `align=0, reg_pen/identity_miss>0` ⇒
  pure register allocation (decl-order / full-TU territory). Handles integer, x87, and MMX code (verified on
  Canvas/World/Zone/Dta). Sub-register names (AL/AX/EAX) canonicalize to one slot. This is the technique
  decomp.me / simonlindholm's decomp-permuter use to make randomized search converge.
  **`--dump` columns: LEFT = original, RIGHT = ours** (r = pure reg diff, S = substituted,
  +/- = insn only on that side). NOTE: asmscore RECOMPILES the TU itself (rm .obj + cl) — don't
  run it concurrently with your own compile of the same TU.
  **Minimal-TU probe (v15, cheap + decisive):** to test whether a residual is intrinsic vs
  TU-position, extract the one function + its `#include "<TU>.h"` into a probe .cpp and asmscore
  that. Identical score solo (UseWeapon: 870 both ways) ⇒ the residual comes from the function
  itself or the HEADER DECL SET (the dial), not from neighboring functions — so search over real
  header decls, not per-function source. Different score ⇒ TU-position-sensitive (lesson #7),
  park for the joint pass. Delete the probe file after (it double-claims the marker).
  **Permuter status + TODOs:**
  - ✅ *Statement reordering* — DONE (2026-07-04). Dependency-safe hill-climb over adjacent independent
    statement swaps; declaration order untouched (offsets stay put for the `_emit`-asm funcs). Auto-found the
    `s=src`-before-`rows` win (BlitMasked 34→4) unattended.
  - ✅ **Register-rename-aware scorer — DONE (2026-07-05, `asmscore.py`, see above).** Gives the hill-climb a
    real gradient (structural → bijection-consistency → identity → bytes) where raw byte-diff was a flat
    plateau. Diagnoses residual *nature* (scheduling vs reg-alloc) at a glance.
  - ✅ **Comparison-form hill-climb — DONE (2026-07-05, `--mode cmp`).** Greedy per-comparison flip: operand
    flip `a<b`↔`b>a` (changes `cmp` operand order + jcc) and constant-form toggle `x<N`↔`x<=N-1`. Doubles as
    the **movability probe** — on `Zone::GetEdgeCode` it flipped every comparison in ~30 compiles and the
    graded score never moved (stayed align=30), *proving* the cmp knob is outside that function (MSVC
    canonicalizes it back) → park, don't keep guessing. Confirms lesson #6.
  - **Parked-function movability results (2026-07-05, via the new scorer):** `Zone::FindObjectAt` (0x405330,
    score align=10/reg_pen=2/identity_miss=5) — the deciding locals `i`/`obj` are declared *inside* the loop,
    so the leading-decl permuter can't reach them (needs mid-body decl hoisting — a TODO). `Zone::GetEdgeCode`
    (align=30) — instruction-selection, cmp-flip proven inert (above). `Dta::ParseZaux` (align=32,
    byte_diff=78) — piecemeal it's *far* off, not a mere rename; strongly confirms lesson #7 (needs full TU).
  - *NEXT permuter levers (now that the scorer gives a gradient):* **mid-body declaration hoisting** (move a
    loop-local decl to the leading block so its allocation is permutable — unblocks FindObjectAt) · parallelize
    compiles (N wine workers) · joint commutative-chain enumeration.
  - *Joint commutative-chain enumeration* — flatten `a+b+c`/`a*b`, enumerate all operand orders ×
    parenthesizations *together* (single-axis reassoc misses the joint points). Small/exhaustive. (Tried on
    BlitMasked's dst — 24 variants, didn't flip its 4-byte residual, but cheap & worth it generally.)
  - *Multi-use temp insertion* — insert `T t = expr;` at each legal position, ONLY where `expr` has ≥2 uses
    (single-use temps are copy-propagated away at /O2 — proven: `int` temps did nothing on BlitMasked). Also
    *CSE-structure toggling* (one shared temp vs recompute-at-each-site) and *short↔int type toggling* (moves
    where `movsx` widening temps materialize; mine the original's 16-bit ops as priors).
  - *Loop transforms* — swap nested-loop variable roles / direction (`CalcSolvedScore` y↔x → EAX/EDX).
  - Quality-of-life: parallelize compiles (N wine workers); with a graded scorer, random-restart+greedy beats
    annealing (spaces are 10²–10⁴, bottleneck is one `wine cl`). Add a "movability probe" (fixed ~200-mutation
    battery: does the contested decision EVER flip? never → knob is outside this function → park immediately).
- **`segment_cus.py <cu_refs2.txt>`** — first-pass `.obj` segmentation from data-ref clustering (Phase 3).
- Ghidra dumps that feed the above live in `toolchain/test/cu_{refs2,calls,strings}.txt` (regen via scripts).

### Conventions
- **Bulk `this`-typing by distinctive field offsets (propagation, 2026-07-04).** To type many
  `__thiscall` functions at once, scan each untyped one's instructions for `[reg+off]` displacements
  matching a struct's *distinctive* offsets and move it into that class namespace (a Ghidra script:
  `func.setParentNamespace(getOrCreateClass("Zone"))` types the auto-`this` as `Zone*`, same as
  `set_function_this_type`). Use only **distinctive** offsets (Zone `0x7ac/0x7c0/0x7d4/0x844`, World
  `0x4b4/0x2c0/0x2e20/0x3330`, Canvas `0x438`); **avoid common ones** (`0x44/0x98/0xc0`) — they
  false-positive. When a signal is weak (GameView `doc@0x44`), require **corroboration** (`0x44` AND a
  class-specific field like `frameCounter@0xb0`/`draggedTile@0x140`). Verify a sample after (e.g. a
  frame method that does `GetActiveView(this)` will look like GameView but isn't — revert those to the
  global namespace). Typed ~106 methods (World 36 / Zone 32 / GameView 32 / Canvas 6) this way.
- **Define/maintain structs in Ghidra so the DECOMPILER does the idiomatic work for you.** Before
  transcribing a function, create its struct(s) in the Ghidra DB with correct field types and apply
  them (`create_struct` / a StructureDataType script, then `set_function_this_type "Zone *"` for the
  `this`, or `set_local_variable_type` / `set_function_prototype` for params). Ghidra then emits
  `this->tiles[i]`, `zone->width`, `p++`, `objects[i]->type` automatically instead of
  `*(short*)((char*)z+0xc)` — transcription becomes copy-paste. Proven: after defining `Zone` (0x848,
  `tiles` as `ushort[972]@0x10`, `objects` as `ZoneObj**@0x7ac`) and `ZoneObj`, `Zone_GetTile`
  decompiled to `this->tiles[(y*18+x)*3+layer]`. Keep the Ghidra struct and the `src/` struct in sync. (docs/structs.md is DEPRECATED — Ghidra + src/ are the only doc sites.)
- **Trace every struct to its allocation before trusting its size, and define it once.** The correct
  size comes from the `operator_new(N)`/alloc site (e.g. `Zone`=0x848, `Tile`=0x40c, `MapEntity`=0x64,
  `IactScript`=0x30 were all pinned this way), NOT from how far field accesses happen to reach. Keep a
  single canonical definition per struct in the DB (docs/structs.md is DEPRECATED 2026-07-05 — Ghidra DB + src/ headers are the only struct-doc sites; the .md is history-only).
  A struct whose size is only bounded by observed accesses is INFERRED (a TODO to trace), not done.
- **Non-idiomatic C++ in a decompilation is a signal to keep documenting, not to stop.** Messy casts
  like `*(short*)((char*)p+0x84)` or a field typed as the wrong struct (`tileArray` as `Zone*` →
  `tileArray->tiles+id*2-8`) mean a struct/type is still missing or wrong. Model it and re-check that
  the function now reads like human code (`this->tileArray[id]`). Prefer documenting over matching until
  the types are clean — matching messy casts wastes effort.
- **Matched C/C++ must be REAL, idiomatic, portable source — not machine-translated pointer math.**
  A human wrote this game; the decomp should read like human code, not Ghidra output. Concretely:
  - Walk arrays with `p++` / `arr[i]`, never `(T*)((char*)p + sizeofT)` or `*(T*)(base + byteoff)`.
    (The compiler strength-reduces `arr[i]` back into the offset walk, so it still byte-matches.)
  - No `(int)ptr` / `(int)`-casts of pointers; give functions their real pointer/enum return types.
  - Prefer struct member access (`z->width`) over `*(short*)((char*)z + 0xc)`. Explicit `_padNN[]`
    fields to hit known offsets are fine (and necessary) — but name the real fields you know.
  - The byte-match is the correctness oracle: rewrite to idiomatic form, recompile, confirm it still
    matches. If an idiomatic form breaks the match, keep the faithful form but leave a `// TODO: idiom`.
- Prefix functions per compile unit (see Phase 3). Loose-Hungarian for variables.
- Mirror OpenJKDF2 structure (`~/workspace/OpenJKDF2`): CMake, per-module source files. **Naming: C++
  `Namespace::Method`** (see the convention block at the top of this file) — `Module_Function` flat names
  are legacy/provisional, to be migrated into namespaces.
- Progress artifacts live in `docs/`. The toolchain lives in `toolchain/`.