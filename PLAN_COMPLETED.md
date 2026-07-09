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


# ⏮ PRIOR session blocks (migrated verbatim from CLAUDE.md, 2026-07-07 v28)

Migrated per user directive: the distilled, still-actionable knowledge lives in CLAUDE.md as
KEY codegen lessons 15-22, the consolidated MFC-matching lessons block, and the Ghidra
write-recipes/struct-edit-gotchas block (Conventions). These blocks are the full per-session
logs, newest first, kept for provenance (per-function autopsies also live in-source next to
their `// FUNCTION: YODA` markers).

### ⏮ PRIOR (2026-07-07 v28 — DoDataExchange discovery + struct de-dup 1-5 + CyclePalette/OnCmdStats; 95.64% coverage)
**▶ v28 RESULTS (commits 60ac1c8..): GameView TU = 70/114 markers (7605B exact); 95.64%
coverage / 19.39% exact globally. Ghidra: YodaDemo ACTIVE all session; run_script_inline
WORKS again (a Ghidra restart cleared the v26 phantom-script cache); all writes landed +
saved. PRIOR blocks migrated to PLAN_COMPLETED.md (user directive); their distilled
knowledge = KEY lessons 15-22 + the consolidated MFC/Ghidra blocks below.**
- **⭐ The 3 slider dialogs have EMPTY DoDataExchange overrides** (bare RET 4 @0x417f30/
  0x418210/0x4184f0; vtable-slot-pinned at +0x88 = StatsDlg's DDX slot delta): NO base
  call — the dev deleted it. Added to the classes + 11 COMDAT markers (4 ??_G scalar
  dtors, 4 GetMessageMaps, 3 DoDataExchanges) — ALL MATCH first compile. Message maps
  moved to their .text-proven position (ctor → DoDataExchange → BEGIN_MESSAGE_MAP →
  OnInitDialog, ClassWizard layout).
- **⭐ STRUCT DE-DUP steps 1-5 DONE** (docs/dedup-plan.md has the full log; one commit per
  step): canonical **MapZone.h** (vptr-true; retired GameData's shifted-by-4 stub whose
  off-by-4 displacements had been poisoning StartGame — DIFF 254→79); canonical
  **Canvas.h** (0x407df0 = the real CTOR, 0x407eb0 = ~Canvas; `class CDC;` fwd-decl only,
  the 2-field CDC stub now PRIVATE to Canvas.cpp; Canvas 8/11→9/11, Worldgen +4); **real
  GameView.h into GameData/Iact** (stub lies fixed: "OnWalk(int,short)" was really
  ZoneTransitionStep(short,short), "PlayerMove" was PlaySound, bSuppressWalkSound@0x2f4 =
  bWeaponIactActiveMaybe; cost GameData −3 / Iact −1, all small same-length tie-breaks,
  annotated). **Step 6 (World merge, ~102 field/granularity reconciliations) REMAINS —
  do as its own session.**
- **CyclePalette 0x415af0 (1280B) EFFECTIVE first compile** (304/304 insns, align 0, 6B:
  one eax↔ecx swap in the FIRST ::AnimatePalette setup — the orig gives its two IDENTICAL
  statements opposite allocations; parity-crossing family, G1). Palette region modeled in
  Worldgen.h + Ghidra: bPaletteAnimEnabledMaybe@0x2e5c gates it, inline-LOGPALETTE WORDs
  @0x2e68, sysPalette PALETTEENTRY[256]@0x2e6c (AnimatePalette source), pSysColorTable
  retyped RGBQUAD* (WorldDoc keeps byte-math via its own header until step 6). Key shape:
  NO pointer-caching locals — every store may alias, cl reloads per statement (lesson 19).
- **OnCmdStats 0x416620 EXACT first compile** (296B; Maybe dropped in src+Ghidra): stack
  StatsDlg + ONE reused CString, Format "%ld" ×4 in member order 2,3,1,0 (m_str2←highScore
  @0x33ac, m_str3←lastScore@0x33b0, m_str1←completionCount@0x332c, m_str0←lastCount
  @0x33a8). **??1StatsDlg = 0x416750 MATCH** (out-of-line dtor emitted by the stack
  instance; Ghidra function created — its body was a broken 1-byte stub).
- Dial flips all annotated in-source per protocol: IN OnCmdDifficulty + DrawEntities (the
  DoDataExchange decls), IsZoneUsed out→in round trip, WorldDoc ??1World (1441B) back IN,
  Canvas Clear IN; OUT FindTile, StepDetonatorEffect, ReadIzon, PlaceZoneObjectTiles,
  FindZoneCellById; LoadStoryHistoryAlaska = the perpetual 2-byte breather (noted as such).


### ⏮ PRIOR (2026-07-07 v27 — GameView tail handlers + option dialogs; 94.42% coverage)
**▶ v27 RESULTS: GameView.cpp = 55/100 markers (6557B exact); marker coverage 94.42% globally.
Transcribed the whole 0x415820–0x4186e0 tail EXCEPT CyclePalette/TextDialog (deferred).**
- **New EXACT:** OnDestroy 0x415ac0, OnHScroll 0x415ff0, OnTogglePause 0x416220, OnUpdatePauseUi
  0x4162a0 (per-arm `Enable(0)`/`Enable(1)` cross-jump, NOT a bEnable var), OnUpdateGameSpeedUi
  0x416460, OnUpdateDifficultyUi 0x4165b0, OnCmdGameSpeed 0x416310 (int-temp for reused
  `0xba-m_nValue`), OnCmdWorldSizeMaybe 0x4164d0, StatsDlg ctor/DoDataExchange/OnInitDialog
  (0x416810/9e0/a40), OnDialogCloseBtn 0x416a60 + 2 nop btns, OnChar 0x416ae0 (per-arm
  `strCheatBuffer += 'x'` cross-jump), all 3 slider-dialog ctors + all 3 OnInitDialog
  (0x417e50/f50, 0x418130/230, 0x418410/510).
- **EFFECTIVE (autopsies in-source):** CheckCheat 0x415820 (align 232; &strCheatBuffer CSE
  reg-swap this=edi vs esi), OnKeyUp 0x415a50 (GetAsyncKeyState scheduling), ConfirmExit
  0x416030 + OnAppExit 0x416110 twins (AfxGetApp-inline scheduling), OnCmdDifficulty 0x416120
  (align 0, this↔nSavedMode esi/edi swap), OnUpdateWorldSizeUi/OnUpdateStatsUi (6B unused-this
  eax-hop), OnCtlColor 0x416a90 (1 benign byte), the 3 slider OnHScroll clones
  (0x417fa0/418280/418560 — lesson #7 position-dependent reg-alloc; DifficultyDlg DIFF(5)).
- **⭐ Slider dialogs are sizeof 0x60** (CDialog + `int m_nValue`@0x5c; Ghidra mis-sized the
  OnCmd stack arrays as [92]). **StatsDlg sizeof 0x74** (CDialog + unk5c@0x5c + World*@0x60 +
  4 CString@0x64-0x70; ClassWizard DDX dialog, empty msgmap @0x44b558). The OnCmd handlers seed
  `dlg.m_nValue`/the 4 CStrings BEFORE DoModal (the Ghidra "local CStrings" overlap the array).
- **⭐ Dialog classes moved OUT of shared GameView.h INTO GameView.cpp** (TU-private; the doc TU
  that includes GameView.h must not see them). Do this for future TU-private types.
- **Dial breathing (v27):** the real StatsDlg decl flipped **ClassifyTile 0x40fca0 (1569B) to
  PHASE-DISPLACED** (source proven; its ctor matching confirms the layout) — the reason global
  %exact reads ~flat (17.4%) despite +32 markers. DrawEntities/ReenableHotspot/UpdatePlayerWalk
  also breathed. All resolve at G1. Track coverage (94.42%), not %exact, this phase.

**▶ START HERE (v28): the last two GameView-TU monsters + the 3 OnHScroll clones.**
1. **CyclePalette 0x415af0 (1280B)** — palette-cycle ring-shift. Needs the inline animated
   palette modeled: World+0x2e6c = RGBQUAD[256] mirroring `pSysColorTable` (index i at
   0x2e6c+i*4; AnimatePalette/SetPalette use idx 10 cnt 5 @0x2e94 and idx 0xa0 cnt 0x56 @0x30ec).
   Slot held in GameView.cpp (comment placeholder between OnDestroy and OnHScroll).
2. **The game TextDialog 0x416b90 Ctor + 0x416c40 Run (2022B) + helpers** (0x417570 Position,
   0x4176f0 Layout 1419B, 0x417c90/d30 ScrollTextLine, 0x417dc0 UpdateDialogButtons, 0x417e... 
   CtorMaybe). ⚠ This is the PLAIN non-CDialog TextDialog (sizeof 0xc8 in GameView.h), DISTINCT
   from Dlg.h's CTextDialog@0x418dd0 (CDialog, sizeof 0x6c). Do NOT conflate.
3. **OnCmdStatsMaybe 0x416620** (deferred; needs StatsDlg — now declared). Formats
   highScore/lastScore/completionCount/lastCount into dlg.m_str0-3 via a temp CString + Format,
   then DoModal. Placeholder slot in GameView.cpp.
4. StatsDlg **ScalarDtor 0x416920** + **GetMessageMap 0x416a30** are unclaimed COMDATs (come
   free from the class; add markers or confirm they emit).

**▶ STRUCT DE-DUP (user directive v27) — see docs/dedup-plan.md.** Full survey + the two real
obstacles (field-name/granularity divergence: World needs ~50 per-field offset reconciliations,
proven 102 compile errors; and the Canvas stub-CDC-vs-MFC-CDC environment split) + recommended
order (Records→MapZone→CDC→Canvas→GameView→World) + the mandatory per-flip annotation protocol.
The v27 dry-run (adding the 7 doc decls to Worldgen.h's World) was REVERTED but showed the World
merge is net-positive on the dial (GameView +2, Worldgen −1). Each struct is a churn-and-reverify
job; do ONE per commit, updating flipped functions' annotations (user requirement).

**▶ GHIDRA SYNC PENDING (no writes done v27 — do when YodaDemo is ACTIVE; run_script_inline was
BLOCKED per v26, use HTTP endpoints):** name the 4 option-dialog classes (Ghidra has partial
DifficultyDlg::/StatsDlg:: namespaces but the OnInitDialogs 0x417f50/0x418230/0x418510 and
several ctors are still `GameView::FUN_*`). Model them as CDialog-derived: slider dialogs
sizeof 0x60 (m_nValue@0x5c); StatsDlg sizeof 0x74 (unk5c@0x5c/pWorld@0x60/4 CString@0x64-0x70).
Mark the StatsDlg ScalarDtor 0x416920 + GetMessageMap 0x416a30 COMDATs. GameView struct/field
names are already synced from prior sessions; nothing new there this session.


### ⏮ PRIOR (2026-07-07 v26 — OnRButtonDown/UpdatePlayerWalkFrame/OnKeyDown; 17.30% exact / 92.28% coverage)
**▶ v26 RESULTS: GameView.cpp = 36/68 markers (6141B exact); 92.28% coverage / 17.30% exact
globally. New EXACT: OnRButtonDown 0x413c10 (WM_RBUTTONDOWN — was the UNIDENTIFIED 445B gap;
switch(nFrameMode){case 3: fire in facing; case 7: close map}; the fire dir-resolve + camera
bounds are shared with OnKeyDown/OnBumpTile), UpdatePlayerWalkFrame 0x4150a0 (23 insns),
+ a Dlg-include dial flip-in. New EFFECTIVE: OnKeyDown 0x4150f0 (1538B, ~83% bytes, autopsy
in-source — shared-tail block placement + 2 movsx are the only residuals).**
- **⭐⭐ ROOT-CAUSE STRUCT FIXES (user directive — "decomp errors point to wrong Ghidra
  structs; fix sooner not later"):** (a) Ghidra `GameView.pWorld@0x44` AND `m_pDocument@0x3c`
  were typed **`-BAD-`** (dangling type refs — the ONLY -BAD- fields in the whole DB, audited
  all 15 key structs). Effect: EVERY World-field access through pWorld decompiled as raw
  `*(int*)(pWorld+off)` (an intermediate `iVarN`), which is exactly what made me pick
  gameState(0x68) over the correct nMapChangeReason(0x60) in OnRButtonDown. Retyped `World *`
  via **`modify_struct_field_type` HTTP** (JSON body {struct_name,field_name,new_type}) — it
  CLOBBERS the field name, restore with `modify_struct_field` field_name=`offset:0xN`
  (renamer auto-prefixes 'p' → m_pDocument became pM_pDocument, cosmetic). Decompiles now
  render `pWorld->nMapChangeReason` etc. (b) **Dlg.h CTextDialog sizeof was 0xc8, really 0x6c**:
  the `_pad6c[0x5c]` was copied from the unrelated game TextDialog@0x416b90; OnKeyDown's stack
  frame proves 0x6c (dlg@[EBP-0x94], SUB ESP,0x88, members end +0x6c). Removed the pad, Dlg TU
  still 5/5 (ctor/dtor/DDX don't encode sizeof). GameView.cpp now `#include "../Dlg/Dlg.h"`.
- **⚠ run_script_inline is BLOCKED**: a phantom stale `McpInline_2bf5a8636ec45.java` (raw
  top-level stmts, no class wrapper) is cached IN THE MCP PLUGIN's memory (not on disk — find
  turns up nothing), breaking every inline-script compile. Use the dedicated HTTP endpoints
  (modify_struct_field_type / modify_struct_field) for struct edits until the Ghidra session
  is restarted.
- **⭐ `*(CPoint *)&nMouseX`** (OnKeyDown tail): reinterpret adjacent nMouseX/nMouseY as a
  CPoint by-value (LEA &nMouseX + deref both dwords). `CPoint(x,y)` spills a stack temp;
  `*(POINT*)&nMouseX` adds a POINT→CPoint conversion copy — BOTH worse (v25's POINT note was
  for a raw ::PtInRect POINT* arg; a CPoint-param call wants the CPoint cast).
- **⭐ Switch shared-tail placement (block-sinking family, PARKED like v8/v9):** cl 4.2 emits
  the post-switch merge block after whichever case it makes the fall-through predecessor.
  OnKeyDown's `if(bMoved)` tail: orig places it LAST (after case VK_F8, no-break fall-through);
  cl here always makes `default:` the fall-through pred (tail right after default, others JMP
  back). default first/mid/last + VK_F8 break/fall-through ALL inert — governed by trace/EH
  ordering cl doesn't expose. Do NOT keep grinding these; annotate + G1.
- **Prior v25 cracks retained (in-source):** memcpy operand-provenance (field-to-field ⇒ lone
  rep movsb); bare `return CONST` cross-jump only as branch target; positive-test else-arm
  deferral; OnBumpTile flags-in-EAX + (short)-cast int locals for GetTile/GetZoneCell.
- **⭐ MEMCPY OPERAND-PROVENANCE RULE (probe-proven, the UpdateDragCursor crack):** the
  intrinsic emits the LONE `rep movsb` form when BOTH args are struct-FIELD loads (any
  pointed type; a value-local copied from a field keeps field-ness); any param/global/
  call-result/deref-of-&field-local operand ⇒ the movsd+movsb split. SEPARATELY, the
  count expression is value-tracked: provably 4-aligned count ((n/8)<<10, n*1024) drops
  the movsb tail ⇒ LONE movsd; tracking dies when the value crosses a call/spill. `n&3`
  does NOT drop the movsd phase (no range analysis, only low-bit zeros).
- **⭐ Bare `return CONST` cross-jumps into the function-end epilogue ONLY as a BRANCH
  TARGET** (OnSetCursor crack): write `if (==) {store; return TRUE;} return TRUE;` — the
  guard's false-jump lands on the bare return and merges with the end block; NESTED
  fall-out of two scopes = a FALL-THROUGH return = inline epilogue copy (not mergeable).
- **⭐ PtInRect/POINT overlay:** adjacent int fields ARE the POINT — pass
  `*(POINT *)&nMouseX` (a `POINT pt` local costs 8 frame bytes + stores); in OnMouseMove a
  `POINT *pMouse = (POINT *)&nMouseX` pointer local also pins the x/y reg roles.
- **⭐ Positive-test nesting defers else-arms** (OnMouseMove): `if (P_out) { if (!P_in) {
  if (P_cell) A else B } } else C` — then-arms inline, else-arms (B, C) deferred to the
  end in discovery order; C's final no-return edge falls into the shared epilogue.
- **FireWeaponStep flags-test axis CONFIRMED in OnBumpTile:** `UINT nFlags = pTile->flags;`
  with TWO uses (character + push tests) keeps flags in EAX (test ah,1 / test al,8);
  single-use tests narrow to byte-mem. Also: ALL GetTile/GetZoneCell results route through
  INT locals with (short) casts (`int t = (short)zone->GetTile(...)`); edge-case arms are
  `if (nCell >= 0) {big} else PlaySound(6);` (else lands at case end); flat push guard
  `if (bPush && (nFlags&8)) {..} if (bPush) break;` (one-deep test elimination); pull
  block uses NEGATED int locals (ndx=-dx) + (short) casts at the DrawZoneCell site.
- OnEraseBkgnd: h/w as locals (h FIRST) batches both subtractions before the PATCOPY push.
- New Worldgen.h fields (Ghidra synced+saved): nQueuedMoveDXMaybe@0x3338,
  nQueuedMoveDYMaybe@0x333c (OnBumpTile tail zeroes DY→copies to DX; transition arms chain
  `nMoveDX = nMoveDY = queuedDX`). asmscore GOTCHA: its want-name regex greps the first
  `Class::Method(` AFTER the marker — a comment like "Character::Get(Walk)FrameTile"
  mispairs the COMDAT (returns None) — keep :: out of marker comments.
- **Parked (autopsies in-source):** UpdateDragCursor's >8bpp pixel-loop import-caching
  (ours caches SetPixel in EDI, demotes y2 to memory — v24 reg-pressure family; minimal-TU
  probe: identical solo ⇒ header-dial); OnBumpTile's this=EDI-vs-ESI prologue swap + the
  (nMask&0x2a) dialog arm inline-vs-deferred (orig's arm owns the shared DrawPlayer/
  DrawGameArea tail — merge-partner family) + six 1-insn GetFrameTile site swaps;
  OnMouseMove imm-vs-reg zero stores + rcOuter.bottom wedge; OnEraseBkgnd stub order.

**▶ START HERE (v27): remaining ~12KB of GameView TU, .text order (progress.py largest
unclaimed):** OnKeyUp 0x415a50 + OnDestroy 0x415ac0 + OnHScroll 0x415ff0 (small handlers
near 0x415820), CheckCheat 0x415820 (552B), CyclePalette 0x415af0 (1280B), ConfirmExit
0x416030, then the biggest remaining: **TextDialog::Ctor 0x416b90 + Run 0x416c40 (2022B,
the game's plain-class TextDialog, NOT CDialog — sizeof 0xc8, distinct from Dlg.h's
CTextDialog!)**, 0x4176f0 (1419B, identify), options dialogs 0x417ec0–0x4186e0 (three mini
CDialog classes — model per v16 notes; 0x418280 397B / 0x417570 384B unclaimed).
- **⚠ Two DISTINCT "TextDialog" classes — do NOT conflate (this session's near-miss):**
  Dlg.h `CTextDialog` @0x418dd0 = CDialog-derived debug dialog, **sizeof 0x6c** (used by
  OnKeyDown Ctrl+F8). The game's `TextDialog` @0x416b90 = plain non-CDialog class,
  **sizeof 0xc8** (ShowTextDialog). Their sizes got cross-contaminated once already.
- **OnKeyDown residual (PARKED, G1):** shared-tail block placement + the 2 GetAsyncKeyState
  `& 0x8000` movsx (orig keeps `movsx eax,ax` before `test ah,0x80`; short/int locals both
  failed to reproduce). ~83% bytes; all case bodies match 1:1.
3. **Open items (carried):** InvScrollBar ??_G/??1 (0x408690/0x4086b0) PARKED;
   World.unk50 → nCurrentZoneIdMaybe rename (4-TU re-verify); MapZone.field30 →
   quest-list selector rename candidate (1=listA, else listB — ShowWinMessage).
4. **G1 dial axes (carried):** ZTS↔WES + AHC/XWing arms parity crossings;
   DrawZoneCellRect/DrawWholeZone rotations; FireWeaponStep erase-block;
   Tick cmp-direction/fire-block/reg-roles; OnTimer + ScrollZoneTransition
   this-reload/import-caching; UpdateItemObjects this/pO swap; DrawText pTile-EBX
   CSE; ShowWinMessage tx/ty homing; plain-helper param widths; AFX_MSG map-order;
   v24/v25 parked lists above.
5. **Re-verify ALL TUs after ANY Worldgen.h/GameView.h/RecordClasses.h/Dlg.h edit** (v26
   sweep: GameView 36/68, Dlg 5/5; re-run the others — Worldgen/WorldDoc/Records/GameData/
   World/Iact — after any shared-header change, the CTextDialog decl add rotated the dial).

### ⏮ PRIOR (2026-07-07 v24 — mouse handlers, condensed)
**▶ v24 RESULTS (2026-07-07, commits e9caa34+): GameView.cpp = 29/57 markers; 85.60%
marker coverage / 15.93% exact globally. SoundInit 0x411520 EXACT FIRST COMPILE (527B —
WaveMix session + strcpy/strcat intrinsics over World.soundNames[64] + g_waveHandles
free-loops; error arms duplicate the 4-statement close tail in source, cl cross-jumps).
OnDragItem 0x4102d0 EFFECTIVE-WIP (945/924 insns), OnLButtonDown 0x411730 EFFECTIVE
(70/2845 bytes!), OnLButtonUp 0x412250 EFFECTIVE (align 274) — full autopsies in-source.**
- **⭐ World+0x5c is nFrameMode, NOT gameState** (gameState=0x68): OnDragItem's mode-9
  pickup / IactRun save-restore all write 0x5c. Check every old "gameState" reading.
- **⭐ Per-case trailing-copy pattern (OnLButtonUp crack, -63 insns):** paths that skip a
  shared trailing store do a plain `break` to ONE `Default()` after the switch; paths that
  store write their OWN `bMouseCaptured = 0;` copy at case end — cl cross-jumps the copies
  into one block. Do NOT write `Default(); return;` copies (they emit full epilogues).
- **More v24 cracks:** ammo refill arms are &field POINTER LOCALS (`short *p = &field;
  short a = *p; if (a <= 0) { *p = K; a = pWorld->field; }` → the add-reg,0xNNNN form);
  value-ternary with the multiply DUPLICATED per arm ((gy == 0) ? gy*28 : (gy+1)*28,
  polarity load-bearing); CString balloon arms in INNER SCOPES so the dtor runs before
  the trailing store; `if (field30 != 1) B; else A;` then-jump polarity; eager `int
  nQuestIdx` widening before a selector branch; a reward scan DECREMENTS its count var
  (separate `int n = count` leaves a self-move); characters walks use GetData() hoisted
  pointers, tiles/objects use GetAt (per-iteration reloads); 2-case type dispatch = switch.
- **OnDragItem minimal-TU probe: identical score solo ⇒ its global reg-rotation is
  HEADER-DIAL, not TU-position (G1).** ClassifyTile PHASE-DISPLACED by the v24 adds
  (was EXACT v23; plate updated). Exact-count breathes: 28→31→29 across the session.
- New World fields (Ghidra synced): unk2e30 (equip char idx+8, write-only),
  ammoTheForceMaybe/ammoLightsaberMaybe @0x3348/4a, nWalkTargetX/YMaybe @0x3340/44.
  TileFlags adds: TILE_KEYCARD/PUZZLE_ITEM_1/2/SEED_END (ITEM aliases of bits 16-19),
  TILE_ITEM_HARMFUL_MAYBE (1<<21). engine-bugs.md #14: Artoo cases 0x13/0x14 leak the DC.
- **Parked (autopsies in-source):** the PS(6)+DrawText(0) tail merge (orig cross-jumps
  full-health→else; ours picks the IACT PlaySound tail as merge partner — not steerable);
  heal-ladder arm cluster/shared-0x32/jle-polarity (BOTH OnDragItem + OnLButtonUp, lesson
  #6 canonicalization); load/TEST/store drift in the 0x12/0x1fe arms; walk-target X/Y
  chain interleave (int locals were +36 insns — TZD family); import-pointer caching
  flips WITH the restructure (reg-pressure-coupled).


### ⏮ PRIOR (2026-07-07 v22+v23 — hotspot/inventory block + ClassifyTile, kept verbatim)
**v22 session results. src/GameView/GameView.cpp = 21 exact + 15 effective + 6 COMDAT / 44
markers, contiguous 0x4084f0–0x40f3c0(excl). New EXACT: StepDetonatorEffect 0x40e400,
TransitionZoneScript 0x40e750 (sig byte-proven: (int nUnused, int nZoneId), ret 8, arg1
never read — "(sig?)" tag cleared), ReenableHotspotObjects 0x40ebe0. New EFFECTIVE (all
with in-source autopsies): ApplyHotspotCamera 0x40e500, TransitionZoneXWing 0x40e7c0,
TransitionZoneDoor 0x40e9d0 (align=22, ONE xor-position residual), TriggerHotspotsMaybe
0x40ec30 (was "DrawObjects" — fires vehicle/xwing hotspots at the camera tile, returns int),
UpdateItemObjectsMaybe 0x40ed90 (was "DrawMap" — item pickup/re-place pass), DrawText
0x40f060 (the inventory-panel painter; windows.h renames it DrawTextA — marker carries the
mangled hint; asmscore.py now PARSES `(?mangled)` marker hints like verify.py).**
- **⭐ World::DrawRect is a __thiscall World MEMBER, not free __stdcall** (proven: DrawText
  loads ECX=pWorld deliberately at call sites; body ignores this, which is why the free
  model byte-matched in v13). Worldgen.h decl moved into class World; all sites are now
  pWorld->DrawRect(...); Worldgen re-verified 31/90 IDENTICAL bytes; Ghidra moved to
  World:: + thiscall. DrawText dropped 219k→179k from this alone.
- **⭐ NEW MECHANISM — per-label jump-table indices** (UpdateItemObjectsMaybe, proven via
  the dword table at 0x40f024): cl 4.2 assigns a table arm-index PER CASE LABEL in VALUE
  order — grouped labels (case 0: case 2: case 6: case 8:) get 4 distinct indices at the
  SAME arm address, and an explicit empty `case 4: case 10: case 15: break;` arm widens
  the byte table to 16 entries (its indices point at the exit block). A lone empty case
  folds away (max label drops); duplicating shared bodies NEVER folds (cl 4.2 doesn't
  merge duplicate arms — +155 insns). Read the dword table to recover the source labels.
- **MFC 4.2: the ONLY virtual CDC::SelectObject overload is (CFont*)** — a vcall at
  vtbl+0x30 means the source passed CFont::FromHandle(hFont) (CGdiObject* selects the
  non-virtual INLINE overload → m_hObject-extraction shape, wrong). Evaluate-callee-first:
  `CFont *pFont = CFont::FromHandle(h);` before the SelectObject. The CBrush* overload is
  the out-of-line non-virtual (0x440c92).
- More v22 cracks: `int slot = nScroll;` dedicated IV strength-reduces to the scroll<<2
  byte walker while bounds tests spell `nScroll + i` (DrawText inventory loop);
  `int vx = 0;` declared AFTER a call statement folds its xor into the call setup
  (AHC/TZD; but exact position inside arg-eval is NOT always steerable — TZD's single
  residual); the AHC/XWing clone pairs are parity-CROSSED like ZTS↔WES (pre-call xor =
  vx in arm1 but i in arm2 — identical source can't produce both, G1); UpdateItemObjects'
  quest-arm flags test (orig loads flags to ECX, ours folds to byte-mem test) = the SAME
  axis as FireWeaponStep's parked flags-test.
- Renames (Ghidra synced + saved): ZoneObj.visible → **arg** (door/vehicle target zone id,
  DA "arg"; Records/GameData/Iact/Worldgen re-verified unchanged); Zone.zoneUnk83c/840 →
  **doorReturnX/Y** (TransitionZoneDoor return pos); BlitTile prototype fixed (queue item
  cleared). NOTE modify_struct_field API silently no-opped on these — used
  run_script_inline setFieldName instead (add to the gotcha list).
- Dial churn: the FindZoneCellById decl add (real method, 0x403250, GameData TU) flipped
  OnActivateView/DrawEntities/BlitTile OUT of exact and moved FireWeaponStep/Tick closer
  (fixed-point rule — do NOT revert). GameData 13/27, Records 24/33, Worldgen 31/90,
  WorldDoc 6/13, World 6/8, Iact 2/10 all at expected levels.

**▶ v23 RESULTS (2026-07-07, committed): GameView.cpp = 28/53 markers, contiguous
0x4084f0–0x4115b0(excl) minus OnDragItem.**
- **ClassifyTile 0x40fca0 EXACT first compile (1569B — biggest exact yet):** three
  sequential switches + guards; asmscore's dump showed table-region noise but the
  masked byte-compare was 0 diffs — for table-heavy functions ALWAYS confirm with a
  direct masked byte-compare before touching anything.
- COMDAT identities pinned via vtable evidence (byte-compare can't disambiguate thin
  ??_Gs — the reloc IS the identity): 0x40f3d0/0x40f420 = ??1/??_G CBrush (DrawText's
  local); 0x40f490 = ??_GCEdit (vft 0x44dcd4 = wndDialogText@0x298, dtor in the ctor
  funclet); 0x40fc80 = ??_GCScrollBar (vft 0x44dda4 via InvScrollBar's inline base
  ctor); 0x411010/0x4110d0 = ??_G/??1 CBitmapButton. All 6 MATCH marker-only.
- **ShowWinMessage 0x40f4b0 EFFECTIVE-WIP** (495/491, autopsy in-source): the orig
  homes tx/ty to slots and pre-loads playerX/Y/equipped/m_pData in regs above the
  3-arm dispatch — one global rank tie-break; PLUS an intra-function arm-pair reg-role
  crossing (field30 if/else pairs) that blocks cl's cross-jump. int-id locals
  (xor+mov dx), int a/b hoisted in arm C, str += " " (0x456108 literal).
- **ScrollZoneTransition 0x411180 EFFECTIVE-WIP** (255/264, ours 9 shorter): orig
  spills this to [esp] + n2 to a slot, giving all 4 callee-saved regs to
  pDC/n/scratch (the OnTimer this-reload family). Cracks: int *pHide = &field
  pointer-local, CWinApp *pApp = AfxGetApp() local, GetSafeHdc() for BitBlt src,
  one-test three-way dispatch, clock()+50 busy-wait.
- Renames/retypes (Ghidra synced+saved): World.equippedItem int→Tile* (UseWeapon's
  ternary keeps an (int) cast — sic, pointer value degrades into nType);
  scrollDirX/Y@0x3360/4, unk3370 added. ⚠ modify_struct_field silently NO-OPs on
  field renames — use run_script_inline setFieldName (confirmed twice).
- ⚠ progress.py fix: PARTIAL used our-COMDAT lengths (funclets+tables) → inflated
  transcribed%. Now also prints Ghidra-extent coverage (regen app_funcs.txt via the
  dump script when Ghidra body-repairs change extents).

### ⏮ PRIOR (2026-07-07 v20+v21 — Tick + OnTimer transcribed, condensed)
OnTimer 0x40d470 EFFECTIVE-WIP (align 802, autopsy in-source; duplicated head condition,
default-before-case-8, blink blit-first, chained camera zero; parked: this-reload role,
nFrameMode=3 copies, import-pointer caching). Tick 0x40b270 (10.8KB) EFFECTIVE-WIP (align
2202; autopsy in-source). ⭐ v20 layout mechanisms: (a) cl 4.2 DEFERS any block ending in
an unconditional transfer, preferring fall-through continuity (`if (c) goto L;` with L
unemitted inlines L as fall-through); (b) an arm that falls through stays inline; (c)
switch comparison-trees survive only with duplicated bodies; (d) GetTile is SIGNED —
`short t = GetTile(...)` temps everywhere (ushort decl makes ==-1 dead). DrawEntities
0x40b160 EXACT (countdown recipe: `int i=0; int n=nCount; do{...i++;n--;}while(n!=0)`
under `if (nCount>=1)`; `int nFrame = pChar->currentFrame;` between calls). FindEntityAt
0x40b210 eff. (decl order pZone/nCharId/n/i load-bearing; short nCharId=-1 AX-resident).
Tick G1 families: cmp-direction mirror on ~40 entity-vs-player compares (frame +4-shift
correlated); bullet/erase reg-role rotation; FIRE/SHOOT block placement (7 shapes probed).
0x424fb0 jmp thunk = OnLoadWorld's ILT entry (plain call in source). v21: scorers TU 6/8
(GetVictoryZoneIndexMaybe/GetLossZoneMaybe MATCH — branchless demo-hardcoded zones[76]/[77]
ternaries); g_pszFontName@0x456130; Canvas stub gained hdc@0; ZONE_TYPE_VICTORY/LOSS enum.

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


### ⏮ PRIOR PICKUP (v29, 2026-07-07 — TextDialog cluster 6/7; 97.56% coverage) [demoted v30]
v29 RESULTS (commits 9d23a75, ad9a30f): the plain-class game TextDialog (speech balloon,
sizeof 0xc8, NOT Dlg.h's CTextDialog) modeled + 6/7 functions transcribed. GameView TU =
73/121 markers; coverage 97.56% (was 95.64% v28). Ghidra: TextDialog struct (0xc8) + 7 names
synced, saved.
- Struct pinned from ctor + every field access (GameView.h): 5 RECTs (rectBox@0x5c /
  rectClose@0x6c / rectUp@0x7c / rectDown@0x8c / rectText@0x9c), nTotalLines@0xac,
  nScrollLine@0xb0 (starts 5), nMode@0x54 (0=world-relative), pView2@0xb4, pParentView@0xc0,
  strText@0xb8.
- Ctor 0x416b90 EXACT (161B). Position 0x417570 eff. DIFF(98): shared `goto do_layout` +
  tail-call to Layout fixed structure (312->152 align); residual = cmp-direction family +
  one reg rotation. ScrollTextLine/ScrollTextLine2/UpdateDialogButtons DIFF(6-7): one uniform
  pParentView-load schedule shift (a local cache made it WORSE — orig re-reads).
  UpdateDialogButtons takes a dead 4-byte stack arg (ret 4, callers push 1) -> (int nUnused).
  Run 0x416c40 eff. 622/622 insns — full structure; residual = this-in-ESI vs orig EDI.
- Added g_pszDialogFont @0x4561cc (CreateFont face-name global, distinct from g_pszFontName).
- v30 finished the cluster: Layout 0x4176f0 transcribed (eff.), TriPoint::TriPoint 0x4186e0 EXACT.

### ⏮ v30 PICKUP (2026-07-07 — PHASE E COMPLETE; 98.47% coverage) — demoted at v31
- v30 RESULTS: TextDialog::Layout 0x4176f0 transcribed (EFFECTIVE, align 374, 1419B jump-table
  balloon painter) + TriPoint::TriPoint 0x4186e0 EXACT. GameView TU ZERO unclaimed. 72/122 exact.
- Layout autopsy: 5-entry jump table (switch nVisibleLines 1/2-4/5), tail triangle from a
  `TriPoint point[3]` custom point type (MFC CPoint ctor inline → no call; TriPoint derives
  tagPOINT + non-inline empty ctor → array construction calls 0x4186e0 3×). Wins align 418→374:
  (a) store point[0].x/[1].x INSIDE each x-branch not hoisted; (b) drop `nShow` var, write
  `btnDialogUp.ShowWindow(0/5)` per case so cl cross-jumps (lesson #18). Parked residual = cl
  trace-driven duplication of a dead `cmp bx,0x20/0x100` fragment (#15) + pointer-reload aliasing
  (#19) + this-in-ESI. → G1.
- NEW SUB-LESSON (out-of-line empty ctor for array construction): a stack `T arr[N]` loop CALLING
  an out-of-line `mov eax,ecx; ret` ctor ⇒ NOT MFC CPoint/CRect (their ctors are _AFXWIN_INLINE).
  Model a custom class with the empty ctor DEFINED out-of-line AND AFTER the use site. (Now in the
  permanent MFC lessons list.)

### ⏮ PRIOR PICKUP (v31 — 2026-07-07, condensed at v32)
PHASE F: the FIRST APP TU (src/AppData/AppData.cpp, 0x401000–0x401450) — the last un-transcribed
real source file — done, 14/14 app funcs EXACT (MapZone/InvItem/WorldgenZoneEntry ctors/dtors +
the AppWnd CWnd class OnTimer/OnPaint/Disable/Enable) + 5 CObject lib COMDATs match folded addrs.
Codegen lessons applied: (a) MapZone ctor writes fields DESCENDING by offset (cl groups same-value
stores keeping source order per group); (b) InvItem needed an EXPLICIT out-of-line ~InvItem (added
to GameView.h) so ??_GInvItem stays a thin 28B call-through, not inlined CString destruction.
AppWnd stayed PROVISIONAL (msgmap-0x44b000 CWnd class identity unknown; Disable/Enable overrides
COMDAT-fold across ~15 UI classes; `::EnableWindow(m_hWnd,BOOL)` written as the direct Win32 call).
Also STOOD UP Phase G0 (tools/link_exe.sh — full-image link as a completeness oracle): found +
closed the ONE true gap GameView::RemoveItem 0x429150 (EXACT, appended to Worldgen.cpp). Left the
link at 0 duplicates / 34 unresolved (10 WAVMIX + 24 cross-TU name/sig/data drifts) — all closed
in v32. Milestone: 98.47→99.09 % coverage, 21.24 % exact.

### ⏮ PRIOR PICKUP (v32, 2026-07-07 — PHASE G0 COMPLETE: links + runnable image)
`tools/link_exe.sh` links the whole image (0 unresolved / 0 duplicates / exit 0) → a RUNNABLE
`yoda.exe` (446 KB with the original's copied .rsrc). Closed all 34 v31 unresolved: 10 WAVMIX via an
in-house non-copyrighted stub (toolchain/wavmix32/wavmix32.def + wavmix32_stub.c → decorated import
lib + no-op DLL importing WAVMIX32.DLL by name; real DLL drops in for sound) + 24 code/data drifts
reconciled (docs/link-audit.md). Resources via tools/extract_res.py (127 resources, .rsrc verbatim).
Reconciliation highlights: FindSpecialZoneMaybe=SetCurrentToIntroZone (0x423d20); Records.h
PlayerMove/PlayerCheckWalkable → real PlaySound(int)/DrawZoneCell(short,short) (0x409060/0x409460);
FrameView→GameView; SaveZoneRecursive=(CFile*,short,int bFull); dropped stray DamageEntityAt(int)
overload (real short); App Log_Write free-fn → member CTheApp::LogWrite; rtcDeskcpp*→RUNTIME_CLASS.
Real .data tables extracted: YodaMasterPalette[1024]@0x456230, gWorldgenGridOrderTable[100]@0x456630,
gNeedleTable(25)@0x456938, Iact_szCmdTextBuf[2048]@0x459558. DIAL COST: exact 21.24→20.57 % (-0.7 %)
— shared-header sig changes flipped Worldgen ParseZaux/ParseZax2/SetCurrentToIntroZone + 1 WorldDoc
fn to PHASE-DISPLACED (source proven; G1 recovers). Objs moved to repo build/ (gitignored). Ghidra:
no writes.

⏮ PRIOR v33 (2026-07-08): first RUNTIME bugfix + InvScrollBar dtors. (1) Invisible-bubble-text bug:
GameView::OnCtlColor 0x416a90 called pDC->SetTextColor(0xffffff) where orig calls SetBkColor (CDC
vtable slots +0x38 vs +0x34) → white-on-white text; user saw the bubble frame but no text (inventory
text + stock YodaDemo.exe rendered fine in the same wine env ⇒ our bug). Fixed → OnCtlColor byte-EXACT;
the long-standing "DIFF 1 benign byte" WAS this slot displacement (lesson #24: a 1-byte CALL-disp diff
= wrong virtual method). Exact 208→209. (2) InvScrollBar dtors (parked mini #1): explicit
`~InvScrollBar(){DestroyWindow();}` → ??1 0x4086b0 (91B, calls CWnd::DestroyWindow) + thin ??_G 0x408690
(30B), byte-exact. #line-neutral placement (decl appended to ctor's line, def at end of GameView.cpp)
avoided the mid-file dial rotation that had displaced 6 funcs 208→204 (lesson #23). Method: found by
diffing our yoda.exe vs stock in the same wine env. Memory [[textbubble-render-lead]]. Ghidra: no writes.

### ⏮ PRIOR PICKUP (v34, 2026-07-08 — static bug-oracle sweep + AfxGetResourceHandle fix)
Built a static complement to v33's running-EXE oracle (job tmp vtscan*.py, ephemeral): decode both
reloc-masked streams, NW-align (asmscore._align), flag aligned same-key non-stack mem pairs with matching
base reg + differing disp. Proved ZERO remaining v33-class `call [reg+disp]` vtable-slot bugs and zero
field bugs in drift-free funcs. Surfaced ONE real mis-transcription: OnInitialUpdate 0x426c40 (11
LoadCursor) + DrawDirectionArrows 0x4270f0 (8 LoadIcon) used AfxGetInstanceHandle (AfxGetModuleState+0x8)
where orig calls AfxGetResourceHandle (+0xc); AFX_MODULE_STATE:CNoTrackObject ⇒ WinApp@+4/Instance@+8/
Resource@+0xc. Swapped all 19 → 14 field bytes closed (OnInitialUpdate DIFF 31→21). NOT runtime-observable
(static-MFC EXE: the two handles are equal) so the EXE oracle MISSES it (lesson #25, memory
[[afx-resource-vs-instance-handle]]). Both funcs stay EFFECTIVE ⇒ 209 exact / Worldgen 34/91 / link
0/0/exit0 unchanged. Ghidra: no writes. (v35 committed the ephemeral scan as tools/bugscan.py.)

⏮ PRIOR PICKUP v35 (2026-07-08 — bug-oracle committed as tools/bugscan.py + clean sweep): committed
v34's ephemeral vtscan as tools/bugscan.py (static #24/#25 oracle: NW-align our stream vs original with
relocs masked, flag aligned same-mnem/opkind pairs with matching base reg + index but differing mem-disp;
buckets HIGH/LOW/FRAME; DIRECTIONALITY discriminator — one-directional systematic group = SHIFT(bug?),
bidirectional = SWAP(sched) scheduler reorder not a bug). Validated: clean tree → 0 HIGH/0 SHIFT/exit 0;
reverting the v34 Afx fix re-flags OnInitialUpdate 10×+DrawDirectionArrows 4× as SHIFT. ShowWinMessage
0x40f4b0's bidirectional 0x1e8↔0x1fc = the documented field30 arm-crossing SWAP (not a bug). Canvas-gap
mini (0x407d90/dc0) SCOPED: one dialog's two combo handlers (msgmap head 0x44b1d8, combo ctrl 0x9e,
`m_field68 = ((CComboBox*)GetDlgItem(0x9e))->GetCurSel()+1`) but BLOCKED — nothing in the binary
references msgmap 0x44b1d8 and the class's ctor/OnInitDialog aren't in-range, so its fields are unknown
(violates structs-before-transcription). 209 exact / Worldgen 34/91 / GameData 12/27 / link 0/0/exit0.

---

## ⏮ PRIOR PICKUP (v36 — 2026-07-08, DIAL MECHANISM CORRECTED + Fable consult) — demoted at v37

v36 was a MECHANISM/STRATEGY session (no exact-count change; stayed 209). Key results, all now
folded into the standing rules / KEY lessons #8:
- **CORRECTED the TU-phase dial:** INERT decls (non-virtual/never-called/undefined), NEW appended
  virtuals, and pure `#line` shifts are ALL byte-neutral (controlled experiment, GameView TU, positive
  control = one immediate 100→101 dropped 73→72). Only these rotate a TU: (a) a CALLED method whose
  signature SHAPE changes call-site codegen (lesson #14→#7 cascade); (b) a vtable change reordering
  EXISTING slots or changing sizeof; (c) reorder/add/remove emitted function DEFINITIONS. ⇒ reconstructing
  the full ~200-method class is OVER-BROAD — only CALLED-method sigs + vtable-slot ORDER + sizeof matter.
- **Frontier/survey analysis:** the ~50 closest non-exact are ALL the `this`/counter reg-coloring +
  jl/jg cmp-direction class. ParseSnds 0x4233f0 (24 decl-orders → all byte_diff=5), FindTile 0x403aa0,
  DrawZoneCellRect 0x4095d0 all proven not single-function-crackable. /O2 UNIQUELY correct (flag sweep).
- **Fable consult:** (1) compiler-VERSION hypothesis for jl/jg DEAD — our cl emits `jg` back-edges in 5
  EXACT funcs incl. LoadStoryHistoryOregon 0x40258b; its twin Nevada 0x401ac0 emits `jl` in the same TU
  = pure TU-position drift. Nevada(jl,broken)/Oregon(jg,exact) = the A/B pair. (2) Variable-NAME hashing
  inert (10 name sets on ParseSnds → all byte_diff=5). (3) whole-image LINKING doesn't change codegen.
  (4) Leads handed to v37: per-TU emitted-COMDAT reconciliation + the /Yu PCH axis.
- New tools committed: tools/survey.py (rank non-exact by closeness), tools/frontier.py (first non-exact
  per TU). Both proven MAP-only (the "first non-exact is body-steerable" hope FAILS — header-phase, not body).

**v37 verdict on the two leads it inherited:** PCH axis KILLED (net-negative + can't flip jl/jg; lesson
#27). COMDAT lever partially checked — Iact's COMDAT set is IDENTICAL across f1ca459 (not the ReadIzon
driver). The REAL v37 win was orthogonal: a MISSING afxcmn.h header (lesson #26) = +3 exact.

## ⏮ v38 PRIOR (demoted from CLAUDE.md pickup at v39)
**v38** (no exact change; closed 2 of v38's 3 START-HERE levers):
- HEADER AUDIT CLOSED — every TU at its optimal MFC header set. afxcmn (v37) is the ONLY beneficial add.
  afxext is REQUIRED for GameData/Iact (GameView.h uses CBitmapButton) but HURTS record/doc TUs that don't
  include GameView.h: canonical afxwin/afxext/afxcmn REGRESSES Records 26→25, WorldDoc 8→7, IactScript 11→10;
  afxdlgs also regresses. Original did NOT use one uniform afxext stdafx for all TUs. Don't add afxext/afxdlgs
  where not needed to COMPILE.
- EMISSION-ORDER REORDER REFUTED. GameData's fn emission order already matches its .text address order (0
  mismatches) yet Nevada still emits jl (needs G2). Worldgen GameView-tail block reordered to .text-address
  order → net-neutral 34/91, DetonateAdjacentTiles stayed DIFF(60) byte-for-byte, DrawWeaponIcon worsened →
  REVERTED. Became lesson #28 (LINKER lays out /Gy COMDATs; EXE address order ≠ source/.obj order).
**v37** (+3 exact): afxcmn header-dial win (lesson #26) — GameData 12→13 (FindTile 0x403aa0 +
PlaceZoneObjectTiles), WorldDoc 7→8 (~World), Iact 1→2; adopted in WorldStub.h + WorldDoc.h. /Yu PCH axis
KILLED (lesson #27): net-negative, doesn't flip jl/jg, its only win (afxcmn decls) is fully textual. afxcmn
DEMOTED GameData LoadStoryHistoryOregon (was a lucky jg match under a false context).

**v39** (212, no change): COMDAT-SET lever DEAD (lesson #29) — reg-coloring residual proven INTRINSIC to
(body + header decl-set), NOT TU-position, via 4 experiments on DetonateAdjacentTiles 0x428680 (v38 reorder;
a new COMDAT inserted right before it; a reg-pressure predecessor; minimal-TU probe = identical score solo
vs full TU). ??_GCPalette = lesson-#28 misattribution (odr-emitted by World ctor in WorldDoc; 0x41e8b0 =
folded copy). Residual class = symmetric 2-reg ROLE swap (ESI/EDI DetonateAdjacentTiles, ECX/EDX GetZoneIndex
0x423dc0, ESI/EDX ReenableHotspotObjects 0x40ebe0), ABI-pinned: sig-swap flips it (60→2B) but is caller-
unfaithful; local-reorder levers inert/structural. 212 = genuine per-TU ceiling (compile-time+intrinsic).
**v40** (212, no change): COMPILER-OPTION axis DEAD (lesson #30) — global-flag battery on Worldgen
(interleaved-baseline): /Gr,/Gy TIE, all others (/Ox,/O1,/Oa,/Ow,/Oy-,/Os,/Og-combos) WORSE; per-function
`#pragma optimize` on DetonateAdjacentTiles: /O2-implied letters reproduce the 60B residual, `a`/`s`/off all
worse. /O2 uniquely optimal → completes the exhaustion list (body/header/emission-order/PCH/COMDAT-set/
option all dead). MEASUREMENT-INTEGRITY: .obj non-deterministic (COFF timestamp + COMDAT symbol order) →
verify.py per-TU can undercount ~10 (Worldgen 34↔24); progress.py's 212 is name-keyed + ROBUST (3× rebuilds).
G2 GROUNDWORK: derived the deterministic app-.obj link order (13 TUs contiguous by first addr, AppData→…→
Worldgen) as the G2 step-1 input. Only remaining path = G2 byte-identical image.
**v41** (212 CONTENT, no change — PHASE G2 STARTED, a LAYOUT effort orthogonal to the exact count): built
tools/g2_link.sh (13 app objs in v40 address order + /OPT:NOREF + /MAP) + tools/g2_diff.py (per marker:
LAYOUT = linked addr==orig addr; CONTENT = reloc-masked bytes equal). Baseline 378/378 paired, LAYOUT 2/378,
CONTENT 226/378. Proved the layout model (docs/g2-layout.md): (1) /OPT:REF eliminates unreferenced COMDATs
(drops 19 markers — AppWnd::Disable/Enable/GetMessageMap…), NOREF keeps all; (2) the linker lays out .text
COMDATs in OBJ EMISSION ORDER (= source order), per obj in link order — proven byte-for-byte on AppData+World
(refines lesson #28: CONTENT order-invariant at fixed marker addrs, LINKED layout == source order). ⇒ layout
reproduction = match kept-set + per-TU source order + COMDAT sizes; fix upstream divergence → downstream
re-aligns (World already in perfect order, +0x10 shifted by AppData being 0x10 long). First divergence: AppData
emits GetMessageMap before OnTimer (orig OnTimer@401000 first). Worklist in docs/g2-layout.md.

---
### ⏮ PRIOR v42 (2026-07-08) — condensed (superseded by the v43 pickup in CLAUDE.md)
PHASE G2, AppData+Records LAYOUT reconciled (g2_diff LAYOUT 2→39, BOTH 1→32; 212 CONTENT stands, all
changes content-neutral). (1) Removed BEGIN_MESSAGE_MAP(AppWnd) from AppData.cpp — orig AppData.obj emits
no GetMessageMap COMDAT; our copy emitted first, shoving OnTimer off 0x401000 + a +0x10 cascade. Deleting
it → OnTimer first at 0x401000, correct length, World scorer run + LoadStory/SaveStory head snap in.
link_exe.sh still 0/0/exit0. (2) Moved Character::Read after Init in Records.cpp (orig order Init,Read,
GetWalkFrameTile,…) → Records collapses to uniform +0x50 (internally in-order). (3) ⭐ KEY FINDING: G2
LAYOUT is gated by per-function LENGTH, and the length divergences ARE the lesson-#29 intrinsic reg-coloring
wall — proven on SaveStoryHistory clones (DIFF 611, +10B each; orig cl allocated one more callee-saved reg
ebx → longer stream on identical IR). ⇒ byte-identical whole image bounded by the same cl reg-allocation
wall as the 212 content ceiling. Two divergence kinds: emission-ORDER scrambles (FIXABLE, content-neutral,
the clean G2 wins) vs intrinsic LENGTH diffs (hard #29 park). Two AppData residuals PARKED (CObject trio
-0x30 self-corrects; WorldgenZoneEntry ??_G-before-ctor quirk). Full model + worklist: docs/g2-layout.md.

### ⏮ PRIOR PICKUP — v43 (2026-07-08, condensed; superseded by v44)
PHASE G2: layout ceiling quantified + Frame reorder + code cleanup. (1) tools/g2_order.py + toolchain/test/
orig_func_addrs.txt: `--walls` = ~43 clean length walls (padded COMDAT len != orig Ghidra slot), all in
non-exact reg-coloring funcs, first = SaveStoryHistoryNevada 0x402670 (+0x10, ours over-allocates EBX vs
orig's 2 pushes); `--scramble` = per-TU emission-order metric. LAYOUT is 39 (not the naive 26) because
cumulative length-drift OSCILLATES (mixed +/- walls cancel through 0 → coincidental re-alignments); absolute
layout == #markers where cum-drift==0. (2) Reordered Frame's CMainFrame message handlers to original emission
order → Frame internally in-order (14/18 holds). (3) Code cleanup (user req): promoted 24 World unk*/*Maybe
fields (with usage comments) to proper names + synced Ghidra (created 6 missing fields, aligned 5 stale
*Maybe); PlanToken enum → decimal. All codegen-neutral (212 stands, link 0/0/exit0, bugscan 0/0/0). Then v44
closed the remaining App(3)+Worldgen-tail(3) scrambles and settled /OPT (original = /OPT:REF; lesson #31).

### ⏮ PRIOR PICKUP — v44 (2026-07-08, condensed; superseded by v45)
PHASE G2: emission-order scrambles closed + /OPT:REF settled (all CONTENT-neutral, 212 stood). (1) App.cpp
reordered to original emission order (msgmap→ctor→theApp→InitInstance→OnIdle→LogWrite→CAboutDlg ctor→
DoDataExchange→msgmap→OnAppAbout→OnInitDialog) → App in-order. (2) Worldgen GameView-tail reordered to
ascending addr (UseWeapon→DetonateAdjacentTiles→OnCmdMinimize→DrawWeaponBox→DrawWeaponIcon→BlitViewport
Dither→PreCreateWindow→AddItemToInv→RemoveItem) → Worldgen 4→1 inversions. All steerable emission-order
scrambles closed; 3 residual inversions are compiler-placed lib COMDATs (BENIGN). (3) /OPT SETTLED (lesson
#31): the original is a /OPT:REF build — our REF .text is within 1.7% of the original (302,716 B) vs NOREF
+19%; overturned the v43 NOREF guess (that used .rsrc-distorted full-file sizes). Then v45 reconstructed the
World document message map (REF-drop oracle, lesson #32): 22→7 REF-drops, ~World PHASE-DISPLACED 212→211.

### ⏮ PRIOR PICKUP — v45 (2026-07-08, condensed; superseded by v46)
PHASE G2: reconstructed the World document message map via the REF-drop oracle (lesson #32 — link build/*.obj
twice /OPT:REF vs /OPT:NOREF, diff app-obj symbols → the functions REF drops as unreferenced = reproduced-
reference-graph gaps). 22 dropped, 19 were the World doc command/update handlers because BEGIN_MESSAGE_MAP(
World,CDocument) was an empty TODO stub. Rebuilt the 14-entry AFX_MSGMAP_ENTRY array byte-for-byte from
@0x44c2d0 (6 ON_COMMAND + 8 ON_UPDATE_COMMAND_UI, handlers matched by pfn address) into WorldDoc.cpp + afx_msg
decls in WorldDoc.h → all 14 fixed fields byte-match, REF-drops 22→7. COST: ~World 0x41b2f0 PHASE-DISPLACED
(212→211, DIFF 6 align=0 intrinsic — the original .obj was built WITH the map). Then v46 closed 2 more
(World vtable overrides IsModified/SetModifiedFlag, codegen-neutral, REF-drops 7→5).

### ⏮ PRIOR PICKUP — v46 (2026-07-08, condensed; superseded by v47)
PHASE G2: closed 2 more REF-drops via World vtable overrides (codegen-neutral). World::IsModified/
SetModifiedFlag were dropped under /OPT:REF because WorldDoc.h's World class (the IMPLEMENT_DYNCREATE TU that
emits the vtable) didn't declare them virtual → the emitted vtable pointed at CDocument's base versions.
Declared both virtual → vtable slots target our overrides → kept. Overriding EXISTING base slots (no reorder/
sizeof change) is codegen-inert (211 held). REF-drops 7→5 (running 22→5 across v45+v46). The 5 remaining are
the hard tail (AppWnd OnPaint/OnTimer msgmap in a different TU; AppWnd Disable/Enable ICF-folded; ??_H CRT
helper). Then v47 built tools/vtcheck.py and validated World+GameView vtable content (8/8 each, CLEAN).

### ⏮ PRIOR PICKUP — v47 (2026-07-08, condensed; superseded by v48)
PHASE G2: built tools/vtcheck.py, the .rdata vtable-content oracle (data-side complement to bugscan — a MISSING
override = a virtual we forgot to declare = the base runs = a runtime bug, e.g. World::IsModified before v46).
Reads the ORIGINAL vtable from the exe + OUR vtable from build/<TU>.obj (??_7<Class>@@6B@ COMDAT + relocs →
per-slot target) and checks both override the same slots with the same class methods. v47: World 8/8 + GameView
8/8 CLEAN. The World dtor slot's ??_E-vs-orig-??_G is benign (??_E/??_G fold to one address, REF and NOREF).
Then v48 made it FULLY AUTOMATIC (auto-locates each vtable by scanning .rdata for override addresses from our
markers) and swept all modeled classes → 10 CLEAN (all UI/dialog/frame/view/doc), 13 single-dtor data classes
skipped. No missing-override bugs anywhere.

### ⏮ PRIOR PICKUP — v48 (2026-07-08, condensed; superseded by v49)
PHASE G2: made tools/vtcheck.py fully automatic (auto-locates each original vtable by scanning .rdata for >=2
override addresses from our markers, no hardcoded bases) and swept all modeled classes → 10 CLEAN (CTheApp,
CAboutDlg, CTextDialog, CMainFrame, GameView, StatsDlg, DifficultyDlg, GameSpeedDlg, WorldSizeDlg, World — every
UI/dialog/frame/view/doc class). 13 single-??_E-dtor data classes skipped (un-anchorable). No missing-override
bugs anywhere. Then v49 built the msgmap sibling (msgcheck.py) → found + fixed CTheApp incomplete map (1→8) and
GameView #11 ON_WM_HSCROLL→WM_VSCROLL bug (inventory vertical scroll); all 11 maps CLEAN, both codegen-neutral.

### ⏮ PRIOR PICKUP — v49 (2026-07-08, condensed; superseded by v50)
PHASE G2: built tools/msgcheck.py (the vtcheck sibling — validates .rdata message maps: reads the original
AFX_MSGMAP_ENTRY array via GetMessageMap→messageMap→lpEntries + ours from the obj's ?_messageEntries COMDAT,
compares fixed fields + handler identity). Found + fixed 2 real issues (both codegen-neutral, 211 held): CTheApp
map INCOMPLETE (1 vs orig 8 — the AppWizard File>New/Open + context-help block missing; this afxres.h has
ID_CONTEXT_HELP=0xe145/ID_DEFAULT_HELP=0xe147 swapped), and GameView #11 ON_WM_HSCROLL where orig is WM_VSCROLL
(vertical inventory scrollbar unhandled — renamed OnHScroll→OnVScroll, body byte-matches, + reordered entries).
All 11 maps CLEAN. lesson #33. Then v50 verified DYNCREATE sizes + renamed World→CDeskcppDoc, GameView→
CDeskcppView (source + Ghidra) to the original class names.

## ⏮ PRIOR PICKUP (v58, 2026-07-09) — condensed (superseded by v59: Indy worldgen now converges + plays)
v58 state: H3 milestone 4 = Indy worldgen TRACED but not working; the Yoda `Generate` quest model was proven
WRONG for Indy (assumes two-item puzzles + per-zone IZX2/IZX3; Indy has single-item puzzles + goal zones with
empty IZX2/IZX3). Decision: decompile DESKADV.EXE worldgen and reimplement under GAME_INDY. Kept base: planet
fix (currentPlanet=-1), goal-selection (WorldgenSelectPuzzle 9999 accepts any WORLD_MISSION), aux DATA loading
(Parse{Zaux,Zax2,Zax3}Indy + ReadIzaxIndy). ⇒ v59 DID the DESKADV RE + reimplementation: worldgen now converges
and Indy boots into a playable rendered world. See v59 pickup + docs/phase-h3-indy.md.

## ⏮ PRIOR PICKUP (v59, 2026-07-09) — condensed (superseded by v60: ACTN scripts distributed + palette cycling fixed)
v59 took H3 milestone 4 from "worldgen traced, Yoda model wrong" to a PLAYABLE rendered Indy world (user-confirmed
"gets in-game"). 7 commits (24a247d→f09c200), anchor 211 throughout. Done: (1) HTSP objects load (routed to
ParseHtsp — item pools live in DOOR_IN child zones, obj type 9); (2) full Indy worldgen reimplemented from DESKADV.EXE
as ~28 CDeskcppDoc::Indy* methods in Worldgen.cpp under #ifdef GAME_INDY (Load()'s retry loop routes to IndyGenerate,
no separate Populate; integration shim #define bool/true/false + aliases to reused Yoda helpers); (3) worldgen CONVERGES
after 2 transcription-bug fixes — IndySelectPuzzle must match the PICKED item (nWorldMissionKey/param_5) not reqItemA,
and the quest chain threads via the step SLOT (a5reqItem2=order-1) not nOrder; (4) reaches PLAY MODE — IndyGenerate tail
replicates Yoda Populate()'s world handoff (pView->bBusy=0) + the STUP-stuck workaround bWorldInvalid=1 (forces
ZoneTransitionStep since the scripted WorldEntryStepMaybe path loops 0→5 without entry scripts); (5) palette
(IndyMasterPalette, v59 wrongly disabled cycling — FIXED v60); (6) Save/Load/Replay menus enabled for Indy.
The bWorldInvalid=1 workaround + the missing whip + can't-enter-buildings are all the SAME entry-trigger gap →
v60 START HERE (Indy IACT entry-trigger semantics). DESKADV.EXE worldgen fully named in Ghidra; see docs/phase-h3-indy.md.
