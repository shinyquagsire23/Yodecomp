# Yodecomp — decompilation cheat sheet

In general, you can adhere to patterns found in `OpenJKDF2`, located at `~/workspace/OpenJKDF2`. CMake should be used, and the assumed build platform is macOS and Linux. Claude is permitted to modify this file with any useful notes that will aid other/later Claudes. Use `wine` to invoke Windows toolchains and executables.

### External references
- **`~/workspace/DesktopAdventures`** — the user's own engine recreation of the *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures). Invaluable for asset-format and game-logic semantics when naming functions/structs. Notably `scrdoc.txt` = reverse-engineered **script opcode format** (pre-script conditions like `BumpTile`, `CheckEndItem`, `EnemyDead`, `HasItem`, `HealthLs`…), plus `SCRIPTS.md`, `README.md`. Use it to name `.DTA`/zone/script parsing code. The 10×10 grid at World+0x4B4 (stride 0x34/zone) is the map's zone grid.
- `~/workspace/OpenJKDF2` — style/naming conventions (`Module_Function`, loose-Hungarian), CMake layout.

In general, variable names should follow a loose-Hungarian Notation, where pointers start with `p` (ie, `pThing`), pointers to arrays are prefixed with `pa` (ie `paIndices`), booleans are prefixed with `b` (ie, 'Main_bMotsCompat').

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
1. Acquire **Visual C++ 4.2** (abandonware; WinWorld / archive.org — MSDN ISOs). Also grab **4.2b** (a patch that tweaks codegen) as a backup knob if we later get 99%-not-100%.
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

### ⏳ PLAN — clean up the over-broad `Dta_` prefix (started 2026-07-05)
**Problem:** `Dta_` was a *provisional* bulk CU-tag smeared across **0x421520–0x429142** (~90 funcs). But
that whole span is **one `.obj`** — the `CDeskcppDoc`/`CDeskcppView` document TU (`0x41c340–0x429000`, see
compile-units.md) — which mixes **four distinct roles**. Only the true `.dta` chunk parsers deserve `Dta_`;
the rest are worldgen, `.wld` save, and view/doc render+logic that got swept in. Re-prefix each by its
**actual role**, decided by decompiling (behaviour + `this`-type + neighbours), NOT by its address range.

**Role → prefix, and the deciding signal:**
| Prefix | Role | Tell-tale in the decompile |
|---|---|---|
| `Dta_*` (keep) | parse a `.dta` IFF chunk | reads via the load `CFile` (`pFile->Read`, vtbl+0x3c); dispatched by `Dta_Load`; `this`=doc/World |
| `Worldgen_*` | generate world content | walks the 10×10 `World.zones` grid; RNG; `this`/arg = `World*`; no GDI |
| `Wld_*` | `.wld` save/serialize | `CArchive`/`Serialize`/`ASAV44`; read+write symmetric |
| `GameView_Draw*` / `Render_*` | view painting | params include `CDC*`; uses `CPen`/`CBrush`/`GetDC`/`BitBlt`/`Canvas`; `this`=`GameView*` |
| `Game_*` / `Doc_*` | doc/view game logic | inventory/weapon/health state on `World*`/`GameView*`, no GDI & no CFile |

**Confirmed so far (2026-07-05):** `Dta_FUN_00428ac0`→`GameView_DrawWeaponBox` ✓. Still `Dta_`-tagged but
identified: `0x424010`(CDC*,RECT* → `Render_DrawRect`), `0x428c40`(GameView,CDC* → health-circle draw),
`0x427490`/`0x4278a0`(GameView,CDC* → view paint helpers), `0x421520`(World* → worldgen grid reset),
`0x428680`(World* → doc data method). The genuine parsers (`Dta_Load`/`Dta_Parse*`/`Dta_ReadZone`,
~0x421e70–0x423bxx) and `Worldgen_*`/`Wld_*` already-named funcs are correct — leave them.

**Procedure (idempotent, address-anchored):** for each remaining `Dta_FUN_<addr>` in 0x424010–0x429142:
1. decompile; read `this`-type + the tells above; 2. rename with the right prefix (keep the addr suffix if
role is clear but purpose isn't yet: e.g. `GameView_FUN_<addr>`); 3. set a plate comment when purpose is
clear; 4. mirror any struct fields found (per the p-prefix/type rules). Verify YodaDemo.exe is the active
program before writes. Track the sweep in docs/compile-units.md (the CU map is the ledger).

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

### ⏭ NEXT SESSION PICKUP (2026-07-05, updated)
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
  *worse* — this binary emits biCompression-first, so it's the CSE that matters, not the field order. **The MMX blits were the hard win** — see lessons below.
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
  **~1.45%**. Run: `python3 tools/progress.py`.
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
  decompiled to `this->tiles[(y*18+x)*3+layer]`. Keep the Ghidra struct and the `src/` struct in sync.
- **Trace every struct to its allocation before trusting its size, and define it once.** The correct
  size comes from the `operator_new(N)`/alloc site (e.g. `Zone`=0x848, `Tile`=0x40c, `MapEntity`=0x64,
  `IactScript`=0x30 were all pinned this way), NOT from how far field accesses happen to reach. Keep a
  single canonical definition per struct in the DB and the registry `docs/structs.md` — no duplicates.
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
- Mirror OpenJKDF2 structure (`~/workspace/OpenJKDF2`): CMake, per-module source files, `Module_Function` naming.
- Progress artifacts live in `docs/`. The toolchain lives in `toolchain/`.