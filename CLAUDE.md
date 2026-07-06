# Yodecomp — decompilation cheat sheet

In general, you can adhere to patterns found in `OpenJKDF2`, located at `~/workspace/OpenJKDF2`. CMake should be used, and the assumed build platform is macOS and Linux. Claude is permitted to modify this file with any useful notes that will aid other/later Claudes. Use `wine` to invoke Windows toolchains and executables.

### External references
- **`~/workspace/DesktopAdventures`** — the user's own engine recreation of the *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures). Invaluable for asset-format and game-logic semantics when naming functions/structs. Notably `scrdoc.txt` = reverse-engineered **script opcode format** (pre-script conditions like `BumpTile`, `CheckEndItem`, `EnemyDead`, `HasItem`, `HealthLs`…), plus `SCRIPTS.md`, `README.md`. Use it to name `.DTA`/zone/script parsing code. The 10×10 grid at World+0x4B4 (stride 0x34/zone) is the map's zone grid.
- `~/workspace/OpenJKDF2` — style/naming conventions (`Module_Function`, loose-Hungarian), CMake layout.

In general, variable names should follow a loose-Hungarian Notation, where pointers start with `p` (ie, `pThing`), pointers to arrays are prefixed with `pa` (ie `paIndices`), booleans are prefixed with `b` (ie, 'Main_bMotsCompat'). Name a pointer after the **struct it points to**, not its MFC role: the `World`(`CDeskcppDoc`) pointer is **`pWorld`**, NOT `doc`/`pDoc` (e.g. `GameView.pWorld@0x44`); a `GameView` pointer is `pView`, etc.

**Function naming = C++ `Namespace::Method` (migration DONE 2026-07-05).** This is a C++/MFC app, so group
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

## Decompiling

A decompiler instance can be accessed via `http://localhost:8089`, which is running an instance of https://github.com/bethington/ghidra-mcp. The binary that should be accessed is `YodaDemo.exe`, which can also be found at `YodaDemo/YodaDemo.exe`.

**CRITICAL GOTCHA:** The Ghidra project has *multiple* programs open (JK.EXE, DroidWorks.exe, YodaDemo.exe, …) and the *default* active program is **JK.EXE**, not YodaDemo. The HTTP endpoints are stateless — `switch_program` does **not** persist across calls. You **must** append `program=YodaDemo.exe` to *every* request, or you will silently be reading JK.EXE (a different game). The `sithThing_*`/`jkGame_*`/`Video_*` names you see without the param are JK.EXE, not Yoda Stories.

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

The five original requirements are reorganized into a dependency-ordered plan. **The critical early milestone is proving the toolchain via a bytematch on a trivial function BEFORE investing in mass decompilation.**

### ⭐ Prior art — the trail is already blazed (USE THIS)
**LEGO Island (1997)** was built with the *identical* config: **MSVC 4.20 + static MFC + Win32 GUI game + tools under wine.** The **isledecomp** project (github.com/isledecomp) solved exactly our problem. Adopt their approach wholesale:
- **`reccmp`** — their address-anchored, relocation-aware function comparator. Source functions get a marker comment `// FUNCTION: YODA 0x401230`; build the project with cl 4.2 (add **`/Zi`** — debug info does NOT change codegen but gives reccmp the recompiled addresses via PDB); reccmp diffs each function against the original at its recorded address and reports per-function match %. **Comparison is anchored by address, not layout** — so we do NOT need to solve TU boundaries / link order up front.
- **`decomp.me`** hosts MSVC 4.x compilers — use it on **day one** to experiment with matching a function *before* the local toolchain exists.
- Their wiki documents MSVC 4.2 codegen idioms — don't rediscover them.
- **Defer byte-identical whole-`.text` to the endgame.** Match functions individually first; identical layout is a deterministic end-puzzle (TU order + lib link order + masking PE timestamp/checksum).

### Phase 0 — Identify the compiler ✅ DONE
VC++ 4.2 (see table above).

### Phase 1 — Stand up the matching toolchain (unblocks everything)
1. Acquire **Visual C++ 4.2** (abandonware; WinWorld / archive.org — MSDN ISOs). ~~Also grab **4.2b** as a codegen knob~~ — **DEBUNKED (2026-07-05, KB Q156934/Q160491): 4.2b does NOT touch the compiler binaries or codegen**; it only updates MFC libs (incl. NAFXCW), SDK headers/libs, wizards, BSCMAKE. Still relevant for *library-region* matching (its NAFXCW.LIB differs), never for app codegen.
2. **Do NOT run the installer** (ancient 16-bit ACME setup — fights wine). Just copy the tool tree out of the ISO into `toolchain/`: `BIN\` (cl.exe + sibling DLLs C1.DLL/C1XX.DLL/C2.DLL/MSPDB*.DLL must stay together), `INCLUDE\`, `LIB\`, `MFC\INCLUDE\`, `MFC\LIB\`, `MFC\SRC\` (MFC source — huge help, message-map/vtable code matches nearly for free once class decls are right).
3. Run under `wine` on Apple-Silicon (Rosetta 2 + new WoW64 runs 32-bit PE tools; `WINEDEBUG=-all`). Set `INCLUDE`/`LIB` env; link with **`/INCREMENTAL:NO`** (incremental linking inserts thunks/padding). **Fallbacks** if wine fights: Docker+Rosetta linux/amd64 with wine or **wibo** (decomp community's minimal PE loader, what decomp.me uses); or a Windows-on-ARM VM.
4. Write a CMake toolchain file (`toolchain/msvc42-wine.cmake`) modeled on OpenJKDF2's `cmake_modules/toolchain_*.cmake`, wrapping `wine cl.exe` as the compiler.

### Phase 2 — Prove it: first bytematch ✅ CODE-MATCHED (2026-07-04)
**`FUN_00401490` (`World_CalcCompletionScore`) matched on decomp.me: instruction stream byte-identical
to the original.** Residual on decomp.me is ~0.5 % = the `.rdata` FP constant-pool references
(`$T175…$T196` = `1.0`, `100.0`, band thresholds), which are relocations that only resolve at final
link and are masked by reccmp — NOT a codegen miss. Winning source is in `docs/bytematch-candidate-401490.md`
(key trick: single result var `score`→EDI via an `else-if` chain + one trailing `return score`, so MSVC 4.2
can't fold `mov edi,VAL; mov eax,edi`). **TODO: record the exact decomp.me flag set that produced this.**
Compiler VC++ 4.2 is now confirmed by codegen, not just header inference.

Below is the original plan for reference:

- **Fence off the library region first** (side quest that also *definitively* pins the version): extract member objects from `LIBCMT.LIB`/`NAFXCW.LIB` of candidate versions (4.2 / 4.2b) and byte-match `memset`/`__ftol`/`__chkstk`/`__CxxFrameHandler` against the high region (0x429000+). A byte-exact match pins the version beyond argument AND lets us FID/hash-label all ~130 KB of library code so we never waste effort on it. Target shrinks to the ~163 KB app region.
- Pick a **suitable test function** from the app region: small (≤~100 B), leaf, **no FP** (avoids `__ftol`/x87), **no `switch`** (jump tables in `.text`), **no C++ EH** (no stack destructors → avoids the `push -1/push handler/mov eax,fs:[0]` prolog), minimal string/global refs. Yoda Stories is data-driven — DTA/tile/zone parsing has many tiny integer helpers. **Do NOT start with an MFC-derived method** (vtables + `CRuntimeClass` + message maps drag in data layout).
- **Likely flag set** (DevStudio 4.x Release default for static-MFC): `/nologo /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS`. Evidence already says `/GX` (`__CxxFrameHandler`) and `/MT` (static CRT). **Brute-force the small matrix** (`/O2` vs `/O1`, `/Oy` vs `/Oy-`) — compiler runs in ms; the answer applies project-wide.
- **Compare at function level, relocation-masked** (isledecomp/reccmp style): compile the one-function TU to a `.obj`, extract the function's COFF section bytes, and compare vs the exe bytes at the known address, treating every relocation site (`call rel32` fields, absolute operands to globals/strings/imports — the `.obj` tells you which offsets) as wildcards. Everything outside reloc fields must be identical. This makes link order/image base irrelevant until the endgame.

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

### App-region module map (first pass — see docs/compile-units.md)
Segmented all 534 app functions into 27 `.rdata`/`.data`-ordered blocks; the 6 **major modules**:
`GameData` (0x401ac0–0x4042b0, reads `.dta`) · `Core utils` (0x408c60–0x40a560, called by all) ·
`Game UI/view/hints/sound` (0x40a560–0x418700, 107 funcs — `OPTIONS`/tile-hints/`goyoda` cheats/`MIDILoad`;
contains the ~10.8 KB `FUN_0040b270` main window proc) · `Logging` (0x419730–0x41b2f0, `yodalog.txt`) ·
`Settings/registry` (0x41b2f0–0x41bee0, `GameSpeed`/`LScore`) · `WorldGen + .wld save/load`
(0x41c340–0x429000, 130 funcs — `*.wld`/`ASAV44`/`Find Puzzle`). Between them, small MFC helper/exception/
doc-view classes (one vtable each). Call-flow (wrapping): **UI → WorldGen → GameData**, Core shared.
Tooling: `tools/segment_cus.py`; Ghidra dumps in `toolchain/test/cu_{refs2,calls,strings}.txt`. Module
heads are marked with plate comments in the DB (search `[MODULE]`).
**Asset parser (`.DTA`) — mapped & named. See `docs/dta-format.md`.** `Dta_Load` (0x422670) is the IFF
chunk dispatcher; 12 handlers named in the DB (`Dta_ParseZone/Zaux/Zax2/Zax3/Caux/Htsp/Actn/Puz2/Snds/
Chwp/Tnam` + `Dta_ReadZone`), each `__thiscall(CDocument*doc, CFile*pFile)` with `[Dta]` plate comments.
`CFile::Read` = vtable slot `+0x3c`. Tag table @ `.data 0x456890`. Chain: `Dta_Load`→`Dta_ParseZone`→
`Dta_ReadZone`→**Zone class** (`Zone_Ctor`/`Zone_ReadData` @ 0x405150, 18×18 grid, 0x848 bytes, 6
CDWordArrays)→MFC. **ENDF triggers world generation**, not parsing: `Worldgen_Randomize` (0x424380,
RNG seed) + `Worldgen_Populate` (0x425e30). So Dta-load and Worldgen share the `Dta_Load` loop. ZONE
record format (18×18, 3 tile layers, hotspots, IACT scripts, area/map-flag enums) documented in
`docs/dta-format.md`. TODO: CHAR handler; `src/Dta/` + `src/Zone/` match modules; Zone_ReadData internals.

### ✅ DONE — `Dta_` cleanup + whole-DB namespace migration (2026-07-05)
**What `Dta_` was:** a *provisional* bulk CU-tag smeared across **0x421520–0x429142**, but that span is the
**doc TU** (`CDeskcppDoc` = `World`, `0x41c340–0x429000`) mixing four roles (`.dta` load/parse · worldgen ·
`.wld` save · doc/UI logic). **Resolution:** the whole doc TU is now the **`World` namespace** (all
`__thiscall` doc methods get correct `this=World*`) — `World::Load`/`ParseZone`/`Randomize`/`Generate`/
`Serialize`/… A separate-`Dta`/`Worldgen`/`Wld` namespace attempt was **rejected** because it broke
`this=World*` (namespace must equal the struct — see the ⚠️ note in the top-of-file convention).

**Migrated the entire function list** (idempotent `run_script_inline` loops): 62 class self-prefix strips +
607 global→namespace moves + 161 doc-TU consolidations into `World` + 7 Iact-`.obj` funcs rescued from a
wrong `Zone` this-typing → `Iact::`. Result: clean `Namespace::Method` across `World`(201)/`GameView`(208)/
`GameData`(70)/`App`(57)/`Settings`(38)/`Iact`(33)/`Mfc`(29)/`Zone`(24)/`Frame`(21)/… ; only `FUN_*`
(undiscovered), `FID_*` (MFC lib) and import thunks remain global.

**Functions identified during the sweep (behaviour + `this`-type + GDI/CFile tells):**
`GameView::DrawWeaponBox` (0x428ac0) · `DrawWeaponIcon` (0x428c40, BitBlt currentWeapon) ·
`BlitViewportDither` (0x428e30 — dithers Canvas + blits the 288×288 play viewport; the first tell-based
guess "BlitWeaponBox" was **wrong**, corrected after reading it — lesson: read the body, don't trust tells) ·
`DrawHealthDial` (0x427490, `Chord()` on `World.nHealthDial{L,T,R,B}@0x32c4..d0`) · `DrawHealthNeedle`
(0x4278a0) · `AddHealth` (0x427690, IACT cmd 0x25) · `FUN_00428680` (0x428680 — was mis-typed `World`, is a
`GameView` tile-clear method) · `Render::DrawRect` (0x424010) · `World::RestoreGridFromBackup` (0x421520 —
copies `zones[100..199]`→`zones[0..99]`, **revealing a 2nd 10×10 `MapZone` grid at zones+100**).
**Still TODO (grind):** the unexamined `World::FUN_*` bodies in 0x424280–0x428680 (worldgen internals) and
the `Iact::void*-this` readers (model the IACT record class to type them).

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

### Vtable-target function recovery — DONE (2026-07-04)
The CALL-target prepass (below) found direct-call coverage complete, but **indirect-call (virtual
method) targets were not all defined**: 67 vtables in `.rdata`/`.data` had 225 entries pointing to
`LAB_*` code (undefined functions) — MFC virtual impls + shared `ret N`/adjustor stubs + app methods.
Recovered them all with a safe scanner: find runs of ≥4 consecutive `.text`(0x401000–0x44b000) code
pointers, `createFunction` at each undefined target that is an instruction start AND not inside an
existing function (that guard skips switch-table case labels → no false positives). Result: 225 funcs
created (82 app-region, 366 lib), 0 undefined vtable targets remain. Re-run the scanner anytime; it's
idempotent.

### Prepass: recovering un-marked code — DONE, effectively a no-op (2026-07-04)
**Conclusion: Ghidra's function coverage of the app region is already complete.** Verified with a script
using Ghidra's authoritative reference data: **all 484 call-targets in 0x401000–0x429000 are already
functions** (0 in-gap, 0 mid-func). The ~13.4 KB of "orphaned" instructions (138 runs) are NOT missed
functions — they classify as: (a) many **10-byte `[c0 j0 d1]` C++ EH continuation funclets** (referenced
by one exception-table data ptr; belong to the parent function); (b) **`[j>0]` switch-case blocks**
reached by jump tables; (c) large `[d1]` runs of **switch/jump code near the giant funcs** (e.g. the
2.4 KB run at 0x40d992 near `FUN_0040b270`); (d) a few trivial **shared vtable `ret N` stubs** (e.g.
0x40e3f0 = `ret 0x4`, referenced by 15 vtable slots). None should be made standalone functions.
**EH funclets are named `<parent>_ehN` (2026-07-04).** The ~260 tiny (<0x18 B) `unaff_EBP`-frame
destructor funclets (`CString::~CString`/`CFile::~CFile`/… via the parent's frame) are C++ exception
cleanup, not real functions. Each is code-referenced from exactly one parent function's body → named
`<parentName>_eh<idx>` (e.g. `Settings_Save_eh0`, `GameData_FUN_00401ea0_eh4`) so the association is
obvious. **Detection (refined 2026-07-04):** size <0x18 AND the funclet sits **adjacent to a parent's
body** (`F.addr ≤ parent.entry + parent.size + 0x100` — funclets are emitted right after their parent)
AND that parent has a **code-ref** to F. Do NOT use raw distance-from-entry: a large parent's funclet
is far from its *entry* but right after its *body* (e.g. `Iact_RunCommands` @0x4070e0 is 0xbd2 B, so
its funclets sit ~0xbd2 after the entry — still legit). A tiny function merely *called* from a distant
function is NOT its funclet (it's a real function → CU-tag it). Isolated tiny stubs with no adjacent
code-ref parent = shared vtable `ret N` stubs → CU-tag by region. The ~65 remaining tiny funcs are shared
vtable `ret N` stubs (referenced only from `.rdata` vtables, no single parent) — left as-is.
**Pitfalls proven:** `find_code_gaps` is ~826 mostly-`0xCC`-padding noise; a **capstone call-scan gives
huge false positives** (misdecoded jump-table/data bytes → fake call targets); and `create_function` on
gap targets in tangled regions (tried 0x403501–0x40379f) yields **overlapping garbage bodies** — undone.
So: trust Ghidra's function list; do NOT bulk-create. Only optional cleanup: mark the shared `ret N`
vtable stubs as functions so vtable refs resolve to names.

**Ghidra scripts WORK** (`GHIDRA_MCP_ALLOW_SCRIPTS=1`, Ghidra 12.1.2). Earlier OSGi error
(`GhidraPlaceholderBundle cannot be cast to GhidraSourceBundle`) was fixed by clearing the OSGi cache
(`~/Library/ghidra/ghidra_12.1.2_PUBLIC/osgi/`) and restarting Ghidra. `run_script_inline` takes a Java
method body; `currentProgram`/`toAddr`/`println`/`createFunction` are in scope; use fully-qualified type
names (no imports). ghidra-mcp source: `~/workspace/ghidra-mcp` (`.../core/ProgramScriptService.java`).
**Reminder:** after a Ghidra restart, re-confirm YodaDemo.exe is the active program before any write.

**Gap-function scanner: `tools/ghidra_scripts/CreateGapFunctions.java` (2026-07-06).** Reusable,
policy-driven port of the prepass: scans [0x401000,0x44b000) for inter-function gaps, skips 0xCC/0x00
padding, classifies each run's first real byte by strongest incoming ref, and creates functions per
`POLICY` (`DRY_RUN` default). Re-confirmed the prepass empirically — of 491 non-padding gap candidates:
**CALL-targets = 0** (no missed real functions; SAFE mode is a no-op, as expected), DATA-ref = 311
(EH/vtable funclets → belong to a parent), JMP = 3 (switch cases), no-ref = 177 (jump-table bytes / dead
code). `POLICY=FUNCLETS` would promote the 311 funclets to `gap_ehlike_<addr>` functions (289 already
instruction starts, 22 need disassembly) — do this only if you want EH/vtable refs to resolve to symbols;
`AGGRESSIVE` also takes JMP/no-ref runs (likely garbage, per the reverted 0x403501 experiment). Copy to
`~/ghidra_scripts/` and run via `run_ghidra_script`, or run its body via `run_script_inline`.

**⭐ EH-funclet parenting → correct function BOUNDS for byte-matching (2026-07-06).** Two companion
scripts fix the "COMDAT length includes EH funclets" trap by making each function's Ghidra body span its
whole /Gy COMDAT `[entry .. last funclet end)`. Both were run LIVE on YodaDemo and **saved**.
- `tools/ghidra_scripts/ParentGapFunclets.java` — absorbs ORPHAN gap funclets (never made into functions)
  into their parent's body. Ran live: **151 funclets, 2097 B into 137 functions** (143 via unique in-body
  ref, 8 via the user's EBP+tail-JMP heuristic → adjacency parent).
- `tools/ghidra_scripts/MergeEhFunclets.java` — folds pre-existing funclet FUNCTIONS (the prior
  `<parent>_ehN` pass) back into their parent (delete function + union range; labels preserved). Ran live:
  **209 `_eh`-named funclets merged, 0 failures**; parents now contiguous (`ranges=1`), app-region func
  count 843→622.
- **The funclet discriminator (hard-won — 3 wrong theories before this one):** a funclet is NOT
  "never called" — MSVC destructor funclets ARE `CALL`ed by the parent for normal-path cleanup. The real
  tell is the FRAME: a funclet **never establishes its own frame** (no `push ebp` in the first ~8 insns;
  real SEH funcs do `mov eax,fs:[0]; push ebp; mov ebp,esp`) and **addresses the parent's frame** (`lea/
  mov ecx,[ebp-X]`) or is the `mov eax,imm; jmp <handler>` state shape. Parent = the unique function that
  references it (caller / EH-table site); window-guarded to the parent's COMDAT. **A human-assigned
  descriptive name (PositionMaybe, OnLoadWorld) = a REAL function → never merged** (this excluded the false
  positives). `_eh`-named merge by default; 150 auto-named (`FUN_*`/`case*`) frameless funclets are reported
  for REVIEW and only merged when `MERGE_UNNAMED_FUNCLETS=true`. App region only (`[0x401000,0x429000)`).
- ⚠ `run_ghidra_script` runs on Ghidra's Swing thread — an infinite loop **freezes the GUI** (hit once via a
  no-progress cursor bug). Always guarantee loop progress + a hard iteration cap in gap-walkers.
- `tools/ghidra_scripts/FillFunctionHoles.java` — the *mid-function* counterpart. MSVC emits C++ EH
  catch/cleanup blocks in the MIDDLE of a function's COMDAT (no normal-flow edge); Ghidra leaves them out
  of the body. Two kinds, both fixed: **(A) between-range holes** — undefined/orphan code in a gap between
  two body ranges (e.g. the 41B catch block at 0x42905b in AddItemToInv) → disassemble + union into body,
  ≤0x40B only; **(B) in-body undefined runs** — undefined bytes already inside the body AddressSet, a run
  of back-to-back `jmp`-terminated destructor funclets where a caller only disassembled the FIRST (e.g. the
  member-dtor funclets at 0x404359 in Ctor) → disassemble fully. Ran live: 14 kind-A (312B) + **28 kind-B
  runs (357B across 18 funcs)** disassembled; parents contiguous.
  **⭐ ROOT-CAUSE (kind B): the funclet scripts unioned a multi-funclet range but called
  `disassemble(rangeStart)` ONCE — which follows fall-through and STOPS at the first funclet's `jmp`,
  leaving the rest as undefined-in-body bytes. Fixed in ParentGapFunclets + FillFunctionHoles with a
  `disassembleRangeFully()` that steps instruction-by-instruction across the whole range.** Always
  fully-disassemble an absorbed range, never just its entry.
  Detection strict: 0xCC-only padding (0x00 is inside `push 0`, NOT pad), TILES cleanly (misaligned = data),
  ends RET/JMP or flows in. **A between-range hole containing `mov eax,fs:[0]` or `push ebp` is REJECTED** —
  that's a SEPARATE function whose broken/tiny body left it in the gap, not a catch block (the
  OnEraseBkgnd-inside-OnMouseMove false positive). Holes >0x40B are REPORTED not filled.
  ⚠ **KNOWN FOLLOW-UP — broken function bodies + ADJ mis-attribution.** Some real functions have
  incomplete bodies (e.g. `OnEraseBkgnd`@0x413b20 defined for only its first 6 bytes). Because their body
  is <0x10, `nearestRealPreceding` treated them as funclet-ish and mis-attached a following funclet to the
  PRECEDING real function (OnMouseMove got OnEraseBkgnd's 0x413bca funclet → a 2-range body spanning a
  whole other function). Remaining REVIEW holes (Tick@0x40d3bb switch region, WorldgenBuildQuestMaybe@
  0x41e143, WorldgenBuildZoneListsMaybe@0x41ee79) are this class or switch data — need manual RE: repair the
  broken body, then re-home the mis-attached funclet.
- **App-region boundary is 0x4292f0** (not the approximate 0x429000): the WaveMix import thunks (`jmp [imp]`)
  and MFC/CRT library region begin there. Last app funcs: AddItemToInv/RemoveItem/TmpObjCtor.

## 🗺 LONG-TERM ROADMAP (written 2026-07-05, after the Records TU — keep this current)

**The unit of completion is the TRANSLATION UNIT, not the function.** Lesson #7 + the Records coupling
matrix prove codegen state flows forward through a TU (and through class decls in its header): functions
match piecemeal only until the TU around them changes. So the plan is TU-by-TU, each TU driven to
"all functions exact-or-annotated-effective", with a single JOINT residual pass per TU at the endgame.

### App-region inventory (~128 KB, 534 funcs) and TU status
| TU / module | range | ~size | state |
|---|---|---|---|
| World scorers (doc-TU fragment) | 0x401450–0x401ab9 | 1.6 KB | 4/6 exact; CalcTimeScore unstarted, CalcSolvedScore x87 park |
| **GameData** (2nd doc-TU src file) | 0x401ac0–0x4042b0 | ~10 KB | ✅ DONE 07-06: 11/27 exact (src/GameData/), rest effective/PHASE-DISPLACED; BQP +9B |
| **Records** (6 record classes) | 0x4042b0–0x405ae0 | 5.5 KB | ✅ DONE 25/33 exact + 8 annotated eff. |
| Iact (`.obj`: Zone readers + IACT) | 0x405ae0–0x407de0 | ~9 KB | named, IactScript struct modeled; RunCommands = 3 KB switch |
| **Canvas** (DIBSection blitter) | 0x407df0–0x4084e8 | 1.8 KB | ✅ DONE 8/11 + 3 eff. (parked) |
| Small MFC classes (Frame/Dlg/InvScrollBar/TextDialog/…) | scattered | ~8 KB | RE'd; MFC-source-assisted |
| Core utils | 0x408c60–0x40a560 | 6.5 KB | shared leaf helpers — high piecemeal match odds |
| **GameView TU** (view/UI/AI monster) | 0x40a560–0x418700 | ~57 KB | RE'd (Tick/main loop/AI); struct partial |
| Logging | 0x419730–0x41b2f0 | 7 KB | named (`yodalog.txt`) |
| Settings/registry | 0x41b2f0–0x41bee0 | 3 KB | named; string-anchored, easy |
| **World/doc TU** (dta-load+worldgen+wld+doc) | 0x41c340–0x429000 | ~52 KB | RE'd; World struct partial (~115/197 named) |

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
- **Milestones** (progress.py %exact): 7.02 % after A (actual, 2026-07-06), ~15 % after B+C, ~55 %
  after D, ~90 % after E, 100 % = G's whole-image build. Track effective-match bytes separately
  (they count for G, not for %).

### ⏭ NEXT SESSION PICKUP (2026-07-06 — GameData CLOSED, start Phase B)
**Phase A (GameData CU) is COMMITTED & CLOSED** (commit after d5925d8 = this session's wrap-up).
State: 11/27 exact per-NAME (verify.py prints 14 — its positional pairing over-credits the clone
families); progress.py **7.02%**. Residuals all annotated in-source (EFFECTIVE MATCH or
PHASE-DISPLACED); BuildQuestPath +9B/8 loci, cracks documented at its marker in GameData.cpp.
Ghidra backport done: `World::FindAdjacentGateDirMaybe` (0x419f60) + `World::GetGridOrderMaybe`
(0x421e50) both __thiscall/this=World* (members with UNUSED this — the ORIGINAL reloads ECX at
every call site, which is how the member-call form was proven); `BuildQuestPathMaybe(short*
paGrid, short* paOrder)`. The TU-phase dial + verification traps are folded into "Standing rules".
Current dial in WorldStub.h = 4 real decls (UpdateScore/GetZoneCell/CalcTimeScore/
RestoreGridFromBackup) + note.

**NEXT (in order):**
1. **Phase B — Iact TU** (0x405ae0–0x407de0, ~9 KB): Zone deserializers (Iact::ReadZaux/Zax2/3/4)
   + the RunCommands interpreter (3 KB mechanical switch; `scrdoc.txt` in
   ~/workspace/DesktopAdventures = opcode bible). Quick-win warm-up inside the phase: the
   Iact-script record TU (0x418700–0x418dd0) is a Records-clone (IactScript/IactCondition/
   IactCommand, structs 100% modeled) — likely near-free match with the src/Records recipe.
   Remember lesson #7: ParseZaux-family needs the FULL TU present.
2. **asmscore/permute funclet fix** before any further permuter run: split candidate + original at
   funclet boundaries (first ret), mask relocs per-instruction per-side (offsets diverge once
   lengths shift).
3. Phase C warm-up sweep (Core utils/Settings/Logging + parked World scorers) per the roadmap.

### ⏭ PREVIOUS PICKUP (2026-07-05, late-night — still-valid facts below)
**GameData CU effectively DONE except BuildQuestPath** (decomp cached at $CLAUDE_JOB_DIR/tmp/questpath.c
may be gone — re-dump from Ghidra 0x403c80). Progress **6.70%**. Session results (commit a4ba541):
- **Savers (x3): EFFECTIVE MATCH** — structural convergence via main-body disasm diffing. Cracks:
  inner-scoped `{ CString key = prefix + buf; Write...; }` (a bare op+ temp gets a frame-BOTTOM slot,
  named+scoped key slots among the locals -> frame layout matched); duplicated full-sprintf if/else arms
  (orig cross-jumps the common tail); `n >= 0` emits test/jl only when n lands in ESI. Residual: int-slot
  3-cycle {lineNo,base,rem} -> arm2 grabs EBX -> cross-jump fails (+16B). All source knobs exhausted.
- **⚠ SCORING TRAP (cost the whole permuter run): COMDAT trim length INCLUDES EH funclets; slicing the
  exe at [addr, addr+COMDAT_len) and byte/asmscore-diffing is GARBAGE for EH functions** (savers showed
  align=1368 noise; the hill-climb chased phantoms). Compare main-body-to-main-body (split at first ret;
  Ghidra body vs funclet layout differs from COMDAT order). TODO: teach asmscore/permute.py to split at
  funclet boundaries and mask relocs per-side (candidate reloc offsets are WRONG for the orig once
  lengths shift — mask immediates per-instruction instead).
- **PROVEN: the ORIGINAL binary has TU phase drift** — its three identical-source loaders emit the
  backedge cmp as jg/jl/jg (Nevada/Alaska/Oregon); ours = jl x3. Not source-controllable (2^3 while-form
  sweep inert; flipping N's form toggled O's phase but never the target site). Loaders N+O carry 2B each
  = effective. Also: verify.py's pairing MIS-PAIRED the identical-length loader triplet earlier (Oregon
  "MATCH" was false) — trust per-NAME diffs for clone families.
- **src/Records/RecordClasses.h**: the six matched record classes now shared (user rule: NEVER stub a
  matched module — promote to the real header). Records 26/33 (+Tile::Ctor, from adding Zone's
  ReadSavedState/WriteSavedState decls — header decls shift TU state; residual diffs shuffled, still 7).
- **GameView documentation sweep LANDED (agent, saved in Ghidra)**: ~50 struct fields renamed/added
  (nTransitionStep@0x118 answered the StartGame mystery; nTargetZoneId@0x114 confirmed), ~60 function
  renames (UpdateFrameMaybe->OnTimer, OnWalk->ZoneTransitionStep, PlayerMove->PlaySound(!),
  PlayerCheckWalkable->DrawZoneCell(!)), message map @0x44b240 fully mapped, sizeof(CView)=0x40 /
  ctor=0x408710 / vft=0x44b638, new classes InvScrollBar/BalloonButton/BalloonBitmap/DebugDlg/option
  dialogs. ⚠ OnKeyDown (0x4150f0) body not fully claimed by Ghidra (0x41526f-0x415658 orphaned,
  FUN_004156f2 = its split EH tail) — needs a body-repair pass. Old CLAUDE.md aliases (PlayerMove/
  OnWalk/UpdateFrameMaybe/UseTile) are STALE — grep the new names.
Next: (1) BuildQuestPath transcription (last GameData func, ~1326B), (2) Phase B Iact TU, (3) the
asmscore funclet fix before any further permuter runs.

### ⏭ PREVIOUS PICKUP (2026-07-05, superseded but facts still valid)
**⭐ STATIC-MFC LINKAGE IS STOOD UP (2026-07-05).** `toolchain/bin/link` + `NAFXCW.LIB` verified end to end
(`toolchain/test/mfctest/` links a `CWinApp`+`CDWordArray` app clean). This unblocks per-function matching
of **MFC-derived app classes** (`World`=`CDeskcppDoc`, `GameView`=`CDeskcppView`, and Zone's `Ctor`/`Dtor`):
model the class `: public CObject`/`CDocument` with real MFC members so the compiler emits the member
ctor/dtor codegen (base/member calls are masked relocations). Recipe + the linker-version-3.10-vs-4.20
endgame flag are in `toolchain/README.md`. **GOTCHA that cost a detour:** run `bin/cl`/`bin/link` DIRECTLY,
never `wine bin/cl` (they're bash wrappers that call wine internally; double-wrapping fails silently and you
then read a stale `.obj`). **Zone matching (2026-07-05):** `GetTile`/`SetTile` exact; `GetEdgeCode`(6)/
`FindObjectAt`(7)/`FlagQuestObjects`(5) are reg-alloc effective matches awaiting the full-TU (needs the new
MFC linkage for `Ctor`/`Dtor`). Byte-matching found a real bug: **`ZoneObj` was mis-modeled** — true layout
`type@4`(uint)/`state@8`/`x@0xa`/`y@0xc` (fixed in Ghidra + `src/Zone/Zone.h`).

**RUNTIME ENGINE now heavily documented (2026-07-05 sessions).** The gameplay/render code is mapped end to
end — see `docs/game-logic.md` (esp. the **Main frame loop & state machine** + **enemy AI** sections),
`docs/worldgen.md`, `docs/settings.md`, `docs/structs.md`. Key anchors:
- **Main loop:** `GameView::UpdateFrameMaybe` (0x40d470) → `switch(World.nFrameMode)` (the 1..8 `FrameMode`
  enum @World+0x5c, was `bIactBusy`): 2=play (player-move dispatch via `nMoveCommand` 0x21-0x28 → OnBumpTile),
  3=dialogue, 6/7=zone transition, etc. Drives `GameView::Tick` (0x40b270, per-entity **enemy AI**: switch on
  `Character+0x36` `CharMoveType`, 4=chase/1-2-7-8-9=wander/…) + `CyclePalette` (0x415af0) + `DrawGameArea`.
- **UI:** inventory scroll (`InvScrollBar`), weapon box / health dial / direction arrows (`DrawDirectionArrows`
  + `GameData::GetExitDirections`), the **options slider dialog** (`OptionsSliderScroll{67,8f,90}Maybe` +
  `EnableOptionsControls*`), `OnDragItem` (R2-D2/`strArtooHelp` hint system + weapon use), `TextDialog` class.
- **Zones/save:** `TransitionZone{XWing,Door,Script}` + `Backup/RestoreRecords` + `BackupZoneGrid`/
  `RestoreGridFromBackup` = the sparse-`.wld` per-zone snapshot mechanism. `World::LoadWorldMaybe` (0x421fd0).
- **Namespace sweep:** `World` (115/197 named) + `GameView` substantial funcs all named; worldgen placement
  family named (`WorldgenPlaceRandInZone*Maybe`, etc.). Remaining `FUN_*` are <0x40 funclets/stubs. `Maybe`
  suffix marks honest guesses to sharpen (grep it). **Lessons this session:** a func calling a shared
  subsystem (palette/settings) says nothing about its *own* role — trace the primary caller (that's how the
  real main loop was found); read the body before naming; and I twice had to *correct* my own labels
  (Tick≠main-loop, AI switch≠flags>>16) — verify field offsets against the struct.
**Byte-matching (the original goal) has NOT advanced this run — it stayed at ~1.45%; this run was pure RE/docs.**

**Canvas CU is DONE** (`src/Canvas/`): **8/11 byte-matched + 3 effective matches** (`Init`/`Clear`/`BlitMasked`,
reg-alloc-only residuals of 22/2/4 B, annotated `// EFFECTIVE MATCH` in-source). Overall progress **~1.45%**
(`tools/progress.py`).
**✅ Priority #1 DONE this session — the register-rename-aware permuter scorer** (`tools/asmscore.py`, wired
into `permute.py` as the oracle; + a `--mode cmp` comparison-flip hill-climb). Capstone + Needleman-Wunsch,
4-tier graded score (align / reg_pen / identity_miss / byte_diff). Turns the flat byte-diff plateau into a
gradient AND diagnoses whether a residual is scheduling/instr-selection (`align>0`) or pure register-alloc
(`align=0, reg_pen>0`). Verified on Canvas/World/Zone/Dta (int, x87, MMX). See the Permuter TODO list for the
full write-up + the parked-function movability results (GetEdgeCode cmp knob proven inert; FindObjectAt needs
mid-body decl hoisting; ParseZaux needs full TU).
**⭐ RECORDS TU DONE THIS SESSION (2026-07-05 v3, src/Records/): the six-class record `.obj`
(0x4042b0–0x405ae0: Puzzle/Character/MapEntity/Tile/ZoneObj/Zone) — 25/33 byte-exact (~3.7KB), every
function structurally recovered.** First-compile matches: Puzzle::Read (486B, `tag[4]=0`+intrinsic-strcmp
+ one memset'd 0x800 text buffer), Character::Read (144B), GetProjectileTile (869B). Cracks worth reusing:
`unk38=-1` placed AFTER an arg-consuming store → forced immediate (Init); nested `int id = e->charId`
local → single movsx serving two range tests (both entity funcs); `int` vs `short`/`ushort` of a local
decides zero-reg reuse vs imm compares (`drop`); SetTile val param is `short` (=> 2-byte `push -1` at
call sites); tail-merged `return 1` = nest the whole drop logic under `if (numItems != 0)`. TU-context
effects PROVEN live: adding later functions/decls flipped GetFrameTile DIFF2→MATCH and FindObjectAt
13→7→2, but ALSO Tile::Ctor MATCH→22 — **the TU is one coupled allocator system; the 8 residuals
(Puzzle/MapEntity/Tile ctors, FindObjectAt(2), GetEdgeCode(6), Zone::Dtor(12), DamageEntityAt, HitEntityAt
— all annotated in-source) are allocator/scheduling tie-breaks to resolve JOINTLY at endgame, not
piecemeal (whack-a-mole proven).** 0x405320 identified: `Zone::EhDeleteHelper` (shared new-cleanup EH
funclet target). asmscore.py got the /D_MBCS include-scan fix (was mis-scoring MFC TUs).
**Best next moves, in priority order:**
1. ⭐ **Match a new CU — the Dta/Zone parsers are ripe** (structs already modeled in docs/structs.md &
   dta-format.md): `Dta_Load` (0x422670) + its 12 named handlers, and the `Zone` class (`Zone_Ctor`/
   `Zone_ReadData` @0x405150). Write `src/Dta/` + `src/Zone/` matching modules à la `src/Canvas/`. Now that
   the graded scorer exists, use `asmscore.py` on each to triage reg-alloc vs structural before investing.
   Expect reg-alloc to be TU-context-dependent (lesson #7) — may need most of the `.obj`'s funcs present.
2. **Extend the permuter** (scorer now provides the gradient): mid-body declaration hoisting (unblocks
   `FindObjectAt`), parallelized wine compiles, joint commutative-chain enumeration.
**References loaded & ready:** `DESKADV.EXE` (Indiana Jones' Desktop Adventures — SAME engine, 1995,
**16-bit NE / WinG / MFC**) is open in Ghidra (`program=DESKADV.EXE`, `seg:off` addrs). NOT byte-matchable
(different arch + WinG≠DIBSection blitting), but a **structure/naming cross-ref** for the data-driven modules
(DTA/zone/IACT/worldgen) alongside `~/workspace/DesktopAdventures`. **Fable** (model `fable`) is available for
planning / review / when stuck on a wall — it correctly diagnosed the Canvas residuals as unreachable
allocator artifacts and steered the `/G`-flag sweep + effective-match decision.

### Matching progress + tooling (Phase 4 underway)
- **`src/World/World.{h,cpp}`** — first matched module, written as C++ (Yoda Stories is C++/MFC; member
  functions compile `__thiscall`, matching the originals). **4/6 World funcs byte-match 100%**:
  `UpdateScore` (0x401450, 4 call relocs), `CalcCompletionScore` (0x401490), `CalcScoreFromCounter`
  (0x4016d0), `GetZoneCell` (0x401a80). `CalcSolvedScore` (0x401780) ~98% (x87 two-accumulator register
  alloc, 9 bytes) — parked; `CalcTimeScore` (0x4019c0) pending (calls `time`/`__ftol`/ext 0x42a3e0).
- **`src/Canvas/Canvas.{h,cpp}`** — DIBSection offscreen buffer CU (0x407df0–0x4084e8). **8/11 funcs
  byte-match**: `Free`,`GetData`,`GetSize`,`CreatePalette`,`SetPalette`,`BitBlt`,`Clear`… wait Clear is
  DIFF — matched set is `Free`,`GetData`,`GetSize`,`CreatePalette`,`SetPalette`,`BitBlt`,`Fill`,**`BlitFast`**.
  **EFFECTIVE MATCHES (parked, register-alloc-only residuals, semantically identical)** — NOT blit-coupled
  (verified by removing the blits): `Init` (0x407df0, **DIFF22**, scheduler placement of the height CSE-temp
  load + `push edi` timing), `Clear` (0x408040, DIFF2, `y` (user var) vs the inlined-memset `width` CSE-temp
  colored to the swapped EBX/ESI pair — Fill matches only because its `value` param shifts reg pressure),
  `BlitMasked` (0x408240, DIFF4, destX-vs-canvasW `movsx` emit order). **`/G` flag ruled out (2026-07-04):**
  swept `/G3 /G4 /G5 /GB`+default — default/`/G3`/`/G4`/`/GB` are IDENTICAL (8/11), **`/G5` (Pentium) is
  catastrophically worse (1/11)**, so the binary uses the default (non-Pentium) scheduler, which we already
  have; the residuals are within-model allocator-priority artifacts, not a flag miss. **Fable consult:** these
  are MSVC-4.2 allocator layer-2 (usage-count priority, first-def/temp-number tie-breaks) fed partly by
  TU-position state — not reliably source-steerable. Joint commutative-chain enumeration of BlitMasked's dst
  (24 variants: all `{pData,destX,w*dy}` orders × both mul orders × groupings) did NOT flip it. Decision
  (isledecomp/reccmp precedent): count as effective matches, revisit at the full-TU endgame (allocator context
  shifts then anyway — lesson #7). 28 residual bytes vs ~126 KB unmatched = not worth over-fitting now. **Init CSE win (from a DIB-init lead):** the
  original CSEs `&biHeader` into EDI for BOTH the `biSize` store (`mov [edi],0x28`) and the
  `CreateDIBSection` `BITMAPINFO*` arg → model it as `BITMAPINFOHEADER* h=&biHeader; h->biSize=...;
  CreateDIBSection(...,(BITMAPINFO*)h,...)` (biSize via `h`, other fields via `biHeader.`); this killed the
  `=0`-store grouping (DIFF40→22). The canonical MS/Quake2 field order (`biSize,biWidth,biHeight,…`) tested
  *worse* — this binary emits biCompression-first, so it's the CSE that matters, not the field order.
  **Init source-shape hypotheses EXHAUSTED (2026-07-05, ~10 more probes):** (a) *store order is
  source-faithful* — reordering assignments reorders the emitted stores 1:1 (natural/stdBmp order scored
  asmscore align=346 vs baseline 36), so the original's weird field order **IS what the dev typed**;
  (b) *memset ruled out* — MSVC 4.2 always emits `mov ecx,0xa; rep stosd` (+`push ebx`), never
  decomposes/DSEs it; (c) *abs ruled out* — `-abs()` intrinsic align=334, ABS-macro ternary align=266;
  the two-store branch form (`biHeight=height; if(0<height) biHeight=-height;`) already byte-matches;
  (d) *pointer-for-everything ruled out* — `h->` on all fields makes MSVC address via `[edi+off]` not
  `[esi+off]` (22→51 B); `[edi]`-only-for-biSize = the existing hybrid, keep it; (e) the 22-B residual
  (push-edi/lea slot + height-load slot, insns 65/65 identical) ignores decl position and a multi-use
  `int ht=height;` temp (copy-propagated away). Pure scheduler tie-break — do NOT re-litigate from
  source; full-TU endgame only. **Hidden-TU-influence family also exhausted (2026-07-05):** (f) a vanished
  helper *before* Init (unreferenced static / uncalled inline) has ZERO effect — non-emitted code carries no
  state, so lesson #7's TU-context effect comes only from *emitted* predecessors (Init is its CU's first
  function → fresh state, same as ours); (g) a helper *inlined into* Init leaves a visible fingerprint (the
  ptr param becomes the `[edi+off]` addressing base) — original lacks it; (h) dead code / unused locals /
  `register` hints inside Init: normalized away, bit-identical output; (i) the whole /O axis (`/O1 /O2 /Ox
  /Os /Ob0 /Oa /Ow`) produces IDENTICAL output for this function; (j) compiling through a `/Yc`+`/Yu`
  stdafx.h **precompiled header**: identical; (k) **VC 4.2b ruled out by documentation** — the patch never
  touched the compiler (see Phase-1 note). Only remaining suspect: a different cl point-build (e.g. 4.1/
  10.10) — weak, since 23 functions already byte-match under our 10.20. **PARKED (user decision 2026-07-05):
  plausible-but-unproven that Canvas was a separately-built/linked library CU (would explain the hand-`_emit`
  MMX craft + the scheduler residuals being confined to this CU, possibly a different cl point-build). Do
  not dig further until ~99% completion; Canvas stands at 8/11 exact + 3 effective. **The MMX blits were the hard win** — see lessons below.
- **BIG discovery — the accelerated blits (`BlitFast` 0x408110, `BlitMasked` 0x408240) are HAND-ASM in
  BOTH branches**, selected by the `App_bCpuHasMMX` global (0x459e28, was `Canvas_bUnk`): a scalar
  copy loop AND an MMX (`movq`/`pcmpeqb`/`pand`/`por`) loop. Since VC++4.2's inline assembler predates MMX
  (rejects the mnemonics), the dev emitted MMX opcodes as raw bytes; we reproduce with `__asm { _emit 0xNN }`,
  **one `_emit` per line** (multiple per line = C2400 syntax error), each annotated `// movq mm0,[esi]`.
  Structure: C setup (dst=pData+destX+canvasW*destY, clip, rows) then `if (App_bCpuHasMMX!=0){__asm MMX;
  return;} __asm scalar;` (MMX inline, scalar out-of-line via the `!=0` JZ). Gotchas that mattered:
  (a) **`keyq`** (BlitMasked's zeroed MMX color-key qword read at `[ebp-0x18]` via `_emit`) is write-only in
  C so DSE drops it → the `movq` reads garbage & the frame shrinks; mark it **`volatile`** to pin it at -0x18.
  (b) The `_emit` modrm hard-codes EBP offsets, so the C locals (s,dst,rows,cw/stride,keyq) MUST land on the
  original's exact slots — get the local *count* right first (frame-size byte is the canary), then coloring.
  (c) scalar loop uses a running `ebx` index reset each row (`srow:` label BEFORE `xor ebx`); last unrolled
  unit omits its trailing `inc ebx`/`add ebx,4`.
- **COLORING TRICKS that cracked the setup (statement order, NOT decl order — decls are offset-locked by the
  asm):** assign `s = src;` *before* declaring `rows` (fixes the dst accumulator reg + store timing:
  BlitMasked 34→4, BlitFast 35→7); declare `stride` *before* `cw` (colors stride→EBX: BlitFast 7→0). These
  are the "statement-reordering" permuter TODO done by hand. The residual load-order/grouping (Init/Clear/
  BlitMasked) is deeper scheduling that resisted every expression/order variation tried.
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