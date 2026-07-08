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
| **GameData** (2nd doc-TU src file) | 0x401ac0–0x4042b0 | ~10 KB | ✅ DONE: 9/27 exact under the v28 dial (real GameView.h), rest effective/PHASE-DISPLACED; StartGame DIFF 79 (was 254 pre-de-dup) |
| **Records** (6 record classes) | 0x4042b0–0x405ae0 | 5.5 KB | ✅ DONE 25/33 exact + 8 annotated eff. |
| **Iact** (`.obj`: Zone readers + IACT) | 0x405ae0–0x407cf4 | ~9 KB | ✅ TU COMPLETE: all 10 funcs transcribed, 88% insn-identical; 1 exact + 9 annotated tie-breaks under the v28 dial |
| **Canvas** (DIBSection blitter) | 0x407df0–0x4084e8 | 1.8 KB | ✅ DONE 9/11 + 2 eff. (parked); v28: canonical Canvas.h, 0x407df0=ctor / 0x407eb0=dtor, CDC stub private to Canvas.cpp |
| **IactScript** (3 script record classes) | 0x418700–0x418dd0 | ~1.7 KB | ✅ 11/12 exact (src/IactScript/) |
| **Dlg TU** (CTextDialog) | 0x418dd0–0x419000 | ~0.6 KB | ✅ DONE 07-06 (src/Dlg/): 5/5 EXACT. Implicit-dtor lesson (??_G inline) |
| **Frame TU** (CMainFrame) | 0x419000–0x419720 | ~1.8 KB | ✅ DONE 07-06 (src/Frame/): 14/18 exact + 4 eff. (2 palette sbb, PreCreateWindow, OnActivate). Owns g_strReplayPath |
| ~~Core utils~~ = GameView TU head | 0x408c60–0x40a560 | 6.5 KB | ⚠ mislabel: GameView methods (Dtor/OnDraw/DrawZoneCell/ZoneTransitionStep) + the ~CGdiObject/GDI COMDAT copies — Phase E, not a warm-up |
| **GameView TU** (view/UI/AI monster) | 0x4084f0–0x418700 | ~64 KB | ⭐ PHASE E step 4 all but done (src/GameView/GameView.cpp). v29: 73/121 markers; TextDialog cluster 6/7 (ctor EXACT; Run 622/622-insn eff.; Position+3 helpers eff.). REMAINING = ONLY TextDialog::Layout 0x4176f0 (1419B, jump table + CPoint[3] Polygon/LineTo + bitmap-button matrix; struct+decompile understood) |
| **App TU** (CTheApp + CAboutDlg + Log_Write) | 0x419720–0x419ed0 | ~2 KB | ✅ DONE 07-06 (src/App/): 15/16 exact + InitInstance eff. (CPUID hand-asm) |
| **WorldDoc TU** (doc main src file) | 0x419ed0–0x41bee0 | ~8 KB | src/WorldDoc/: 8/13 exact (v28: the 1441B DTOR back IN) + ctor-derived REAL World class; ctor/OnNew/OnOpen/GetLocatorIcon parked (imm-store batching + block-layout opens) — joint-pass fodder |
| **World/doc TU** (dta-load+worldgen+wld+doc) | 0x41bee0–0x429150 | ~54 KB | ✅ TRANSCRIPTION COMPLETE v14/v15: src/Worldgen 90 markers = EVERY function incl. the GameView tail block; 36/90 exact under the v28 dial + the rest annotated EFFECTIVE (per-function autopsies in-source); zero FUN_*. Unclaimed in range: 4 exception COMDATs @head, ??_GCPalette 0x41e8b0, EH thunk 0x424f69, jmp 0x424fb0 (all Phase G). Joint pass AFTER Phase E (shared Worldgen.h ⇒ E's GameView decls re-rotate this TU) |

With the doc TU transcribed (v14), the untranscribed remainder is essentially ONE monster: the
GameView TU + its mislabeled head (~63 KB ≈ 26 % of the app region). Its struct is complete and its
runtime behavior documented (docs/game-logic.md) — Phase E is transcription plus the method-decl-set
reconstruction, not research. After E: mop-up minis (F), then joint passes + whole-image (G).

### Struct status board (audited 2026-07-05 — regen with the run_script_inline coverage dump)
Coverage = defined-bytes ÷ sizeof; unk = fields still named unk*/field_*/…Maybe.

| struct | size | cover | unk | state / phase to finish |
|---|---|---|---|---|
| Zone / ZoneObj / Tile / Canvas | 0x848/0x10/0x40c/0x43c | 100 % | 0 | ✅ done (byte-match-proven); Canvas CANONICAL in src/Canvas/Canvas.h (de-dup steps 3+4, v28) |
| MapEntity | 0x64 | 96 % | 5 | ✅ good; unk10/18/20/2c/60 have no readers found (runtime-only scratch?) |
| Puzzle | 0x2c | 95 % | 3 | ✅ good; unk2/unk3/unk14 parse-only (unknown in DA too) |
| Character | 0x4c | 94 % | 3 | ✅ good; unk40/unk44 parse-only, unk48 tail pad |
| CObArray family / CDWordArray / BITMAPINFO256 | 0x14/0x428 | 100 % | 0 | ✅ modeling helpers |
| **World** | **0x33c0** | **~98 %** | few | ✅ v10 full Ghidra↔Worldgen.h mirror + v15 rect-block/tail sync. Worldgen.h IS the emerging real CDeskcppDoc header. Residual unks (unk2e58/unk2e60/unk3378/unk33b8…) are semantics-only — layout is ctor/byte-match-proven |
| **GameView** | **0x310** | **~95 %** | ~10 | ✅ struct COMPLETE, now in src/GameView/GameView.h (v16, promoted from Worldgen.h) + Ghidra synced. METHOD decl set reconstructed v16 (overrides via vtable-diff, afx_msg via msgmap, ~35 helpers). Open: byte-prove "(sig?)" helper widths + AFX_MSG order during step-4 transcription |
| MapZone (10×10 grid cell) | 0x34 | 100 % | few | ✅ CANONICAL src/Worldgen/MapZone.h (de-dup step 2, v28) — the GameData shifted-by-4 stub is retired; grids START at 0x4b0 |
| IactScript | 0x30 | 100 % | 0 | ✅ solved 2026-07-05: vtbl@0 (0x44bc68) + 2 inline CObArray (conditions@4, commands@0x18) + doneFlag@0x2c — Zone-pattern. Whole Iact-script TU (0x418700–0x418dd0) renamed Records-style: IactScript/IactCondition/IactCommand ::Ctor/ScalarDtor/Dtor/Read |
| IactCondition / IactCommand | 0x1c/0x20 | 100 % | 0 | ✅ vftable@0 added (0x44bc80/98); opcode@4 + args[5]@8 (+text@0x1c for commands) |
| InvScrollBar | 0x44 | good | 0 | ✅ v14/v15: CScrollBar-derived, scrollMax@0x3c, scrollPos@0x40; Ctor=0x4085c0 (its own mini-TU) |
| TextDialog | 0xc8 | good | 3 | ✅ v14/v15: NOT CDialog-derived (plain class). unk10/unk14/unk54 (ShowTextDialog args a/b/c), strText@0xb8 (CString), pParentView@0xc0, soundSession@0xc4; Ctor 0x416b90 / Run() 0x416c40 / ??1 0x427440 (Run is 0-arg, byte-match-proven) |
| InvItem | 0xc | 100 % | 0 | ✅ v15: CObject-derived (vftable/pTile/name); Ctor 0x4011d0 + CtorTileName 0x401270 (first app TU); element of World.inventory@0xa8 |
| CFile (stub) | 0x40 | 6 % | 0 | intentional — DB stub only pins Read@vtbl+0x3c; real MFC used at compile time |
| StatsDlg / 3 slider dialogs | 0x74 / 0x60 | 100 % | 1 | ✅ v27/v28 (GameView-TU-private, declared in GameView.cpp): sliders = CDialog + m_nValue@0x5c + EMPTY DoDataExchange; StatsDlg = CDialog + unk5c + World*@0x60 + 4 CString@0x64-0x70 (m_str2←highScore etc.); Ghidra structs synced |

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
- **Milestones** (progress.py; since v23 track the Ghidra-extent COVERAGE % — the old
  transcribed% was inflated): 7.02 % after A → 13.52 % mid-D (v7) → 73.63 % transcribed at
  doc-TU completion (v15) → 92.28 % coverage / 17.30 % exact (v26) → 94.42 % coverage (v27,
  GameView tail + option dialogs) → 95.64 % coverage / 19.39 % exact (v28: slider
  DoDataExchange discovery, struct de-dup steps 1-5, CyclePalette + OnCmdStats) →
  **97.56 % coverage / 16.06 % exact — v29 (2026-07-07): the plain-class game TextDialog
  modeled + 6/7 functions transcribed (ctor EXACT; Run 622/622-insn eff., Position + 3
  helpers eff.); only TextDialog::Layout 0x4176f0 remains unclaimed of the big code**. Full
  per-session milestone history in PLAN_COMPLETED.md. ~90 % after E, 100 % = G's whole-image
  build. Track effective-match bytes separately (they count for G, not for %).

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
   done items removed, next steps concrete); demote the old pickup to a condensed ⏮ PRIOR
   block APPENDED TO PLAN_COMPLETED.md (since v28, CLAUDE.md carries only the current pickup —
   distill any new mechanism into the numbered KEY lessons / MFC / Ghidra-gotcha lists instead);
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

### ⏭ NEXT SESSION PICKUP (2026-07-07 v29 — TextDialog cluster 6/7 done; 97.56% coverage)
**▶ v29 RESULTS (commits 9d23a75, ad9a30f): the plain-class game TextDialog (speech balloon,
sizeof 0xc8, NOT Dlg.h's CTextDialog) is modeled + 6/7 functions transcribed. GameView TU =
73/121 markers; coverage 97.56% (was 95.64% v28). Ghidra: TextDialog struct (0xc8) + 7 names
synced, YodaDemo ACTIVE, run_script_inline works, saved.**
- **Struct pinned from the ctor + every field access** (in GameView.h): 5 RECTs
  (rectBox@0x5c / rectClose@0x6c / rectUp@0x7c / rectDown@0x8c / rectText@0x9c), line
  counters (nTotalLines@0xac, nScrollLine@0xb0 starts at 5), nMode@0x54 (0=world-relative,
  else screen), two view-ptr copies (pView2@0xb4, pParentView@0xc0), strText@0xb8.
- **Ctor 0x416b90 EXACT** (161B). **Position 0x417570 eff.** DIFF(98): the shared
  `goto do_layout` + tail-call to Layout fixed the structure (312→152 align); residual is
  the cmp-direction family (two clamp inversions helped, nViewRight didn't) + one reg
  rotation. **ScrollTextLine/ScrollTextLine2/UpdateDialogButtons DIFF(6-7)** — insn-identical
  but for ONE uniform pParentView-load schedule shift (a local cache made it WORSE — the
  orig re-reads). **UpdateDialogButtons takes a dead 4-byte stack arg** (ret 4, callers push
  1) → `(int nUnused)`. **Run 0x416c40 eff.** 622/622 insns — full structure (the < 0x101 /
  < 0x112 message-range ladders + WM_KEYDOWN wParam switch + the WM_LBUTTONDOWN 4-rect
  PtInRect nest with shared dispatch:/rbtn: cross-jumps); residual = this-in-ESI vs the
  original's EDI (the esi/edi this-swap allocator family) + its scheduling ripple.
- Added `g_pszDialogFont` @0x4561cc (CreateFont face-name global, distinct from
  g_pszFontName@0x456130).

**▶ START HERE (v30): TextDialog::Layout 0x4176f0 (1419B) — the ONE remaining cluster
function + the LAST big unclaimed app code.** Struct + full decompile already understood
(Ghidra plate on 0x4176f0 has the map). It: CreateFont; fills rectBox; computes rectText;
RoundRect the frame; MoveWindow the child CEdit; builds a **CPoint[3]** tail triangle
(Polygon fill + CDC MoveTo/LineTo outline) — **0x4186e0 IS `CPoint::CPoint()`, an out-of-line
empty ctor (`mov eax,ecx;ret`) the array-construction calls 3× at 0x417857** (declare a point
type with a NON-inline empty default ctor so the 3 calls emit; plated in Ghidra); then a
**switch(nVisibleLines) with a 5-entry jump table @0x417c78** (arms 1 / 2-4 / 5) that
SetWindowPos+EnableWindow+ShowWindow the 3 CBitmapButtons into the bubble corner and fills the
button rects @0x6c/0x7c/0x8c. Watch the shared `switchD_default` tail (ReleaseDC+DrawGameArea+
WM_SETREDRAW(1)+ShowWindow) that most arms fall into. Expect effective (jump table + this-reg).
1. Then Phase F audit: FUN_00407d90/FUN_00407dc0 (gap before Canvas TU head), the first app
   TU (0x401000–0x401450: InvItem/MapZone/WorldgenZoneEntry ctors + CObject no-op COMDATs),
   InvScrollBar ??_G/??1 (0x408690/0x4086b0, PARKED).
2. Then de-dup **step 6 (World, ~102 field reconciliations)** or the G1 joint passes.


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
  15. **Block layout is trace-driven and mostly NOT source-steerable (the block-sinking family).**
     cl 4.2 defers any block ending in an unconditional transfer, prefers fall-through continuity
     (`if (c) goto L;` with L not yet emitted inlines L as the fall-through), and places a
     switch's merge-tail after whichever arm it makes the fall-through predecessor (usually
     `default:`). When the only residual is such placement, annotate EFFECTIVE + park (v8/v9/
     v20/v26 evidence; probes on default-position and break/fall-through were all inert).
  16. **Per-label jump-table indices.** cl assigns a table arm-index per case LABEL in value
     order — grouped labels (`case 0: case 2:`) get distinct indices at the same arm address; an
     explicit empty `case a: case b: break;` widens the byte table (its indices point at the
     exit block); a LONE empty case folds away (max label drops); duplicated arm bodies never
     merge (+insns). Read the dword table to recover the source labels (v22).
  17. **memcpy/memset intrinsics are operand-provenance- and value-tracked.** Lone `rep movsb`
     iff BOTH memcpy args are struct-FIELD loads (a value-local copied from a field keeps
     field-ness); any param/global/call-result/deref-of-&field operand ⇒ the movsd+movsb split.
     A provably 4-aligned count drops the movsb tail (LONE movsd); the tracking dies across
     calls/spills; `n&3` does NOT drop the movsd phase (v25, probe-proven).
  18. **Cross-jump geography.** A bare `return CONST` merges into the function-end epilogue only
     as a BRANCH TARGET (write `if (c) { ...; return K; } return K;`); nested fall-out of two
     scopes = an inline epilogue copy. Shared trailing stores are written PER CASE and
     cross-jumped into one block; paths that skip the store plain `break` to a single
     post-switch call (never `F(); return;` copies — full epilogues). Duplicated call-in-each-arm
     source cross-jumps the common tail leaving per-arm constant pushes (v14/v24/v25).
  19. **Aliasing dictates locals.** If the original reloads pWorld/table pointers per statement,
     the source had NO caching locals — stores through pointer lvalues (RGBQUAD*/char*/struct*)
     may alias them, so cl must reload each time (CyclePalette: 300 insns of per-statement
     [this+0x44] reloads = plain member expressions). Conversely a `T *p = &field;` POINTER
     local is real source (the lea+spill+deref shape, v13 save/load + v24 ammo-refill families).
     Match the reload pattern you see, not your taste.
  20. **Short results & POINT overlays.** GetTile/GetZoneCell results route through int/short
     locals with (short) casts (movsx; -1 = empty; a ushort local makes ==-1 dead code, v20).
     Adjacent int x/y fields ARE the POINT/CPoint at call sites: pass `*(POINT *)&nMouseX` (raw
     ::PtInRect arg) or `*(CPoint *)&nMouseX` (CPoint param) — a real POINT local costs 8 frame
     bytes + stores (v25/v26).
  21. **Vtable DATA-xrefs are identity proofs.** Thin ??_G/??1 dtors and tiny overrides
     byte-match anything — the vtable-slot RELOC is the identity (v23 pinned six COMDATs that
     way; v28 pinned the sliders' empty `RET 4` DoDataExchange by its slot delta vs StatsDlg's
     known DDX slot). An unexplained tiny function referenced from a dialog vtable = an empty
     override that does NOT call the base.
  22. **Stubs can poison silently (the de-dup lesson).** A partial/shifted stub can byte-match
     leaf functions while corrupting others: GameData's shifted MapZone fed off-by-4 grid
     displacements into StartGame's residual for days (DIFF 254→79 once fixed), and its
     GameView stub mis-widened ZoneTransitionStep's short arg as "OnWalk(int,...)". When a WIP
     shows systematic displacement/width deltas, audit the stub against the real layout FIRST
     — and prefer promoting the real shared header (docs/dedup-plan.md) over growing the stub.
- **MFC vtable calls** (e.g. `CFile::Read`): VC4.2 rejects the `__thiscall` keyword on free funcs/typedefs.
  Model the class with N dummy `virtual` methods so the real one lands at the observed vtable offset
  (`Read` = slot 15 = `+0x3c`); call it as a normal virtual. Works — see the CFile stub in
  `src/Records/RecordClasses.h` (src/Dta, the original example, was retired in v15). Non-virtual
  `__thiscall` helpers (e.g. record loaders) → model as member functions (implicitly thiscall); the
  call is a masked relocation so the exact target/name is irrelevant to the match.
- **Completion-% endgame** (user goal): the rigorous version is an **asm-first build** — disassemble all,
  assemble+link a byte-identical EXE with link.exe 3.10, then swap asm→C keeping it identical. Heavy infra;
  `tools/progress.py` gives the byte-% now without it.

**⭐ MFC-matching lessons (consolidated; distilled from the v2–v14 sessions, logs in PLAN_COMPLETED.md):**
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

**Ghidra struct-edit + write gotchas (consolidated):**
- `modify_struct_field` silently NO-OPs field renames — use `run_script_inline` `setFieldName`
  (confirmed twice, v22/v23). `modify_struct_field_type` clobbers the field NAME — restore with
  modify_struct_field `field_name="offset:0xN"` (its renamer auto-prefixes p/n onto names).
- `recreate_struct` force IGNORES offsets (packs from 0) and auto-prefixes names;
  `remove_struct_field` on packed structs DELETES the bytes and SHIFTS the tail. Never use
  either on offset-precise structs — `run_script_inline` + `replaceAtOffset` is the tool.
- `Structure.deleteAll()` leaves a notional length-1 that VANISHES on the first grow — grow
  with `while (getLength() < size) growStructure(size - getLength());` (one-shot arithmetic
  leaves the struct 1 byte short — the v28 StatsDlg trap).
- Audit key structs for `-BAD-` dangling field types (v26: GameView.pWorld/m_pDocument) — they
  silently degrade every dependent decompile to raw `*(int*)(p+off)` math and CAUSE
  misidentifications downstream.
- `run_script_inline` can wedge on a phantom broken script cached in the MCP plugin's MEMORY
  (v26) — restarting Ghidra clears it (v28: confirmed working again). The compile-error noise
  from old ~/ghidra_scripts *.java files in every run is normal; check for your own println.
- HTTP writes: JSON bodies only; rename key = `"function_address"`, plate key = `"address"`;
  `program=` is honored on READS only — writes always hit the ACTIVE program (verify with
  list_open_programs first; see the WRITE GOTCHA at the top of this file).
