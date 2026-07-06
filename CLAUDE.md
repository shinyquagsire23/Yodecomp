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

## 🗺 LONG-TERM ROADMAP (written 2026-07-05, after the Records TU — keep this current)

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
| ~~Core utils~~ = GameView TU head | 0x408c60–0x40a560 | 6.5 KB | ⚠ mislabel: GameView methods (Dtor/OnDraw/DrawZoneCell/ZoneTransitionStep) — Phase E, not a warm-up |
| **GameView TU** (view/UI/AI monster) | 0x40a560–0x418700 | ~57 KB | RE'd (Tick/main loop/AI); struct partial. InvScrollBar/option-dialogs embedded here |
| **App TU** (CTheApp + CAboutDlg + Log_Write) | 0x419720–0x419ed0 | ~2 KB | ✅ DONE 07-06 (src/App/): 15/16 exact + InitInstance eff. (CPUID hand-asm) |
| **WorldDoc TU** (doc main src file) | 0x419ed0–0x41bee0 | ~8 KB | ⭐ NEW 07-06 (src/WorldDoc/): 7/13 exact incl. the 1441B DTOR + ctor-derived REAL World class; "Settings::Save" was World::~World! ctor/OnNew/OnOpen/GetLocatorIcon WIP |
| **World/doc TU** (dta-load+worldgen+wld+doc) | 0x41c340–0x429000 | ~52 KB | ⭐ PHASE D UNDERWAY: src/Worldgen 43 funcs, ~26 exact; docs complete, zero FUN_* |

The two monsters (GameView ~57 KB + World ~52 KB) are ~85 % of the remaining bytes. Everything before
them is deliberately sequenced to FILL THEIR STRUCTS as a side effect, so the monsters become
transcription rather than research.

### Struct status board (audited 2026-07-05 — regen with the run_script_inline coverage dump)
Coverage = defined-bytes ÷ sizeof; unk = fields still named unk*/field_*/…Maybe.

| struct | size | cover | unk | state / phase to finish |
|---|---|---|---|---|
| Zone / ZoneObj / Tile / Canvas | 0x848/0x10/0x40c/0x43c | 100 % | 0 | ✅ done (byte-match-proven) |
| MapEntity | 0x64 | 96 % | 5 | ✅ good; unk10/18/20/2c/60 have no readers found (runtime-only scratch?) |
| Puzzle | 0x2c | 95 % | 3 | ✅ good; unk2/unk3/unk14 parse-only (unknown in DA too) |
| Character | 0x4c | 94 % | 3 | ✅ good; unk40/unk44 parse-only, unk48 tail pad |
| CObArray family / CDWordArray / BITMAPINFO256 | 0x14/0x428 | 100 % | 0 | ✅ modeling helpers |
| **World** | **0x33c0** | **42 %** | 12 | ⚠ the big one — asset half being filled by the Phase-A GameData sweep NOW; script/frame-mode fields in Phase B; worldgen/save fields in Phase D. Strategy: never grind it in the abstract — each TU match pulls its fields in. |
| **GameView** | **0x310** | **31 %** | 19 | ⚠ second big one — cursor/paint fields mapped this week; the rest is Phase-E prep (entity-loop, inventory, dialog fields). 19 Maybe-fields to confirm. |
| MapZone (10×10 grid cell) | 0x34 | 73 % | 5 | Phase D (worldgen semantics decide the 5 unks) |
| IactScript | 0x30 | 100 % | 0 | ✅ solved 2026-07-05: vtbl@0 (0x44bc68) + 2 inline CObArray (conditions@4, commands@0x18) + doneFlag@0x2c — Zone-pattern. Whole Iact-script TU (0x418700–0x418dd0) renamed Records-style: IactScript/IactCondition/IactCommand ::Ctor/ScalarDtor/Dtor/Read |
| IactCondition / IactCommand | 0x1c/0x20 | 100 % | 0 | ✅ vftable@0 added (0x44bc80/98); opcode@4 + args[5]@8 (+text@0x1c for commands) |
| InvScrollBar | 0x44 | 17 % | 0 | Phase E/F (MFC CScrollBar-derived — model like Records did with CObject) |
| TextDialog | 0xc8 | 4 % | 0 | Phase E/F (MFC CDialog-derived) |
| CFile (stub) | 0x40 | 6 % | 0 | intentional — DB stub only pins Read@vtbl+0x3c; real MFC used at compile time |

**Classes with methods but NO struct yet (void\* this)** — modeling TODOs: `Frame` (CFrameWnd-derived,
21 funcs), `App` (CWinApp-derived, 57), `Dlg` (dialogs), `InvScrollBar`/`TextDialog` bodies beyond the
thin structs, plus the module namespaces that may be free-function TUs (`Settings`, `Log`, `Render`,
`Iact`, `GameData` — Phase-A agent is settling whether GameData is a class or World methods + free funcs).
MFC-derived modeling recipe proven in src/Records: real base class + real members ⇒ ctor/dtor codegen free.
**Type-identity findings (2026-07-05, backported to Ghidra + Records.h): Zone.cobArray4/5 are `CWordArray`
(NOT CDWordArray — ReadZaux calls CWordArray::SetAtGrow; identical 0x14 layout so Zone::Ctor still
byte-matches, but the ctor reloc + element width differ — check genCandidateA/B in Phase D). CFile vtable:
Seek = slot +0x30 (ReadIzon seeks past mismatched records), Read = +0x3c. The Iact-script record TU at
0x418700–0x418dd0 is a Records-clone (3 CObject classes, ctor/??_G/dtor/Read each) — likely quick match
in Phase B. ReadIzon uses the same `tag[4]=0` + intrinsic-strcmp idiom as Puzzle::Read.**

### Phase plan
- **A — GameData CU ✅ DONE (2026-07-06, commit d5925d8).** Final: src/GameData/ 27 markers, 11 exact
  (incl. Nevada+Alaska story loaders, 990B each), savers/BQP/Place/FindZC = effective or PHASE-DISPLACED
  (annotated in-source); progress 7.02%. The TU-phase dial (see standing rules) was discovered here.
  Original notes: The `.dta` chunk handlers + asset accessors write `World` fields directly
  (tile/zone/character/sound/puzzle arrays @ +0x80..+0xc0 region, name lists, counts). Matching it
  forces the World struct's ASSET half to be modeled correctly — this is the cheapest way to fill World
  (user insight: don't grind the World struct in the abstract; let GameData matching pull it in).
  GameData sweep DONE (2026-07-05): planet tables = per-planet story-replay histories (registry keys
  Nevada/Alaska/Oregon = Indy-engine US-state slots for Tatooine/Hoth/Endor; demo-limited: goal=108
  hardcode, pre-seeded Alaska list, grayed Save/Load/Replay). 7 message-map handlers recovered from the
  0x4035xx gap (OnNewWorld/OnReplayStory/...). StartGame(nSeed,bSkipGenerate) RET-8 fix. CU = a SECOND
  doc-TU source file (all this=World; "GameData" stays a docs label). Remaining Maybe: BuildQuestPathMaybe
  (behavior documented — 10x10 plan-grid order assignment; purpose label = Phase-D confirm).
  Original steps were: (1) identify/rename the ~70 funcs non-Maybe (reader sweep, agent-able), (2) model the touched
  World fields + any GameData-local structs in Ghidra & docs/structs.md, (3) src/GameData/ TU in .text
  order, iterate with verify/asmscore. Watch for the TU boundary: 0x401ac0 start (after CWinApp block)
  and the 0x4042b0 end (Records).
- **B — Iact TU ⭐ NEXT.** Zone deserializers (ReadZaux/Zax2/3/4) + IactScript/commands + the RunCommands
  interpreter (big mechanical switch; scrdoc.txt in ~/workspace/DesktopAdventures is the opcode bible).
  Fills Zone/IACT runtime structs + more World fields (script state @ +0x5c/frame modes).
- **C — Warm-up sweep: Core utils + Settings + Logging (+ World scorers cleanup).** Small, mostly
  leaf/string-anchored, high exact-rate; finishes tool confidence and clears the map around the monsters.
  Include the two parked World scorers (CalcTimeScore needs `time`/`__ftol` externs).
- **D — World/doc TU.** By now its struct should be largely filled (A: assets, B: script state, plus
  existing worldgen/save docs). Transcribe in .text order; the Dta chunk dispatch + worldgen + serialize
  sub-modules are documentation sections, ONE TU for matching. Biggest single payoff (~52 KB).
- **E — GameView TU.** Last monster: needs GameView struct completion (cursor/paint fields already
  mapped this week) + all cross-TU stubs accumulated in A–D. Contains the 10.8 KB window-proc/Tick.
- **F — Small MFC TUs + message maps/vtables.** Frame/App/Dlg/InvScrollBar/TextDialog/PaletteDlg etc.
  `toolchain/vc42/MFC/SRC` makes message-map/vtable codegen nearly free once class decls are right.
- **G — Endgame.** (1) JOINT residual pass per TU: the annotated effective-matches (Canvas 3, Records 8,
  World scorers 1, …) are allocator/scheduler tie-breaks that shift with TU context — resolve them with
  full-TU permuter runs (parallel wine workers TODO) once each TU is otherwise complete. (2) Whole-image
  build: real link order (app .objs in address order, then LIBCMT/NAFXCW), .rdata/.data layout, linker
  3.10-vs-4.20 flag question (toolchain/README), PE timestamp/checksum masking, reccmp-style final diff.

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
- **Milestones** (progress.py %exact): 7.02 % after A, **13.52 % mid-D (2026-07-06 v7)**, ~55 %
  after D, ~90 % after E, 100 % = G's whole-image build. Track effective-match bytes separately
  (they count for G, not for %).

### ⏭ NEXT SESSION PICKUP (2026-07-06 v8 — Phase D: 48 funcs incl. BOTH IFF dispatchers; 13.85%)
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
**NEXT:** pickup item (4) unchanged — PlacePuzzle 0x421620 + WorldgenPlacePuzzles 0x421930,
placer family 0x41c580-0x41d660, CarveQuestPath 0x41d940, PlaceBlockades 0x41e350,
SelectPuzzle 0x41eab0, PlaceQuestNode 0x41f120, Generate 0x41f960 (now DECLARED in
Worldgen.h), save/load monsters (OnSaveWorld/OnLoadWorld/Serialize/LoadWorldStateFile),
then (5) the GameView methods 0x426c40-0x429150. Ghidra renames pending (YodaDemo ACTIVE
needed): EnterZone→GetZoneIndex, the 0x32d4 quad→view rect, 0x41c340→~CFileException.

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
**Retire src/Dta/ when its 3 addresses (0x423110/190/210) are exact here** (double markers).
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
- **MFC vtable calls** (e.g. `CFile::Read`): VC4.2 rejects the `__thiscall` keyword on free funcs/typedefs.
  Model the class with N dummy `virtual` methods so the real one lands at the observed vtable offset
  (`Read` = slot 15 = `+0x3c`); call it as a normal virtual. Works — see `src/Dta/Dta.h`. Non-virtual
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