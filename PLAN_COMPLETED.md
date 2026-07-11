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

---

## ⏮ ARCHIVED FROM CLAUDE.md — v72 consolidation (2026-07-10)

Everything below was moved VERBATIM out of CLAUDE.md when it was consolidated around the Phase-H
extension goals (Indy tails / SDL / Indy RE sweep). This preserves: the phased plan + CU progress log,
the LONG-TERM ROADMAP (TU status + struct board + phase plan A–G + standing rules incl. the TU-phase
dial + the full v31–v71 milestone chain), the Phase-H specs as originally written, the v71 session
pickup, and the full 'Matching progress + tooling' section (⭐ KEY codegen lessons 1–33 + MFC lessons
+ permuter status). Cite lessons as 'PLAN_COMPLETED.md lesson #N' from now on.

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
| **AppData** (1st app TU) | 0x401000–0x401450 | ~1.1 KB | ✅ DONE v31 (src/AppData/): **14/14 app funcs EXACT** + 5 CObject lib COMDATs match their folded addrs. MapZone/InvItem/WorldgenZoneEntry ctors/dtors + the AppWnd CWnd class (OnTimer/OnPaint/Enable/DisableSelfWindow). InvItem needed an explicit out-of-line ~InvItem (added to GameView.h; dial-neutral) |
| World scorers (doc-TU fragment) | 0x401450–0x401ab9 | 1.6 KB | ✅ 5/6 exact (07-06: CalcTimeScore matched); CalcSolvedScore x87 park proven permuter-immune |
| **GameData** (2nd doc-TU src file) | 0x401ac0–0x4042b0 | ~10 KB | ✅ DONE: **13/27 exact (v37 +afxcmn: FindTile+PlaceZoneObjectTiles gained, Oregon demoted→jl/jg re-grind)**, rest effective/PHASE-DISPLACED; StartGame DIFF 79 |
| **Records** (6 record classes) | 0x4042b0–0x405ae0 | 5.5 KB | ✅ DONE 25/33 exact + 8 annotated eff. |
| **Iact** (`.obj`: Zone readers + IACT) | 0x405ae0–0x407cf4 | ~9 KB | ✅ TU COMPLETE: all 10 funcs transcribed, 88% insn-identical; **2/10 exact (v37 +afxcmn via WorldStub.h)** + 8 annotated tie-breaks; COMDAT set proven stable across f1ca459 (ReadIzon rotation = header-decl-context, not a COMDAT change) |
| **Canvas** (DIBSection blitter) | 0x407df0–0x4084e8 | 1.8 KB | ✅ DONE 9/11 + 2 eff. (parked); v28: canonical Canvas.h, 0x407df0=ctor / 0x407eb0=dtor, CDC stub private to Canvas.cpp |
| **IactScript** (3 script record classes) | 0x418700–0x418dd0 | ~1.7 KB | ✅ 11/12 exact (src/IactScript/) |
| **Dlg TU** (CTextDialog) | 0x418dd0–0x419000 | ~0.6 KB | ✅ DONE 07-06 (src/Dlg/): 5/5 EXACT. Implicit-dtor lesson (??_G inline) |
| **Frame TU** (CMainFrame) | 0x419000–0x419720 | ~1.8 KB | ✅ DONE 07-06 (src/Frame/): 14/18 exact + 4 eff. (2 palette sbb, PreCreateWindow, OnActivate). Owns g_strReplayPath |
| ~~Core utils~~ = GameView TU head | 0x408c60–0x40a560 | 6.5 KB | ⚠ mislabel: GameView methods (Dtor/OnDraw/DrawZoneCell/ZoneTransitionStep) + the ~CGdiObject/GDI COMDAT copies — Phase E, not a warm-up |
| **GameView TU** (view/UI/AI monster) | 0x4084f0–0x418700 | ~64 KB | ✅ PHASE E step 4 COMPLETE (src/GameView/GameView.cpp): 72/122 exact + rest eff. v30: TextDialog::Layout 0x4176f0 transcribed (eff., align 374 — jump table + TriPoint[3] tail triangle + bitmap-button matrix) and TriPoint::TriPoint 0x4186e0 EXACT (the TU's last function, an out-of-line empty point ctor). ZERO FUN_* / unclaimed code left in the view TU range — remainder is Phase F/G |
| **App TU** (CTheApp + CAboutDlg + Log_Write) | 0x419720–0x419ed0 | ~2 KB | ✅ DONE 07-06 (src/App/): 15/16 exact + InitInstance eff. (CPUID hand-asm) |
| **WorldDoc TU** (doc main src file) | 0x419ed0–0x41bee0 | ~8 KB | src/WorldDoc/: **8/13 exact (v37 +afxcmn: `~World` dtor gained)** + ctor-derived REAL World class; ctor/OnNew/OnOpen/GetLocatorIcon parked (imm-store batching + block-layout opens) — joint-pass fodder |
| **World/doc TU** (dta-load+worldgen+wld+doc) | 0x41bee0–0x429150 | ~54 KB | ✅ TRANSCRIPTION COMPLETE v14/v15: src/Worldgen **91 markers** = EVERY function incl. the GameView tail block + GameView::RemoveItem 0x429150 (v31: the TU's true last function, EXACT — was missed by the sweep, found via the G0 link audit); **35/91 exact** (the v29/v30 TextDialog field renames in GameView.h rotated its dial ~2 off the old "36" tally; pre-existing, not a v31 regression) + the rest annotated EFFECTIVE (per-function autopsies in-source); zero FUN_*. Unclaimed in range: 4 exception COMDATs @head, ??_GCPalette 0x41e8b0, EH thunk 0x424f69, jmp 0x424fb0 (all Phase G). Joint pass AFTER Phase E (shared Worldgen.h ⇒ E's GameView decls re-rotate this TU) |

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
| InvScrollBar | 0x44 | good | 0 | ✅ v14/v15: CScrollBar-derived, scrollMax@0x3c, scrollPos@0x40; Ctor=0x4085c0. **v33: EXPLICIT dtor `~InvScrollBar(){DestroyWindow();}` byte-matched — ??1 0x4086b0 (91B) + thin ??_G 0x408690 (30B), defined #line-neutral at GameView.cpp end (lesson #23)** |
| TextDialog | 0xc8 | good | 3 | ✅ v14/v15: NOT CDialog-derived (plain class). unk10/unk14/unk54 (ShowTextDialog args a/b/c), strText@0xb8 (CString), pParentView@0xc0, soundSession@0xc4; Ctor 0x416b90 / Run() 0x416c40 / ??1 0x427440 (Run is 0-arg, byte-match-proven) |
| InvItem | 0xc | 100 % | 0 | ✅ v15/v31: CObject-derived (vftable/pTile/name); Ctor 0x4011d0 + CtorTileName 0x401270 + explicit out-of-line ~InvItem 0x401300 (all EXACT, src/AppData); element of World.inventory@0xa8 |
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

- **G — Endgame (three sub-phases, IN THIS ORDER).**
  - **G0 — "link-to-complete" completeness audit (NEW v31; run FIRST, it's cheap + ongoing).**
    `tools/link_exe.sh` compiles every TU and attempts a full static-MFC link (NAFXCW+LIBCMT+Win32
    imports) into one EXE. The link is a COMPLETENESS ORACLE, not (yet) a byte-image: the linker
    enumerates every **unresolved external** (a true gap OR cross-TU name/sig/linkage drift) and
    every **duplicate symbol** (stub-vs-real body collision). This did Phase F's audit as compiler
    errors — it found the ONE true gap `GameView::RemoveItem` 0x429150 (now EXACT, in Worldgen.cpp)
    that the manual "zero FUN_*" sweep missed (it sits one function past the recorded TU end).
    Current state (v31): **0 duplicates, 34 unresolved** = 10 WAVMIX32 external imports (need a
    `wavmix32.lib` stub) + ~21 cross-TU DRIFTS (functions that ARE transcribed but referenced under
    a mismatched name/sig — e.g. `FindTile(void*)` vs `(Tile*)`, `EnterZone`=real `GetZoneIndex`,
    `FrameView::*`=real `GameView::*`, `CTheApp::LogWrite`=free `Log_Write`) + 3 DYNCREATE rtc
    objects + a few data-global linkage drifts. Full categorized checklist + reconciliation notes:
    **docs/link-audit.md**. Key lever: **a pure rename reconciliation is BYTE-NEUTRAL** (call sites
    are masked relocations), so most of G0 can proceed without disturbing existing byte-matches; a
    signature change needs the caller re-verified. Done when link_exe.sh reports 0 unresolved (WAVMIX
    stubbed) + 0 duplicates ⇒ a linkable image. Then pull EXE resources (RT_DIALOG/MENU/BITMAP/
    STRING via `wrestool`/PE parse) for a RUNNABLE image. G0 overlaps/feeds the de-dup work and G1.
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
- **⭐ THE TU-PHASE DIAL (2026-07-06; MECHANISM CORRECTED v36 2026-07-08):** allocator/cmp-direction
  tie-breaks carry forward through a TU and rotate EVERY function's tie-breaks together — but the
  ROTATION DRIVER is **actually-emitted code**, NOT bare declaration-set membership. ⚠ v36 controlled
  experiment (GameView TU, 73/124 baseline, isolated each axis): adding a non-virtual, never-defined,
  UNREFERENCED member decl → **byte-identical**; making it `virtual` (a NEW/non-override slot appended
  at vtable end) → **byte-identical**; a pure `#line` shift (blank line above all bodies) → **byte-
  identical** (MSVC 4.2 line numbers go to `.debug$S`/COFF line tables, NOT `.text` — masked-code
  compare is line-invariant). Positive control (one immediate 100→101) → correctly detected 73→72. So
  an INERT decl does nothing. What ACTUALLY rotates the dial: (a) a decl that is **CALLED** in the TU —
  its signature SHAPE changes the CALL-SITE codegen (arg widths / return handling, lesson #14), and
  THAT real code change cascades to siblings via the lesson-#7 TU-allocation carry; (b) a vtable change
  that **reorders existing slots** or changes `sizeof(class)`; (c) reordering/adding/removing emitted
  FUNCTION DEFINITIONS (lesson #7 proper). The canonical `+int GetZoneCell(int,int)` → Nevada-loader-jg
  evidence fits (a): GetZoneCell is CALLED in that loader. **STRATEGIC COROLLARY:** reconstructing the
  full ~200-method class is OVER-BROAD — only the SUBSET of methods a given TU actually CALLS (with
  exact signatures) + vtable-slot-ORDER + sizeof affect that TU's dial. Get called-helper signatures
  right; adding decls for methods the TU never calls is inert busywork. **Do NOT chase per-function
  phase with fake decls.** A function byte-exact under one dial but not the current one = annotate
  `PHASE-DISPLACED` (source proven correct), not a source miss.
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
  97.56 % coverage / 16.06 % exact (v29: the plain-class game TextDialog modeled + 6/7
  functions transcribed) → 98.47 % coverage (v30: PHASE E COMPLETE — TextDialog::Layout
  0x4176f0 eff. + TriPoint::TriPoint 0x4186e0 EXACT) → **99.09 % coverage / 21.24 % exact —
  v31 (2026-07-07): PHASE F — the FIRST APP TU (src/AppData/, 0x401000–0x401450) fully
  transcribed, 14/14 app funcs EXACT (MapZone/InvItem/WorldgenZoneEntry ctors/dtors + the
  AppWnd CWnd class) + 5 CObject lib COMDATs match their folded addrs. The last real un-
  transcribed source file is done — every remaining unclaimed addr is Phase-G whole-image
  plumbing (EH funclets, ??_GCPalette 0x41e8b0, static-init, linker thunks) or the two
  parked minis (InvScrollBar ??1 0x4086b0, Canvas-gap 0x407d90/dc0). Also stood up Phase G0
  (tools/link_exe.sh — full-image link as a completeness oracle): 0 duplicate symbols, 34
  unresolved (10 WAVMIX imports + ~21 cross-TU name/sig drifts + rtc/globals), and it surfaced
  + closed the ONE true gap GameView::RemoveItem 0x429150 (EXACT). docs/link-audit.md** →
  **99.09 % coverage / 20.57 % exact — v32 (2026-07-07): PHASE G0 COMPLETE. tools/link_exe.sh
  links the WHOLE image (0 unresolved / 0 duplicates / exit 0) into a RUNNABLE yoda.exe (446 KB
  with the original's copied resource section). All 34 v31 unresolved closed: 10 WAVMIX via an
  in-house non-copyrighted stub lib + 24 code/data drifts reconciled to the real defs + real .data
  tables extracted from the binary; tools/extract_res.py copies the .rsrc verbatim. Exact dipped
  21.24→20.57 % as the shared-header signature fixes rotated the dial (Worldgen ParseZaux/ParseZax2/
  SetCurrentToIntroZone + 1 WorldDoc fn → PHASE-DISPLACED; recoverable in G1). Objs moved to repo
  build/. docs/link-audit.md** → **99.17 % coverage / 209 exact funcs — v33 (2026-07-08): G1 begun +
  first RUNTIME bugfix. (1) Fixed the invisible-bubble-text bug: OnCtlColor called SetTextColor where
  the original calls SetBkColor (adjacent CDC vtable slots +0x38/+0x34) → white-on-white; now byte-EXACT
  (lesson #24: a 1-byte CALL-disp diff = wrong virtual method, not benign). (2) InvScrollBar dtors
  matched (mini #1): explicit `~InvScrollBar(){DestroyWindow();}` → ??1 0x4086b0 (91B) + thin ??_G
  0x408690 (30B), byte-exact; lesson #23: #line-neutral placement (decl on ctor's line, def at file end)
  avoided the mid-file dial rotation that had displaced 6 funcs (208→204). The running image is now a
  first-class bug oracle.** → **99.17 % coverage / 209 exact funcs — v34 (2026-07-08): STATIC bug-oracle
  sweep + AfxGetResourceHandle fix. Built a static complement to v33's running-EXE oracle (decode both
  reloc-masked streams, NW-align, flag aligned same-key non-stack mem pairs with matching base reg +
  differing disp). It proved ZERO remaining v33-class `call [reg+disp]` vtable-slot bugs and zero field
  bugs in drift-free funcs, and surfaced ONE real mis-transcription: OnInitialUpdate (11 LoadCursor) +
  DrawDirectionArrows (8 LoadIcon) used AfxGetInstanceHandle (module-state +0x8) where the orig calls
  AfxGetResourceHandle (+0xc) — both inline through AfxGetModuleState, differ only in the field. Swapped
  all 19 → 14 field bytes closed (OnInitialUpdate DIFF 31→21, DrawDirectionArrows −4); NOT runtime-
  observable (static-MFC EXE: instance==resource handle) so the EXE oracle would MISS it (lesson #25).
  Both funcs stay EFFECTIVE ⇒ 209 exact / Worldgen 34/91 / link 0/0/exit0 unchanged.** →
  **209 exact / 99.17 % coverage — v36 (2026-07-08): MECHANISM/STRATEGY session, no exact-count change.
  (1) CORRECTED the TU-phase dial: inert (non-virtual/never-called/undefined) decls, new appended
  virtuals, and #line shifts are ALL byte-neutral (proven) — only CALLED-sig shape / vtable-slot-ORDER /
  sizeof / emitted-func-set rotate a TU. ⇒ reconstructing the full ~200-method class is over-broad; lesson
  #8 + the dial rule rewritten. (2) Frontier analysis (tools/frontier.py + survey.py): the ~50 closest
  non-exact are all `this`/counter reg-coloring + jl/jg cmp-direction, proven inert to source forms +
  compiler flags (/O2 unique). (3) Fable consult: jl/jg VERSION hypothesis KILLED (our cl emits jg
  back-edges — LoadStoryHistoryOregon 0x40258b exact+jg vs sibling Nevada jl); variable NAMES ruled out;
  whole-image LINKING proven irrelevant to codegen; NEW leads = per-TU emitted-COMDAT reconciliation +
  the /Yu PCH axis (v37 START HERE).** →
  **212 exact / 99.17 % coverage — v37 (2026-07-08): the afxcmn HEADER-DIAL win (+3) + PCH hypothesis
  KILLED. (1) ⭐ A MISSING MFC HEADER is a real TU-context dial input (lesson #26): several TUs lacked
  `afxcmn.h` (the app is a common-controls MFC 4.2 app → AppWizard stdafx had it in every TU). Adding
  `#include <afxcmn.h>` textually: GameData 12→13 (FindTile 0x403aa0 [a survey top near-miss] +
  PlaceZoneObjectTiles → EXACT), WorldDoc 7→8 (`~World` → EXACT), Iact 1→2; ZERO regressions on 8 TUs
  tested. Adopted in WorldStub.h (GameData+Iact) + WorldDoc.h. A DECL effect (not PCH — proven textual).
  (2) ⭐ The /Yu PCH axis is DEAD (lesson #27, kills Fable's central v36 hypothesis): PCH is a real dial
  axis but net-NEGATIVE (Worldgen −2 from the MECHANISM, content-independent) and does NOT flip jl/jg
  (Nevada @0x30b stays jl under every config; orig is jg). The only PCH win = afxcmn decls, fully
  reproducible textually. (3) COMDAT lever (partial): Iact's emitted COMDAT set is IDENTICAL across the
  f1ca459 ReadIzon regression (11 funcs both) — that rotation is header-decl-context, not a COMDAT
  change. Worldgen over-emits GDI dtor COMDATs mid-TU (untested — ground-truth obscured by folding).** →
  **212 exact / 99.17 % coverage — v39 (2026-07-08): the LAST cheap lever CLOSED + residual class fully
  characterized (no exact change; a mechanism/strategy session like v36). (1) ⭐ COMDAT-SET lever DEAD
  (lesson #29): the reg-coloring residual is INTRINSIC to (body + header decl-set), proven on
  DetonateAdjacentTiles by 4 experiments (v38 reorder; a new COMDAT inserted right before it; a reg-pressure
  predecessor; the minimal-TU probe = IDENTICAL score solo vs full TU). Neither emitted-COMDAT set, order,
  nor neighbors move it. ??_GCPalette was a lesson-#28 misattribution (correctly emitted by the World ctor in
  WorldDoc; 0x41e8b0 = folded copy). (2) ⭐ Residual class = symmetric 2-reg ROLE swap (ESI/EDI, ECX/EDX),
  ABI-pinned: DECL/PARAM order CAN flip it (Detonate sig-swap → 60→2 bytes, regs then exact) but the true
  order is caller-pinned (nDetonatorX@0x15c=arg1) and local-reorder levers are inert/structural. ⇒ the 212
  per-TU ceiling is genuine; compile-time + intrinsic so even G2 linking won't move it — needs the exact
  original source form or a cl build/flag difference (the central open problem). ALL per-TU levers now
  provably exhausted (body/header/emission-order/PCH/COMDAT-set).** →
  **212 exact / 99.17 % coverage — v40 (2026-07-08): the COMPILER-OPTION axis CLOSED + measurement-integrity
  clarified + G2 link-order derived (no exact change; a strategy session). (1) ⭐ COMPILER-OPTION lever DEAD
  (lesson #30): NO global flag beats /O2 (interleaved-baseline battery on Worldgen — /Gr,/Gy TIE; /Ox,/O1,
  /Oa,/Ow,/Oy-,/Os,/Og-combos all WORSE) and NO per-function `#pragma optimize` flips DetonateAdjacentTiles'
  60-byte symmetric-register residual (/O2-implied letters reproduce it byte-for-byte; `a`/`s`/off all
  worse). /O2 uniquely optimal — completes the exhaustion list (body/header/emission-order/PCH/COMDAT-set/
  OPTION all dead). (2) ⚠ MEASUREMENT-INTEGRITY: the .obj is NON-deterministic (COFF timestamp + COMDAT
  symbol order; md5 varies each compile) while reloc-masked .text is stable ⇒ verify.py's best-fit pairing
  can UNDERCOUNT a TU by ~10 (Worldgen 34↔24) on clone families; progress.py's 212 is name-keyed + ROBUST
  (stable across 3 rebuilds) — trust progress.py, treat a lone verify.py number as a lower bound. (3) G2
  GROUNDWORK: derived the deterministic app-.obj link order (13 TUs, contiguous non-overlapping by first
  addr: AppData→World→GameData→Records→Iact→Canvas→GameView→IactScript→Dlg→Frame→App→WorldDoc→Worldgen) —
  the input to G2's "link app objs in address order". ⇒ ALL per-function/per-TU exact-raising is closed;
  the sole remaining path is G2 (byte-identical IMAGE, known .text reg-coloring deltas) or a different cl.** →
  **212 exact / 99.17 % coverage — v41 (2026-07-08): PHASE G2 STARTED — whole-image layout tooling + model
  (no per-func exact change; G2 is a LAYOUT effort, ORTHOGONAL to the 212 CONTENT count). Built
  tools/g2_link.sh (links the 13 app objs in address order + /OPT:NOREF + /MAP) and tools/g2_diff.py (per
  marker: LAYOUT = linked addr == orig addr; CONTENT = reloc-masked bytes equal). Baseline: 378/378 paired,
  LAYOUT 2/378, CONTENT 226/378. Proved the layout model (docs/g2-layout.md): (1) /OPT:REF eliminates
  unreferenced COMDATs (drops 19 markers in our partial image — AppWnd::Disable/Enable/GetMessageMap…);
  NOREF keeps all. (2) ⭐ the linker lays out .text COMDATs in OBJ EMISSION ORDER (= source order), per obj
  in link order — PROVEN byte-for-byte on AppData + World (refines lesson #28: content is order-invariant at
  fixed marker addrs, but LINKED layout == source order). ⇒ layout reproduction = match kept-set + per-TU
  source order + COMDAT sizes; fix an upstream divergence and everything downstream re-aligns (World already
  in perfect order, merely +0x10 shifted by AppData being 0x10 long). First divergence: AppData emits
  GetMessageMap before OnTimer (orig OnTimer@401000 first). Worklist in docs/g2-layout.md.** →
  **212 exact / 99.17 % coverage — v42 (2026-07-08): PHASE G2 — AppData+Records LAYOUT reconciled
  (LAYOUT 2→39/378, BOTH 1→32/378; 212 CONTENT stands, all changes CONTENT-neutral). (1) Removed
  BEGIN_MESSAGE_MAP(AppWnd) from AppData.cpp — the original AppData.obj emits NO GetMessageMap COMDAT (all 9
  real ones live >0x408000; AppWnd's belongs to its class's primary TU / was REF-dropped). Our copy emitted
  FIRST (shoved OnTimer off 0x401000) and added +0x10 cascading downstream. Deleting it: OnTimer emits first
  at 0x401000, AppData is correct total length → the ENTIRE World scorer run + LoadStory/SaveStory head snap
  into LAYOUT alignment. link_exe.sh still 0/0/exit0 (nothing referenced AppWnd::messageMap symbolically).
  (2) Moved Character::Read after Init in Records.cpp (orig order Init,Read,GetWalkFrameTile,…) → Records
  region collapses from a local scramble to a UNIFORM +0x50 (internally in-order, "prepared"). (3) ⭐ KEY
  FINDING (docs/g2-layout.md): G2 LAYOUT is GATED by per-function LENGTH, and the length divergences ARE the
  intrinsic reg-coloring wall — proven on SaveStoryHistory clones (0x402670/9c0/d10, DIFF 611, +10B each):
  OUR cl allocates one MORE callee-saved reg (ours pushes ebx/esi/edi vs the original's esi/edi) → longer
  stream on identical IR = the lesson-#29 ABI-pinned class, but here it changes LENGTH not just reg-names, so
  it SHIFTS everything downstream. ⇒ the byte-identical whole image is bounded by the SAME cl reg-allocation wall as
  the 212 content ceiling. Productive G2 = fix emission-ORDER scrambles (cheap/content-neutral); ABSOLUTE
  layout caps at the first intrinsic length divergence (GameData SaveStory, +0x50). Two AppData residuals
  PARKED (CObject trio -0x30 self-corrects; WorldgenZoneEntry ??_G-before-ctor quirk).** →
  **212 exact / 99.17 % coverage — v43 (2026-07-08): PHASE G2 layout-ceiling quantified + Frame reorder +
  code-cleanup pass (212 CONTENT stands). (1) Built tools/g2_order.py (--walls: ~43 clean length walls, all
  in non-exact reg-coloring funcs, first = SaveStoryHistoryNevada 0x402670 capping absolute LAYOUT at 26
  markers; --scramble: per-TU emission-order metric) + toolchain/test/orig_func_addrs.txt cache. ⭐ Found
  LAYOUT is 39 not 26 because cumulative length-drift OSCILLATES (mixed +/- walls cancel, passing through 0
  → coincidental re-alignments); absolute layout == #markers where cum-drift==0, so the true fix is
  eliminating ALL walls = cracking the cl reg-alloc wall (same as the 212 content ceiling). CORRECTED the
  v42 direction error: OURS over-allocates EBX on SaveStory (3 pushes vs orig's 2, longer). (2) Reordered
  Frame's message handlers to original emission order → Frame internally in-order (14/18 holds). Remaining
  scrambles: App (3), Worldgen GameView-tail (3) — relative-only (downstream of the walls), noted for later.
  (3) CODE CLEANUP (user req): promoted 24 World unk*/*Maybe fields WITH detailed usage comments to proper
  names + synced to Ghidra (created 6 fields Ghidra lacked, aligned 5 stale Ghidra *Maybe names); PlanToken
  enum → decimal values + dropped redundant decimal comments. All codegen-neutral (progress 212, link
  0/0/exit0, bugscan 0/0/0).** →
  **212 exact / 99.17 % coverage — v44 (2026-07-08): PHASE G2 — emission-order scrambles CLOSED + the
  /OPT question SETTLED (212 CONTENT stands; a layout/strategy session, no per-func exact change). (1)
  Reordered App.cpp to the original emission order (msgmap→ctor→theApp→InitInstance→OnIdle→LogWrite→
  CAboutDlg ctor→DoDataExchange→msgmap→OnAppAbout→OnInitDialog) → `g2_order.py --scramble` App: in-order.
  (2) Reordered Worldgen's GameView-tail block to ascending address (UseWeapon→DetonateAdjacentTiles→
  OnCmdMinimize→DrawWeaponBox→DrawWeaponIcon→BlitViewportDither→PreCreateWindow→AddItemToInv→RemoveItem)
  → Worldgen 4→1 inversions. Both CONTENT-neutral (link 0/0/exit0, bugscan 0/0/0, g2 LAYOUT 39 / CONTENT
  226 unchanged — the reordered funcs are all downstream of the first length wall so absolute LAYOUT can't
  move yet). ALL steerable emission-order scrambles now closed; the 3 residual inversions are compiler-
  placed library COMDATs (AppData ??_GWorldgenZoneEntry, GameView lib-dtor interleavings + end-placed
  InvScrollBar dtor, Worldgen ??_GCProgressCtrl@first-odr-use) — BENIGN, not source-steerable. (3) ⭐
  SETTLED /OPT:REF vs NOREF (lesson #31): the original is a **/OPT:REF** build — our REF .text is within
  1.7 % of the original (302,716 B) while NOREF is +19 % (keeps ~57 KB the original dropped). Overturns
  the v43-pickup NOREF guess (that used .rsrc-distorted full-file sizes). The G2 final image targets REF +
  reference-graph completion; the −5 KB REF gap = we slightly under-reference (COMDAT geography work).** →
  **211 exact / 99.17 % coverage — v45 (2026-07-08): PHASE G2 — World document message map reconstructed
  (the REF-drop oracle in action). Built the REF-vs-NOREF symbol-diff oracle (lesson #32): 22 app functions
  were dropped under /OPT:REF as unreferenced, 19 of them the World doc's command/update-UI handlers because
  BEGIN_MESSAGE_MAP(World,CDocument) was an empty TODO stub. Reconstructed the 14-entry AFX_MSGMAP_ENTRY
  array byte-for-byte from the binary @0x44c2d0 (IDs read from the array, handlers matched by pfn address:
  6 ON_COMMAND ToggleSound/ToggleMusic/NewWorld/SaveWorld/LoadWorld/ReplayStory + 8 ON_UPDATE_COMMAND_UI)
  into WorldDoc.cpp + declared the handlers afx_msg in WorldDoc.h's World class → all 14 fixed-field entries
  byte-match; REF-dropped 22→7. COST: ~World 0x41b2f0 PHASE-DISPLACED (212→211, DIFF 6 align=0 — the original
  .obj was built WITH the map so it's the true TU context; adding it re-rotates ~World's known intrinsic
  esi/edi reg 2-cycle). Net IMAGE gain (not shown by the .text-marker counters): byte-exact .rdata msgmap +
  15 REF-recovered functions the original keeps. The map is FUNCTIONALLY ESSENTIAL (menu-command dispatch)
  so it's required source. link 0/0/exit0, bugscan 0/0/0, g2 LAYOUT 39 / CONTENT 225.** →
  **211 exact / 99.17 % coverage — v46 (2026-07-08): PHASE G2 — World vtable overrides close 2 more REF-drops
  (CODEGEN-NEUTRAL, unlike v45). `World::IsModified`/`SetModifiedFlag` were REF-dropped because WorldDoc.h's
  World class (the IMPLEMENT_DYNCREATE TU) didn't declare them virtual → the emitted vtable pointed at the
  CDocument base versions. Declared both virtual in WorldDoc.h → vtable slots now target our overrides → kept.
  Overriding EXISTING base slots (no slot reorder / sizeof change) is inert for codegen (211 held, no
  displacement — contrast v45's msgmap). REF-dropped 7→5 (running total 22→5 across v45+v46). The 5 REMAINING
  are the hard tail (documented, NOT chased — each risks a regression for ≤2 funcs): AppWnd OnPaint/OnTimer
  (real msgmap @0x44b008 but in a DIFFERENT original TU — adding it to AppData.obj re-breaks the v42 layout),
  AppWnd Disable/Enable (ICF-folded 12-byte bodies, 19 vtable xrefs + a game call site we don't reproduce),
  and ??_H __vector_constructor_iterator (CRT helper odr-use needle). link 0/0/exit0, bugscan 0/0/0, g2 39/225.** →
  **211 exact / 99.17 % coverage — v47 (2026-07-08): PHASE G2 — .rdata vtable-content oracle + World/GameView
  vtables VALIDATED (no source change; verification session). Built tools/vtcheck.py: reads the ORIGINAL vtable
  from the exe (VA→file offset) + OUR vtable from build/<TU>.obj (the ??_7<Class>@@6B@ data COMDAT + relocs →
  per-slot target) and checks both override the SAME slots with the SAME class methods — the data-side complement
  to bugscan (a MISSING override = a virtual we forgot to declare = the base runs = a runtime bug, exactly
  World::IsModified before v46). Result: World 8/8 + GameView 8/8 override slots match, CLEAN. Confirmed v45/v46's
  .rdata is correct. The World dtor slot's ??_E-vs-orig-??_G is BENIGN: ??_EWorld/??_GWorld link to the SAME
  address under REF AND NOREF (folded — identical for a never-array-allocated class). Tool gotchas encoded: ICF
  folds trivial BASE methods to app-region addrs (FOLDED_BASE whitelist: CObject defaults 0x401060/70/80,
  DisableSelfWindow/Enable 0x401090/a0, CView no-op 0x40e3f0), and the compare must be BOUNDED to our vtable's
  real slot extent (else it reads past into adjacent .rdata — embedded Balloon sub-vtables, ??_GCPalette). link
  0/0/exit0, bugscan 0/0/0, progress 211 (unchanged).** →
  **211 exact / 99.17 % coverage — v48 (2026-07-08): PHASE G2 — vtcheck made FULLY AUTOMATIC + swept ALL modeled
  classes (no source change; verification). Rewrote tools/vtcheck.py to drop hardcoded vtable bases: it builds
  mangled_name→original-address from our own // FUNCTION markers (match.pair_by_name) then LOCATES each original
  vtable by scanning .rdata for ≥2 override addresses at consistent offsets (auto-found GameView 0x44b638 / World
  0x44c438 match the v47 manual bases — finder validated). Result: 10 classes CLEAN — CTheApp, CAboutDlg,
  CTextDialog, CMainFrame, GameView, StatsDlg, DifficultyDlg, GameSpeedDlg, WorldSizeDlg, World — every
  UI/dialog/frame/view/doc class where a missing override would be a runtime bug. 13 skipped = single-??_E-dtor
  data classes (Zone/Tile/Character/Iact*/…), un-anchorable + low-risk. Whole modeled-class vtable set now
  validated-clean or trivial-dtor. link 0/0/exit0, bugscan 0/0/0, progress 211.** →
  **211 exact / 99.17 % coverage — v49 (2026-07-08): PHASE G2 — .rdata MESSAGE-MAP oracle (tools/msgcheck.py) +
  fixed 2 real issues (BOTH codegen-neutral, 211 held). Built msgcheck (the vtcheck sibling): reads the ORIGINAL
  AFX_MSGMAP_ENTRY array via GetMessageMap→messageMap→lpEntries + OURS from the obj's ?_messageEntries COMDAT,
  compares fixed fields + handler identity per entry. Found + fixed: (1) CTheApp map INCOMPLETE (1 entry vs orig
  8 — the AppWizard File>New/Open + 5-command context-help block missing; reconstructed, ⚠ this afxres.h has
  ID_CONTEXT_HELP=0xe145/ID_DEFAULT_HELP=0xe147 swapped); (2) ⭐ GameView #11 REAL BUG — ON_WM_HSCROLL where the
  original is WM_VSCROLL (the vertical inventory scrollbar's messages went unhandled); renamed OnHScroll→OnVScroll
  (0x415ff0 body byte-matches, reflects to InvScrollBar) + ON_WM_VSCROLL + reordered entries to original → all 11
  maps CLEAN. Codegen-neutral because a non-empty msgmap's reorder/completion is .rdata data (contrast v45's
  empty→full World map that displaced ~World). lesson #33. link 0/0/exit0, bugscan 0/0/0, vtcheck 10 CLEAN.** →
  **211 exact / 99.17 % coverage — v50 (2026-07-08): DYNCREATE CRuntimeClass verification + CLASS RENAME to
  original names (USER-directed source fidelity; CODEGEN-NEUTRAL, 211 held). (1) Verified the 3 DYNCREATE
  CRuntimeClass object SIZES match (World 0x33c0, GameView 0x310, CMainFrame 0xd8 — no mis-sized-allocation
  bug). (2) Found the class-name STRINGS differ: original CRuntimeClass.m_lpszClassName = 'CDeskcppDoc'/
  'CDeskcppView' (the real MFC names; project was "Deskcpp"=Desktop Adventures) but we'd renamed them World/
  GameView. (3) ⭐ RENAMED the classes to their ORIGINAL names throughout: `World`→`CDeskcppDoc`,
  `GameView`→`CDeskcppView` — 323 tokenizer-based code-only edits across 16 files (comments/strings/#includes
  skipped), + Ghidra STRUCT rename + Ghidra NAMESPACE rename (thiscall `this` now types as CDeskcppDoc*/
  CDeskcppView*). The rename makes the DYNCREATE macro emit the correct .rdata strings NATURALLY (no
  hand-expansion). Byte-match unaffected (symbol names are masked relocs): 211 held, link 0/0/exit0, bugscan
  0/0/0, vtcheck 10 CLEAN, msgcheck 11 CLEAN. VARIABLES keep pWorld/pView (game-concept readable; original var
  names unknown).** →
  **211 exact / 99.17 % coverage — v54 (2026-07-08): ⭐ PHASE H STARTED — H1 CMake build environment DONE
  (docs/cmake-build.md). Wrote `toolchain/vc42.cmake` (toolchain file: Windows/x86 target + wrapper/VC paths)
  and top-level `CMakeLists.txt` that builds a runnable `build-cmake/yoda.exe` via `add_custom_command`s over
  the `toolchain/bin/{cl,link}` wrappers — LANGUAGES NONE, because cl 10.20 predates CMake's MSVC ruleset and
  the wrappers need link_exe.sh's exact invocation shape (cwd=src, bare-basename source → identical `Z:\`
  provenance) to stay byte-faithful. Config matrix as cache options: `YODA_GAME`(YODA|INDY), `YODA_VARIANT`
  (DEMO|FULL), `YODA_PLATFORM`(WIN32|SDL; SDL configure-FATALs till H4). Extensions ADDITIVE: default corner
  adds NO `-D`, INDY→`-D GAME_INDY`, FULL→`-D YODA_FULL`. ⭐ ANCHOR PROVEN PRESERVED: all 13 default-config
  TUs are reloc-masked byte-identical to the harness `build/*.obj` (per-named-COMDAT compare); progress.py
  still 211/99.17 %; objects isolated in `build-cmake/obj/` so oracles are untouched. Verified the built exe
  LOADS + enters its window message loop under wine (0 unresolved). Added a `run` target (mirrors run.sh).
  USER note recorded for H3: the Indy build must use the Indy app icon/resources, not Yoda's. NEXT = H2
  (demo→full Yodesk via ifdefs).** →
  **211 exact / 99.17 % coverage — v55 (2026-07-08): ⭐ PHASE H2 — full-game worldgen UNBLOCKED (anchor 211
  held; docs/phase-h2-full-game.md). Loaded retail Yoda Stories/Yodesk.exe as a 2nd Ghidra program and diffed its
  worldgen against our demo source (twins found via story-history registry strings → callers). Guarded the 4
  worldgen-blocking demo overrides `#ifdef YODA_FULL` (fall-through=demo=anchor): (1) data file YODADEMO→YODESK.DTA;
  (2) CDeskcppDoc ctor currentPlanet=2/worldSize=1; (3) ⭐ LoadWorld ~L4013 currentPlanet=2 — the OPERATIVE
  Hoth-forcer that re-forces after the ctor (verified vs retail FUN_004248a0: rotation switch + WriteProfileInt of
  the COMPUTED Terrain, no =2); (4) the goal: demo const 0x6c → `nRequestedGoalItem>=0 ? nRequestedGoalItem :
  WorldgenSelectPuzzle(-1,-1,9999)` + `if(goal<0) return 0` (verified vs retail Generate FUN_00422210 — the demo
  replaced the whole selection with 0x6c). FULL build now loads the 4.6MB data + generates without the demo's
  end-of-loading stall. ✅ USER-CONFIRMED: all 3 planets (Hoth/Tatooine/Endor) generate + play, Save/Load World
  work — functional parity with retail on the core loop (H2 core COMPLETE). ⚠ Found FOUR mislabeled source "demo" comments that are
  actually retail-IDENTICAL (NOT restrictions — corrected the goal-whitelist one in-source): the WorldgenSelectPuzzle
  per-planet goal whitelist (retail FUN_00421360), ReadZone `currentPlanet==nPlanet||bForce` (bForce=shared zones),
  Populate rand()%4 goal-zone pick, victory/loss 76/77=0x4c/0x4d (shared force-loaded). Cosmetic gates (Save/Load/
  Replay/WorldSize/Stats) enabled in FULL. Anchor: 6 src files ifdef-guarded, progress.py 211 (all fall-through
  token-neutral). ⚠ CMake gained JOB_POOL wine=1 (parallel wine cl deadlocks the wineserver — serialize). NEXT:
  confirm non-Hoth worlds generate+play; then full save/load audit; then H3/H4.** →
  **211 exact / 25989 B byte-identical — v56–v57 (2026-07-08): ⭐ PHASE H2 COMPLETE (user-confirmed all 3 planets
  play + Save/Load/Replay) + PHASE H3 STARTED, milestone 2 (Indy DAW load) DONE. H3: port shared engine to
  Indiana Jones' Desktop Adventures under `GAME_INDY` (`build-indy`; data → DESKTOP.DAW). The FULL 2.36MB DAW now
  parses to ENDF — engine-confirmed 366 zones/157 puzzles/27 chars. Indy load deltas (all #ifdef GAME_INDY,
  Yoda fall-through, anchor byte-IDENTICAL): ⭐ zones are PARALLEL-ARRAY not self-contained; IZON header 8B
  (drop globalVar+planet — was the zone-misalign/jumping-bar bug); ParseZone Indy chunkLen; ReadZone tiles-only;
  PUZ2 drops unk3+itemB; CHAR record 0x4E (frames 0x2a); TNAM name 0x10; dispatcher length-skips global
  aux/HTSP/ACTN + Indy-only ZNAM/PNAM/ANAM. ⭐ METHOD: raw-byte SIMULATION (walk DAW w/ each candidate delta,
  confirm next tag lands exactly) proved every delta anchor-safe w/o a run — beat C++ instrumentation (which
  perturbs a byte-matched TU's dial even #ifdef'd out; used only temporarily). Next = MILESTONE 4 Indy worldgen
  (Generate infinite-retries — Yoda 3-planet/goal-whitelist logic; needs DESKADV.EXE decompile+naming). Full H1
  (CMake) + H2 + H3-milestone-2 all this session; anchor never moved. docs/phase-h3-indy.md, phase-h2-full-game.md,
  cmake-build.md.** →
  **211 exact / 25989 B byte-identical — v60 (2026-07-09): ⭐ PHASE H3 milestone 4 — Indy ACTN zone-scripts
  DISTRIBUTED (the plan's "biggest delta", one line). Indy's ACTN is the SAME keyed [zone_id(2),count(2),scripts]
  block as Yoda's inline IACT, just relocated to one global chunk — proven via DESKADV.EXE IndyParseActn 1010:b5d4 ≡
  our ParseActn (14B cond record), a raw-byte DESKTOP.DAW simulation (delta=0, 319 zones/2825 scripts), and a live
  headless YDBG load. FIX = drop ACTN from the Indy dispatcher length-skip list (src/Worldgen.cpp ~L4269) → shared
  ParseActn. Anchor 211 held (GAME_INDY-guarded). ⚠ Did NOT fix the whip / unblock the bWorldInvalid workaround:
  headless verify showed WorldEntryStepMaybe still loops 0→5 because the Indy start zone's scripts are not
  COND_FirstEnter/Enter under Yoda opcode numbering — the entry-trigger/opcode semantics are the real next delta
  (docs/phase-h3-indy.md).**
  Full per-session milestone history in PLAN_COMPLETED.md.
  ~100 % = G2's byte-identical whole-image build. Track effective-match bytes separately (G, not %).

## 🚀 PHASE H — from byte-match fidelity to a living, portable, multi-game engine (roadmap set 2026-07-08)
Phases A–G drove the DEMO to **211 byte-exact** + a runnable/validated `/OPT:REF` image + fully decoded content;
the remaining byte-identity is compiler-wall-blocked (docs/compiler-hunt.md + g2-layout.md — do NOT re-chase).
**Phase H PIVOTS from fidelity to EXTENSION:** turn the decompiled source into a real, buildable engine that
(H1) builds cleanly via CMake, (H2) plays the FULL Yoda Stories, (H3) plays Indiana Jones' Desktop Adventures,
(H4) runs natively off-Windows. Byte-matching is NOT a goal past H1 — functional correctness is.

**⭐ GOVERNING PRINCIPLE — the byte-exact build is the DEFAULT, preserved corner of a config matrix.** Every
extension is ADDITIVE (ifdefs / a platform HAL); the vanilla config must STILL produce the 211-exact demo so
`progress.py` + all oracles keep passing on the anchor. Config axes:
- **GAME**: `GAME_YODA` (default) | `GAME_INDY`
- **VARIANT**: `YODA_DEMO` (default) | full (Yoda only)
- **PLATFORM**: `WIN32`/MFC (default — the original, byte-match anchor) | `YODA_PORTABLE` (SDL)

Byte-match build = (GAME_YODA + YODA_DEMO + WIN32/MFC + /O2 + address-order). Each extension relaxes exactly ONE
axis. Guard style: MFC/Win32 code under `#ifndef YODA_PORTABLE`; demo caps under `#ifdef YODA_DEMO`; Indy deltas
under `#ifdef GAME_INDY`. **Rule: any ifdef must leave the default config's PREPROCESSED tokens identical** (put
the guard so the demo/Win32/Yoda path is the fall-through) — else the 211 anchor regresses. Keep the existing
byte-match scripts (link_exe.sh/verify/progress) as the fidelity gate; CMake is for the EXTENDED builds.

### H1 — CMake build environment (VC++ 4.2 under wine) — ✅ DONE 2026-07-08 (docs/cmake-build.md). FOUNDATION, unblocks H2–H4.
**✅ Result:** `CMakeLists.txt` + `toolchain/vc42.cmake` build a runnable `build-cmake/yoda.exe` (verified: loads
+ enters its window message loop under wine, 0 unresolved). Custom-command based (LANGUAGES NONE; drives the
`toolchain/bin/{cl,link}` wrappers in the SAME shape as link_exe.sh — cl 10.20 predates CMake's MSVC ruleset).
Config matrix exposed as cache options `YODA_GAME`(YODA|INDY) / `YODA_VARIANT`(DEMO|FULL) / `YODA_PLATFORM`
(WIN32|SDL, SDL FATALs till H4); extensions are ADDITIVE (`-D GAME_INDY`/`-D YODA_FULL`, default adds nothing).
**ANCHOR PROVEN PRESERVED:** the default corner's 13 TUs are reloc-masked byte-identical to `build/*.obj`;
progress.py still 211/99.17%. Objects → `build-cmake/obj/` (separate from harness `build/`). Original spec ↓:
- Write `toolchain/vc42.cmake` (CMAKE_TOOLCHAIN_FILE): point CMAKE_C/CXX_COMPILER at `toolchain/bin/cl`, linker
  at `toolchain/bin/link` (the wine wrappers already Z:\-ify paths), set the MFC/CRT include+lib dirs, force the
  `/MT /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS` flags, static-MFC (NAFXCW) + LIBCMT + Win32 imports +
  `wavmix32` stub + `yoda.res` (extract_res.py). Mirror OpenJKDF2's CMake layout.
- `CMakeLists.txt`: target `yoda_win32` = glob `src/*.cpp` → link a runnable yoda.exe (functionally == link_exe.sh).
  Expose the config matrix as CMake options (YODA_GAME=YODA|INDY, YODA_VARIANT=DEMO|FULL, YODA_PLATFORM=WIN32|SDL)
  that set the -D defines. Default = the byte-exact corner.
- ⚠ wine+CMake friction is the main risk (CMake probing a wine cl). Fallback: a thin CMake that shells the
  existing wrappers per-file, or a Ninja/custom-command build. Keep it SIMPLE; the goal is reproducible extended
  builds, not replacing the byte-match harness.
- **Done:** `cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake && cmake --build build-cmake`
  produces a yoda.exe that RUNS the demo (verify under wine). progress.py/oracles unaffected (separate harness).

### H2 — Demo → full Yodesk.exe (ifdef'd; byte-match NOT required) — ✅ COMPLETE v55 (docs/phase-h2-full-game.md).
**✅ USER-CONFIRMED: all 3 planets (Hoth/Tatooine/Endor) generate + play; Save World, Load World, Replay Story all
work — full functional parity with retail (generate→play→save→load→replay).** Details ↓.
**✅ Worldgen unblocked:** the 4 worldgen-blocking demo overrides are guarded `#ifdef YODA_FULL` (fall-through=demo=
anchor, still 211): (1) data file YODADEMO→YODESK.DTA; (2) ctor currentPlanet=2/worldSize=1 removed; (3) ⭐ LoadWorld
~L4013 `currentPlanet=2` (the OPERATIVE Hoth-forcer — re-forces after the ctor, verified vs retail FUN_004248a0 which
has NO =2); (4) goal const 0x6c → `nRequestedGoalItem>=0 ? nRequestedGoalItem : WorldgenSelectPuzzle(-1,-1,9999)` +
`if(goal<0) return 0` (verified vs retail Generate FUN_00422210). FULL build loads the 4.6MB data + generates
without the demo's end-of-loading stall; Hoth playable (user-confirmed). ⚠ VERIFIED NOT demo (source comments were
misreads — retail identical, left as-is): the WorldgenSelectPuzzle per-planet goal whitelist (retail FUN_00421360),
ReadZone `currentPlanet==nPlanet||bForce` (bForce=shared zones), Populate's rand()%4 goal-zone pick, victory/loss
76/77=0x4c/0x4d (shared force-loaded). Cosmetic gates (Save/Load/Replay/WorldSize/Stats menus) enabled in FULL.
**⏳ NEXT:** confirm non-Hoth (Nevada/Endor) worlds generate+play (fix 3 wired, pending visual); then audit full
save/load + replay + any endgame planet-specifics (none block initial play). Retail Yodesk.exe now a 2nd Ghidra
program (diff twins by story-history strings→callers). Ref binaries: `Yoda Stories/Yodesk.exe`, `~/workspace/
DesktopAdventures/YODESK.DTA` (its world_generate() is a STUB — not a worldgen ref). Original spec ↓:
- **Reference on disk:** retail `Yoda Stories/Yodesk.exe` (455 KB, linker 3.10, dated 4 days before the demo) +
  the FULL data `~/workspace/DesktopAdventures/YODESK.DTA` (4.6 MB). Load Yodesk.exe as a 2nd Ghidra program
  and DIFF its functions against our demo source (same engine, near-identical version → most match closely).
- Find the DEMO RESTRICTIONS (what the demo caps): zone/story-item subset, disabled save/load, the demo nag,
  any `if (demo)` gates. Wrap each as `#ifdef YODA_DEMO` (default ON = the byte-exact demo path) vs the full path.
- Build target `yoda_full` (YODA_DEMO off) that loads the full YODESK.DTA and plays past the demo.
- **Done:** `yoda_full` launches, loads the full game data, and is playable beyond the demo boundary (verified by
  running it — functional parity with retail Yodesk.exe, not byte-identity).

### H3 — 32-bit Indiana Jones' Desktop Adventures (ifdef'd) — ⏳ STARTED v56 (docs/phase-h3-indy.md). needs H1 + H2's engine/game split.
**✅ Scaffolding:** `GAME_INDY` config wired; `Deskcpp.cpp` data file → `DESKTOP.DAW` under `#if defined(GAME_INDY)`
(anchor 211 held); `build-indy` (`-DYODA_GAME=INDY`) compiles to a valid exe; `YodaIndy/` run folder + `run_indy.sh`
staged (INDYDESK data+assets). ⭐ APPROACH (USER-directed): DESKADV.EXE (16-bit Indy, in Ghidra `program=DESKADV.EXE`)
is GROUND TRUTH for every delta; DesktopAdventures `is_yoda` gates are the MAP of WHERE to look (NOT byte-accurate —
its Yoda ZONE header doesn't even match our ParseZone). **Delta surface (all in the Load()/Parse* asset loaders):**
data file✅, VERS value, ZONE chunk header (Indy reads chunk LEN), IZAX zone-count, ⭐ACTN/IACT (Indy lumps ALL IACTs
in one giant section — biggest delta), CHAR record 0x54→0x4E, record/name sizes 26/24→18/16, HTSP object-qty,
Indy palette (no cycling), worldgen (Indy has NO 3-planet system — planet/goal-whitelist logic is Yoda-specific),
resources (Indy icon/menu, [[indy-app-icon]]). Milestones: 1✅ scaffolding → 2 DESKTOP.DAW parses (per-delta vs
DESKADV.EXE) → 3 renders (Indy palette) → 4 worldgen/playable → 5 resources/sound. ⚠ 16-bit DESKADV RE is harder
(segmented addrs, different codegen) — recover LOGIC not codegen. Original spec ↓:
- **Reference:** `INDYDESK/DESKADV.EXE` (16-bit NE, the SHARED CDeskcpp engine — confirmed same class names +
  WaveMix + WinG this session) + `~/workspace/DesktopAdventures` (the user's recreation implements BOTH games'
  formats/logic — the authoritative semantic reference) + Indy's data `DESKTOP.DAW`.
- Factor a game-agnostic ENGINE CORE (zone/tile/script/worldgen/inventory) from GAME-SPECIFIC deltas (asset
  format details, IACT opcode set, tile/character semantics, WinG-vs-DIBSection). Diff Indy↔Yoda via the 16-bit
  disasm + the DesktopAdventures reference; guard the deltas `#ifdef GAME_INDY`.
- Build a 32-bit Indy: engine compiled with GAME_INDY, loads DESKTOP.DAW, runs. (A NEW 32-bit port of the 16-bit
  game on the shared engine — byte-match N/A.)
- **Resources/icon (USER note 2026-07-08):** the Indy build must use the **Indy app icon** (and Indy's
  resource set), NOT Yoda's. The H1 WIN32 build copies `YodaDemo.exe`'s `.rsrc` verbatim (Yoda icon) via
  extract_res.py; for `GAME_INDY`, source the icon/resources from the Indy binary (`INDYDESK/DESKADV.EXE`
  is 16-bit NE — its resources may need a different extractor; or supply an Indy `.res`/`.ico`). Wire this
  into CMake so the `-DYODA_GAME=INDY` build links Indy's `.rsrc`.
- **Done:** a 32-bit binary that plays Indiana Jones' Desktop Adventures from DESKTOP.DAW.

### H4 — Beyond Win95: portable SDL target — needs the platform HAL grown across H1–H3. LARGEST lift.
- De-MFC/de-Win32 behind a platform HAL (`#ifdef YODA_PORTABLE`): Canvas/DIBSection/WinG blitting → SDL surfaces/
  renderer; WaveMix/MMSYSTEM → SDL_mixer; MFC CDeskcppApp/Doc/View app shell → a portable main loop + event pump;
  Win32 CFile/registry/paths → SDL_RWops/stdio; dialogs/menus/bitmaps (resources) → an SDL UI or embedded assets.
- **Reference/arch target:** the user's `~/workspace/DesktopAdventures` is already a portable recreation — mirror
  its platform abstraction, but drive it from OUR decompiled logic (more faithful than behavior-RE).
- Incremental order: video/blit layer first (Canvas → SDL), then input, then audio, then the app/doc/view shell.
  SDL2 target on macOS/Linux/Windows.
- **Done:** a native SDL build of Yoda Stories running on macOS (and the Win32/MFC byte-match build still intact).

### Phase H dependencies & suggested order
H1 (CMake) FIRST — foundation. Then **H2** (nearest win: same engine, retail binary + full data on disk; forces
the demo/full + engine/game factoring that H3/H4 reuse). Then **H3** (game axis: Indy) and **H4** (platform axis:
SDL) can proceed largely in parallel; H4 is the biggest. Throughout: the (YODA_DEMO+WIN32) config stays the
byte-exact anchor — re-run progress.py/oracles after any shared-code edit to prove no anchor regression.

### 📋 SESSION PROTOCOL (follow this shape every session)
   **⭐ v53: `src/` is now a SINGLE FLAT FOLDER (no per-TU subdirs)** — the original was a single-folder
   MFC AppWizard project ("Deskcpp"). Files renamed to their real AppWizard names where known. TU→file map
   (13 .cpp, address order = link order): `GameTypes`(was AppData 0x401000) → `Score`(World) →
   `WorldgenHelpers`(GameData) → `GameObjects`(Records) → `Iact` → `Canvas` → `DeskcppView`(GameView) →
   `IactScript` → `TextDialog`(Dlg) → `MainFrm`(Frame) → `Deskcpp`(App) → `DeskcppDoc`(WorldDoc) → `Worldgen`.
   Headers: `Deskcpp.h`(App), `DeskcppDoc.h`(WorldDoc), `DeskcppView.h`(GameView), `MainFrm.h`(Frame),
   `TextDialog.h`(Dlg), `GameObjects.h`+`GameObjectClasses.h`(Records), `IactScript.h`(was IactScriptClasses),
   `DeskcppStub.h`(was WorldStub), + kept `Canvas.h`/`MapZone.h`/`Worldgen.h`. Older docs below say
   `src/<Folder>/` — mentally map via this table.
1. **Orient:** read the ⏭ NEXT SESSION PICKUP block below; `cd src && rm -f ../build/<File>.obj &&
   ../toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
   /Fo../build/<File>.obj <File>.cpp` then `python3 tools/verify.py src/<File>.cpp` from repo root
   — confirm the recorded exact-count reproduces BEFORE changing anything (if not, a header drifted;
   bisect first). **⚠ OBJS LIVE IN `build/` (repo root), not next to the .cpp** (v32 hygiene change):
   all tooling (verify/match/progress/asmscore + link_exe.sh) reads/writes `build/<File>.obj` via `/Fo`.
2. **Ghidra check:** writes now ROUTE by `program=` (v51 fix). ALWAYS pass `program=YodaDemo.exe`
   on every mutation (rename/comment/struct) — it lands on the named program regardless of which is
   active. (No longer need YodaDemo to be the active program; just don't OMIT `program=`, or it
   targets the active one — many KOTOR/JK programs share this Ghidra.)
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

### ⏭ NEXT SESSION PICKUP (2026-07-10 v71 — milestone 5 RESOURCES done (Indy icon+title+About, full-Yoda About no-Demo) + CDeskcppDoc RECT/unk docs; full-Yoda hang investigated (works); OPEN: minor Indy tails; anchor 211)
**▶ v71 — this session (three items; anchor 211 held throughout). USER: playtesting build-indy finds few remaining
Indy issues; fixed the weapon-box centering by eye; Indy About works; full-Yoda "working now".**

**(1) ⭐ Milestone 5 — extended-config RESOURCES: Indy app icon + title + About credits, and full-Yoda About
(no "Demo"). Retires the temp SetWindowText hack + [[indy-app-icon]].** `tools/make_res.py` (+ shared
`tools/reslib.py`) builds the extended-config `.res` = Yoda's `.rsrc` base (our code references YodaDemo's integer
resource IDs — a wholesale swap breaks the UI) with only game/variant-specific resources overridden. CMake picks
the flag by config: `make_res.py <yoda> <out> --indy <DESKADV.EXE>` (GAME_INDY) / `--full <Yodesk.exe>`
(YODA_FULL) / else `extract_res.py` (pure demo anchor). Superseded/removed `make_indy_res.py`.
- **Indy app icon:** `IDR_MAINFRAME==2` for this app (`new CSingleDocTemplate(2,…)`; menu/icon/string/About all
  id 2, NOT the usual 128). reslib parses the 16-bit NE DESKADV.EXE (extract_res.py only walks a PE `.rsrc`),
  copies Indy's GROUP_ICON 2 + member ICON verbatim (DIB + dir formats identical NE↔PE), remaps its RT_ICON
  ordinal to a free 901 (Yoda uses 1..11), drops Yoda's GROUP_ICON 2 / ICON 11.
- **Indy title = "Desktop Adventures"** (AUTHENTIC DESKADV.EXE title — string id 2 doc-template AND
  AFX_IDS_APP_TITLE 0xE000/57344; NOT "Indiana Jones' Desktop Adventures"). Removed the runtime SetWindowText
  override from `src/Deskcpp.cpp` (title now flows from the resource via the doc template).
- **Indy About dialog (id 100):** rewritten with Indy's title/credits ("Indiana Jones and his Desktop
  Adventures", "The Desktop Adventures Team" + 6 names, "© 1996 LucasArts", caption "About Desktop Adventures")
  by substituting Indy text into YodaDemo's OWN 32-bit DLGTEMPLATE — reslib's parse_dlg32/build_dlg32 is
  round-trip byte-validated on both binaries, so this avoids the fiddly 16-bit→32-bit dialog conversion
  (16-bit ordinal-width ambiguity). Title static widened for the longer text; About icon → app icon (id 2).
- **Full-Yoda About (YODA_FULL):** `--full` replaces DIALOG 100 with the retail Yodesk.exe's (a 32-bit PE →
  copies verbatim) so the About reads "Yoda(tm) Stories" instead of the demo's "Yoda(tm) Stories Demo".
- VERIFIED end-to-end: build-indy + build-full both link; PE re-parse of each exe confirms the swapped icon/
  title/About. Anchor unaffected — the DEMO anchor uses extract_res.py and NO `src/` changed for the About
  work (only tools/ + CMake). ⏳ USER: `./run_indy.sh` (Indy icon/titlebar/About) + full-Yoda About has no "Demo".
  Menus left Yoda's (optional swap; command IDs match).

**(2) CDeskcppDoc struct documentation** (user-requested cleanup; codegen-neutral — Ghidra's struct was ahead of our
header, synced both ways; verified progress 211, bugscan 0/0/0, link 0/0, GAME_INDY compiles):
- **2 mystery RECTs identified + reader-verified:** +0x32b4 `rectAmmoBar` (DrawWeaponIcon 0x428c40), +0x32c4
  `rectHealthDial` (DrawHealthDial 0x42754d / DrawHealthNeedle 0x4279a4). Converted 3 more int-quads → RECT
  (`rectWeaponBox` +0x32a4, `rectArrowBox` +0x32e4), and renamed +0x3294 `rectRightPane`→`rectInvScroll`. nView*
  (+0x32d4) stays 4 ints per Ghidra (288x288 offscreen view; was the corrected-once nHealthDial misnomer).
- **~14 unks resolved (from Ghidra readers):** bWeaponHitPending +0x2e44, genSkipTeleCheck +0x2e64, bDtaLoaded
  +0x32f8, bStateFileLoaded +0x32fc, nQueuedMoveDX/DY +0x3338/333c, nWalkTargetX/Y +0x3340/3344, ammoTheForce/
  ammoLightsaber +0x3348/334a, scrollDirX/Y +0x3360/3364; genScratch[8] +0x3380 → 8 named genCell*Scratch slots.
- Files: `src/DeskcppDoc.h` (+ ctor `src/DeskcppDoc.cpp`), `src/DeskcppStub.h` (the GameData facade — kept offsets
  identical, fixed its stale nHealthDial→nView misnomer), + Ghidra struct (rectUnk3274/84→rectViewport/Inventory,
  save_program). Remaining unks (unk2c8/2e28/2e30/2e60/3368/336c/3370/3378/33a4/33b4) are undefined in Ghidra too
  — left as unk (no confidently-correct name).

**(3) full-Yoda (YODA_VARIANT=FULL) "hangs at the loading bar" — INVESTIGATED, could NOT reproduce; user: "working
now".** User reported the full build stuck at the loading bar (startup wav plays, window unresponsive). Instrumented
the FULL worldgen path with YODA_DEBUG YDBG probes (temp; reverted — anchor 211) + headless CrossOver run: worldgen
**converges on try #1** every run (`GEN SUCCESS`, planet=1 goal=85/201 — both valid full-game WORLD_MISSION ids in
the planet-1 whitelist), and the whole `Load → Populate → play` handoff completes (`nFrameMode=11`). So NO
deterministic worldgen bug. Most likely the user ran a STALE `build-full` binary — adding the FULL resource path this
session forced a full rebuild from current source; rebuilt clean (YODA_DEBUG=OFF). ⚠ WATCH: if it recurs it's likely
an intermittent seed/planet-dependent **transition stall** (OnTimer case-0xb → `WorldEntryStepMaybe` needs the
generated start zone to have a mode-advancing entry script — same family as the Indy STUP stall). NEXT if it recurs:
ask WHICH planet (Tatooine/Hoth/Endor), trace that seed's OnTimer transition, add a timeout guard. build-full is at
`YodaFull/` (YODESK.DTA); `run_full.sh` runs it; the YDBG-in-Worldgen recipe (Generate entry/goal/fail-gate probes +
retry-loop counter) is the tool.

**▶ v70 (prior; all USER-CONFIRMED, GAME_INDY-guarded, anchor 211 — FULL detail docs/phase-h3-indy.md "v69"+"v70",
memory [[h3-indy-load]]):** (1) DOOR fixed at root — the `kIndyCmdToYoda` table (src/IactScript.cpp) had a shifted
0x0b–0x14 cluster; Indy cmd 0x11=RedrawTile not SetPlayerPos (was teleporting the player onto the door, bypassing the
walk-in DOOR_IN warp); 8 entries corrected vs the decompiled dispatcher `FUN_1010_2eb6`. (2) Ammo bar removed
(`#ifdef GAME_INDY return;` in DrawWeaponIcon — Indy has no charge column, RE-confirmed). (3) Weapon box re-centered
(rectWeaponBox override in the doc ctor; user then fine-tuned by eye).
**▶ START HERE (v71): Indy is broadly PLAYABLE + resources done; pick from the OPEN tails.** USER-CONFIRMED this
session: Indy door works, ammo bar gone, weapon box centered, Indy About shows Indy credits; full-Yoda "working now"
(v71 item 3 — no repro; watch for an intermittent transition stall). Remaining OPEN (non-blocking, priority order):
(1) startup-wav name (minor); (2) hero-HP tail (entity+0x90=120 in IndyGenerate tail — we set only doc fields);
(3) verify still-uncertain IACT opcodes (0x13 rect arg-order vs DrawZoneCellRect; condition specials
0/8/9/0xb/0x14..0x16); (4) INI replay persistence; (5) OPTIONAL: Indy menus (menu id 2 — swap only if the menu text
should read Indy's; risks command-dispatch mismatch, deferred; if done, extend `tools/make_res.py --indy` since it
already keeps Yoda's `.rsrc` base + overrides identity resources). If full-Yoda hangs again, see v71 item (3): ask
WHICH planet, trace OnTimer via the YDBG recipe. All anchor-safe / GAME_INDY-guarded. ⭐ STANDING LESSONS: a Yoda
HUD/UI element may simply NOT exist in Indy (RE the DESKADV HUD-refresh draw list before "fixing" a broken-looking
one); audit the IACT remap TABLE case-for-case vs the real jump table; and for resources, KEEP Yoda's `.rsrc` base +
override only game/variant resources (icon/title/About) — our code depends on YodaDemo's integer resource IDs.
<!-- Prior H3 pickups v61–v69 condensed below — FULL detail in docs/phase-h3-indy.md (per-version sections) + memory
     [[h3-indy-load]]. CLAUDE.md carries only the current pickup (v71).
  v69 = the door root-fix (item 1 above; USER-CONFIRMED).
  v67/v68 = whip DAMAGE (UseWeapon nType=3 for non-blaster) + sound (drop `sfx\` prefix, Indy WAVs in game root) +
    temp window title (SetWindowText in InitInstance — retire via Indy .res) + mid-textbox Replay crash (frame-mode-
    gate OnUpdateReplayStory/OnUpdateLoadWorld like OnUpdateNewWorld). All user-confirmed.
  v66 = removed the Yoda-only persistent text/input lock after IactRun(2) for Indy (#ifndef GAME_INDY). v65 = whip
    reusable attempt + Replay-STUP (#ifndef GAME_INDY bWorldInvalid=0 so Indy keeps the self-climb entry).
  v64 = ⭐⭐ THE FUNDAMENTAL FIX: Indy IACT condition AND command OPCODES are RENUMBERED vs Yoda (record sizes/arg
    offsets/tile formula/events/field offsets IDENTICAL) → kIndyCondToYoda[0x17]/kIndyCmdToYoda[0x24] remap tables in
    IactScript.cpp translate at Read time; the byte-matched Yoda interpreter runs unchanged. (v69 corrected the cmd
    table's shifted cluster.)
  v62/v63 = New World infloop (route StartGame's Generate loop to IndyGenerate; ⚠ LoadWorld 0x421fd0 is a SECOND DAW
    loader — apply EVERY load-format delta to BOTH Load 0x4158 AND LoadWorld) + palette animate ranges (Indy differs)
    + ICHR char-read (Indy record 0x4E: 2 shorts after name then a full 0x30 frame block) + door-crash tile-index
    guard (band-aid, superseded by v64/v69 opcode remap). v61 = start-zone target = GetZoneCell(nStartX,nStartY).
  ⭐ RECURRING LESSON: DesktopAdventures (~/workspace/DesktopAdventures) is a REIMPLEMENTATION — its is_yoda gates +
    flag/enum assumptions do NOT match the real binary; confirm every "Indy differs" claim vs DESKADV.EXE.
-->

**▶ BUILD / RUN / DEBUG (all proven this session):**
- Build: `cmake -B build-indy -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_GAME=INDY && cmake --build build-indy`
  (⚠ JOB_POOL wine=1 serializes; parallel wine cl deadlocks). Run: `./run_indy.sh` (CrossOver GUI, USER visual).
- ⭐ **HEADLESS DEBUG ORACLE (key enabler this session):** CrossOver wine DOES reach `Load()`/worldgen/`OnTimer`
  headless (the window's timer fires). So `-DYODA_DEBUG=ON` + `#include "DebugLog.h"` (GAME_INDY/YODA_DEBUG-guarded)
  + `YDBG((...))` logs to `YodaIndy/yoda_debug.log` — a FAST logic-bug oracle for worldgen AND the game loop
  (traced the goal-gate breakdown, the quest-chain threading, and the OnTimer nFrameMode/transStep loop). ⚠ YODA_DEBUG
  perturbs byte-matched TUs — keep all YDBG GAME_INDY/YODA_DEBUG-guarded and REVERT (git) before an anchor check;
  headless run in background + Monitor for the log. Kill stale wine (`pkill -9 -f yoda.exe`) between runs.
- ⚠ `-DYODA_DEBUG=OFF` for the clean/committed build. `run_full.sh`/build-full (Yoda full) is slow/doesn't trace
  headless quickly — not a good comparison oracle.

**▶ KEY REFERENCE (DESKADV.EXE, all NAMED in Ghidra `program=DESKADV.EXE`):** `IndyGenerate` 1010:8524,
`IndyPlaceQuestNode` 1010:7f0c (param map: param_3=gridOrder/tag=our nOrder, param_4=reqItem=a4reqItem,
param_5=step-1/orderSlot=a5reqItem2, param_6=nodeType), `IndySelectPuzzle` 1010:7b58, `IndyPopulateGoalZone`
1010:5dac, `IndyParseActn` 1010:b5d4 (≡ our ParseActn — ACTN DONE), `IndyCyclePalette` 1018:8e40 (≡ Yoda
CyclePalette, enable-flag doc+0xc3c set at 1010:506c). For the entry-trigger work: RE DESKADV's IactRun-equiv
(condition switch) — find Indy's zone-entry condition opcode. Full function table + algorithm in
docs/phase-h3-indy.md (milestone-4 sections). ⚠ DesktopAdventures (`~/workspace/DesktopAdventures`) = a
REIMPLEMENTATION (where-to-look map, NOT byte/behavior truth — its `if(!is_yoda)` gates can be WRONG, e.g. it
falsely says Indy doesn't cycle the palette); CONFIRM every "Indy differs" claim against DESKADV.EXE.

**▶ ANCHOR / BYTE-MATCH (phases A–G, parked at 211):** unchanged. `progress.py` **211 exact / 99.17%**, link_exe.sh
**0/0/exit0**, bugscan **0/0/0**, vtcheck **10 CLEAN**, msgcheck **11 CLEAN**. The byte-match ceiling is compiler-
wall-blocked (docs/compiler-hunt.md + g2-layout.md) — do NOT re-chase. Every H3 edit is GAME_INDY-guarded (fall-
through = exact Yoda) or a `#else`/`#ifndef` that reproduces the original tokens.

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
  8. **The TU-phase dial refines #7 — but the driver is EMITTED CODE, not the bare decl set (CORRECTED
     v36).** A method DECLARATION that is never called and never defined is INERT (v36: adding one to
     GameView, virtual or not, and even a pure `#line` shift, all gave BYTE-IDENTICAL output). What
     rotates the TU: (a) a **called** method whose signature SHAPE (`int f(int,int)` vs `void f()`)
     changes its CALL-SITE codegen — which cascades via #7; (b) a vtable change that reorders EXISTING
     slots or changes `sizeof`; (c) reordering/adding emitted function DEFINITIONS. Signature shape is
     load-bearing ONLY for methods the TU actually calls. Only REAL methods, never fake decls. Full
     write-up + the PHASE-DISPLACED convention in "Standing rules" (roadmap section, ⭐ THE TU-PHASE DIAL).
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
  23. **#line-stable placement wins the dial for free (v33, InvScrollBar dtor).** Adding a correct
     helper-class dtor def mid-file (GameView.cpp line 186) DISPLACED 6 exact functions below it
     (208→204) — NOT because the decl set changed but because the added source lines shifted the
     **#line provenance** of everything after, rotating the TU dial (v16's blank-line lesson).
     Fix that kept BOTH the new match AND the 6: (a) put the class DECL on an EXISTING line (append
     `virtual ~T();` to the ctor's line — zero net header lines); (b) define the function at the
     END of the .cpp (after the last function) so its lines shift nothing above; (c) replace the
     old placeholder comment with the SAME line-count pointer comment. Restored 208 + the 2 dtors
     match (verified: ??1 masked-diff 0 at file-end — an SEH thunk dtor's bytes are TU-phase-
     independent, so end-placement is codegen-neutral for IT). Corollary: for a leaf/COMDAT whose
     own bytes don't depend on phase, end-of-file definition is the free-lunch placement. ⚠
     progress.py/match.py best-fit MIS-PAIRS these clone-shaped dtors (assigns 0x4086b0 to
     TextDialog::Run) and under-counts — confirm with a name-keyed coff_functions + M.mask compare.
  24. **A 1-byte `CALL [reg+disp]` diff is a WRONG-VIRTUAL-METHOD bug, NEVER a "benign immediate"
     (v33, the invisible-bubble-text fix).** The disp IS the vtable slot. `GameView::OnCtlColor`
     was `pDC->SetTextColor(0xffffff)` (slot +0x38) but the original `CALL [EAX+0x34]` = CDC::
     SetBkColor (one slot earlier — SetBkColor precedes SetTextColor in the MFC 4.2 CDC attribute-
     DC virtual cluster). Result: the balloon edit got white text on white (invisible) instead of a
     white text-BACKGROUND with default-black text (visible) — a real FUNCTIONAL bug the running
     EXE exposed, mis-annotated as a "DIFF 1 benign byte" for versions. Whenever a lone byte diff
     lands on a `CALL [reg+disp]` displacement, map disp→exact vtable slot and check whether the
     source named an ADJACENT method (Set/Get attribute-DC pairs, the CObArray/CDC clusters). The
     RUNNING IMAGE is now a first-class oracle: functional bugs (invisible text, wrong colors)
     point straight at these adjacent-slot / wrong-method mistranscriptions that byte-% shrugs off.
  25. **Wrong inlined-accessor field offset = wrong MFC helper (v34, AfxGetResourceHandle fix).**
     A same-mnemonic mem read whose base reg matches the orig but whose DISP differs by a small
     amount (here `[eax+0xc]` orig vs `[eax+0x8]` ours, ×14 across two funcs) is the twin of #24
     for INLINED accessors: both sides go through the same base call (`AfxGetModuleState()` @0x448e7e)
     and differ only in the field. `AFX_MODULE_STATE : public CNoTrackObject` ⇒ m_pCurrentWinApp@+4,
     **m_hCurrentInstanceHandle@+8** (AfxGetInstanceHandle), **m_hCurrentResourceHandle@+0xc**
     (AfxGetResourceHandle). The 11 LoadCursor / 8 LoadIcon loads in OnInitialUpdate/DrawDirection
     Arrows read +0xc ⇒ orig used **AfxGetResourceHandle()**; we had InstanceHandle (+0x8). Swap →
     14 field bytes closed. NOT runtime-observable (static-MFC EXE: instance==resource handle), so
     the running-EXE oracle MISSES this — the complementary oracle is a STATIC scan: decode both
     reloc-masked streams, NW-align (asmscore._align), flag aligned same-key pairs with a non-stack
     mem operand, matching base reg, differing disp. That scan proved ZERO remaining v33-class
     `call [reg+disp]` vtable-slot bugs and zero field bugs in drift-free funcs (only this Afx class).
  26. **⭐ A missing MFC HEADER is a real TU-context dial input — the DECL side of the corrected dial
     (v37, the afxcmn win).** The original app is a common-controls MFC 4.2 app, so its AppWizard
     `stdafx.h` = `afxwin.h`+`afxext.h`+`afxcmn.h`, included FIRST by EVERY TU. Several of our TUs
     were compiled WITHOUT `afxcmn.h` (an oversight — WorldStub.h/WorldDoc.h predate the realization).
     Adding `#include <afxcmn.h>` to a TU that lacked it ROTATES its register/instruction-selection
     tie-breaks toward the original: **GameData 12→13** (FindTile 0x403aa0 + PlaceZoneObjectTiles →
     EXACT; a survey.py top near-miss cracked), **WorldDoc 7→8** (`~World` dtor → EXACT), **Iact 1→2**;
     ZERO regressions on the 8 TUs tested (Records/Frame/App/Dlg/IactScript neutral). This is a DECL
     effect (lesson #8/#14 class), NOT a `/Yu` PCH effect — proven: textual `#include <afxcmn.h>` gives
     the identical result as an afxcmn PCH. Mechanism: afxcmn's class decls (CProgressCtrl/CToolTipCtrl/
     CImageList/CSpinButtonCtrl…) change name-lookup/type-table state the compiler carries into codegen.
     Corollary — WHEN A TU'S FIRST-EMITTED FUNCTIONS ARE THE NON-EXACT ONES, AUDIT ITS MFC HEADER SET
     against the AppWizard stdafx (afxwin+afxext+afxcmn) BEFORE grinding — a missing header is a
     free dial correction. Worldgen/GameView/World already had afxcmn (via Worldgen.h); GameData+Iact
     (WorldStub.h) and WorldDoc got it in v37. ⚠ The correct context can DEMOTE a lucky match:
     GameData's LoadStoryHistoryOregon was EXACT (jg) sans afxcmn, now non-exact — it was matching
     under a FALSE context; its source is a legit re-grind target UNDER afxcmn (a v38 jl/jg lead).
  27. **The `/Yu` precompiled-header axis is DEAD as a matching lever (v37, kills Fable's central v36
     hypothesis).** PCH IS a real dial axis (it changes codegen deterministically) but (a) it is
     net-NEGATIVE: an afxcmn PCH gives GameData +1 / Iact 0 / **Worldgen −2**, and the Worldgen −2 is
     the PCH MECHANISM ITSELF — content-independent (even an afxwin-only PCH gives −2). No config is
     globally net-positive. (b) It does NOT flip the jl/jg family: LoadStoryHistoryNevada @0x30b stays
     `jl 0x240` under EVERY PCH config; the original is `jg 0x240` (jgaudit already proved our cl emits
     jg elsewhere — it's TU-position, not build-flag). (c) The one real PCH win (afxcmn decls) is fully
     reproducible TEXTUALLY (lesson #26), so there is no reason to adopt `/Yu`. Do NOT re-litigate PCH.
     ⚠ Tooling gotcha: a `/Yc` PCH must be rebuilt IMMEDIATELY before each `/Yu` consume (stale .pch →
     silent 0/27), and the cl wrapper needs flags passed INLINE (a `$VAR` of flags reaches cl as one
     arg → D4002 → cl drops /c and tries to LINK, leaving a truncated obj).
  28. **Final-.text ADDRESS order ≠ source/.obj COMDAT order — do NOT reorder source to chase addresses
     (v38).** For a /Gy C++ COMDAT build, each function is its own section and the LINKER lays them out in
     the final image; the address order you read from the EXE is the linker's arrangement, not the .obj
     emission order (which is source order). Proven: reordering Worldgen's GameView-tail block (UseWeapon
     0x427d20 … AddItemToInv 0x428f50) to strict .text-address order was net-NEUTRAL (34/91), left the
     bellwether DetonateAdjacentTiles byte-for-byte identical (DIFF 60 → 60), and slightly WORSENED
     DrawWeaponIcon ⇒ address order is not the original source order, and this block's residuals are
     position-INSENSITIVE intrinsic reg-coloring. Corollary to lesson #7: emission-ORDER only matters
     WITHIN an .obj for the allocator's forward carry; you cannot recover it from EXE addresses, and it is
     NOT a per-TU lever — the reg-coloring/jl-jg residuals need the G2 joint build. (GameData's emission
     order happens to already match its address order — 0 mismatches — yet Nevada still emits jl; that
     seals it: correct header + correct order + correct flags, still position-locked to the whole-image build.)
  29. **⭐ The reg-coloring residual class is INTRINSIC to (function body + its headers), NOT TU-position —
     and the emitted-COMDAT-SET is a DEAD lever (v39, corrects/bounds lesson #7).** Proven on the bellwether
     DetonateAdjacentTiles 0x428680 (align=0, the pure symmetric-register class) by FOUR experiments, ALL
     leaving it byte-for-byte identical: (a) v38's full tail-block reorder; (b) inserting a brand-new COMDAT
     (`CRgn` local => ??_GCRgn+??1CRgn+the probe fn, 3 new sections, emitted-set 111→114) IMMEDIATELY before
     it; (c) inserting a register-pressure predecessor fn immediately before it; (d) the MINIMAL-TU probe —
     extract the function ALONE with just `#include "Worldgen.h"` and asmscore it: IDENTICAL score
     (total=1060, align=0, reg_pen=4, identity_miss=60) as in the full 95-func TU. So lesson #7's "context-
     sensitive, needs the whole TU" does NOT apply to this class — the score is fixed by the function's own
     IR + header decl-set, and neither preceding functions, emission order, nor the emitted-COMDAT set
     perturb it. ⇒ Worldgen's "over-emitted GDI-dtor COMDATs" cannot rotate its neighbors (the v38-pickup
     lever #1 is CLOSED), and ??_GCPalette was a lesson-#28 misattribution (correctly odr-emitted by the
     World ctor in the WorldDoc TU; its 0x41e8b0 address is just where the linker folded that copy).
     WHAT the residual IS: a symmetric 2-register ROLE swap (ESI↔EDI on Detonate; ECX↔EDX on GetZoneIndex
     0x423dc0; ESI↔EDX loop index/count on ReenableHotspotObjects 0x40ebe0). DECL/PARAM ORDER *can* flip it
     — DetonateAdjacentTiles's faithful (int x,int y) → our cl enregisters param1(x)→ESI; swapping the sig to
     (int y,int x)+caller collapsed it 60→2 bytes (registers then EXACT). BUT it is NOT faithfully steerable:
     the true param order is ABI-pinned by the caller (proven: nDetonatorX@0x15c is pushed as arg1), and for
     loop LOCALS the levers are inert or structural — GetZoneIndex cmp-operand flip: inert; ReenableHotspot
     stmt-reorder: breaks structure (align 0→16); decl-order hoist: inert (matches the ParseSnds 24-perm
     park). CONCLUSION: with the faithful source our cl deterministically picks the OPPOSITE symmetric
     register from the original cl on identical IR. This is the true 212 per-TU ceiling; since the residual
     is COMPILE-time + intrinsic, even G2 LINKING won't move it — closing the gap needs either the exact
     original source form (unrecoverable per-function) or a subtly different cl build/state (the standing
     central open problem; version hypothesis already killed v37). Do NOT re-grind this class per-function.
  30. **⭐ The COMPILER-OPTION axis is DEAD — no global flag NOR per-function `#pragma optimize` flips the
     intrinsic symmetric-register choice (v40; completes the "levers exhausted" list).** Tested on Worldgen
     (34/91 baseline) with an INTERLEAVED-baseline harness (measure /O2, then the variant, back-to-back —
     required to defeat the v40 measurement-noise finding below): every global flag is ≤ baseline —
     `/Gr` and `/Gy` exactly TIE (they don't touch our explicit-__thiscall member codegen), while `/Ox`,
     `/O1`, `/Oa`, `/Ow`, `/Oy-`, `/Os`, and `/Og`-piecewise combos all score strictly WORSE. Per-function
     `#pragma optimize("L", on)` on DetonateAdjacentTiles 0x428680: the /O2-implied letters (`g`,`t`,`y`,`w`,
     `gt`) reproduce the 60-byte residual byte-for-byte (they're already active), and every letter that
     changes something (`a`=assume-no-alias, `s`=size, ``=off) makes it far WORSE (align 0→502/638/1870).
     ⇒ /O2 is uniquely optimal; the symmetric-register residual is invariant to every VC++ 4.2 build knob.
     Combined with #27 (PCH dead), #28 (emission-order linker-owned), #29 (COMDAT-set dead): ALL per-TU +
     all option levers are now closed. The only paths left to raise exact beyond 212 are G2 (byte-identical
     IMAGE, not exact .text) or a genuinely DIFFERENT cl.exe build (unobtainable). Do NOT re-test flags.
     ⚠ **v40 MEASUREMENT-INTEGRITY finding (know this before trusting a per-TU count):** the compiled .obj
     is NON-deterministic run-to-run (COFF timestamp + COMDAT symbol ORDER vary; md5 differs each compile)
     while the reloc-masked .text is STABLE. Consequence: `verify.py`'s best-fit COMDAT pairing occasionally
     mis-pairs clone-family COMDATs depending on the obj's symbol order and UNDERCOUNTS a TU by ~10 (saw
     Worldgen flip 34↔24 across identical recompiles; verify.py is deterministic *per fixed obj* — 34×5).
     **progress.py's headline 212 is name-keyed and ROBUST** (stable across 3 fresh full rebuilds) — trust
     it; treat a lone verify.py per-TU number as a lower bound, and re-run/confirm with a name-keyed check.
  31. **⭐ The original is a `/OPT:REF` build — the G2 final image must link with `/OPT:REF`, not NOREF
     (v44, settles the v43-pickup open question).** Measured `.text` vsize of three links of the SAME
     build/*.obj set: original 0x49e7c (302,716 B); our **/OPT:REF 0x48ac7 (−5,045 B / −1.7 %)**; our
     /OPT:NOREF 0x57df7 (+57,211 B / +19 %). NOREF keeps ~57 KB the original DROPPED ⇒ the original is
     NOT NOREF; REF lands within 1.7 % (transitive COMDAT elimination is the release/non-`/DEBUG` default
     for link 3.10). The v43-pickup "454K vs 446K hints NOREF-like" used full-FILE sizes distorted by the
     verbatim-copied `.rsrc` — the `.text` comparison points the OPPOSITE way. `tools/g2_link.sh`'s NOREF
     is ONLY a layout-analysis scaffold (keeps transcribed-but-not-fully-cross-referenced funcs visible in
     the map); the byte-identical image needs REF **+** a complete reference graph. The −5 KB REF gap = we
     slightly UNDER-reference (a few real MFC/GDI COMDATs the complete original odr-used that our stubbed
     source doesn't — the ??_GCPalette-style under-emit); closing it is the COMDAT fold-vs-survive geography
     work (docs/g2-layout.md worklist #3), NOT a flag change. Do NOT re-test REF-vs-NOREF.
  32. **⭐ The REF-vs-NOREF symbol diff is a precise "reproduced-reference-graph gap" oracle — and empty
     message-map stubs are the #1 leak (v45).** Link the SAME obj set twice (`/OPT:REF` and `/OPT:NOREF`,
     each `/MAP`), diff the app-obj symbol sets: `NOREF − REF` = functions REF drops as unreferenced. In the
     COMPLETE original each IS referenced, so every dropped symbol is a real hole in OUR reference graph.
     v45: 22 dropped, 19 of them the World doc's command/update-UI handlers because
     `BEGIN_MESSAGE_MAP(World,CDocument)` was an empty TODO. **Reconstructing an MFC 4.2 message map from the
     binary:** GetMessageMap returns `&messageMap` = `AFX_MSGMAP{pBaseMap, lpEntries}` (8B); `lpEntries` → a
     24-byte `AFX_MSGMAP_ENTRY{UINT nMessage,nCode,nID,nLastID,nSig; PMSG pfn}` array + a zeroed terminator.
     Read the fixed fields, match each `pfn` to a handler by ADDRESS, recover the macros (ON_COMMAND: nCode=0
     nSig=0x0c; ON_UPDATE_COMMAND_UI: nCode=-1 nSig=0x2c), write them in ARRAY order + declare the handlers
     `afx_msg` in the class the map's TU compiles against (address-only refs ⇒ codegen-inert for that TU's
     functions). The 14 entries' fixed fields byte-matched; REF-dropped 22→7. ⚠ **A correct map can still
     PHASE-DISPLACE a previously-exact function:** the original .obj was built WITH the map, so the map is the
     TRUE TU context; adding it re-rotated `~World`'s intrinsic reg 2-cycle (212→211, align=0, lesson #29). A
     functionally-essential map (menu-command dispatch) is REQUIRED source — keep it, annotate the displaced
     function PHASE-DISPLACED. progress.py/g2_diff count only .text markers, so a byte-exact .rdata data COMDAT
     + REF-recovered functions are real IMAGE gains they don't show — weigh the image, not just the headline.
  33. **⭐ .rdata CONTENT oracles catch runtime bugs the .text % can't — vtable slots (vtcheck, v47/48) AND
     message-map entries (msgcheck, v49).** vtcheck: a MISSING vtable override = a virtual we forgot to declare
     (base runs). msgcheck: a WRONG/MISSING AFX_MSGMAP_ENTRY = a menu command or window message silently mis-
     dispatched. Read the ORIGINAL map via GetMessageMap (`mov eax,&messageMap; ret` → {pBaseMap, lpEntries} →
     24-byte entries till zero nMessage) and OURS from the obj's `?_messageEntries@Cls@@` COMDAT; compare fixed
     fields (nMessage/nCode/nID/nLastID/nSig) + handler identity (pfn addr via markers). v49 found: **CTheApp map
     INCOMPLETE (1 vs 8 — the AppWizard File>New/Open + context-help block missing)** and **GameView #11 was
     `ON_WM_HSCROLL` where the original is `WM_VSCROLL` (0x115)** — the vertical inventory scrollbar's messages
     went UNHANDLED (mis-named `OnHScroll`+mis-registered; the 0x415ff0 body byte-matches, it reflects to
     InvScrollBar). Fix = rename→OnVScroll + ON_WM_VSCROLL + reorder entries to match. BOTH fixes CODEGEN-NEUTRAL
     (211 held — a msgmap is .rdata data, reordering/completing it doesn't rotate code, UNLIKE v45's empty→full
     World map which displaced ~World; the difference: World went from 0 entries, these were already non-empty).
     ⚠ IDs are afxres.h-version-specific: this MFC 4.2 has ID_CONTEXT_HELP=0xe145 / ID_DEFAULT_HELP=0xe147
     (swapped from the usual) — trust the EMITTED value, not the assumed constant. A wrong `CALL [reg+disp]`
     (lesson #24) is the .text twin: all three (vtable slot / msgmap entry / call disp) are silent to byte-%.
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
- **Out-of-line empty ctor ⇒ custom type, not MFC CPoint/CRect (v30, TextDialog::Layout).** When
  the disasm shows a stack `T arr[N]` construction LOOP calling an out-of-line `mov eax,ecx; ret`
  ctor, the source type is NOT MFC's CPoint/CRect — their default ctors are `_AFXWIN_INLINE` (get
  inlined at /O2, so no call emits). Model a custom class (`struct T : public tagPOINT { T(); };`)
  with the empty ctor DEFINED out-of-line and AFTER the use site (cl compiles top-down, so it
  can't inline what it hasn't seen) — it emits as this TU's own COMDAT (TriPoint 0x4186e0 EXACT).
- **A big jump-table function with two CDCs + a this-swap is EFFECTIVE-by-construction (v30).**
  TextDialog::Layout (1419B) landed align 374: the wins are structural (store per-branch not
  hoisted; per-case `ShowWindow(K)` cross-jumped via #18 instead of a `nShow` var), but cl's
  trace-driven duplication of dead range-compares (#15), pointer-reload aliasing (#19), and the
  this-in-ESI landing are not source-steerable — annotate and defer to G1, don't grind.



## ⏮ PRIOR PICKUP (v72, 2026-07-10) — condensed (superseded by v73: microfx M0 + remap verified)

v72 delivered: CLAUDE.md consolidation (history → this file); ⭐ Indy MIDI music end-to-end +
USER-CONFIRMED (MCI command strings, SNDS 0x0e–0x11 = MIDs, eerie=0x12/eep=0x13, per-id opened
flags, stop-all on ToggleMusic-off/new-game, close-all in view dtor; DESKADV functions named:
IndySoundInit 1018:4c54, IndyPlaySound 1010:e43c, IndyStopAllMusic 1018:6e34, IndyPlayThemeMusic
1018:6dd0, IndyViewOnUpdate 1010:e1aa, IndyViewTeardownMaybe 1010:dff0, IndyOnToggleMusic
1010:c092); ⭐ Yoda→Indy sound-id remap `Indy_MapSoundId` (games' SNDS tables differ; data-driven
ids bypass via PlaySoundData, demo `#define PlaySoundData PlaySound` token-neutral); v71 "hero-HP
entity+0x90=120" was a 16-bit-offset misread — 0x78 = Indy INTRO ZONE (Yoda 0x5d), StartGame
literals GAME_INDY→0x78. Gotcha: mmsystem.h #defines PlaySound→PlaySoundA (#undef); winmm.lib in
CMake link. Open items rolled to v73: USER-VERIFY of the remap SFX/music moments; IACT cmd 0x13
rect arg-order + cond specials 0/8/9/0xb/0x14–0x16; INI replay; optional Indy menus; DESKADV sweep
scope measured (~214 app-code unnamed: seg 1010=91, 1018=109, 1020=14; 1000/1008 = MFC/CRT library,
skip). v73 then VERIFIED the whole remap against DESKADV (all 28 IndyPlaySound call sites + both
SNDS tables: every entry CONFIRMED, none wrong, none missing).

## ⏮ PRIOR PICKUP (v73, 2026-07-10) — condensed (superseded by v74: M0 oracle GREEN)

v73 delivered: (1) H4 microfx strategy designed AND M0 achieved — MFC-4.2-subset drop-in headers
(`microfx/`), all 13 game TUs compile + whole-archive-link natively on arm64 macOS; guarded
shared-TU edits (Canvas asm blits ×2, Deskcpp CPUID, PTRINT ×11, GameTypes AppWnd map, Worldgen.h
portable dtor decl), design + portable lessons (key-function/ODR vtable trap; END_TRY swallow) in
docs/phase-h4-sdl.md. (2) Indy sound-id remap FULLY VERIFIED vs DESKADV (both SNDS tables + all 28
IndyPlaySound call sites; every Indy_MapSoundId entry confirmed, none missing) — verdict table in
docs/phase-h3-indy.md "v73". Anchor 211 re-verified. Open items carried to v74's pickup: M0-finish
(native doc creation + YODESK.DTA + wine log diff — DONE in v74), M1 gdi/, Indy stragglers (IACT
specials, INI replay, optional menus, audible USER-VERIFY), DESKADV Ghidra sweep (~214 unnamed).

### ⏮ v74 pickup (condensed 2026-07-10, superseded by v75)
H4 M0 COMPLETE + oracle GREEN: native arm64 bootstrap (theApp InitInstance → doc template → real
microfx CWinApp::OnFileNew → CDeskcppDoc), YODESK.DTA Load (658 zones), fixed-seed
Generate+Populate, WORLD/CELL digest byte-identical to same-seed wine run. Delivered: MSVC-4.2
rand/srand LCG shim, INI-backed profile API, real GetModuleFileName, CFile '\\'→'/', DIBSection
calloc buffer, stub-view DATA-layout fix (lesson 5: union overlays in DeskcppStub.h/GameObjects.h),
lesson 6 (header presence = dial input at zero tokens — DebugLog.h include must stay guarded),
YODA_SEED/digest debug rig, m_nFrameDelay named. Open items carried: M1 gdi/ (DONE in v75),
MainFrm.h stub views before M2, Indy stragglers (IACT specials, INI replay, menus), DESKADV sweep.

### ⏮ v75 pickup (condensed; superseded by v76)

H4 M1 COMPLETE: real gdi/ layer (mfxgdi.cpp — tagged HDC__/HBITMAP__ memory DCs + 8bpp DIB
sections, clipped all-SRCCOPY BitBlt, Set/GetDIBColorTable; pure C++, no SDL) + microfx.h
extension API (MfxGetDCDib/MfxWriteDibBMP) + zone_view harness (BMP dump + --show SDL window);
oracle GREEN by eyeball on title/desert/interior/snow zones. Canvas's `(BITMAPINFO*)&biHeader`
cast is load-bearing (palette[256] after the header = bmiColors). TRAP: GetZoneById off-planet
slots hold -1 not NULL. Also v75: YODA_BUGFIX flag — 12 crash/UB/leak sic-sites fixed via
line-neutral YODA_SIC_FIX/YODA_SIC_RETURN/BUGLOG macros (3 synced header-tail copies), behavior
bugs kept; digest A/B identical ON/OFF; sic#11 fires on seed 0x2a. Open items carried: M2 pump
(DONE in v76 — game runs natively, hero walks), MainFrm.h stub views (DONE in v76), Indy
stragglers, DESKADV sweep.

### ⏮ v76 pickup (condensed; superseded by v77)

H4 M2 COMPLETE — THE GAME RUNS NATIVELY ON macOS (build-sdl/yoda: title → intro → Dagobah,
game loop, input, walking + camera scroll). mfxwnd.cpp = HWND objects + THE message-map
dispatch engine (map-chain walk, 17 AfxSig decodes, WM_COMMAND view→frame→app routing), real
SetTimer/posted-msg queue, real SDI bootstrap (OnFileNew never paints — first WM_PAINT from
the pump keeps headless M0 flow; digest A/B identical). mfxpump.cpp = the one SDL file: events
→ VK → WM_*, screen-DIB present per frame (YODA_SCALE=2), SDL_QUIT→SC_CLOSE (ConfirmExit,
auto-IDYES headless; SDL maps SIGTERM→SDL_QUIT). gdi palettes real (Create/Select/Realize/
Animate → DIB color table; cycling works). game_walk = deterministic M2 oracle (synth arrows
⇒ cameraX/Y moved). MainFrm.h portable stub views (lesson-5; offsetof probe identical; anchor
GREEN after header line-shift). TRAPS distilled: playerX/Y = world-map cell, cameraX/Y =
in-zone anchor; keyboard walk needs key-state AND WM_KEYDOWN repeats. User playtest v76:
walking/dragging/IACT/pickup/weapons work; text bubbles + F8 = M4 stubs; zone transitions
missing → v77 root causes: BitBlt self-overlap row order + in-handler animations never
presented (present-on-screen-write hook) + clock() CPU-µs vs Win32 wall-ms.

### ⏮ v77 pickup (condensed; superseded by v78)

H4 M2 TAIL — zone transitions + X-Wing flight + drag save-under, all USER-CONFIRMED. Three
stacked root causes fixed, all microfx-only: (1) overlap-aware BitBlt (same-surface downward
blit must copy rows bottom-up — ScrollZoneTransition blits the screen over itself);
(2) ⭐ present-on-screen-write hook (MfxSetScreenWriteHook): Win32 shows screen-DC writes
IMMEDIATELY, our pump presents between handlers — in-ONE-HANDLER clock()-busy-wait animations
(ScrollZoneTransition, StartGame's STUP flight) collapsed silently to their final frame while
per-TIMER-TICK animations (mode-6 doors) worked — check this FIRST for "animation missing";
gdi fires a raw callback, pump registers the SDL presenter, gdi stays SDL-free;
(3) clock() shim (MSVC clock()=wall ms, host=CPU µs ⇒ busy-waits ~1000x fast; #define clock
mfx_clock → monotonic ms, rand/srand pattern). Also real: CreateBitmap/Set/GetBitmapBits (8bpp
DDB≡DIB, drag save-under). YODA_SHOT=<prefix>[:count]. User playtest: weapons/ammo work; drag
redraws at tick rate (keep software cursor as build option — DS port interest); bubbles +
F8 = M4. ⚠ When PatBlt/FillRect/SetPixel become real (M4 HUD), fire the screen-write hook
there too. Open items carried: M3 audio (DONE in v78 — WaveMix+MCI over SDL2_mixer,
user-confirmed, dwFlags=2=USELRUCHANNEL lesson), INDY×SDL untested (DONE in v78 — builds+runs,
native MIDI), Indy stragglers, DESKADV sweep.

### ⏮ v78 pickup (condensed; demoted 2026-07-10 when v79 completed M4 core)
H4 M3 AUDIO COMPLETE (user-confirmed): snd/mfxsnd.cpp = WaveMix*+mciSendString over SDL2_mixer.
Key lesson 7: MIXPLAYPARAMS dwFlags=2 = WMIX_USELRUCHANNEL (not CLEARQUEUE) — sounds MIX;
"cut off prematurely" = flag misread. Handles = 1-based indices into a Mix_Chunk table
(g_waveHandles int[64], LP64); packed params via memcpy; AfxBeginThread deliberately runs no
thread (SDL_mixer self-mixes). INDY×SDL first built & ran natively this session (THEME.MID via
fluidsynth; GM SoundFont at /opt/homebrew/share/soundfonts/default.sf2 — GeneralUser GS; demo
font = wip-wip percussion symptom). Only src/ edit: 3 YODA_PORTABLE old-for-scope decls in
Worldgen.cpp's GAME_INDY tail (lesson 8) — anchor re-ran FULL GREEN. The M4 goals it set
(res loader, GDI chrome, modal TextDialog w/ locator-click test case, scrollbar, teardown,
software-cursor option) all landed in v79a-g.
