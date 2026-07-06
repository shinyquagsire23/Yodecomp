## Decompilation strategy (phased plan)

The five original requirements are reorganized into a dependency-ordered plan. THIS FILE SHOULD NOT BE USED FOR ACTIVE PLANNING, it is a log/migration-only file.

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

## Subprojects

### ✅ DONE — whole-DB namespace migration (2026-07-05, condensed)
The whole doc TU (0x41c340–0x429000 + 0x419ed0–0x41bee0) is the **`World` namespace** (a separate
`Dta`/`Worldgen` namespace attempt broke `this=World*` — namespace must equal the struct; see the ⚠
note at top). Entire function list migrated to `Namespace::Method` via idempotent `run_script_inline`
loops: `World`(201)/`GameView`(208)/`GameData`(70)/`App`(57)/`Settings`(38)/`Iact`(33)/`Mfc`(29)/
`Zone`(24)/`Frame`(21)/…; only `FUN_*`, `FID_*` (MFC lib) and import thunks remain global. Function
identifications from the sweep are all named+commented in the Ghidra DB (DrawWeaponBox/DrawHealthDial/
BlitViewportDither/RestoreGridFromBackup — the last revealed the 2nd 10×10 grid at zones+100).
Lesson enshrined at top of file: read the body before naming (the BlitWeaponBox miss).

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