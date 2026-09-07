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

### ⏮ PRIOR (2026-07-26 v97, cond. from CLAUDE.md — the dial hunt: retract the compiler-wall, 215>211, member-vs-file-scope resolved)
Commits b0d430a (de-hex) · fff083b (dialsweep tools + 215) · f747e02 (prior retractions) · 2c9067f (headersweep). Anchor held 211/99.17% + all 5 oracles.
- **THE COMPILER WALL WAS WRONG (v96 retraction):** `tools/dialsweep.py` hits **215 exact project-wide +4/−0** from only **7 extra file-scope symbols via Worldgen.h**; plateau 6/7/8 (214/215/214); validated 4 ways (struct/typedef/extern/6-field-enum) + determinism repeat ⇒ **interim-compiler hypothesis DEAD** — ParseZaux 0x423110 + ZoneHasIzxItemMaybe 0x41bfa0 both go byte-exact under OUR VC4.2.
- **THE DIAL IS A FILE-SCOPE SYMBOL COUNT** (`tools/enumfieldtest.py`): enum = tag+field count (empty body free ⇒ unused enumerators ARE dial-active); identifier length IRRELEVANT; macros FREE (never enter the symbol table); decl kind irrelevant. DIFFERENT mechanisms exist too: an empty include FILE costs a func (v95); `sizeof(T)` for a literal costs one (v96). Don't over-unify.
- **THE RULE (v96):** the dial is an INSTRUMENT, not a knob. FREE GAIN (zero regressions) = fingerprint of truth; TRADE (+3/−3) = fingerprint of padding. NEVER pad to a number; 215 is placeholder, deliberately UNCOMMITTED.
- **membertest (v97): members are INERT** — a struct's N members don't dial (flat n=1..12), only the TAG; externs move it, **n=7 uniquely unlocks 0x41f830** ⇒ missing symbols MUST be file-scope or enum enumerators.
- **headersweep (v97 localized):** gap is TWO TUs — DeskcppView.cpp ~6-8 symbols short (gains 0x40ebe0/0x40fca0), Worldgen.cpp EXACTLY 7 short (gains 0x423110 n≥3, 0x41f830 only n=7); every other TU ALREADY correct (only LOSE when perturbed). Reach via Worldgen.h/Deskcpp.h; TextDialog.h is the clean control.
- **Ghidra globals inventory (v97):** only unmodelled REAL worldgen global = DTA/.wld record-tag table 0x00456890 (16×8: ENDF ACTN HTSP ZAX3 ZAX2 ZAUX VERS ZONE PUZ2 SNDS CAUX CHWP CHAR TNAM TILE STUP; YODASAV44 magic at +0x20). SHIPPED unreferenced at Worldgen.cpp EOF; **wiring the strcmp sites was the planned pickup #1.**
- **Placement/#line (v97, measured):** the table at TOP of Worldgen.cpp flips 0x41d8d0 OFF (34→33, #line rotation NOT the +1 symbol); at **EOF it is 34/34 zero-delta** ⇒ EOF = dial-safe home for any new real global in a byte-matched TU.
- **Safe sweep protocol:** every header-sweep tool restores via atexit+finally (+ .bak). NEVER run two sweeps concurrently or during a progress.py. Verify clean: `git diff --stat src/` + `grep -rn DIALSWEEP src/`.

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
     ⚠⚠ **PARTIALLY CONTRADICTED v96 — scope this to CLASS MEMBERS.** v36 tested an unreferenced
     *member* declaration inside a class and found it byte-identical. v96 measured **FILE-SCOPE**
     inert declarations (`struct S;` / `typedef int T;` / `extern int v;` / an `enum`) appended to a
     header tail and they DO move the dial, hard: +7 file-scope symbols = 211 -> 215 project-wide,
     and one 23-name `enum ArtooHint` cost 6 functions. An enum counts as tag + field count; an
     empty enum body is free; identifier length is irrelevant (pure count); macros are free (never
     enter the symbol table). ⇒ "inert decls do nothing" holds for MEMBERS, not for file scope.
     ⚠ the member-vs-file-scope reconciliation is the LIKELY explanation but is NOT yet directly
     A/B-tested — do that before relying on it (tools/dialsweep.py --header <h>).
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
  29. ⚠⚠ **OVER-GENERALIZED — see the v96 correction at the end of this entry.**
  29. **⭐ The reg-coloring residual class is INTRINSIC to (function body + its headers), NOT TU-position —
     and the emitted-COMDAT-SET is a DEAD lever (v39, corrects/bounds lesson #7).** Proven on the bellwether
     DetonateAdjacentTiles 0x428680 (align=0, the pure symmetric-register class) by FOUR experiments, ALL
     leaving it byte-for-byte identical: (a) v38's full tail-block reorder; (b) inserting a brand-new COMDAT
     (`CRgn` local => ??_GCRgn+??1CRgn+the probe fn, 3 new sections, emitted-set 111→114) IMMEDIATELY before
     it; (c) inserting a register-pressure predecessor fn immediately before it; (d) the MINIMAL-TU probe —
     extract the function ALONE with just `#include "Worldgen.h"` and asmscore it: IDENTICAL score
     (total=1060, align=0, reg_pen=4, identity_miss=60) as in the full 95-func TU. So lesson #7's "context-
     sensitive, needs the whole TU" does NOT apply to this class — the score is fixed by the function's own
     ⚠⚠ **v96 CORRECTION: true for DetonateAdjacentTiles, FALSE as a statement about the class.**
     Detonate is genuinely dial-invariant — it never went exact once across ~70 probed dial positions,
     independently corroborating this entry FOR THAT FUNCTION. But the class does not follow it: the
     v96 sweep took 10 other stuck functions to exact purely by changing the header decl-set, incl.
     both surviving "4.0-only" cases (ParseZaux 0x423110, ZoneHasIzxItemMaybe 0x41bfa0). Note this
     entry already says "function body + ITS HEADERS" — the header term was the live one all along,
     and v39 under-explored it. Fingerprints + method: docs/compiler-hunt.md v96, tools/dialsweep.py.
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
  34. **⭐ Inlining a non-static MEMBER stages the pointer arg through EAX — a file-scope/`static`
     helper does NOT (v99, +5 exact in one stroke).** MSVC 4.2 lowers `p->Virt(0)` inside a plain
     member as `mov ecx,[esp+4]; push 0; mov eax,[ecx]; call [eax]` (11 B), but when the call is
     reached by INLINING another **non-static member** of the same class it emits
     `mov eax,[esp+4]; push 0; mov ecx,eax; mov edx,[eax]; call [edx]` (13 B) — the inlinee's
     implicit `this` nominally owns ECX, so the argument must be materialized in EAX and copied.
     PROVEN by probe (scratch TU, same flags): in-class definition, out-of-class `__inline`
     definition, an inline member *predicate* used as the ARGUMENT (`p->Enable(IsFull())`), and
     `((COther*)this)->Dis(p)` ALL reproduce the 13 bytes; `static __inline` free functions,
     `static` MEMBERS, local-object member calls, local copies, casts and references ALL fold to
     the 11-byte form. So the tell is specifically **an inlined member with an implicit `this`**,
     not "inlining" and not "a helper".
     ⇒ WHEN YOU SEE IT: a redundant `mov reg,reg` right before a thiscall, plus the vtable load
     using the ORIGINAL register instead of ECX, means the body you are looking at came through
     an inline member — go add that member rather than filing it as "allocation artifact". v99
     turned all five of the demo-grayed `OnUpdate*` stubs (0x403510/0x403600/0x403610 on
     CDeskcppDoc, 0x4165a0/0x416800 on CDeskcppView) exact this way; they had been parked since
     v34 as "EFFECTIVE / allocation artifact of a `this`-ignoring member — park".
     ⚠ COROLLARY for idiomscan (tools/idiomscan.py): a lone extra `mov` makes the mnemonic
     multisets differ, so these land in **class D SOURCE/IDIOM**, not class B REGALLOC — do not
     assume a small class-D delta is unreachable regalloc noise. Conversely a register-COPY diff
     is not automatically regalloc: check for this idiom first.
     ⚠ Header COMMENT lines are provably INERT (A/B: deleting 5 comment lines from a class body
     left three Worldgen.cpp asmscores bit-identical) — consistent with the v96 symbol-count dial
     model. Adding the member itself IS a dial event (v99 net +10/-6 across the project).
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

⏮ v79 pickup (demoted 2026-07-10, superseded by v80) — H4 M4 UI CHROME CORE, USER-CONFIRMED (a-g,
ZERO src/ edits). res/mfxres.cpp parses embedded yoda.res → LoadString/LoadIcon+DrawIcon/LoadCursor/
named RT_BITMAPs; real GDI pens/brushes/fonts + FillRect/PatBlt/Pie/RoundRect/Polygon/lines/pixels/
GetClipBox/GetSysColor; genuine MS Sans Serif 13/16px FNT strikes (fon2c.py→mfxfont_data.c, -8→13px
-14→16px, synth bold); REAL GetMessageA over ONE MSG queue (both CWinThread::Run and the game's modal
loops drain it; msg.pt=cursor, hit-test to children, capture-aware); word-wrap CEdit + CBitmapButton;
overlay hook re-composites children on every screen write (MfxTouchHold/Release batches); SB_CTL
scrollbar (bevel arrows/checker/thumb → WM_VSCROLL to InvScrollBar); teardown (pump exit → view dtor →
WaveMixCloseSession). Lessons 9-13 (docs/phase-h4-sdl.md): WM_ERASEBKGND before WM_PAINT (redraw
storm); PC_RESERVED skip in color match (health-dial flash) + LineTo excludes endpoint (bevel);
shared-surface children need overlay re-composite + boot-dirty care; modal loops = one queue +
headless GetMessage bail; SOFTWARE cursor composited at present (YODA_HWCURSOR=1 hw). v79h fixed
inventory-scroll: ::GetParent was a stub returning 0 → InvScrollBar repaint never ran (audit hint:
"X updates but Y doesn't" ⇒ grep mfxstubs.cpp for a silent 0-returning stub the Y-path needs).

### ⏮ v80-v82 pickup (condensed; demoted 2026-07-10 when v83 landed the visible menu bar)
v80: CDialog::DoModal REAL (mfxdlg.cpp) — parses RT_DIALOG, MfxDlgItem:CWnd per control, modal
loop, DDX_Text; menus reachable via the game's REAL RT_ACCELERATOR id 2 (Ctrl-chords only, no OS
menu bar yet); CFileDialog::DoModal REAL (lists *.wld rows, no native picker). Lessons 14-17:
DDX_Text overloads can't be extern "C"; dialog controls carry ABSOLUTE root-client rects; new
microfx/src files need a cmake reconfigure (GLOB is configure-time); BN_CLICKED(0)-vs-accelerator
WM_COMMAND collision in a picker's modal loop needs an `msg.hwnd==m_hWnd` scope check. v81: GOAL-0
present-batching fix, user-confirmed "much snappier" — MfxPresentOnScreenWrite just marks dirty;
flush happens per pump tick / per MfxIdle / via MfxSetClockHook, throttled by YODA_PRESENT_MS
(lesson 18). v82: platform-backend split, user-confirmed — `microfx/include/mfxplat.h` contract
(MfxPlat*/MfxSndPlat*), exactly one backend TU per surface from microfx/src/platform/, SDL3 now
the desktop default; mfxpump.cpp/mfxsnd.cpp hold ALL policy so a new port only implements the two
backend TUs (lesson 19). Indy×SDL playtest + SDL3 MIDI audibility still open at the time.

### ⏮ v83 pickup (condensed; demoted 2026-07-11 when v84 fixed the Indy crash)
v83: a REAL VISIBLE MENU BAR, user-confirmed live ("the menu items do work as I'd expect, very
nice") — new `microfx/src/app/mfxmenu.cpp`. Two coordinate worlds kept apart: the bar itself
(parsed from the game's real RT_MENU id=2) lives in its own chrome HDC/DIBSection, composited
above the game's screen DC at present time (`MfxComposeWindowDib`) — window grows
`MFX_MENUBAR_H` (19px) taller, game DC itself untouched; mouse events with window-y <
MFX_MENUBAR_H are intercepted before the normal WM_* pipeline, everything else gets y -=
MFX_MENUBAR_H so game logic sees unchanged coordinates. A clicked popup lives INSIDE the game's
normal screen-DC space and rides the existing dialog child/capture/overlay-repaint machinery
(`MfxMenuPopup:CWnd`); item enable/check state comes from `MfxQueryCmdUI` (real
`OnInitMenuPopup` shape) driving the game's own `OnUpdate*` handlers — nothing about menu
semantics was invented. Debug: `YODA_AUTOCLICK=<ms>:<x>:<y>[,...]` (up to 4).
4 real bugs found + fixed via live testing (all microfx-only): (1) **`CDeskcppDoc` was missing
from the WM_COMMAND/CN_UPDATE_COMMAND_UI dispatch chain** — real MFC routes
View→**Document**→Frame→App, ours only checked View→Frame→App, so New World/Save/Load/
Replay/Sound-toggle/Music-toggle (all on `CDeskcppDoc`'s message map) were unreachable via ANY
WM_COMMAND path, predating v83 — ⭐ lesson: any ON_COMMAND/ON_UPDATE_COMMAND_UI audit must check
ALL FOUR of View/Document/Frame/App. (2) `EnableWindow`'s repaint was gated on stale
`m_mfxRedraw` state — fixed to repaint unconditionally on a state change, matching `ShowWindow`.
(3) Menu-popup close left a 1-frame highlight flash — `MfxMenuCloseAll` now calls
`MfxPaintIfDirty()` synchronously. (4) `CFileDialog` didn't find files that existed — root cause
was `CDeskcppDoc`'s ctor resolving `installPath` to a literal `"YODA"` on this port (no real
registry/drives to probe) and our SDL picker not falling back to cwd the way real COMDLG32
does — fixed via an `opendir` probe + `"."` fallback in `CFileDialog::DoModal`. Found but
NOT fixed: a DEMO-variant `CFile::Read` assertion crash (`m_pStream` null) reproduces with zero
user interaction ~10-15s into idling on the title/intro; not isolated to a call site.

## ⏮ v84 (2026-07-11) — INDY×SDL live playtest: crash #16 fixed, Hide Me!/P-pause wired (condensed pickup)

- **Indy indoor-talk/bump crash FIXED, user-confirmed** (docs/engine-bugs.md #16): `CDeskcppView::ShowWinMessage`
  has Yoda-hardcoded tile ids 780/2034, never GAME_INDY-ifdef'd; Indy's smaller tile catalog (1144 entries) made
  `tiles.GetAt(2034)` read OOB on nearly every bump/talk (the 2034 arm evaluates before the goal-item check via
  left-to-right `&&`). Fixed with the line-neutral `YODA_SIC_FIX` short-circuit shape (standing lesson in
  CLAUDE.md THE ANCHOR). ⭐ Method: the fix came from a LIVE BACKTRACE via `MfxArrayOOBTrap` (kept in
  microfx/include/afxwin.h; logs array/index/size + backtrace to stderr and yoda_crash.log) — the first
  code-read guess (#15 charId bounds) was a plausible-looking red herring. Reach for the trap FIRST on OOB hunts.
- **Hide Me! wired, user-confirmed**: `CWnd::OnSysCommand` was a no-op stub; added `MfxPlatMinimize()` to the
  mfxplat.h backend contract (sdl3/sdl2 = SDL_MinimizeWindow, null = no-op).
- **P pause hotkey wired, user-confirmed**: `MfxTranslateAccel` broadened from Ctrl-chords-only to plain
  FVIRTKEY entries (`bWantCtrl == bCtrl`); F1→ID_HELP_INDEX rides the same path (still not live-verified).
- New harness: `YODA_AUTOMOD=<start>:<vk>:<dur>` (holds a modifier's g_mfxKeyState only, for chord tests).
- End-of-session live-confirmed STILL BROKEN list (F8 dialog, Statistics, INDY menu bar, the roaming
  CFile::Read assert) — ALL cleared in v85 (see the v85 pickup/⏮): F8+CFile were one microfx bug (CFile ops
  must THROW CFileException on unopened stream, not assert), Statistics = retail Indy HAS no Stats feature
  (real Indy menu imported instead), menu bar = `make_res.py --indy` ne_menu_to_win32.

## ⏮ v85 (2026-07-11) — condensed (demoted from the pickup in v87)

- **F8 debug dialog + the roaming `CFile::Read` crash = ONE bug (F8 USER-CONFIRMED).** microfx
  `CFile::Read/Write/Seek/GetPosition/GetLength` ASSERTED on a never-opened stream; real MFC release
  semantics THROW `CFileException`, and `CDeskcppDoc::LoadWorldStateFile` (OnDraw startup path)
  RELIES on that throw for the "no .wld state file" case (TRY/CATCH → nFrameMode=12). The assert
  turned a designed control path into an abort ~seconds into any run whose doc path is absent — v83's
  "idle crash" AND why Ctrl+F8 looked broken. Fixed by making those CFile ops throw (mfxcore.cpp).
  ⭐ Lesson: when a microfx ASSERT fires in game-driven code, first ask "does MFC THROW here and does
  the game CATCH it?" — grep the call-site for TRY/CATCH before blaming the game. (lldb `-b -o run -k
  "bt 25"` gave the call site in one shot.)
- **Statistics + INDY menu bar resolved by GROUND TRUTH: retail Indy has NO Stats feature.**
  DESKADV.EXE's RT_MENU id 2 has no Statistics item and its RT_DIALOG set has no 0xe1/0xe0/0xdf — the
  grayed item was never the bug; shipping YODA's menu on Indy was. `make_res.py --indy` now converts
  DESKADV's 16-bit ANSI menu template to Win32 (`ne_menu_to_win32`) and overrides RT_MENU 2 — every
  command id already in our dispatch space (note Indy "Using Help" 0xe143 vs Yoda 0xe144, kept
  faithful). Indy's dialog 0xbf is control-for-control identical to Yoda's (F8 needs no import).
- **INI replay persistence DONE + cross-process verified.** RE'd from DESKADV (savers 1020:0000/
  1020:0339, loaders 1018:e7af/1018:eb39): Indy persists TWO `[GameData]` lists — `Wyoming<N>` = the
  single story history (doc+0x194; kept in our storyHistoryAlaska slot) and `Hawaii<N>` = placed-zone
  list (+0x14e). Format vs Yoda: ',' separator, EVERY value comma-terminated, 10/line, per-line
  obfKey (rand()%255+1), worldSeed prefix; history caps at 10 (Yoda 3); EMPTY history seeds default
  story 0x86. Wired into IndyGenerate head/goal-append/tail + OnReplayStory GAME_INDY branch. ⚠ The
  old `((void)0)` no-op MACROS for the four new names silently ATE the new definitions
  ("expected unqualified-id" AT a definition = the name is a macro; `-E` and look). Stale-key flaw
  (shrinking line count leaves old keys readable) is DESKADV-faithful — reproduce, don't fix.
- **IACT condition table VERIFIED case-for-case vs DESKADV.** The condition switch is INLINED in the
  IACT runner (FUN_1010_2910 → IndyIactRunScripts; jump table 1010:2a28) — NO separate condition fn.
  Six `kIndyCondToYoda` entries were WRONG, now fixed: 0x08↔0x09 swapped (gameState==-1=LOST→0x11,
  ==1=WON→0x12 — Yoda's enum NAMES lie, its CODE is truth); 0x0b→0x20 RandVarNe(arg0:=-1); 0x14→0x0e
  CheckEndItem (142 DAW uses — highest impact); 0x15→0x0f CheckStartItem; 0x16→0x13 HealthLs. Cond
  0x00 has no Yoda twin but ZERO uses in all 2825 DESKTOP.DAW scripts → always-pass safe.

## ⏮ v87 pickup (condensed 2026-07-11, on starting v88)

**AfxMessageBox real modal (M5 tail):** `MfxShowMessageBox` (mfxdlg.cpp) reuses the M5 control kit
(CK_DLGFRAME + wrapped CK_LABELs + OK/CANCEL/YES/NO button row from the MB_ type), own GetMessageA
loop (Enter/Esc/Y/N, click scoped to the dialog), exposed via microfx.h so AfxMessageBox (mfxcore)
and MessageBoxA (mfxstubs) share it; caption = app name else AFX_IDS_APP_TITLE. Headless-safe:
returns -1 when `MfxPumpIsUp()` is false → callers keep the M-era auto-answer (smoke harnesses
unchanged). USER-CONFIRMED live. **Debug oracles in modal loops:** AUTOMOD/AUTOKEY/AUTOCMD/
AUTOCLICK/SHOT factored into shared `MfxDebugOracles()` (mfxpump.cpp), pumped by the modal
GetMessageA wait too; AUTOCLICK now emits the LBUTTONUP (kit buttons commit on UP). ⚠ a speech
BUBBLE is itself a nested modal that swallows non-bubble clicks. **Redraw residue on New/Replay/
Load (user-found):** modal boxes overlapping the inventory left pixels after close — the handler
returns into a regen+transition whose busy-wait redraws only the game area (clock-hook presents,
no full OnDraw), so the close-time MfxSetDirty never ran. Fix: force `MfxPaintIfDirty()`
synchronously at EVERY modal close (MfxShowMessageBox, CDialog::DoModal, both CFileDialog paths).
⭐ LESSON: a modal returning into a partial-redraw operation must repaint ON CLOSE, not mark dirty.
**GOAL 1 closed (Indy):** the v84 "hero-HP tail" was a misread — DESKADV's "entity+0x90=120" is
`view->nTargetZoneId=120` (local_3e is the VIEW from GetFirstViewPosition), and Indy health is
doc+0x1096/0x1098 reset 1/1 by the StartGame twin (1020:0ed0) — all already in shared code. Added
the genuinely missing IndyGenerate tail writes (GAME_INDY-guarded): timeBase=time(NULL) (+0x58,
timeOffset NOT cleared — faithful), unk50=startZoneId/unk2e34=0 (+0x44/+0xc34), cameraX/Y=0x160/0xa0
(+0x10a2, vs Yoda's 0x140/0x140). Anchor re-verified green (211/99.17, all 5 oracles).

## ⏮ v86 pickup (condensed 2026-07-11, during v88 cleanup)

**Statistics dialog:** dialog 0xe1 is the game's ONE DLGTEMPLATEEX resource — microfx
CDialog::DoModal only parsed classic DLGTEMPLATE → bogus cx/cy → early IDCANCEL (v80's "corrupt
in the .res" note explained). Added an EX branch (0xFFFF signature) to header+control parse
(mfxdlg.cpp); classic path untouched. **SDL3 native file dialog:** `MfxPlatShowFileDialog` on the
mfxplat.h contract; SDL3 backend drives the async SDL_ShowOpen/SaveFileDialog to completion
(pump-spin); other backends return -1 → in-window row picker. Focus nit fixed: flush
FOCUS_LOST/GAINED + SDL_RaiseWindow after the panel closes (a stale lone FOCUS_LOST paused the
game — nFrameMode=0/bBusy=1 stuck on STUP). **Ctrl+D → F8 dialog:** macOS eats the Ctrl+F8 CHORD
(system shortcut) before SDL; neutral-pump alias injects synthetic VK_F8 while real Ctrl held;
`YODA_KEYLOG=1` (sdl3 backend) logs key events. **Testing lesson (superseded in v87):** the game
sits in a blocking GetMessageA modal loop from ~3s in, so main-loop-only debug oracles never fired
once the intro started — v87's shared MfxDebugOracles fixed this.


---

### ⏮ v95 PICKUP (demoted 2026-07-26 by v96) (2026-07-26 v95 — READABILITY SWEEP, mostly done, tree GREEN.)

**▶ GOAL (user-set 2026-07-26): de-hex the source.** Four asks, in the user's words:
(a) decimal-ize hex values that don't make sense as hex — "mostly, coordinates";
(b) where a hex value is really a *value domain*, make an **enum** instead of decimalizing;
(c) make defines for the MFC/Win32 stuff that's written as raw hex;
(d) make a define for **18 → Zone width/height**.
Leave genuinely-hex things alone: DTA tile/item catalog ids, bitmasks, Canvas.cpp's MMX
`_emit` opcode bytes, and `+0xNN` struct offsets in comments.

**⚠ THE LESSON THIS SWEEP BOUGHT (new, load-bearing — add to the lessons list):**
**Adding one more `#include` FILE to `Worldgen.cpp`'s chain costs a byte-exact function
(211 → 210, `Worldgen.cpp` 34 → 33, −80 B) — and it does so EVEN WHEN THE INCLUDED FILE IS
EMPTY.** Measured three ways (empty guard-only header; unmodified HEAD `Worldgen.cpp` +
the include; include removed → 34 returns). So it is the *include itself*, not the macros,
not the line count — same dial family as the afxcmn.h lesson (memory [[afxcmn-header-dial]]).
⇒ **New shared constants must be APPENDED TO THE TAIL OF AN ALREADY-INCLUDED HEADER, never
put in a new file.** A `src/Resource.h` was written and then deleted for exactly this reason;
its body now lives in the **"═══ Resource ids ═══" block at the tail of
`GameObjectClasses.h`** (which every hex-heavy TU already includes). Pure `#define` (never
enum) for anything a byte-matched TU sees, so the token stream is untouched.
TUs that can't see `GameObjectClasses.h` carry their own few ids at their header's tail:
`Deskcpp.h` (IDS_APP_TITLE, IDS_ERR_16_COLOR_VIDEO), `TextDialog.h` (IDD_TEXT_ENTRY,
IDC_TEXT_FIELD0..3), `MainFrm.h` (MAIN_WINDOW_WIDTH/HEIGHT 525×310).

**▶ DONE (anchor re-verified 211 exact / 99.17 % after EVERY batch below — the tree is
GREEN as left. NOT yet committed. Batches 1-4 from the v94 session are described in the
⏮ block; v95 added batches 5-10):**
1. **Zone geometry (ask d)** — `GameObjectClasses.h` head block: `ZONE_WIDTH/ZONE_HEIGHT 18`,
   `ZONE_LAYERS`, `ZONE_CELL_COUNT`; applied across GameObjects/Iact/DeskcppView/Worldgen.
2. **Pixel geometry** — `TILE_PIXEL_SIZE/COUNT`, `VIEW_TILES/VIEW_PIXEL_SIZE`,
   `CANVAS_PIXEL_SIZE`, `VIEW_SCROLL_MAX`, `LOCATOR_CELL_SIZE/INSET`, `SCROLL_STEP_PIXELS`,
   `SCROLL_WRAP_SRC`; HUD rect block in the `CDeskcppDoc` ctor fully decimalized.
3. **Small TUs finished**: `Deskcpp.cpp`, `MainFrm.cpp`, `TextDialog.cpp`, `Score.cpp`.
4. **Menu command ids** named + applied to both message maps.
5. **String table recovered + named** (real strings via `tools/reslib.py`), AND ⭐ **applied at
   all ~83 call sites** (`LoadString`/`AfxMessageBox`): DeskcppView 58, Worldgen 17,
   WorldgenHelpers 5, DeskcppDoc 1, IactScript 2. Low-band ids 3/4/5/6 were recovered and
   added (`IDS_CONFIRM_LOAD_WORLD`, `IDS_WARN_MIDI_DISABLED`, `IDS_ERR_OPEN_DTA`,
   `IDS_ERR_DTA_SHARING`). Header tails got the ids their TU cannot otherwise see:
   `IactScript.h` → `IDS_ERR_UNRECOVERABLE`; `Deskcpp.h` → `IDS_WARN_MIDI_DISABLED`.
6. **MFC/Win32 flags named**: `MB_YESNO`/`MB_ICONSTOP`, `IDYES`/`IDOK` return compares,
   `CFileDialog(FALSE|TRUE, …, OFN_EXPLORER|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT /
   OFN_FILEMUSTEXIST|…)`, `m_ofn.Flags &= ~OFN_SHOWHELP`, `InvScrollBar::Create(WS_CHILD|
   WS_VISIBLE|SBS_VERT, …, IDC_INV_SCROLLBAR)`, the 3 bubble `CBitmapButton::Create(WS_CHILD|
   WS_VISIBLE|BS_OWNERDRAW, …, IDC_BUBBLE_*)`, `wndDialogText.Create(WS_CHILD|WS_VISIBLE|
   ES_MULTILINE|ES_NOHIDESEL|ES_OEMCONVERT, …, IDC_BUBBLE_TEXT)`.
7. **GDI named/decimalized**: `PATCOPY`, `COLOR_BTNFACE/BTNSHADOW/BTNHIGHLIGHT`,
   `RDW_INVALIDATE|RDW_ERASE|RDW_UPDATENOW`, `RGB(255,255,255)`, health-dial
   `RGB(255,255,0)/(0,255,0)/(255,0,0)`, `CLR_INVALID`, `GetPixel(…,312,284)`,
   `TextOut(…,190,235)`, `SetPalette(0,256,…)` / `(160,86,…+160)` / `(10,5,…)` +
   the matching `AnimatePalette` bands, `sysPalette[160]`.
8. **Resources by id applied**: 10 `LoadCursor(MAKEINTRESOURCE(IDC_CURSOR_*))`,
   8 `LoadIcon(MAKEINTRESOURCE(IDI_ARROW_*))`, `IDT_GAME_TICK`/`IDT_ANY`,
   `CDialog(IDD_STATISTICS/DIFFICULTY/GAMESPEED/WORLDSIZE, …)`, `DDX_Text(…, IDC_STATS_*)`,
   `GetDlgItem(IDC_*_SLIDER)`, `IDC_LOAD_PROGRESS`, progress `CRect(x+17,y+272,x+286,y+285)`.
9. **VK codes**: the deferred-move `switch (nMoveCommand)` arms 0x21..0x28 →
   `VK_PRIOR/NEXT/END/HOME/LEFT/UP/RIGHT/DOWN` (each confirmed by the dx/dy it sets).
10. **Enums applied (ask b, the safe half)**: existing `ZoneType` across Worldgen's
   `PlaceQuestNode`/`IndyPlaceQuestNode` args, `genZoneTypeScratch`, `mapGrid[].zoneType`,
   `pZone->type` compares + the Indy populate switch arms, and DeskcppView's two zone-type
   tests; existing `ZoneObjType` (`OBJ_LOCK`, `OBJ_TELEPORTER`); existing `TileFlags` for the
   UNAMBIGUOUS masks only — `0xc0`→`TILE_WEAPON|TILE_ITEM`, `0x100080`→`TILE_LOCATOR|
   TILE_ITEM`, `0x100000`→`TILE_LOCATOR`, `0x100081`→`TILE_LOCATOR|TILE_ITEM|
   TILE_GAME_OBJECT`, and the `tflags & TILE_WEAPON / TILE_ITEM` categorizer.
   `nFrameMode` 0xb/0xc/0xd/0xe decimalized to 11/12/13/14 everywhere (incl. the `case`
   labels at DeskcppView.cpp 3493/3556/3557/3565/8049/8101).

**▶ REMAINING (pick up here):**
- ⭐ **`ArtooHint` enum — WRITTEN, THEN REVERTED UNVERIFIED (do this first).** The exact
  enum body was appended to the tail of `src/DeskcppView.h` (just before the final `#endif`,
  after the `PlaySoundData` block) and then backed out because the session ended before
  `progress.py` could confirm it. **The mapping is fully recovered and correct** — it is
  ClassifyTile's (0x0040fca0) result, a 1:1 index into the IDS_HINT_* table that OnDragItem's
  switch consumes (both verified by reading the two switches side by side):
  `-1 NONE, 0 ENEMY, 1 DARTH_VADER, 2 STORAGE_DEVICE, 4 DOOR, 5 CHARACTER, 6 YODA,
  7 PUSH_PULL, 8 XWING, 9 VICTORY, 10 DEFEAT, 11 EWOK, 12 JAWA, 13 DROID, 14 LUKE,
  15 TELEPORT_ACTIVE, 16 TELEPORT_IDLE, 17 MEDICAL_DROID, 18 WEAPON, 19/20 BEEP_1/BEEP_2`
  (value 3 is never produced nor consumed — do not invent a name for it). Re-add as
  `enum ArtooHint { ARTOO_HINT_* }`, **run `tools/progress.py` with the header change ALONE
  first** (that isolates "enum tokens in a widely-included header" from the .cpp identifier
  substitution), then apply at the two switches in `DeskcppView.cpp` (~4570-4630 producer,
  ~4940-5030 consumer) and re-oracle. ⚠ if the header alone costs a function, fall back to
  plain `#define`s — same reasoning as the Resource-ids block.
- **`IactResult` enum** — the `result |=` mask bits in `Iact.cpp` (0x20 redraw / 0x80
  full-redraw / 0x100 player / 0x200 game-over / 0x400 inventory / 0x800 zone-warp), already
  documented in the `Iact.cpp:885` comment. Untouched this session.
- **Ambiguous `TileFlags` masks deliberately LEFT RAW** (`& 0x10000`, `& 0x20000`,
  `& 0x40000`, `& 0x60000`): bits 16-19 are GROUP-DEPENDENT ALIASES (WEAPON vs ITEM vs
  CHARACTER subtypes), and the enum has TILE_PLAYER/TILE_ENEMY/TILE_FRIENDLY only as
  COMMENTS, not enumerators. Naming them needs real RE (read the DTA group bit first) —
  do not guess. `(tflags >> 16) & 0x10` in the Worldgen categorizer was also left alone
  (rewriting the shift would change the byte-matched shape).
- **Zone-catalog ids** `0x5d/0x5e/0x5f/0x60/0x217` (the fixed spaceport 2x2 at world-grid
  cells 44/45/54/55): decided these are catalog ids (leave-hex family), but the READABLE
  win is naming them — from the placement code 0x5e=NW, 0x5f=NE, 0x5d=SW, 0x60=SE and
  0x217 substitutes for the NE quadrant in one variant. Add as `#define ZONE_SPACEPORT_*`
  in the Resource-ids block if you want them.
- **Leftover counters not yet done**: `mapGrid + 0x2c` → 44, `pFile->Read(buf, 0x10/0x18)`,
  health-band thresholds `0x19/0x32/0x4b/0x64`, `nCode == 0x68`(104, a grid marker compared
  beside decimal 1 and 300 — appears in Worldgen/WorldgenHelpers/DeskcppDoc, a value domain
  worth a name), `DeskcppDoc.cpp`'s `return 0xffffffff` sentinels + sibling `0x11/0x10/0xe`
  zone-state codes (enum territory, semantics not established), and `Canvas.cpp`'s
  `biSize = 0x28` / `biClrUsed = 0x100` / `for (int i = 0x1a; …)`.
- **Consider**: `WORLD_GRID_SIZE 10` for the pervasive `playerY * 10 + playerX` (user's call).
- ⚠ **The other four oracles have NOT been re-run this session** — only `progress.py`
  (211 exact / 99.17 %, green after every batch). Before committing, run `tools/link_exe.sh`,
  `bugscan.py --all`, `vtcheck.py`, `msgcheck.py`.

**▶ HOW TO WORK THIS SAFELY:** batch edits with a python script that asserts an exact
occurrence COUNT per replacement (that caught two miscounts already), keep every edit
LINE-COUNT-NEUTRAL in byte-matched TUs (lesson #23 — one multi-line reflow of
`ShowTextDialog` had to be undone), then re-run `tools/progress.py` after each batch.
If the count drops, bisect by restoring `git show HEAD:src/<TU>.cpp` and re-running —
that isolates .cpp edits from header effects (and is how the include lesson above was found).
⚠ `tools/verify.py`'s per-TU number is a LOWER BOUND and disagreed with `progress.py`
(33 vs 34) — it made one bisect read as a false negative. Trust `progress.py`.


---

### ⏮ PRIOR PICKUP (v95–v97 — the byte-match dial re-opened; 211 is a plateau, not a ceiling)

**▶ v95/v96 (2026-07-19/26) — "the compiler wall" DISPROVED; an ambient-dial is real.**
Re-opened the parked compiler-hunt after the de-hex sweep's dial lessons. **211 → 215 with +4
gained / −0 lost** was reachable (`tools/dialsweep.py`), purely from **7 extra file-scope
symbols through Worldgen.h**; validated 4 ways (struct/typedef/extern/6-field-enum all land
215) + determinism. The two surviving "interim-cl" pillars (`ParseZaux` 0x423110,
`ZoneHasIzxItemMaybe` 0x41bfa0) go byte-exact under OUR VC 4.2. Mechanism (all measured,
`tools/enumfieldtest.py`): a PURE file-scope SYMBOL COUNT — enum = tag + field count,
identifier LENGTH irrelevant, **macros are FREE** (never enter the symbol table), declaration
KIND irrelevant. NOT everything is symbol count (empty include file still costs one; sizeof(T)
for a literal costs one — at least 3 distinct mechanisms). ⚠ **the 215 was PLACEHOLDER decls,
deliberately NOT committed source** — never pad to a number (`tools/dialsweep.py`, restored
via atexit; ⚠ never run two sweeps concurrently — they fight over the header AND build/*.obj).
`tools/headersweep.py` localized the gap: **DeskcppView.cpp ~6–8 short, Worldgen.cpp exactly 7
short (0x423110 n≥3, 0x41f830 only n=7); every other TU already correct** (Iact/Helpers/
Objects/IactScript/Doc only ever LOSE when perturbed). Use `tools/idiomscan.py` to classify
residuals (⚠ slice original at OUR trimmed COMDAT length, not `toolchain/test/app_funcs.txt`).

**▶ v97 (2026-07-26, this session) — pickup steps 1 & 2 EXECUTED; real RE artifacts shipped.**
- **Member-vs-file-scope A/B — RESOLVED (was UNTESTED).** `tools/membertest.py` on Worldgen.cpp:
  adding N members to an UNUSED struct (one constant tag) is **INERT** — exact stays flat at
  +2 across n=1..12 (the +2 is the single struct TAG symbol, not the members); file-scope
  externs move it, and **n=7 uniquely unlocks 0x41f830**. ⇒ **the missing symbols MUST be
  file-scope** (or enum ENUMERATORS, which leak to the enclosing scope; struct members never
  escape the class scope — that's why enums dial and members don't). This reconciles v36
  #8 (member inert) with v96 (file-scope active).
- **Ghidra globals inventory (pickup step 1) — done.** Enumerated all .data globals referenced
  from the Worldgen TU (0x41c340–0x429000; no .bss — MSVC folded into .data). Result: every
  named worldgen global was ALREADY modelled EXCEPT the **DTA/.wld record-type tag table
  (0x00456890)**: 16×8 bytes `ENDF ACTN HTSP ZAX3 ZAX2 ZAUX VERS ZONE PUZ2 SNDS CAUX CHWP
  CHAR TNAM TILE STUP` + the `YODASAV44` save magic at 0x456910, which we'd transcribed as
  inline `strcmp(tag,"...")` literals. **Added as `char g_aDtaRecordTags[16][8]`** at
  **EOF of Worldgen.cpp** (unreferenced-but-faithful; wiring the strcmp sites deferred).
- **Placement / lesson-#23 refinement (MEASURED):** the same table placed at TOP of
  Worldgen.cpp (adds ~19 lines) **flips 0x41d8d0 OFF (34→33)**; placed at **EOF (line-neutral)
  it is byte-exact-neutral (34/34, zero gain/loss)**. So the earlier 33-dip was a **#line
  rotation artifact, NOT the +1 symbol**. EOF is the dial-safe home for any new real global.
- **`TileFlags` +3 enumerators SHIPPED:** promoted the CHARACTER-subtype aliases
  `TILE_PLAYER/TILE_ENEMY/TILE_FRIENDLY` (=1<<16/17/18, aliases of TILE_LIGHT_BLASTER/
  HEAVY_BLASTER/LIGHTSABER) from comments to real enumerators in `GameObjectClasses.h`
  (readability-motivated; pickup-sanctioned).
- **Dial did NOT move to 215** — real additions (cpp-EOF array + shared-enum aliases) kept
  **211 exact / 99.17%** and all 5 oracles green (bugscan 0/0, vt 10 CLEAN, msg 11 CLEAN,
  link 0/0, build-sdl green). ⚠ IMPORTANT MODEL REFINEMENT: the "missing 7" is **not any 7
  file-scope decls** — the +7 sweep was specific to that artificial extern pattern; real
  additions did NOT reproduce it, so 0x41f830 remains gated on exactly matching the original's
  ~7 symbols (unknowable from the binary alone; more RE or lucky reload needed, or accept 211
  as the honest plateau per "never pad"). All changes commit-verified; anchor never dropped.


### ⏮ PRIOR PICKUP — DEMOTED at v99 (2026-07-26 v98 — pickup #1 (tag-table wiring) EXECUTED → honest re-baseline 211→213; found + FIXED a pinned-seed worldgen retry spin; added the smoke-harness watchdog the user asked for. All 5 oracles GREEN on 213, build-sdl + build-sdl-indy green, save_smoke 1/42/7 + worldgen_smoke 1 + game_walk pass. Tree GREEN + COMMITTED b0ee41a. Old v97 dial-hunt log demoted to PLAN_COMPLETED.md ⏮.)

**▶ WHAT HAPPENED.** v98 executed pickup #1 (wiring the v97-shipped `g_aDtaRecordTags` table into the
records) — that made the honest anchor count RISE **211 → 213**, so we re-baselined deliberately with
all five oracles re-run in one pass. Along the way we tripped a real 100%-CPU infinite loop in
`save_smoke 1` (fragile-seed worldgen retry × the YODA_DEBUG seed PIN) and FIXED it, plus shipped the
smoke-harness WATCHDOG the user asked for. Commit `b0ee41a`; tree GREEN.

**▶ ⭐ PICKUP #1 — `g_aDtaRecordTags` is now WIRED, not dead, and the count legitimately rose to 213.**
YodaDemo disasm settles the shape first: every original tag site is a **per-index COMPILE-TIME constant**
(`MOV ECX,0x456890` → a byte-wise inlined pair-strcmp against `DAT_00456890+8k`), **NOT a table loop** —
so per-index wiring IS the faithful reproduction (the "reproduce the loop if it was a loop" condition in
the pickup resolves to "it wasn't"). We wired **37 strcmp sites** in LoadWorld(0x421fd0)/Load(0x422670)/
LoadWorldStateFile/Serialize to `g_aDtaRecordTags[i]` with the faithful map (0 ENDF … 7 ZONE … 15 STUP;
`Load` alone touches indices 0–13 — all but the two save-only tags). **VC4.2 /O2 emits byte-identical
code for the array reference vs the string literal** (verified in isolation: same inlined 2-byte-pair
strcmp loop, only a masked reloc differs), and the forward `extern` is SAME-LINE on the .data-tables
comment line → **every site byte- and #line-neutral**. Remaining literals (ZAX4/IZAX/PNAM/ANAM alias
group, INDYSAV44/YODASAV44) are not table entries and correctly stayed literal.
- **Result: 211 → 213 exact** (Worldgen 34→36), all other TUs untouched. The wiring removes ~16 unique
  string-literal symbols → a REGALLOC-aftershock on the ambient dial: **+4 genuine** (IsItemPlaced,
  SetCurrentToIntroZone, GetZoneIndex, ParseZax2) **/−2 regalloc variants** (ParseZaux 0x423110,
  RemoveItem 0x429150 — semantically identical, just different register assignment; verified by byte
  diff). Not padding (the rule holds: these are real byte-equalities + honest RE, and we did NOT chase
  the number with filler). The caveat to remember: **string-literal count is now a KNOWN dial input in
  Worldgen.cpp** (removing the ~16 literals moved the exact-set), same family as the enum/typedef ones.

**▶ ⭐ BUG FOUND + FIXED — pinned-seed worldgen retry SPIN (Indy `save_smoke 1` at 100% CPU).**
Mechanism (stack-sampled: `Load() → IndyGenerate → IndyLoadPlacedZoneList → GetProfileString/fopen` all
burning CPU): `Load()`'s retry does `else  nSeed = Randomize();` while `Randomize()`'s YODA_DEBUG
`YODA_SEED` pin returned the **SAME seed on every call** → a seed that can't place an Indy mission
(`IndySelectPuzzle` returns <0 for some seed+[GameData] states, e.g. seed 1 with `save_smoke.INI`) was
re-tried **forever**. Retail never spins because production Randomize reseeds from cursor+clock; only a
pinned harness hits it. **FIX (same-line, YODA_DEBUG-only → zero anchor impact):** the pinned value now
**ADVANCES one step per call** (`+sRetryRound++`); the first call is still exactly `YODA_SEED`, so the
worldgen_smoke cross-host digest A/B is unchanged. `save_smoke 1` now PASSES (escapes to seed 3);
42/7 unchanged. ⚠ Related pre-existing caveat re-confirmed: worldgen_smoke/save_smoke **REWRITE their
own [GameData] INI each run** (v85 replay persistence), so repeated runs of a harness drift seed→zones
nondeterministically — **snapshot/restore the INI before any cross-run A/B** (the docs already say this;
the harnesses still do not self-restore).

**▶ HARNESS WATCHDOG (user ask — "set a timer event to catch this, otherwise it's a silent failure").**
New `microfx/harness/harness_watchdog.h` (SIGALRM time budget + best-effort backtrace, then a LOUD
non-zero `_Exit(1)`) is now armed by all 5 smoke harnesses so any future infinite loop fails loudly
instead of silently burning CPU. **Verified firing** on an artificial 2s spin (printed a real backtrace
and exited 1). `save_smoke`/`worldgen_smoke`/`zone_view`/`dlg_smoke` arm 60s; `game_walk` 180s (it
pumps a live loop). The real game (`yoda_main`) is NOT armed (runs forever by design).

**▶ NEXT — pick up here (real RE, not sweeping):**
1. **(OPTIONAL polish) reclaim the two v98 regalloc losses.** ParseZaux 0x423110 was v97's marquee
   "byte-exact under our own VC4.2" function and is now a — semantic-identical — regalloc variant after
   the literal→table symbol shift. A `tools/dialsweep.py` position sweep could re-land it (+N/−0 or
   +0/−0), but NEVER pad to a number: 213 with ParseZaux partial is the honest state.
2. **PICKUP #4 — reopen the residual hunt with the right partition** (`tools/idiomscan.py`): **41**
   functions differ by regalloc/scheduling ONLY (16 perfectly aligned) — that is the dial's population,
   ~10 already proven dial-reachable. **~134** are unfaithful SOURCE (ordinary decomp work; the
   small-`align` ones are the cheap wins). **A hard core is dial-invariant** — `DetonateAdjacentTiles`
   never moved once across ~70 positions, corroborating PLAN_COMPLETED #29 *for that function*.
3. **PICKUP #5 — de-hex leftovers** (all still valid): `0x68`→PLAN_WALL in WorldgenHelpers/DeskcppDoc
   (blocked — a shared `#define PLAN_WALL` would rewrite Worldgen.h's enum declaration into `104 = 104`;
   needs the enum relocated, a dial risk now measurable); ambiguous `TileFlags` bits 16-19 (need real RE);
   DeskcppDoc's `0xffffffff` sentinels + `0x11/0x10/0xe` zone-state codes; `WORLD_GRID_SIZE 10` (user's
   call); the `Canvas::Canvas` `sizeof` dial note at Canvas.cpp EOF.
4. **(NEW WATCH) the dial model grew one input:** string-literal symbol removal now demonstrably moves
   Worldgen's exact-set (the v98 +4/−2). Any future change that de-duplicates literals or swaps a literal
   for a data symbol is a dial event — re-run ALL FIVE oracles after such edits, not just progress.py.
**▶ HOW TO WORK THE DIAL SAFELY:** every sweep MUTATES a header — always restore (the tools do, via
atexit+finally, and leave a `.bak` if restore fails). ⚠ never run two sweeps concurrently or start one
while a `progress.py` is in flight: they fight over the header AND `build/*.obj` (this confounded the
first ArtooHint measurement and cost a full re-run). Verify a clean tree with
`git diff --stat src/` + `grep -rn "DIALSWEEP GENERATED" src/` before trusting any number.

---

**(v99 follow-up on the above):** pickup #1 of this block is CLOSED; the "optional polish"
item (reclaim ParseZaux 0x423110) is still open, and **RemoveItem 0x429150 came back for
free** in the v99 inline-MEMBER shuffle. Pickup #4 (the idiomscan residual hunt) was OPENED
at v99 and produced KEY codegen lesson #34 + the 213→217 re-baseline; see the current ⏭
block in CLAUDE.md.

---

### ⏮ v99 (2026-07-27) — the inline-MEMBER idiom (lesson #34); re-baseline 213 → 217

⚠ **Read with the v100 correction in hand:** the 217 headline this session produced was itself
computed by a `progress.py` that was UNDER-counting by 17 (the marker-pairing cascade — see
CLAUDE.md "THE INSTRUMENT ITSELF CAN LIE"). The v99 *matching* work below is real and unaffected —
the five stubs genuinely went byte-exact — but every absolute number in this block is 17 low.

**▶ THE WIN — lesson #34, the inline-MEMBER idiom (+5 intended).** The five permanently-grayed
`ON_UPDATE_COMMAND_UI` stubs (`OnUpdateFileSave` 0x403510, `OnUpdateLoadWorld` 0x403600,
`OnUpdateReplayStory` 0x403610 on CDeskcppDoc; `OnUpdateWorldSizeUi` 0x4165a0, `OnUpdateStatsUi`
0x416800 on CDeskcppView) had been parked since v34 as "EFFECTIVE … allocation artifact of a
`this`-ignoring member". **That diagnosis was wrong.** MSVC 4.2 lowers `p->Enable(0)` inside a plain
member as `mov ecx,[esp+4]; push 0; mov eax,[ecx]; call [eax]` (11 B), but when the call arrives by
**inlining a non-static MEMBER** it emits `mov eax,[esp+4]; push 0; mov ecx,eax; mov edx,[eax];
call [edx]` (13 B) — the inlinee's implicit `this` nominally owns ECX, so the arg must be staged in
EAX. Proven by scratch-TU probe under the anchor flags: in-class defs, out-of-class `__inline` defs,
an inline member *predicate* used as the ARGUMENT, and `((COther*)this)->Dis(p)` all reproduce it;
`static __inline` free functions, `static` MEMBERS, local-object member calls, local copies, casts
and references ALL fold. Fix = the old file-scope `static __inline DemoDisable` became a real member
on each class (decl in DeskcppStub.h, in-class def in DeskcppView.h); both .cpp edits are
line-neutral and the demo path's tokens are unchanged. Exact-set delta **+10 / −6**: the 5 targets +
LoadStoryHistoryOregon, StepDetonatorEffect, ClassifyTile, Randomize and **RemoveItem 0x429150**
(one of v98's two regalloc losses, back for free); lost to aftershock: LoadStoryHistoryNevada,
PlaceZoneObjectTiles, FindTile, RemoveZoneEntry, SetCurrentToIntroZone, GetZoneIndex.
ParseZaux 0x423110 still out.

**▶ TWO DIAL FACTS MEASURED** (both refine the v96 symbol-count model over lesson #23's "lines"
framing): (1) **COMMENT lines are INERT** — deleting 5 comment lines from a class body left three
Worldgen.cpp asmscores BIT-IDENTICAL, and adding a 9-line comment block mid-file to Worldgen.cpp
held the count exactly. Comments cost nothing; write them freely. (**Re-confirmed at v100**: the
three marker-hint comment edits left all 140 DeskcppView COMDATs bit-identical.) (2) **member
FUNCTIONS are NOT inert** — adding one to CDeskcppView rotated Worldgen.cpp's exact set. This
**partly retracts v97's "members are INERT"** (membertest.py tested member DATA only).

**▶ TWO RESIDUAL FAMILIES RE-SWEPT AND PARKED — with evidence, so nobody re-chases them:**
1. **ReadZax2 0x406410 / ReadZax3 0x406490** (`mov ax,[esp+0x12]; movsx ebp,ax` vs our folded
   `movsx ebp, word ptr [...]`; the ONLY diff in either, 46/46 insns). Swept 20+ source forms —
   short/int counters, for- vs do-while, casts, decl reordering, `register short`, unsigned+cast,
   ternary, post-decrement, count-reused-after-loop, short temp chains — ALL fold. The one form that
   DOES stage through AX (assigning into an ADDRESS-TAKEN short) emits an extra store the original
   lacks. Also: merging count into `n` is WRONG — the original keeps TWO slots (count@0x12, n@0x10).
   Full note at **Iact.cpp EOF**. ⇒ allocation state, not source.
2. **LoadWorldStateFile 0x423850 + Serialize 0x423b30** (`add [nDone],ecx` vs our `inc [nDone]`).
   Checked at the DISASSEMBLY level: the arms are byte-identical on both sides across 0x1b0..0x1fe
   except that one 3-byte slot, and **ours materializes `mov ecx,1` at the very same offset 0x1d3** —
   so the CSE'd 1 is live in our build too and the compiler just preferred `inc mem` over the
   equal-length `add mem,ecx`. The original's own else-arm uses `inc` where no 1-register is live.
   ⇒ pure peephole tie-break. Note expanded in place above the function.

---

### ⏮ v100 PICKUP (2026-09-02) — harness bugs #1 and #2; anchor re-baselined 217 → 234

Worked v99's advice (mine idiomscan class D for repeated signatures) and the top "cluster" it
surfaced — five `??_G` scalar-deleting-dtor thunks — turned out to be a **pairing artifact**.
Two real tool bugs, both silently manufacturing work that did not exist:

1. **Marker-pairing CASCADE.** `progress.py`/`idiomscan.py` filtered lib-owned COMDATs BEFORE
   `pair_by_name`, dropping ones that markers explicitly name by mangled hint. Those markers fell
   back POSITIONALLY, each stealing the COMDAT the next marker wanted → **28 mis-pairs cascading
   through DeskcppView.cpp**, scoring 17 already-exact functions against wrong addresses.
   `verify.py` had the correct `hinted` exception all along and had been reporting the true 80/124
   while progress.py said 63 — the cross-check that would have caught it years earlier. Ported the
   exception; fixed 3 stale marker hints (`??_GGameView`, `?DrawTextA@GameView` → `CDeskcppView`;
   explicit `??_GInvScrollBar@@` on 0x408690). `pair_by_name` now WARNS on positional fallback.
2. **Two definitions of "exact".** `idiomscan` used asmscore's DISASSEMBLY verdict; the anchor uses
   a reloc-masked BYTE compare. A function with an embedded switch JUMP TABLE decodes the table as
   instructions → phantom `byte_diff` on **8 provably byte-exact functions**. Class D: 129 → 103.

**Honesty note:** 234 − 217 = 17 functions that were ALREADY byte-exact and were being scored
against the WRONG addresses. **No new matching happened.** Only DeskcppView.cpp moved
(63+61/127 → 80+44/130). Verified 3 ways: source diff is comments only; all 140 COMDATs
bit-identical old-vs-new; `verify.py` independently reports the same 80.

**Sequel (v101):** the SAME bug was found a third time in `tools/dialsweep.py` — which
`membertest.py`/`headersweep.py`/`enumfieldtest.py` all measure through — and its correction
RETRACTED the v96 "215 / +4 gained / 0 lost / find the seven missing symbols" programme
entirely. See docs/compiler-hunt.md v101.


---

### ⏮ v101 (2026-09-02) — the dialsweep cascade bug; the v96 "missing symbols" quest RETRACTED

Condensed from the v101 ⏭ pickup (demoted at v102). Headline: `tools/dialsweep.py`'s
`exact_set()` still carried the pre-v100 COMDAT filter, and `membertest.py`/`headersweep.py`/
`enumfieldtest.py` all measure THROUGH it — so every sweep number the project ever published was
computed on the 28-mis-pair positional cascade. Fixed; dialsweep's baseline then agreed with the
anchor at 234 (pre-fix it said 211). Re-measured `Worldgen.h` n=0..8 for extern/struct/typedef:
the three kinds agree exactly with each other (**the dial IS pure symbol count** — that part of
v96 survives), but **no position beats baseline**; n=7 gives 227 with **+0 gained / −7 lost**, not
the claimed 215 / +4 / −0. The two functions v96 said DeskcppView.cpp was "6-8 symbols short" of
gaining (0x40ebe0, 0x40fca0) were **already byte-exact** at the corrected baseline — two of the 17
the v100 fix recovered — and `ParseZaux` 0x423110 differs in **78 of 116 bytes**, not a
one-declaration near-miss. ⇒ the "find the real seven missing symbols" quest is CLOSED.

Also shipped at v101: `tools/bytediff.py` (the anchor's own definition of exact for ONE function —
reloc-masked byte diff + hexdump of each differing run) and `tools/residuals.py` (census of the
non-exact functions ranked by byte diff, with a commutative/tie-break classifier). The census
quantified the tie-break seam: **only 5 residuals are pure commutative/selection tie-breaks**, all
2-byte and all source-inert, so that seam's total upside is +5 and it has no known lever. The other
~139 need real source/structure work — 74 differ in instruction COUNT or length. Diff-site
histogram: mov-operand 127, mov/lea 65, inc/add 32, jcc 31, cmp-swap 22.

Full detail: docs/compiler-hunt.md v101. The verbatim v101 pickup block follows.

### ⏭ NEXT SESSION PICKUP (2026-09-02 v101 — worked pickup #2 (cheap class-D targets) and it led
straight into a THIRD harness bug, this one in `tools/dialsweep.py`. Net: the v96 "compiler hunt
re-opened / find the seven missing symbols" quest is **RETRACTED** — it was measuring the v100
cascade. No src/ changes; anchor unmoved at **234**, all 5 oracles green. Two new tools shipped.
v100 log demoted to PLAN_COMPLETED.md ⏮.)

**▶ WHAT HAPPENED.** Took the pickup's cheap targets, but honoured its own instruction #1 —
"re-verify with a raw reloc-masked byte diff" — which had no tool. Built one (`tools/bytediff.py`),
then a full census (`tools/residuals.py`). Chasing the cheapest residuals surfaced that
`dialsweep.py` had never been given the v100 `hinted` fix.

**▶ ⭐ THE HEADLINE — v96's "215, +4 gained / 0 lost" DOES NOT EXIST** (full write-up:
docs/compiler-hunt.md v101; summarised at the top of this file). `dialsweep.exact_set()` carried the
pre-v100 COMDAT filter, and `membertest.py`/`headersweep.py`/`enumfieldtest.py` all measure through
it, so EVERY sweep number this project published was computed on the 28-mis-pair cascade. Fixed in
the one shared place → dialsweep's baseline now equals the anchor's 234 (pre-fix: 211). Re-measured
`Worldgen.h` n=0..8 for extern/struct/typedef: the three kinds still agree exactly with each other
(the dial IS pure symbol count — that part of v96 is real), but **no position beats baseline**; n=7
is 227 with **+0/−7**. `0x40ebe0`+`0x40fca0` (v96's claimed DeskcppView gains) are **already exact**
at the corrected baseline — two of the 17 v100 recovered. `ParseZaux` 0x423110 differs in **78 of
116 bytes**. ⇒ **Do not resume the missing-symbols hunt.**

**▶ ⭐ THE TIE-BREAK FAMILY IS SMALL — quantified, so stop guessing at it.** `tools/residuals.py`
classifies all 144 residuals by the anchor's byte oracle: **only 5 are pure commutative/selection
tie-breaks**, all 2-byte, and all are source-INERT (I re-probed `GetZoneIndex` 0x423dc0 and
`ParseTilesMaybe` 0x41a030 myself this session — flipping the source comparison is canonicalized
away; `GetFrameTile`/`LoadWorldStateFile`/`Serialize` were already documented). So that seam's total
upside is +5 and it has no known lever. The other **139 need real source/structure work**:
74 differ in instruction COUNT or length (genuine structural difference — the honest place to dig),
and the diff-site histogram is mov-operand=127, mov/lea=65, inc/add=32, jcc=31, cmp-swap=22.

**▶ NEXT — concrete, in priority order.**
1. **Audit the remaining harnesses against the baseline rule** (see the new ⇒ bullet under "THE
   INSTRUMENT ITSELF CAN LIE"): any tool that reports an exact-count must equal `progress.py` at
   zero perturbation. `asmscore.py`, `permute.py`, `frontier.py`, `exactset.py`, `survey.py` are
   UNAUDITED and several predate v100. Cheap, and this is now 3-for-3 on finding real bugs.
2. **Work the 74 instruction-count-differing residuals**, not the tie-breaks. Start where the count
   delta is smallest — `tools/residuals.py --csv out.csv`, then `tools/bytediff.py <tu> <addr>` for
   the hexdump and `asmscore.py --dump` for the instruction view. Cheapest unexamined:
   `FindTile` 0x403aa0 (4 B/53 B), `BlitMasked` 0x408240 (4 B), `ParseSnds` 0x4233f0 (5 B),
   `UpdateDialogButtons` 0x417dc0 (6 B).
3. **Re-run the v97 member conclusions** if anyone wants them — "members are INERT" was measured
   through the broken `exact_set()` and v99 already contradicted it. `membertest.py` is fixed now.
4. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19, DeskcppDoc's
   `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp `sizeof` dial note).
5. **Not touched this session:** `build-sdl`/`build-sdl-indy` (no game-code change, so no portable
   risk — but rebuild if you touch shared headers next). Phase-H goals 2-5 untouched.

**▶ HOW TO WORK THE DIAL SAFELY:** every sweep MUTATES a header — always restore (the tools do, via
atexit+finally, and leave a `.bak` if restore fails). ⚠ never run two sweeps concurrently or start one
while a `progress.py` is in flight: they fight over the header AND `build/*.obj`. Verify a clean tree
with `git diff --stat src/` + `grep -rn "DIALSWEEP GENERATED" src/` before trusting any number.


---

### ⭐ KEY codegen lesson #35 — the MFC MEMBER-CALL FORM (v102)

The argument-ordering guise of lesson #34. `pWnd->SendMessage(msg, w, l)` and
`::SendMessage(pWnd->m_hWnd, msg, w, l)` are semantically identical, and MFC's member is a thin
inline — but they are NOT codegen-identical when the object expression is non-trivial. The
member's implicit `this` (the `pParentView->ctrl` subobject address) must be evaluated BEFORE the
argument pushes; the global form lets cl fold the `m_hWnd` load in among them.

**Fingerprint:** same instruction count, same registers, one load sitting 1-2 bytes earlier or
later around a run of `push`es — `asmscore` reports `align 12, reg_pen 0, identity_miss 0`. This
looks exactly like an unreachable scheduling tie-break, which is why the three TextDialog scroll
helpers (ScrollTextLine 0x417c90, ScrollTextLine2 0x417d30, UpdateDialogButtons 0x417dc0) sat
parked as "EFFECTIVE — cl schedules the pParentView load two bytes earlier than ours" from G1
through v101. A prior session had tried caching `pParentView` in a local and made it far worse
(align 120) and stopped there.

**Rule:** when a residual is a load shifting across argument pushes, try the MFC member wrapper
BEFORE touching the schedule. Converting all 16 `::SendMessage(<CWnd>.m_hWnd, …)` sites to the
member form made all three exact: 234 → 237, +3 gained / 0 lost (the ✅ free-gain signature).

**Corollary (measured the same session, so don't churn it):** where the object expression is a
bare `m_hWnd` (this-relative), or a plain `CScrollBar*` with a constant `SB_CTL`, the two forms
fold to identical bytes. The 36 `pScrollBar->m_hWnd` sites in the three `OnHScroll` bodies were
converted, measured byte-for-byte inert, and reverted.

### ⭐ Method lesson — PROBE SPELLINGS, DON'T REASON ABOUT SCHEDULES (v102)

Both v102 wins came from enumerating ~10 ways the 1997 author could have SPELLED one statement and
measuring each against the anchor's byte oracle (`tools/vartest.py`), rather than reasoning about
what the scheduler "should" do. Reasoning had already failed on both of these for multiple
sessions; the batch A/B took minutes.

Most spellings fold to identical codegen. That **"source-inert" verdict is a real result** — it
retires a residual honestly instead of leaving it open with a speculative note. Proven inert this
way: `FindTile` 0x403aa0 (cast placement, `void**` walk, cmp operand order, decl order,
for-vs-do-while — all 8 fold to the same 4-byte ECX/EDX tie-break), `BlitMasked` 0x408240 (every
associativity and operand ordering of `pData + destX + canvasW * destY`; note its sibling
`BlitFast` 0x408110 is byte-exact with the identical expression), and the `savedId != child`
compare in `LoadZoneRecursive`.

⚠ Keep variants LINE-NEUTRAL — a line-count change mid-TU rotates the dial on its own (lesson #23)
and confounds the measurement. And `vartest.py` enforces the v100/v101 baseline rule on itself:
`--expect N` hard-fails when its own zero-perturbation measurement disagrees with the anchor.


---

### ⏮ v102 (2026-09-02) — condensed (demoted from CLAUDE.md at v103)

Closed the v101 harness audit by finding a 4th and 5th tool bug (`exactset.py` carried the same
pre-v100 COMDAT filter, reporting 218 not 234; `permute.py`'s SUCCESS test used asmscore's
disassembly-derived byte_diff so it could never declare a win on a jump-table function — both
fixed; `survey.py`/`frontier.py`/`asmscore.py` audited CLEAN). Incidental: `/D _MBCS` is not
cosmetic — it changes code in 5 of WorldgenHelpers.cpp's 27 COMDATs. Then the first REAL matching
movement since v99, **234 -> 237**: the three TextDialog scroll helpers (0x417c90/0x417d30/
0x417dc0), parked since G1 as a shared "pParentView-load schedule shift", were a CALL-FORM
difference — the original called `CWnd::SendMessage`, not `::SendMessage`. All 16
`::SendMessage(<CWnd>.m_hWnd, ...)` sites converted to the member form (line-neutral); +3, zero
regressions. Mechanism = standing lesson #35 in CLAUDE.md. Also `LoadZoneRecursive` 0x403450
7 B -> 1 B (pre-caching `short child = o->arg;` hoisted the load above the type test; assigning
inside the `&&` restores cl's order — the parked note blamed "residual register roles", wrong).
New instrument `tools/vartest.py` (batch-A/B source spellings vs the anchor byte oracle,
`--expect N` enforces the baseline rule on itself) landed both wins.

---

### ⏮ v103 (2026-09-02) — 237 → 240, +3 gained / 0 lost (demoted from CLAUDE.md at v104)

1. **`ParseSnds` 0x4233f0 (5 B → EXACT) — a buffer's DECLARED SIZE is a dial.** `char fname[9]`
   (DOS 8.3 basename + NUL), not `[12]`. v36 had exhaustively permuted all 24 decl ORDERS and
   parked it as irreducible; it never varied sizes. Standing lesson #36.
2. **`OnEraseBkgnd` 0x413b20 (6 B → EXACT) — `pDC->PatBlt(...)`, lesson #35.** The residual was
   the TAIL FUNCLET ORDER, an axis the old note declared "not source-steerable". It is.
3. **`CyclePalette` 0x415af0 (6 B → EXACT) — `pWorld->pPalette->AnimatePalette(...)`.** The
   conversion is NON-MONOTONIC: both calls = 6 B, first only = 0 B, both + DC members = 0 B.
4. **`DrawDirectionArrows` 0x4270f0 28 B → 21 B** via `pDC->FillRect(&rc, &br)`. Its last block's
   x/y decl order re-probed and CONFIRMED correct (swapping = 27 B); pOldPal-first head is inert.
5. **microfx gained `CDC::PatBlt`** (afxwin.h) for the portable build.
6. Harness lie #4 found: `tools/residuals.py --csv` writes `va` in DECIMAL; an ad-hoc scan parsed
   it with `int(va, 16)`, reported zero member-call sites, and nearly closed that seam. Corrected
   scan found 28. ⇒ print a positive control before believing an empty result.

### ⏮ v104 (2026-09-02) — 240 → 244, +4 gained / 0 lost

New lesson **#37: a local's DECLARATION SCOPE is a register-allocation dial** — see the standing
bullet in CLAUDE.md. Landed `IactScript::~IactScript` (7→0), `Zone::~Zone` (12→0),
`PlaceZoneObjectTiles` (22→0), `LoadZoneRecursive` (fell out alongside); improved `FindObjectAt`
11→2. New tool `tools/hoisttest.py`. Method note: the productive move was a **register-permutation
census** over every residual (does ONE consistent reg→reg renaming explain the whole diff?) — 10 of
138 residuals are in that class, and it is the class the lever addresses. Three source park notes
claiming "no source lever reaches this" were retracted by measurement.

---

## ⏮ v104 PICKUP (demoted at v105, 2026-09-03) — 240 → 244 via lesson #37

**Was:** NEXT SESSION PICKUP (2026-09-02 v104 — **240 → 244**, +4 gained / 0 lost, all real
matching, plus `FindObjectAt` 11 B → 2 B. All 5 oracles green (244 exact / link 0-0-exit0 /
bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN); build-sdl + build-sdl-indy relinked.
v103 log demoted to PLAN_COMPLETED.md ⏮.)

**▶ WHAT LANDED — one new lever, lesson #37: A LOCAL'S DECLARATION SCOPE IS A REG-ALLOC DIAL.**
Full write-up is the standing bullet in this file; the short version is that a residual explained
ENTIRELY by a register RENAMING (esi↔edi, ebx↔ebp) is usually a decl-scope bug, not a compiler
ceiling. Landed exact: `IactScript::~IactScript` 0x4187e0 (7→0), `Zone::~Zone` 0x4054d0 (12→0),
`PlaceZoneObjectTiles` 0x403140 (22→0, needed `z`/`o`/`t` hoisted IN THAT ORDER **and** `t2`
merged into one `t`), `LoadZoneRecursive` 0x403450 (fell out alongside). Improved:
`FindObjectAt` 0x405330 11→2 (whole 3-register rotation gone; last 2 B is `test edi,edi` vs
`cmp edi,eax`, proven inert over 7 more spellings). New tool **`tools/hoisttest.py`**.

**▶ RETRACTIONS — three park notes in the source said "no source lever reaches this". All wrong:**
`FindObjectAt` ("no stmt/decl/cmp lever, one leading decl"), `~IactScript` ("phase drift,
dial/endgame"), `PlaceZoneObjectTiles` ("proven correct; settles at G1"). ⇒ A park note is a
record of which axis was probed, NOT proof of irreducibility (same shape as v103's lesson #36).

**▶ NEXT — concrete, in priority order.**
1. **⭐ Finish the decl-scope seam — it is NOT exhausted.** 111 of 138 residuals have inner-block
   declarations. Unprobed PERM-class targets: `ParseZax3` 0x423190 (11 B, bp→di→si 3-cycle),
   `DetonateAdjacentTiles` 0x428680 (60 B, di↔si), `~CDeskcppDoc` 0x41b2f0 (6 B, di↔si — ⚠ plain
   hoisting makes it WORSE, so its `p` is genuinely loop-scoped; the lever must be something else).
   Then the wider block-decl list ranked by ndiff (`ZoneHasIzxItemMaybe` 0x41bfa0 17 B/6 decls,
   `WorldgenAssignTransitItemMaybe` 0x41d480 13 B/9, `ReadSavedState` 0x405bd0 21 B/10).
   Recipe: `tools/hoisttest.py <tu.cpp> <addr> --expect N` (N from `tools/bytediff.py`).
2. **`Zone::WriteSavedState` 0x405f30 sits at 13 B with a 7 B spelling available** (any PAIR of
   {o,e,p,k} hoisted). NOT applied — 7 B is not a match and I would not churn a shared TU on
   suggestive-only evidence. Finish it (the waypoint `p`/`k` loop is the untouched part) or drop it.
   ⚠ its candidate list includes decls inside the `#ifdef GAME_INDY` branch, which are INERT for
   the anchor but real for the Indy build — check which sites you are actually moving.
3. **Proven INERT at v104 — do not re-tread:** `DifficultyDlg::OnHScroll` 0x417fa0 (13 spellings:
   member-call form, assign-in-condition, sub/fold, cmp order — its 6 B is a cmp-swap + a
   mov/cmp order in the SB_PAGEUP arm; PAGEDOWN matches only because `add` folds into `lea`),
   `SetCurrentToIntroZone` 0x423d20 (9 loop spellings), `ReadIzon` 0x405ae0 (`char tag[5..16]`
   sizes + all decl orders — sizes 5/6/8 identical, 9+ worse), `WorldgenCollectZoneRefs` 0x41f8e0,
   `ZoneRequiresItemMaybe` 0x41c0b0, `SaveZoneRecursive` 0x4033b0.
4. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19, DeskcppDoc's
   `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp `sizeof` dial note).
5. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY:** every sweep MUTATES a source file — always `git diff --stat src/`
AFTER each one. ⚠ **v104 caught two real traps here:** (a) a sweep piped through `grep` that the
harness BACKGROUNDED at its 600 s limit left `src/Iact.cpp` in a variant state despite vartest's
atexit restore — run sweeps with `run_in_background` writing to a LOG FILE, never through a pipe;
(b) `git checkout <file>` to undo a probe silently reverted an ALREADY-LANDED fix in the same file,
and the next three measurements were quietly wrong. Restore a single function from
`git show HEAD:<file>`, not the whole file. Never run two sweeps concurrently or one while
`progress.py` is in flight (they fight over the source AND `build/*.obj`).


## ⭐ v105 (2026-09-03) — 244 → 247; lesson #38 and the JOINT-PHASE finding

**Landed (exactset diff, +3 / −0):** `ZoneHasIzxItemMaybe` 0x41bfa0 17 B → 0 (the targeted
match); `CheckZoneItemsAvailable` 0x41f830 and `DetonateAdjacentTiles` 0x428680 fell out on the
resulting TU shift.

**⭐ LESSON #38 — the decl dial is a SET *and* an ORDER, and it can be ASYMMETRIC.**
Lesson #37 / `hoisttest.py` ask a yes/no question per declaration. That is too coarse: the real
input is *which* locals sit at function scope and *in what order*, and the answer can differ
between two textually identical branches of the same function. Winning configuration for
0x41bfa0: `nCount` at function scope (both branches assign the one variable), the **else**
branch's `i` at function scope, and the `sel!=0` branch keeping its **own** `i` that shadows it,
declared `int nCount; int i;` in that order. Measured descent — 17 → 10 (`nCount` before `i`;
`i` first is 14) → 8 (3-name subset) → 7 (only `nCount`, from both branches) → **0**. Plain
all-hoist never gets below 8, and `--max-hoist 1` reads completely flat (17/17/22/16/17/19).
⚠ So a flat single-hoist probe is NOT a dead end; escalate to a set×order sweep before parking.
The exact family also includes `pObj` either way and the B branch's `i` renamed instead of
shadowing; `nObjs` hoisted costs 2 B.

**⭐ THE JOINT-PHASE FINDING — a TU's register allocation is NOT per-function.**
The clean control: `ParseZaux` 0x423110, `ParseZax3` 0x423190 and `ParseZax2` 0x423210 are
textually identical (only the `ReadZaux`/`ReadZax2`/`ReadZax3` callee differs — and those three
are declared identically in `GameObjectClasses.h`). Every emitted instruction matches; only
register NAMES differ. Yet each image allocates differently and a *different* sibling deviates in
each: the original's odd one out is `ParseZax3`, ours is `ParseZaux`, and our `ParseZax3`'s
codegen IS the original `ParseZaux`'s allocation ({offset,i,pFile} = {esi,ebp,edi}).
Corroboration: (a) `DetonateAdjacentTiles` gained on a 5-line shift ~5000 lines earlier, after
measuring inert to all 12 of its own decl-scope subsets; (b) `ZoneFindInIzxList`'s exact spelling
costs exactly the same 4 downstream functions whether the edit is line-neutral or +11 lines — so
it is the TOKEN change re-rolling the phase, not lesson #23.
⇒ (1) "intrinsic / not source-steerable" park notes record an axis probed, never irreducibility;
v39's on 0x428680 is RETRACTED. (2) Before grinding a PERM residual, look for a textually
identical SIBLING in the TU — if one is exact and the other is not, the source is not the
variable. (3) Greedy per-function search hits trades; a saturated TU needs a JOINT search.
(4) Always measure with `tools/exactset.py` + `comm`, not progress.py's total.

**Parked with full data — `ZoneFindInIzxList` 0x41c490 (+2 / −4 = 245, deliberately not landed).**
Exact spelling known: hoist all inner locals to function scope, order `nCount, i, nObjs, j, pObj`
BEFORE `v` (in-block 21 B; `i` before `nCount` 14 B; decls after `v` 19 B; exact family
{+nObjs, +nObjs+j, +all}, all with an identical project-wide footprint). Under that phase:
`RemoveItem` 0x429150 → **EXACT** by hoisting `InvItem *pEntry`; `CheckZoneItemsAvailable`
0x41f830 → 2 B (cmp operand order, guard form, pObj hoist and decl swap ALL inert);
`ParseTnam` 0x423380 → 5 B (all 9 decl-scope subsets/orders inert); `DetonateAdjacentTiles`
→ 77 B. So the phase reaches 246 with two known fixes and needs 2 of the remaining 3 to beat 247.

**Proven inert at v105 (do not re-tread):** `ParseZax3` 0x423190 over 18 spellings (decl scope,
buffer sizes 9/10/12/16, `i` as short/uint, `nCount` int/uncast, while-form, `!pZone` / `NULL ==`,
cmp order, pre/post-increment) — every faithful spelling gives exactly 11, only wrong
length-changing ones move it. `ParseZaux` 0x423110's "78 of 116 bytes" (v101) is misleading: it is
a ONE-byte length change (`mov eax,[ebp+0]` vs `mov eax,[edi]`, ebp needing a disp8) cascading
through the rest — the same PERM class, not 78 independent differences.

---

## ⏮ v105 PICKUP (demoted at v106, 2026-09-03)

Everything in it was re-measured this session; the ZoneFind trade table REPRODUCED EXACTLY
(zonefind spelling + `RemoveItem` `pEntry` hoist ⇒ 246: gained 0x41c490 + 0x423d20
`SetCurrentToIntroZone`, lost 0x41f830 / 0x423380 / 0x428680). Still open as a lead, but see the
v106 downstream-only finding below for the constraint that makes it searchable. The v105
"proven inert" list stands unchanged.

## ⭐ v106 (2026-09-03) — 247 → 249; lesson #39, and the phase is DOWNSTREAM-ONLY

**▶ ⭐ LESSON #39 — STATEMENT ORDER RELATIVE TO A MATERIALIZED CONSTANT IS A DIAL.**
`LoadWorldStateFile` 0x423850 and `Serialize` 0x423b30 (copy-paste siblings) each sat at DIFF(2),
parked since G1 as an "inc-vs-add tie-break". The entire residual was ours `inc [nDone]` vs the
original's `mov ecx,1; add [nDone],ecx`. The cause is NOT the increment's spelling — `nDone++`,
`nDone += 1`, `nDone = nDone + 1` and `++nDone` ALL fold to the identical `inc` — it is the
increment's POSITION among the neighbouring `= 1` stores. The block does
`bHidePlayer = 1; bWorldReadyMaybe = 1; nMapChangeReason = 1;`, so cl materialises 1 into ecx for
those; if the increment is sequenced AFTER the first such store, cl folds it into that live
register as `add mem,ecx`. Measured: position 0 = 2 B, positions 1–6 (anywhere after
`bHidePlayer = 1`) ALL EXACT, `nDone = 1` = 9 B. +2 / −0.
⇒ **Generalisation to try elsewhere:** when a residual is an immediate-vs-register form
(`inc mem` vs `add mem,reg`; `mov mem,imm` vs `mov mem,reg`), the lever is the STATEMENT ORDER
around the other users of that constant, not the arithmetic spelling. The oracle pins a FAMILY
(positions 1–6), so pick the idiomatic member and say so — same discipline as lesson #36.
⚠ Mined out for now: all 29 remaining `inc/add` diff sites live in functions with 78 B+ residuals.

**▶ ⭐ THE TU JOINT PHASE IS DOWNSTREAM-ONLY (sharpens the v105 finding).**
Every function that moved under the ZoneFind edit sits AFTER it in file order, and an edit
confined to the TU's LAST function (`RemoveItem` 0x429150) moves NOTHING project-wide
(247 → 247, +0/−0). ⇒ A joint search IS tractable: an upstream edit can never break a fix that
sits earlier in the file, and a compensating edit for the ZoneFind trade must live between
line 279 and line 2349 of Worldgen.cpp to reach all three losses.
⇒ Corollary that bit immediately: after landing ANY change, every stale residual number for a
LATER function in the SAME TU is invalid. `vartest --expect 13` on `WriteSavedState` HARD-FAILED
right after the ReadSavedState fix — its residual had moved to 20. The v100/v101 baseline guard
caught it automatically; that is exactly what it is for. **Per-function byte counts in a pickup
are PHASE-RELATIVE, not absolute.**

**▶ `Zone::ReadSavedState` 0x405bd0, 21 B → 12 B — lesson #37's refinement (2) in the wild.**
The original used ONE `ZoneObj *o` where we had two (`new ZoneObj` in the grow loop and
`(ZoneObj *)objects[i]` in the read loop). Found by a 95-spelling `hoisttest.py` pair sweep: 21 →
19 for ANY single hoist → 12 for that one merge, the only configuration below 19. It killed two
of three diff sites — the tile-grid loop's esi↔edi swap AND the grow loop's `cmp esi,ebx / jl` vs
`cmp ebx,esi / jg` operand mirror. Two independent sites falling to one declaration change is
what makes it evidence rather than a register coincidence. Project-wide 247, +0/−0 (free).
⚠ **Honest counter-observation:** `WriteSavedState`'s best REACHABLE residual worsened 7 → 13
under the resulting phase. Neither function is exact either way so the count is unaffected, but
if a later session finds WriteSavedState's true spelling, re-test this pair jointly.
Residual 12 B = the final IACT done-flags loop only (esi↔ebx between the strength-reduced byte
offset and `i`, plus a `mov this` / `push 4` order swap); 10 further loop spellings and all extra
hoists (e/p/k) inert or worse.

**▶ ⭐ NEW SEAM FOR NEXT SESSION — the DUPLICATED-LOCAL scan.** "One variable where we had two"
is now a repeatable probe, and a scan says **48 non-exact functions carry a repeated same-name
local declaration** (script pattern: split each TU on `// FUNCTION: YODA` markers, regex the
`Type name = ...;` declarations, report names declared >1× in one function, filter to non-exact).
Richest: `Tick` 0x40b270 (pX ×9, pY ×9, t ×8, nStep ×6), `OnBumpTile` 0x413df0 (5 names ×4),
`WorldgenSelectPuzzle`, `Generate` 0x41f960 (11 names), `CDeskcppDoc::~CDeskcppDoc` (p ×7),
`IactRunCommands` 0x4070e0 (11 names). ⚠ Not universal: for `WorldgenAssignTransitItemMaybe`
0x41d480 merging `i` or `v` across BOTH sel-branches costs (21 B); one-branch merges are inert.

**▶ AXES CLOSED AT v106 (measured; do not re-tread).**
| fn | resid | swept | verdict |
|---|---|---|---|
| `GetZoneIndex` 0x423dc0 | 2 B | 10 spellings — both cmp mirrors, `!=`, for/do-while, cached size | INERT. ⚠ residuals.py labels it `cmp-swap,jcc-mirror`, which LOOKS source-steerable; it is not. |
| `FindObjectAt` 0x405330 | 2 B | 9 spellings incl. the early-return restructure | INERT — and early-return emits a **72-byte** function vs the original's 79, definitively ruling that structure out. |
| `RemoveZoneEntry` 0x41d740 | 13 B | 10 decl configs | only `nCount`/`i` ORDER moves it (13 vs 18); current spelling already best; `pEntry` hoist inert. |
| `ReadIzon` 0x405ae0 | 7 B | 16 spellings — decl order ×6, `tag[4/6/8/9/12/16]`, for-init scope, cmp mirror | ALL faithful spellings = 7 (`tag[5]/[6]/[8]` one padded-slot family; [9]/[12]=20, [16]=16, [4]=21). Closes the lesson #36 + #38 axes v36 never tried. |
| `WorldgenAssignTransitItemMaybe` 0x41d480 | 9 B | 70 spellings / 4 sweeps | floor 7. Config A (`first,bIn,nUsed,k`) has the REGISTERS right and the `this`-reload schedule wrong; config B (`first,nUsed,bIn,k`) is the exact reverse. The two halves never combine. `SetAtGrow(GetSize())` = 8 is a separate axis. |
| `WriteSavedState` 0x405f30 | 13 B (old phase) | 74 hoist subsets | floor 7 for ANY PAIR, 13 for singles/triples/quads — a family, so no unique historical answer. Now re-based at 20 B (see above). |
| `LoadStoryHistoryNevada` 0x401ac0 | 2 B | — | left parked: its own note already PROVES phase (the three sibling loaders oscillate jg/jl/jg on identical source). |

**▶ HARNESS NOTES (v106).**
- `hoisttest.py` DEDUPS hoisted decls by NAME, so a label like `o+o` means the SET {o} merged from
  two same-named declarations — that is the only way the "one variable, not two" configuration is
  reachable, and it is why `--max-hoist 1` misses it. ⚠ Labels are AMBIGUOUS when candidates share
  a name: index the log POSITIONALLY against `itertools.combinations` order to identify a winner.
- A dead SHADOWED declaration is INERT (measured): "hoist + convert inner to assignment" and
  "add a fn-scope decl but KEEP the inner one shadowing it" are codegen-identical. So hoisttest
  listing candidates from both sides of an `#ifdef` does NOT manufacture padding wins.
- `residuals.py --csv` columns are `cpp,va,name,L,ndiff,span,lenmis,kinds,tie` (NOT tu/ndif), and
  `va` is DECIMAL (the v103 trap). The terminal `kinds` column is TRUNCATED at 28 chars — use the
  CSV to find a class such as `inc/add`.
- `asmscore.py` best-fit mis-paired `CDeskcppDoc::Serialize` with the lib `CObject::Serialize`
  stub and reported a 1-instruction function. Cross-check with `bytediff.py` before believing it.

## ⭐ v107 (2026-09-03) — 249 → 250; lesson #40 (the LOOP FORM), and a fidelity trade taken

**▶ ⭐ LESSON #40 — THE LOOP FORM IS A DIAL, AND IT IS NOT A REGISTER TIE-BREAK.**
`SaveZoneRecursive` 0x4033b0 had sat at DIFF(6) since G1 annotated as a "walker/counter
ebx<->ebp 2-cycle" — the canonical lesson-#37 fingerprint. It is not one: **all 9 hoist /
decl-order / decl-set variants measure 6 B, dead flat.** The lever is the shape of the loop.
The original writes the house countdown

    int n = z->objects.GetSize();
    if (n > 0) { int i = 0; do { ...; i++; n--; } while (n != 0); }

not `for (int i = 0; i < n; i++)`. That is the recipe `PlacePuzzle` 0x421620's note already
documented for its three delete loops ("yields the DEC/JNE countdown the plain guard+do-while
i<n form never produces") — v107's contribution is recognising it as a GENERAL lever and
probing for it. All five spellings of the idiom measure 0 B (`i++; n--;` vs `} while (--n)`,
`i` before or after `n`, `GetAt` vs `operator[]`), so the oracle pins a FAMILY — pick the house
member (lesson #36 discipline). ⚠ The `n > 0` GUARD is load-bearing: an unguarded do-while
emits 145 bytes against the original's 149.
⇒ **When a residual looks like a walker/counter register 2-cycle and every declaration variant
is flat, stop treating it as a register problem and vary the LOOP FORM.** A flat decl sweep is
now a POSITIVE signal for this lever, not a dead end.
⚠ **It is NOT universal, and the counter-evidence is cheap to get** — the emitted LENGTH rules
it out immediately where it is wrong:
| fn | resid | countdown verdict |
|---|---|---|
| `SaveZoneRecursive` 0x4033b0 | 6 B | **0 B — landed** |
| `WorldgenCollectZoneRefs` 0x41f8e0 | 9 B | 7 B, identical length — probably right, PARKED (see below) |
| `LoadZoneRecursive` 0x403450 | 1 B | **165 B vs 177 — structurally WRONG** |
| `FindObjectAt` 0x405330 | 2 B | **77 B vs 79 — structurally WRONG**; caching GetSize() also costs 13 B |
| `RemoveEmptyZonesFromPlacedList` 0x403070 loop 1 | 26 B | byte-IDENTICAL to the `for` (inert) |
| `RemoveEmptyZonesFromPlacedList` 0x403070 loop 2 | 26 B | 204 B vs 206 — wrong |
So `SaveZoneRecursive` and `LoadZoneRecursive`, textual mirrors, genuinely differ in loop form.

**▶ `SetCurrentToIntroZone` 0x423d20, 5 B → EXACT (+1/−0) — lesson #38 by the book.**
Both `i` and `pZone` belong at FUNCTION scope, **in that order**, with `nCount` initialised in
its own declaration: `int nCount = zones.GetSize(), i; Zone *pZone;`. The descent is the whole
lesson in one function — hoist `i` alone = 5 (nothing), hoist `pZone` alone = **9 (worse)**,
hoist both with `pZone` first = 9, `Zone *pZone;` ahead of `nCount` = 11, all-top with `nCount`
assigned separately = 5, **hoist both with `i` first = 0**. A single-hoist probe would have
reported "inert or worse" and parked it. Inert here: `zones[i]` vs `GetAt(i)`, the loop-condition
cmp mirror, an early-`continue` body. The do-while countdown emits 53 B vs 60 — ruled out.

**▶ ⚠ A NET-ZERO FIDELITY TRADE, TAKEN DELIBERATELY (Save gained, Load lost).**
Landing `SaveZoneRecursive`'s countdown costs `LoadZoneRecursive` 0x403450, immediately below it,
its last byte — project total stays **250**, +1/−1. Taken because it strictly improves the source
as a reference: before, Save carried a form now PROVEN wrong; after, both carry their
most-likely-correct form. Load's own source is untouched (the v102 assign-in-condition crack
stands) and the byte it regains is the `savedId`/`child` cmp operand order its note already
recorded as an inert commutative tie-break — re-confirmed inert here.
⇒ **This is NOT the v96 "a trade is the fingerprint of padding" case.** That rule governs adding
filler DECLARATIONS to move the dial. Here the gained form has independent structural proof (the
unguarded variant emits the wrong LENGTH) and matches a house idiom already documented elsewhere
in the source. The v45 `~CDeskcppDoc` message-map trade is the precedent.
⚠ **The trade is TOKEN-driven, not lesson #23**: a LINE-NEUTRAL spelling of the same change
trades identically. That is a clean second confirmation of v105's joint-phase finding.

**▶ THE JOINT SEARCH AROUND IT — bounded and already partly executed.** Per v106 downstream-only,
the compensating fix must sit UPSTREAM of line 613 in WorldgenHelpers.cpp. Probed:
- `LoadZoneRecursive` itself under the new phase: **16 spellings** (decl set/order for `i`/`o`/
  `child`, `savedId`/`savedFull` order, cmp mirror, `==` form, countdown) → floor 1 B,
  unreachable from its own source. Textbook v105 "the source is not the variable".
- `RemoveEmptyZonesFromPlacedList` 0x403070, the obvious upstream candidate: **10 variants**, inert
  at 26 B. ⇒ look further up (`LoadStoryHistoryNevada` 0x401ac0 is a known phase oscillator; the
  three `SaveStoryHistory*` are 611 B each and untouched).

**▶ THE DUPLICATED-LOCAL SEAM (v106's lead) — RE-RUN AND LARGELY DISAPPOINTING.** The scan
reproduces at 48 functions, but the merges do not pay where v106 guessed they would:
| fn | resid | verdict |
|---|---|---|
| `DrawDirectionArrows` 0x4270f0 | 21 B | **13 spellings, floor 21.** Merging x/y across all four arrow blocks costs **+7**; block-2-only hoists inert; block-2 decl-order swap = 23. CLOSED on this axis. |
| `PlacePuzzle` 0x421620 | 32 B | floor **29** (hoist `i`, or hoist `pPt`). Renaming the three delete counters to one `n` is inert; hoisting `n` alone or `pPt`+`i`+`n` = 35. Not landed (partial). |
| `GetFrameTile` 0x404850 | 2 B | **one-variable hypothesis REFUTED STRUCTURALLY** — merging `bank` into `idx` collapses the `dy == -1` test and emits **177 B vs the original's 183**. Decl order and every addend order inert. |
⇒ The v106 claim that this is "the richest untouched seam" is **downgraded**: 3 of 3 worked
candidates closed without a gain, and the one v106 win (`ReadSavedState`) may be the exception.
The productive lever this session was the LOOP FORM, found on a function the dup-scan also
flagged — so the scan is still useful as a TARGET LIST, just not for the merge probe specifically.

**▶ A USEFUL COMPILER CONSTRAINT (new).** VC 4.2 uses OLD for-scope: `for (int i = ...)` leaks
`i` into the enclosing scope, so **two `for (int i ...)` loops in one function is a redefinition
ERROR**. That is why `RemoveEmptyZonesFromPlacedList` uses `i` and `j`, and it bounds the space of
spellings the 1997 author could have written in every multi-loop function — a same-name merge
across two `for`-init declarations is not just unlikely, it is uncompilable.

**▶ HARNESS NOTES (v107).**
- `tools/residuals.py --csv` takes a PATH argument (`--csv out.csv`); bare `--csv` raises IndexError.
- The `mov-operand` class is 119 of 129 residuals — far too broad to be a targeting signal on its
  own. `cmp-swap` and `jcc-mirror` continue to LOOK source-steerable and measure inert (GetZoneIndex,
  LoadZoneRecursive, LoadStoryHistoryNevada all sit there).
- Positive-control discipline held up again: the dup-local scan prints its parsed-row count before
  reporting, per the v103 rule.


### ⏮ v107 PICKUP (demoted at v108, 2026-09-03) — anchor 249 → 250

#### (was: NEXT SESSION PICKUP) (2026-09-03 v107 — **249 → 250**, +1 gained / 0 lost, plus a
deliberate net-zero FIDELITY TRADE (SaveZoneRecursive gained, LoadZoneRecursive lost).
All 5 oracles green (250 exact / link 0-0-exit0 / bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN /
msg 11 CLEAN); build-sdl + build-sdl-indy relinked. v106 log demoted to PLAN_COMPLETED.md;
full v107 detail lives in the new "⭐ v107" section there.)

**▶ WHAT LANDED.**
1. **`SetCurrentToIntroZone` 0x423d20 5 B → EXACT (+1, commit 3c52c5b)** via **lesson #38**:
   `int nCount = zones.GetSize(), i; Zone *pZone;` — both locals at function scope, `i`
   BEFORE `pZone`. Textbook descent: `i` alone = 5, `pZone` alone = **9 (worse)**, reversed
   order = 9, all-top with nCount assigned separately = 5, **both with `i` first = 0**.
2. **⭐ `SaveZoneRecursive` 0x4033b0 6 B → EXACT via the NEW LOOP-FORM lever, lesson #40
   (commit ca0edae)** — see the standing bullet above. Its "walker/counter 2-cycle" was
   never a register problem: 9 decl variants were dead flat, and the answer was the house
   `i++/n--` countdown under an `n > 0` guard.
   ⚠ **Net zero on the count**: it costs `LoadZoneRecursive` 0x403450 (immediately
   downstream) its last byte, so the total stays 250. Taken for source fidelity, with the
   reasoning recorded in both source notes and the standing bullet. **Revert with
   `git revert ca0edae` if the trade is unwanted** — nothing else depends on it.
3. **Closed axes recorded in-source (commit 8cb4cc5, comment-only, verified +0/−0).**

**▶ NEXT — concrete, in priority order.**
1. **⭐ MINE LESSON #40 SYSTEMATICALLY (best lead).** Only 6 loops were probed this session
   and one landed. The targeting rule: pull residuals whose note says "register 2-cycle" /
   "walker/counter" AND whose decl sweep is flat, then vary the loop form. Refutation is
   cheap — a wrong loop form emits the WRONG LENGTH, visible in one `vartest` line.
   `tools/residuals.py --csv <path>` + the for-init scan (recipe in PLAN_COMPLETED ⏮ v107)
   gives 18 candidates; unprobed ones with small residuals: `BlitMasked` 0x408240 (4 B,
   2 loops), `ParseTilesMaybe` 0x41a030 (3 B), `ReadSavedState` 0x405bd0 (12 B, 4 inner
   ptrs), `WriteSavedState` 0x405f30 (20 B), `StartGame` 0x4037a0 (79 B, 4 loops).
2. **The WorldgenHelpers JOINT SEARCH** (recover LoadZoneRecursive's 1 B without giving up
   Save). Bounded by v106 downstream-only: the compensating edit must sit UPSTREAM of line
   613 in that TU. Already probed and inert: `RemoveEmptyZonesFromPlacedList` 0x403070
   (10 variants) and LoadZoneRecursive's own 16 spellings. Remaining upstream candidates:
   `LoadStoryHistoryNevada` 0x401ac0 (2 B, a known phase oscillator) and the three
   `SaveStoryHistory*` (611 B each, untouched — big but they are copy-paste siblings, so
   one crack pays 3×).
3. **`WorldgenCollectZoneRefs` 0x41f8e0 is a PARKED PARTIAL** — countdown takes it 9 B → 7 B
   at identical length (all 4 spellings agree, so the form is probably right). Land it only
   inside a joint pass that also closes the remaining 7 B; alone it re-rolls Worldgen.cpp's
   phase below line 2388 for no gain.
4. **`PlacePuzzle` 0x421620 is a PARKED PARTIAL too** — 32 B → 29 B by hoisting `i` (or
   `pPt`). Same reasoning; needs the rest of the function before it is worth landing.
5. **Do NOT re-tread** the v106 closed table PLUS the v107 additions: `DrawDirectionArrows`
   0x4270f0 (13 spellings, floor 21), `GetFrameTile` 0x404850 (one-variable REFUTED at
   177 B vs 183), `FindObjectAt` 0x405330 (loop form + cached-GetSize now closed too; floor
   2 B over 15 spellings), `RemoveEmptyZonesFromPlacedList` 0x403070 (10 variants inert).
   ⚠ The **duplicated-local seam is DOWNGRADED** — 3 of 3 top candidates closed with no gain
   (standing bullet above). Keep the scan as a target list, not as a merge probe.
6. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
7. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104/v105/v106 rules stand, plus v107 additions):** every
sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run sweeps
with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps
concurrently, or one while `progress.py`/`exactset.py`/`residuals.py` is in flight (they all
share `build/*.obj`). Measure with **`tools/exactset.py` + `comm`**, never progress.py's total
alone. **NEW v107:** (a) `residuals.py --csv` needs a PATH argument — bare `--csv` raises
IndexError; (b) a COMMENT-ONLY edit that changes a TU's line count must still be verified with
`exactset.py` when exact functions sit downstream of it (both v107 doc commits were checked and
were free); (c) the `mov-operand` kind covers 119 of 129 residuals — useless as a targeting
signal on its own; (d) when a variant fails to COMPILE, suspect VC4.2 old-for-scope before
suspecting the generator.

---


## ⏮ v108 (2026-09-03) — 250 → 251 (+1 gained / 0 lost)

- **`ParseTilesMaybe` 0x41a030 DIFF(3) → EXACT** via **decl ORDER at function scope** (lesson #38):
  `int i;` BEFORE `int n = nBytes / 0x404;`. Every order with `n` first costs 40–43 B; `pNew,i,n`
  and `i,pNew,n` both give 0 (a FAMILY — kept `pNew` first as the minimal change, lesson #36).
- **Lesson #41 + `tools/declorder.py`**: a FLAT sweep is a signal to CHANGE AXIS, not to park. The
  loop-form sweep (#40) came back dead flat across 8 spellings, and that is what pointed at the
  decl dial. Cycle scope (#37) → set+order (#38) → loop form (#40) before parking.
- **Harness trap (a)**: a "BACKGROUND COMMAND COMPLETED" notification does NOT mean a detached
  sweep is done — a `nohup`'d batch reported exit 0 while its process tree ran ~10 more minutes,
  silently invalidating a second sweep AND all five oracles run alongside it. Confirm with
  `ps aux` AND the driver's own DONE marker. (b) `residuals.py --csv` sorts by `ndiff`, not `diff`.
- Decl-order axis reported MINED OUT over residuals ≤13 B (0 improvements / 17 targets, 11 with
  "no permutable block"). ⚠ **v109 showed that last figure was partly a tool bug** — see below.

---


---

### ⏮ v109 PICKUP (demoted at v110 — condensed)

#### (was) NEXT SESSION PICKUP (2026-09-03 v109 — **251 → 251**, +0 / −0, no trades. A HARNESS-BUG
session: one real tool fix, four measured-flat axes, one refuted hypothesis, one root cause
nailed. All 5 oracles green (251 exact / link 0-0-exit0 / bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN /
msg 11 CLEAN). v108 log demoted to PLAN_COMPLETED.md.)

**▶ WHAT LANDED.**
1. **⭐ `tools/declorder.py` was BLIND TO ARRAY DECLARATORS — a 6th entry in the "harness can
   lie" family.** Its `DECL` regex accepted `int n;` and `int n = expr;` but not `char buf[32];`,
   and because `leading_block()` walks from the first body line and BREAKS on the first
   non-declaration, a function whose first local is a buffer reported **"no permutable block"**
   — the exact shape of v108's "11 of 17 targets have no permutable leading block at all".
   **9 residuals were silently unreachable**, incl. all three `SaveStoryHistory*` (611 B each),
   the `ReadZax2`/`ReadZax3` pair, `ReadIzon`, and `LoadStoryHistoryNevada`. Fixed (one regex
   clause; the tool now finds an 8-decl block in SaveStoryHistoryNevada where it saw none).
   ⚠ **Honest bottom line: the fix did NOT overturn v108's conclusion for the ≤13 B census** —
   of the small residuals only 0x401ac0 and ReadIzon were newly reachable, and 0x401ac0 swept
   flat. What it unblocked is the BIG untouched functions, which is where it should now be aimed.
2. **`SaveStoryHistoryNevada` 0x402670 — root cause NAILED (still 611 B, but no longer vague).**
   (a) The slot 3-cycle is exact: orig `lineNo@-0x1c base@-0x20 rem@-0x24`, ours
   `base@-0x1c rem@-0x20 lineNo@-0x24`; the ebp-slot histograms are otherwise identical
   slot-for-slot. (b) The **16 B is a FAILED CROSS-JUMP**: the original loads `base` ONCE before
   `cmp esi,9`, so both `sprintf` arms start with `eax=base`, end identically, and share the tail
   `lea buf; inc esi; push buf; call`. Ours loads `base` per-arm → the arms land the buf pointer
   in different registers (arm 2 additionally needs EBX, hence the extra `push ebx` at +0x1e) →
   the tails cannot merge. (c) **REFUTED by the original's own bytes**: the tempting "arm 2 should
   reuse the running `base` instead of recomputing `lineNo*10`" — the orig emits
   `lea edx,[eax+eax*4]; lea edx,[esi+edx*2]`, i.e. it DOES compute `k + lineNo*10`. `idx` as
   transcribed is correct; do not merge it into `base`.
3. **Four axes measured FLAT (record, don't re-tread):** SaveStoryHistoryNevada — `int base;`
   hoisted to all 6 leading-decl positions (including the reverse-decl-order PREDICTION that it
   belongs right after `int rem;`) plus a rem/lineNo swap, all 611. `ReadZax2` 0x406410 — **all
   120** decl permutations flat at 47 (so its +3 B `mov ax,mem; movsx ebp,ax` survives both the
   v99 statement sweep and the decl axis; same verdict for its textual twin `ReadZax3` 0x406490).
   `LoadStoryHistoryNevada` 0x401ac0 — its one permutation flat at 2.
4. **`CalcSolvedScore` 0x401780 — a POSITIVE result inside a park.** The decl-order axis is
   genuinely LIVE here (unlike the three above): `x` must lead (moving it costs 16 B) and `solved`
   must precede `total` (swapping costs 15, full reversal 18); the other 10 of 14 swaps are inert
   at 13. The current transcription is therefore at the axis's optimum — measured evidence now,
   not an untested guess. Still parked on the x87 2-accumulator axis.

**▶ NEXT — concrete, in priority order.**
1. **Aim `declorder.py` where the fix actually opened ground: the BIG functions.** The ≤13 B
   census is genuinely mined out; the newly-reachable mass is large residuals whose first local is
   a buffer. Untried: `Worldgen.cpp` 0x41f960 (5704 B, block starts `short aOrder[100];` — and
   per lesson #36 that extent is itself a free variable), `Iact.cpp` 0x406270 (111 B, 5-decl
   block), `RefreshZone` 0x403ae0 (70 B), `StartGame` 0x4037a0 (79 B).
2. **SaveStoryHistoryNevada's live axis is now identified, so it is no longer a blind grind:**
   find the spelling that makes cl hoist the `base` LOAD above `cmp esi,9` (that single change
   cascades into the shared tail and the whole 16 B). Decl scope/order are proven inert, so this
   is a statement-order / expression-shape question — lesson #39 territory. Pays 3× (Alaska,
   Oregon are textual twins). ⚠ `int idx = base + k;` before the `if` is NOT it: the original
   duplicates `add eax,esi` in each arm and only shares the LOAD.
3. **The WorldgenHelpers JOINT SEARCH** (carried from v107, still the best structural lead):
   recover `LoadZoneRecursive` 0x403450's last 1 B without giving up SaveZoneRecursive; bounded by
   v106 downstream-only ⇒ the compensating edit must sit UPSTREAM of line 613. The three
   `SaveStoryHistory*` are that upstream region — item 2 is the way in.
4. **Parked partials, unchanged:** `WorldgenCollectZoneRefs` 0x41f8e0 (9 → 7 B via countdown) and
   `PlacePuzzle` 0x421620 (32 → 29 B via hoisting `i`) — land only inside a joint pass.
5. **Do NOT re-tread** the v106/v107/v108 closed table, plus the v109 additions in item 3 above.
   `ReadIzon` 0x405ae0 stays parked as header-phase displacement (v36 root-cause; do not mangle)
   — it now HAS a detectable block, so resist the temptation.
6. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19, DeskcppDoc's
   `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp `sizeof` dial note).
7. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v108 rules stand; v108's trap RE-CONFIRMED).** Every sweep
MUTATES a source file — always `git status --porcelain src/` AFTER each one; run sweeps with
`run_in_background` writing to a LOG FILE; restore a single function from `git show HEAD:<file>`,
never `git checkout <file>` mid-sweep; never run two sweeps concurrently, or one while
`progress.py`/`exactset.py`/`residuals.py` is in flight (they share `build/*.obj`). Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone.
⭐ **v108's "completion notification lies" trap REPRODUCED VERBATIM this session** — the
`declorder` run on 0x406410 was reported `completed (exit code 0)` with only 12 of 120 variants
logged, and `ps aux` showed it still running for ~16 more minutes. **Always gate on BOTH `ps aux`
and the driver's own DONE marker**; the wait loop that works is
`while ! (grep -q DRIVER-DONE log && ! pgrep -f 'declorder|vartest'); do sleep 5; done`.
⚠ **Adding COMMENT lines to a byte-matched TU is a lesson-#23 risk, not a free action** — this
session added 4 note blocks (Iact/Score/WorldgenHelpers) and re-ran all five oracles to prove
the per-TU counts were unchanged. Do that, don't assume.
---


---

## ⏮ v110 (2026-09-03) — 251 → 255 (+5 gained / −1 lost, all deliberate)

One new instrument drove the whole session: **`tools/savescan.py`**, which compares the PROLOGUE
callee-save set (`push ebx/esi/edi` before the first call/branch) of every non-exact residual
against the original's. Full write-up in CLAUDE.md's standing-lesson block "THE CALLEE-SAVE SET
IS THE CHEAPEST DIAGNOSTIC IN THE PROJECT".

- **Lesson #42 — the CSE-TEMP / LICM dial (ours saves MORE than the original).** A named temp for
  a subexpression that is partly loop-INVARIANT lets cl hoist the invariant part out of the loop
  into a callee-saved register, which then cascades: extra push/pop, asymmetric if/else arms, a
  FAILED CROSS-JUMP of their shared call tail, and rotated frame slots. `SaveStoryHistory*`
  **611 B → 22 B ×3** by repeating the subscript inline in both arms instead of naming `idx`.
  Same family in a different guise: a global-form call whose object is a POINTER CHAIN lets cl
  keep the base alive across an inner call — the MFC member form forces the reload
  (`DrawTextA` 663 → 60).
- **Lesson #43 — the NAMED-LOCAL lever (ours saves FEWER).** The original kept a value alive
  across a call that we spell as an argument expression. `CheckCheat` 0x415820 **372 → 0**
  (`int x = pWorld->playerX * 7 + 18;` ahead of the `str = "..."` assignment, both call sites);
  `ConfirmExit` 0x416030 **10 → 0** (`CWinApp *pApp = AfxGetApp();` INSIDE the `if` body, plus
  `CWinThread *pT = (CWinThread *)pMusicThread;`). Directional and combination-sensitive, which
  is what makes it evidence: x before y, the pair before the assignment, pApp inside not outside.
- **A DTOR CALL'S POSITION PROVES AN INNER SCOPE.** DrawTextA's original destroys its `CBrush`
  BEFORE the loop increments, impossible for a brush declared directly in the do-body. An
  explicit nested block: 24 → 2.
- **`OnAppExit` 0x416110 was never a function body** — it is a 5-byte `jmp ConfirmExit` forwarder
  and we had transcribed the twin's 220-byte body there. progress.py compares OUR length's worth
  of bytes at the marker VA, so a too-short original yields a plausible residual instead of an
  error. An extent scan over all 378 markers found no second case.
- **Harness lie #7 (caught before publishing):** implementing the callee-save set from EPILOGUE
  pops instead of the prologue desyncs on embedded jump tables / EH data and flagged six
  functions whose originals visibly DO save those registers.
- **v106's "TU joint phase is DOWNSTREAM-ONLY" has an exception:** a line-neutral token-only edit
  at 0x416110 moved `CyclePalette` 0x415af0, which precedes it in address and file order.
- Parked WITH MEASUREMENTS (do not re-tread): SaveStoryHistory*'s 2/4 B (8 spellings flat, an
  upstream perturbation moves them by 0, and the three twins disagree with each other);
  DrawTextA's 2 B (10 spellings flat, decl order already optimal); the three OnHScroll slider
  twins; `ScrollTextLine` 0x417c90.

---

### ⏮ v110 PICKUP (demoted at v111)

### ⏭ NEXT SESSION PICKUP (2026-09-03 v110 — **251 → 255**, +5 gained / −1 lost across the
session, all deliberate. One new INSTRUMENT drove everything; two new lessons (#42/#43); one
phantom residual retired. All 5 oracles green (255 exact / link 0-0-exit0 / bugscan 0 HIGH 0
SHIFT / vt 10 CLEAN / msg 11 CLEAN). v109 log demoted to PLAN_COMPLETED.md.)

**▶ WHAT LANDED — read the new standing-lesson block "THE CALLEE-SAVE SET IS THE CHEAPEST
DIAGNOSTIC" above first; it is the through-line of the whole session.**
1. **`SaveStoryHistory{Nevada,Alaska,Oregon}` 611 B → 2/4/2 B** (lesson #42, the CSE-TEMP/LICM
   dial). A named temp for a partly-loop-invariant subexpression was the whole thing. The
   phase shift recovered `LoadZoneRecursive` 0x403450's last byte: net +1.
2. **`OnAppExit` 0x416110 is a 5-byte `jmp ConfirmExit`** — a forwarder, not a copy of the twin
   body. Retired a phantom 163 B residual. Cost 2 phase losses at the time; both later came back.
3. **`DrawTextA` 0x40f060 663 B → 2 B** via three composed levers (MFC member form on a pointer
   chain → PatBlt member form ×2 → an explicit inner scope for the CBrush). +1.
4. **`CheckCheat` 0x415820 372 B → EXACT** and **`ConfirmExit` 0x416030 10 B → EXACT** via
   lesson #43 (an argument expression the original held in a named local). **+2.**
5. **`CyclePalette` 0x415af0** — its two AnimatePalette calls must take DIFFERENT forms (first
   member, second global); the source carried both as member. Re-measured all 16 combinations.
6. **New `tools/savescan.py`**; `vartest.py` gained a `saves=` column.

**▶ NEXT — concrete, in priority order.**
1. **Work the lesson-#43 target list.** `savescan.py` is mined out (0 mismatches), but the
   *chain* scan that found ConfirmExit is not: non-exact functions carrying `f()->m(...)`,
   `((T *)p)->m`, `a->b->c(...)` or `->m_hXxx` as an argument. 40 residuals match; the ones
   NOT yet worked, cheapest first: `ReadSavedState` 0x405bd0 (12, `cast()->` ×2),
   `OnInitialUpdate` 0x426c40 (14, `call()->`), `WorldEntryStepMaybe` 0x409c10 (18, `a->b->c(`
   ×4), `WriteSavedState` 0x405f30 (20, `cast()->` ×2), `DrawDirectionArrows` 0x4270f0 (21),
   `DrawPlayer` 0x41a6d0 (50, `cast()->`), `RefreshZone` 0x403ae0 (70, `cast()->` ×3),
   `DrawWeaponIcon`/`DrawWeaponBox`/`DrawHealthDial`/`DrawHealthNeedle` (`->m_hXxx` args).
   The scan script is easy to re-derive (see the v110 session) — a `tools/chainscan.py` is
   worth writing if you work more than two of these.
2. **The DTOR-POSITION probe is new and unexploited.** DrawTextA's crack was recognising that a
   destructor call sitting EARLIER than the source allows proves a narrower scope. Any residual
   whose diff involves `mov [ebp-4],<state>` / a `call ~Foo` at an unexpected position is a
   candidate. Grep for stack objects with dtors (`CBrush`, `CPen`, `CString`, `CFont`,
   `CClientDC`) declared directly in a loop body.
3. **The v110 wins are all in DeskcppView.cpp / WorldgenHelpers.cpp; Worldgen.cpp is untouched**
   and holds the biggest residual mass (`Generate` 0x41f960 at 5704 B, `OnInitialUpdate`,
   `PlaceQuestNode`). Run `savescan.py` again after any transcription work there.
4. **Parked with MEASURED evidence this session — do NOT re-tread:** the SaveStoryHistory* 2/4 B
   (8 in-function spellings flat; an upstream token perturbation moves them by 0; the three
   twins DISAGREE with each other, v105's "source is not the variable" signature);
   `DrawTextA`'s last 2 B (10 spellings flat, decl order already optimal); the three OnHScroll
   slider twins 6/111/290 (expression fusion is worse, 4 compare spellings flat);
   `ScrollTextLine` 0x417c90 (10 spellings, current is the optimum).
5. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
6. **Phase-H goals 2-5 untouched** this session.

**▶ ⚠ ONE STANDING RULE IS NOW KNOWN TO HAVE AN EXCEPTION.** v106's "THE TU JOINT PHASE IS
DOWNSTREAM-ONLY" does not hold universally: the OnAppExit edit (line-neutral, tokens only)
moved `CyclePalette` 0x415af0, which sits BEFORE it in both address and file order. Treat
downstream-only as a useful heuristic for bounding a joint search, not a guarantee — re-run
`exactset.py` + `comm` over the WHOLE project after any token change, not just the tail.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v108 rules stand).** Every sweep MUTATES a source file —
always `git status --porcelain src/` AFTER each one; run long sweeps with `run_in_background`
writing to a LOG FILE and gate on BOTH `ps aux` and the driver's own DONE marker; restore a
single function from `git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run
two sweeps concurrently, or one while `progress.py`/`exactset.py`/`residuals.py` is in flight
(they share `build/*.obj`). Measure with `tools/exactset.py` + `comm`, never progress.py's total
alone — this session had a +2/−1 that the total reported as "+1".
⚠ A large comment rewrite is a LINE-COUNT change; either keep it line-neutral against what it
replaces (the OnAppExit note is padded on purpose) or re-run the oracles and check the set.


---

### ⏮ v111 PICKUP (demoted at v112, 2026-09-03)

### ⏭ NEXT SESSION PICKUP (2026-09-03 v111 — **255 → 255, NO MATCHING GAIN.** A deliberate
negative-result session: six residuals swept to a measured floor, two harness bugs fixed, one
new tool. All 5 oracles green (255 exact, set IDENTICAL before/after via `exactset.py` + `comm`
/ link 0-0-exit0 / bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches).
v110 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST: the new standing lesson "THE SCRATCH-REGISTER BIJECTION IS A DISTINCT,
SOURCE-CLOSED CLASS" (lesson #44) above.** It is the whole result of this session and it should
change how the next one is spent.

**▶ WHAT LANDED.**
1. **Two harness bugs, both of the "reports NOTHING TO DO" family** (now item 7 of the
   instrument-can-lie list): `declorder.py` could not see a line holding SEVERAL declarations —
   exactly the line-neutral idiom this project uses — and reported "nothing to permute" on the
   first function it was aimed at; `residuals.py` called `main()` unguarded at module level, so
   importing it ran the census and ate the importer's `sys.argv`. Both fixed.
2. **New `tools/chainscan.py`** — the lesson-#43 target list the v110 pickup asked for. It
   reproduces v110's hand-derived list exactly (`0x405bd0` cast()->x2, `0x405f30` cast()->x2),
   which is the cross-check that it works. ⚠ its own first draft had two bugs, both caught by a
   known-answer positive control before publishing anything: the `cast()->` regex missed the
   canonical `((T *)p)->m` spelling, and restricting to argument lists dropped BOTH real v110
   wins (a call receiver and an assignment RHS). Hits are now tagged `arg`/`recv`/`other`.
3. **Six residuals swept to a measured floor, zero gain** — see lesson #44 for the list and the
   axes. Four got park notes recording exactly which axes are now closed (`ReadSavedState` had
   none at all before); the notes are LINE-COUNT changes in byte-matched TUs and were verified
   safe with the full before/after exact-set diff (+0/-0).

**▶ NEXT — concrete, in priority order.**
1. **⭐ Build the JOINT decl search.** This is the one identified path for the cheap band and it
   is now the highest-value piece of tooling missing. Per v105 (residuals in a TU are not
   independent) and v106 (the phase propagates DOWNSTREAM ONLY, so an upstream edit can never
   break an earlier fix), a joint search IS tractable: pick a TU, take its residuals in FILE
   order, and vary several functions' decl configurations together, scoring with `exactset.py` +
   `comm` (never progress.py's total — it reports +2/-4 as "-2"). Start with **Iact.cpp**: only
   3 residuals (`ReadIzon` 7, `ReadSavedState` 12, `WriteSavedState` 20), all three individually
   floored, and ReadIzon is the TU's FIRST emitted function so it seeds the whole phase.
2. **`WriteSavedState` 0x405f30 (20 B) is the one Iact.cpp residual NOT yet swept** — do it
   before the joint run so its own floor is known. It is `ReadSavedState`'s write mirror, so the
   v105 "textually identical sibling" test applies directly.
3. **The dtor-position probe (v110 item 2) is now SCOPED but still unexploited.** A scan of all
   123 residuals for an `[ebp-4]` EH-state store among the DIFFERING instructions gives ~20 real
   candidates; the cheapest are `PickUnplacedItemMaybe` 0x41c200 (13), `Puzzle::Puzzle` 0x4042b0
   (24), `RemoveEmptyZonesFromPlacedList` 0x403070 (26), 0x404c80 (73). ⚠ "a call appears in the
   diff" is NOT a useful filter (83 of 123 hit it); only the EH-state store is. ⚠ `Puzzle::Puzzle`
   is already refuted as a dtor case — its emit ORDER (CString member ctors BEFORE the scalar
   stores) positively CONFIRMS body assignments over an initializer list, and the rest is an
   esi/edi bijection.
4. **Worldgen.cpp still holds the biggest residual mass and is still untouched** (`Generate`
   0x41f960 at 5704 B, `OnInitialUpdate` 0x426c40, `PlaceQuestNode`). ⚠ `declorder` on
   `Generate` is NOT the move — its leading block is just `{aOrder, aPlan}`, 1 permutation, and
   a decl tweak cannot touch a 5704 B structural residual. These need transcription-level work.
5. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
6. **Phase-H goals 2-5 untouched** this session.

**▶ PARKED WITH MEASURED EVIDENCE THIS SESSION — do NOT re-tread** (details in each function's
source note): `ReadSavedState` 0x405bd0 · `ReadIzon` 0x405ae0 · `ZoneRequiresItemMaybe` 0x41c0b0
· `OnHScroll` 0x417fa0. Also re-confirmed correct by byte-diff against their existing notes:
`FindObjectAt` 0x405330 (2 B, the `test edi,edi` guard) and `LoadStoryHistoryNevada` 0x401ac0
(2 B, the proven jg/jl phase drift across the three loaders).

**▶ ⚠ v110's EXCEPTION TO "DOWNSTREAM-ONLY" STILL STANDS.** The OnAppExit edit moved
`CyclePalette`, which sits BEFORE it. Treat downstream-only as a heuristic for BOUNDING a joint
search, not a guarantee — always re-run `exactset.py` + `comm` over the WHOLE project.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v108 rules stand).** Every sweep MUTATES a source file —
always `git status --porcelain src/` AFTER each one; run long sweeps with `run_in_background`
writing to a LOG FILE and gate on BOTH `ps aux` and the driver's own DONE marker; restore a
single function from `git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run
two sweeps concurrently, or one while `progress.py`/`exactset.py`/`residuals.py` is in flight
(they share `build/*.obj`). Measure with `tools/exactset.py` + `comm`, never progress.py's total
alone. ⚠ A `vartest.py` BASE must be UNIQUE in the file — anchor it on the function signature
when the decl block is a common shape (4 functions in Iact.cpp share `char tag[5]; int size;`),
and remember `str.count()` is a SUBSTRING test, so a 4-space decl matches inside an 8-space one.
⚠ A comment rewrite is a LINE-COUNT change; keep it line-neutral or verify with the exact-set
diff (this session added 19 comment lines across 3 byte-matched TUs: +0/-0, verified).

---

### ⏮ v112 PICKUP (2026-09-03, demoted at v113)

### ⏭ NEXT SESSION PICKUP (2026-09-03 v112 — **255 → 255 exact, but a REAL fidelity gain:
`WriteSavedState` 0x405f30 went 20 B → 7 B**, and the joint search v111 asked for is BUILT,
RUN, and came back EMPTY. All 5 oracles green (255 exact / 99.17 %, exact set IDENTICAL
before/after via `exactset.py` + `comm` at every step / link 0-0-exit0 / bugscan 0 HIGH 0 SHIFT
/ vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches). v111 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST: the two new standing lessons above** — "THE DECL DIAL IS AN INTERACTION"
(#45, the win) and "THE TU-JOINT PHASE IS NOT REACHABLE BY DECL CONFIGURATION, AND NOT
POSITIONAL" (the two negatives that should stop the next session repeating this one).

**▶ WHAT LANDED.**
1. **`WriteSavedState` 0x405f30: 20 → 7 B** (lesson #45). Two levers that are each invisible
   to the other's sweep: `i` declared before `count` fixed the backedge compare form in all
   three count loops at once (20→14), then hoisting the three object pointers with `i` LAST
   fixed the iactScripts loop's bijection (14→7). Rival shapes all refuted by LENGTH.
2. **`tools/jointdecl.py`** — the joint search. Works, baseline-guarded, agrees with
   `verify.py` at zero perturbation. Its first run is the negative result above.
3. **Three well-evidenced PARKS**, each with the closed axes written into the source note:
   `ReadSavedState` 0x405bd0 (12 B — the #45 interaction re-opened it and 51 configurations
   re-closed it), `RemoveZoneEntry` 0x41d740 (13 B — the cleanest sibling case in the project;
   see below), and `WriteSavedState`'s own remaining 7 B (the objects loop's index/walker pair).

**▶ NEXT — concrete, in priority order.**
1. **⭐ Apply lesson #45's sweep shape to the rest of the cheap band.** This is the one axis
   with a fresh win behind it, and no existing tool performs it: for each residual, enumerate
   (hoist subset) × (position of the loop index, especially LAST). Best candidates, all
   confirmed pure bijections by `bytediff.py` and all with several inner-block locals:
   `PickUnplacedItemMaybe` 0x41c200 (13, esi↔edi + one schedule shift), `CalcSolvedScore`
   0x401780 (13), `Populate` 0x425e30 (13), `BlitTile` 0x40a320 (13),
   `RemoveEmptyZonesFromPlacedList` 0x403070 (26), `WorldgenAddZoneEntry` 0x41d800 (27).
   ⚠ Do NOT reach for `hoisttest.py`/`declorder.py` for this — they structurally ask only one
   of the two questions. Drive `vartest.py` with a generated set×order variant file, as this
   session did (the four `wss_*.py` sweeps are the template).
2. **⛔ Do NOT build another joint/decl search.** Both bounding negatives are measured now.
3. **The dtor-position probe (v110 item 2) is still the best UNEXPLOITED seam** — an
   `[ebp-4]` EH-state store among the DIFFERING instructions, ~20 real candidates. Cheapest:
   `PickUnplacedItemMaybe` 0x41c200 (13), `RemoveEmptyZonesFromPlacedList` 0x403070 (26),
   0x404c80 (73). ⚠ "a call appears in the diff" is NOT a filter (83 of 123 hit it).
   ⚠ `Puzzle::Puzzle` 0x4042b0 is already refuted as a dtor case.
4. **Worldgen.cpp still holds the biggest residual mass and is still untouched** (`Generate`
   0x41f960 at 5704 B, `OnInitialUpdate` 0x426c40, `PlaceQuestNode`) — transcription-level
   work, not a dial. ⚠ `declorder` on `Generate` is NOT the move (1 permutation).
5. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
6. **Phase-H goals 2-5 untouched** this session.

**▶ PARKED WITH MEASURED EVIDENCE THIS SESSION — do NOT re-tread** (closed axes are in each
function's source note): `WriteSavedState` 0x405f30 at 7 B · `ReadSavedState` 0x405bd0 at
12 B · `RemoveZoneEntry` 0x41d740 at 13 B. The last is worth reading in full before working
any bijection: its twin `RemoveZoneEntry2` 0x41d7a0 is byte-EXACT from CHARACTER-IDENTICAL
source, the two ORIGINALS also differ from each other, and swapping the two definitions in the
file changes nothing — statement order is positively confirmed (i-first is worse, 18 B), loop
form is inert, the countdown is refuted by length.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v108 rules stand, all re-confirmed this session).** Every
sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE and gate on BOTH `pgrep` and the driver's
own DONE marker; restore a single function from `git show HEAD:<file>`, never `git checkout
<file>` mid-sweep; never run two sweeps concurrently, or one while `progress.py`/`exactset.py`/
`residuals.py`/`jointdecl.py` is in flight (they share `build/*.obj`). Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone. ⚠ A `vartest.py` BASE must be
UNIQUE in the file — **four functions in Iact.cpp share `char tag[5]; int size; int i;`
verbatim**, so anchor a decl-block BASE on the function SIGNATURE (this bit twice this session).
⚠ Prefer a WHOLE-FUNCTION BASE when a variant must change both a decl and its body use — the
two spans have to stay coupled. ⚠ A comment rewrite is a LINE-COUNT change; verify with the
exact-set diff (this session added ~40 comment lines across 2 byte-matched TUs: +0/−0, verified).

---

### ⏮ v113 (2026-09-03, condensed — demoted from CLAUDE.md at v114)

**255 → 255 exact, one REAL fidelity gain.** `RemoveEmptyZonesFromPlacedList` 0x403070 went
26 B → 24 B on the LOOP FORM: loop 1 is the house guarded countdown, not a `for`, and the gain
was structural — `this` moved into ESI as the original has it, killing four this-relative load
diffs. Loop 2 correctly stays a plain `for` (both countdown spellings are worse at 27-29 B; the
register-guarded form is refuted by LENGTH, 204 vs 206). Shipped `tools/loopform.py` (read-only,
positive-controlled target list: the original's countdown backedges vs our `for` spellings).

**Measured negatives, all written into the functions' source notes — do NOT re-tread:**
- `PickUnplacedItemMaybe` 0x41c200 at 13 B, 55 configurations. Lesson #45's index-position
  interaction does NOT generalise: i-first/second/last is INERT across 9 hoist subsets, and
  hoisting the loop-2 locals is strictly WORSE (`j` +7 B, `nObjs` +8 B), which positively
  confirms the current spelling. Lesson #44 bijection class.
- `RemoveEmptyZonesFromPlacedList` 0x403070's remaining 24 B — 32 decl set × order
  configurations over {i,id,j,m}, all flat. An ebx↔edi 2-cycle plus an entry schedule shift.
- `CalcSolvedScore` 0x401780 at 13 B, 48 configurations. `pct`'s position is COMPLETELY INERT;
  only `y`-before-`x` moves and it costs 3 B. The residual is the x87 2-accumulator allocation.

**Method rules learned the hard way (still standing, see CLAUDE.md "HOW TO WORK THE DIAL
SAFELY"):** a variants file that reads the SOURCE to build its BASE must read
`git show HEAD:<file>`, not the working tree; `vartest.py` can be stopped cleanly mid-sweep with
SIGINT (`pkill -INT -f tools/vartest.py`), which runs its restore — SIGKILL would leave the
source mutated.

---

### ⏮ v114 SESSION LOG (2026-09-04 — **255 → 252 exact: a DELIBERATE, USER-APPROVED
RE-BASELINE DOWN**, bought with one large length-PROVEN fidelity gain. `WorldgenShuffleList`
0x41ef90 went **269 B → 16 B** on a NEW dial (loop ROTATION, lesson #46) plus decl ORDER, and
`PlaceQuestNode` 0x41f120 gained 432 B → 405 B alongside it; three downstream functions lost
byte-exactness to the v105 TU-joint phase. Also fixed a build break that had been sitting in
`build-sdl` since v110. All oracles green at the new baseline (252 exact / 99.17 % / link
0-0-exit0 / bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches over
126 residuals). v113 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST, IN THIS ORDER:** (1) the re-baseline note at the TOP of this file — 252 is the
new floor and reverting 0x41ef90 to "fix" it back to 255 is the wrong move; (2) the new standing
lesson **"THE LOOP FORM IS TWO DIALS, NOT ONE — ROTATION IS THE SECOND, AND LENGTH SETTLES IT"**.

**▶ WHAT LANDED.**
1. **`WorldgenShuffleList` 0x41ef90: 269 B → 16 B**, two levers. The init loop is
   `for (;;) { if (i >= nSize) break; ... }`, not a `while` — the original does not ROTATE it
   (test at top, unconditional `jmp` back, no duplicated bottom test). Proven from outside the
   byte diff: Ghidra's extent is **396 B**, the `while` spelling emits **400**, this one emits
   **396**. Then `short i` declared BEFORE `short nSize` killed all three backedge cmp mirrors
   at once (22 → 16), while all 8 ways of respelling those three conditions were dead flat.
2. **`tools/unrotscan.py`** — read-only, positive-controlled sibling of `loopform.py` for the
   rotation dial. ⚠ **1 hit project-wide (0x41ef90 itself) ⇒ the lever is MINED OUT.** Re-run it
   only on newly-transcribed functions.
3. **`build-sdl` was BROKEN since v110 and nobody noticed.** Lesson #42's `DrawTextA` win
   switched `DeskcppView.cpp:4271` to `pWorld->pPalette->GetNearestPaletteIndex(...)`, and
   microfx's `CPalette` had no such member. Added it (`microfx/include/afxwin.h`); build-sdl and
   all six harnesses link again, `worldgen_smoke` runs. ⇒ **The anchor is not the only oracle:
   build `build-sdl` after ANY call-form edit** — this is the second time that rule has been
   proven in anger (v95 was the first).

**▶ MEASURED NEGATIVES — do NOT re-tread** (all written into the functions' source notes):
- **Recovery of the three phase-displaced functions is IMPOSSIBLE from their own bodies.**
  `SetCurrentToIntroZone` 0x423d20 (2 B): 12 decl SET × ORDER configs + the cmp mirror + the
  `zones[i]` subscript form, floor 2. `CheckZoneItemsAvailable` 0x41f830 (9 B): 5 decl configs,
  floor 9. `DetonateAdjacentTiles` 0x428680 (60 B): proven phase-only at v39 AND v105.
- **The re-roll is driven by EMITTED CODE, not token count.** All 8 unrotated spellings of
  0x41ef90's loop produce BYTE-IDENTICAL damage vectors across every marker in Worldgen.cpp.
- **Third confirmation of v112's "no cross-function coupling from decl configuration":** a
  30-combination `jointdecl.py` grid over 0x41ef90 × 0x41f830 separates perfectly — each
  function's column depends only on its OWN edit.
- **0x41ef90's last 16 B** is the `{pSlot,m}` ecx↔eax 2-cycle + the `GetAt(k)` zero-extend
  register (lesson #44, source-closed). `int m` before `pSlot` measures 15 — one byte better,
  but it contradicts the original's load ORDER, so it was not taken; `m = nSize` (24 B) and
  `short m` (195 B) positively confirm `int m = nInt`.

**▶ NEXT — concrete, in priority order.**
1. **`tools/loopform.py`'s list is still the axis with fresh wins behind it** (v113 and v114 both
   came out of loop shape). Untouched hits, confirm each with `bytediff.py` FIRST:
   `0x4037a0` StartGame (79 B — its tail also has a clean je/jne POLARITY diff at +0x249:
   orig `test eax,eax; je; inc edi` vs ours `test eax,eax; jnz; mov ecx,esi; call`, i.e. the
   original's `if/else` arms are the other way round), `0x406780` IactRun, `0x421930`,
   `0x4070e0`, `0x406270`, `0x41ef90` (now worked).
2. **`IactRun` 0x406780 has TWO cheap source-visible items** inside its 1548 B (the rest is a
   ±9 B local length redistribution): at +0x14a the original computes `dx + x` and compares
   (`mov eax,[dx]; add eax,[x]; cmp eax,[args0]`) where ours emits a SUBTRACT
   (`mov eax,[args0]; sub eax,[dx]; cmp eax,[x]`); at +0x15b the original adds `y + dy` in that
   order and ours emits `dy + y`. Its header note already documents the `(ty = y + dy)`
   in-condition assignment that fixed the second compare — the same trick for `x` is untried.
3. **⛔ Do NOT run another per-function decl sweep on the cheap band.** v111 swept six to a
   floor, v113 three more, v114 three more (0x423d20, 0x41f830, and 0x41ef90's tail). Below
   ~25 B the decl dials are mined out; that verdict now rests on twelve functions.
4. **The dtor-position probe (v110 item 2) is STILL the best unexploited seam** — an `[ebp-4]`
   EH-state store among the DIFFERING instructions, ~20 real candidates. ⚠ `[ebp-4]` holding a
   REGISTER is a spill/EH-state-zero reuse, not a dtor-position tell; look for
   `mov [ebp-4], imm` or a dtor CALL sitting earlier than our source allows. ⚠ "a call appears
   in the diff" is NOT a filter. ⚠ `Puzzle::Puzzle` 0x4042b0 already refuted.
5. **Worldgen.cpp still holds the biggest residual mass** (`Generate` 0x41f960 at 5710 B,
   `OnInitialUpdate` 0x426c40, `PlaceQuestNode` 0x41f120 now at 405 B) — transcription-level
   work, not a dial. ⚠ `declorder` on `Generate` is NOT the move (1 permutation).
6. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
7. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v113 rules all stand and were all re-used this session).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE and gate on BOTH `pgrep` and the driver's
own DONE marker; restore a single function from `git show HEAD:<file>`, never `git checkout
<file>` mid-sweep; never run two sweeps concurrently, or one while `progress.py`/`exactset.py`/
`residuals.py`/`jointdecl.py` is in flight (they share `build/*.obj`). A variants file that reads
the SOURCE to build its BASE must read `git show HEAD:<file>`. `vartest.py` stops cleanly on
SIGINT (`pkill -INT -f tools/vartest.py`), which runs its restore. Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone. A comment rewrite IS a line-count
change (lesson #23) — this session added ~50 comment lines to Worldgen.cpp and verified +0/−0
with the exact-set diff afterwards.
⭐ **v114 additions:**
- **`run_in_background` + `nohup ... &` returns immediately and the harness reports the LAUNCHER
  as "exited with code 0" while the sweep is still running.** Confirm with `pgrep -f
  tools/vartest.py` before concluding anything; a mutated `git status` at that moment is the
  sweep working, not a crash.
- **An unquoted bash heredoc runs BACKTICKS as commands.** A generator script whose comments
  contained `` `cmp reg, mem` `` silently executed them and wrote empty text into the generated
  file. Quote the heredoc delimiter (`<<'EOF'`) whenever the body contains backticks.
- **`jointdecl.py` is the right tool for "did my fix break anything downstream in this TU?"** —
  one compile per variant, a byte-diff column for EVERY marker. Far cheaper than an
  `exactset.py` run per variant, and it shows the damage vector rather than just a total.


---

### ⏮ v115 PICKUP (2026-09-04 — condensed at v116; anchor 252, +0/−0)

Held at **252 exact**; no new byte-match, but two residuals cut and lesson #47 named.
1. **`StartGame` 0x4037a0: 79 B → 69 B** — the IF/ELSE ARM ORDER (lesson #47, new). The
   original emits the `ok = ok + 1` arm as the FALLTHROUGH, pinning
   `if (Generate(seed) != 0) ok = ok + 1; else seed = Randomize();`. The negation/increment
   SPELLING is inert (`!Generate(...)`, `ok++` both 79 B) — only the ORDER moves anything.
   Rivals refuted by LENGTH: `ok = 1` emits 671 B, `while (ok < 1)` 668, original 667.
2. **`IactRun` 0x406780: 1548 B → 1538 B** — `(tx = dx + x)` in the COND_BumpTile condition,
   the twin of the `(ty = y + dy)` trick from G1.
3. **Oracle correction:** bugscan's green state is **1 HIGH (known benign)**, not 0 — the
   `StartGame @+0x14a lea orig=0x4b4 ours=0x4b0` finding, a proven false positive (all 30
   grid stores have `ours_disp == orig_disp + 4`, exactly cancelling). It had been recorded
   wrong for an unknown number of sessions. ⇒ **a stale GREEN STATE is as dangerous as a
   broken tool; re-measure at HEAD (`git stash push -- src/`) before assuming a regression.**

v115 measured negatives (all still recorded in the functions' source notes): `StartGame`'s
remaining 69 B is source-closed (10 spellings flat at 68–72, decl swap strictly worse);
`IactRun`'s +0x15e is flat across 7 `ty` spellings and dropping `ty` costs 252 B;
`IactRun`'s `tx` is NOT reused in the tile subscript (refuted by length, 2404 vs 2408);
`WorldgenPlacePuzzles` 0x421930's +0x05f movsx site had three hypotheses refuted, confirming
`a4` is `short`; `ReadZaux` 0x406270 is the double-swept `mov ax`/`movsx` park.

⚠ v116 note: v115's pickup item #1 (build the lesson-#47 scanner) was DONE — `armscan.py`,
10 hits, 6 two-armed, nearly mined out. Item #2 (the dtor-position seam) was scanned by
`dtorscan.py` and turned out to be 4 functions, not ~20. Item #4 (`TriggerHotspotsMaybe`)
was CLOSED with a bounding negative.


---

### ⏮ v116 PICKUP (demoted at v117)

### ⏭ NEXT SESSION PICKUP (2026-09-04 v116 — **255 exact (+3 REAL: 252 → 253 → 254 → 255,
each verified by `exactset.py` + `comm`)**. Three byte-matches, all from ONE newly-found
lever: the **CONTAINER CALL FORM** (lesson #48, new) — `a.Add(v)` vs
`a.SetAtGrow(a.GetSize(), v)` and `a[i]` vs `a.GetAt(i)`. Two new scanners built
(`armscan.py`, `dtorscan.py`). All oracles green: 255 exact / 99.17 % / link 0-0-exit0 /
bugscan 1 HIGH (known benign) 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches /
build-sdl links, worldgen_smoke converges. v115 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST:** the new standing lesson **"THE CONTAINER CALL FORM IS A MATCHING LEVER"**
(#48) — including its four ⚠ clauses, which cost real time to learn this session and which a
future session will otherwise repeat.

**▶ WHAT LANDED.**
1. **`ParseChar` 0x421e70: 110 B → BYTE-EXACT** — `characters.Add(pNew)`, not
   `characters.SetAtGrow(characters.GetSize(), pNew)`.
2. **`RemoveEmptyZonesFromPlacedList` 0x403070: 24 B → BYTE-EXACT** — same lever. This
   RETIRED a v113 park that had closed 32 decl configurations, all flat at 24. That sweep was
   accurate and aimed at the wrong axis (lesson #41 in the wild).
3. **`ParseChwp` 0x423300: 47 B → BYTE-EXACT** — `characters[id]`, not `.GetAt(id)`.
4. **`DamageEntityAt` 0x405710: 556 B → 51**, and its LENGTH now matches (696 → 690 = 690).
5. **`WorldgenPlaceItemOnLock` 0x41cdc0: 96 B → 56** — lesson #47 arm swap (96 → 93, the
   first find by `armscan.py` rather than by eye) then lesson #39 statement order (93 → 56).
6. Also: `ParseCaux` 41 → 11, `PickUnplacedItemMaybe` 13 → 5, `StartGame`-era notes tidied.

**▶ NEW TOOLS (both COMPILE — never run them during a `vartest.py` sweep).**
- **`tools/armscan.py`** — lesson #47 target list. 10 hits, 6 two-armed. Nearly mined out.
- **`tools/dtorscan.py`** — lesson-#110 dtor-position target list. With the right filters the
  seam is **4 functions, not ~20**: 0x422670 `Load`, 0x405710 (now cleared), 0x421e70 (now
  exact), 0x422fd0 `ParsePuz2`. So item #2 of the v115 pickup is largely CLOSED.

**▶ MEASURED NEGATIVES — do NOT re-tread** (all written into the functions' source notes):
- **`TriggerHotspotsMaybe` 0x40ec30 is CLOSED.** All 10 line-neutral permutations of its
  opening statements are strictly WORSE (42–290 B), so the current order is the unique
  minimum and positively confirmed. Residual 17 B is a lesson-#44 scratch bijection.
- **`WorldgenPlaceItemOnLock`'s last 56 B** is a bijection ({bFound,i} = EBX,EBP orig vs
  EDX,EBX ours); all 6 decl SET/ORDER variants are worse AND emit the wrong length.
- **`OnSaveWorld` 0x424540's container conversion is REFUTED** — its own sites move it not at
  all, and landing it costs `DrawRect` 403 B and its length match.
- **`ParseCaux`'s 11 B** is a 3-cycle bijection whose twin `ParseChwp` is byte-exact from
  character-identical source — the v105 sibling signature. Don't grind it.
- **`ParseChwp`'s arm order** is genuinely inert (15 spellings). The win was elsewhere.

**▶ NEXT — concrete, in priority order.**
1. **⭐ WORK THE REST OF THE CONTAINER-CALL-FORM SEAM — it is by far the best lead.** Only a
   handful of ~80 `SetAtGrow(X.GetSize(), …)` and ~277 `.GetAt(` sites have been tried, and
   three of the tries reached BYTE-EXACT. Method that works: `formsweep`-style TU-wide sweep
   to build a CANDIDATE list, then **isolate each function and land only on byte-exactness or
   a big diff cut at a non-worsening LENGTH**. The `a[i] = v` vs `a.SetAt(i, v)` pair is
   measured-mixed and entirely unworked. ⚠ re-read lesson #48's ⚠ clauses first.
2. **Other MFC inline pairs, same idea, never probed:** `GetSize()` vs `GetUpperBound()+1`,
   `RemoveAt`/`InsertAt`/`RemoveAll`, and `CString` ops. Lesson #35 generalised much further
   than SendMessage; lesson #48 suggests the container/inline surface generally is a dial.
3. **`dtorscan.py`'s two live hits**: `Load` 0x422670 (2 loops flagged, 1634 B) and
   `ParsePuz2` 0x422fd0 (165 B, the smaller and better start).
4. **`loopform.py`'s remaining untouched hits**, confirmed with `bytediff.py` FIRST:
   `0x4070e0` IactRunCommands (1401 B), `0x41bb10` OnNewDocument (537 B), `0x403c80`
   BuildQuestPathMaybe (1121 B).
5. **⛔ Do NOT run another per-function decl sweep on the cheap band** (< ~25 B). That verdict
   now rests on ~17 functions across v111–v116.
6. **Worldgen.cpp still holds the biggest residual mass** (`Generate` 0x41f960 at 5710 B,
   `OnInitialUpdate` 0x426c40, `WorldgenPlacePuzzles` 0x421930) — transcription-level work.
7. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
8. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v115 rules all stand and were all re-used).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run
long sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps
concurrently, or one while `progress.py`/`exactset.py`/`residuals.py`/`jointdecl.py`/
`armscan.py`/`dtorscan.py` is in flight (they share `build/*.obj`). Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone. A comment rewrite IS a
line-count change (lesson #23).
⭐ **v116 additions:**
- **CHECK THE LENGTH, NOT JUST THE DIFF, BEFORE LANDING ANYTHING.** Our trimmed length vs
  `toolchain/test/app_funcs.txt`'s extent is a second, stronger oracle. Three conversions
  this session bought noise-level diff gains while moving the length AWAY and had to be
  reverted; one upstream landing silently cost `DrawRect` its 654-byte length match.
- **ISOLATE BEFORE LANDING.** A sweep that rewrites many sites measures the COMBINATION, not
  each function's own source.
- **v106's "downstream-only" is not a safety proof for MULTI-function landings** — an
  EARLIER function moved only when three edits were combined. Re-check upstream too.
- **When you fix a harness bug of form X, grep every sibling tool for X** — `bytediff.py` had
  the same unguarded `main()` that `residuals.py` was fixed for at v111.
- **`residuals.py --csv` needs a PATH argument**; its `va` column is **DECIMAL**.
- **`loopform.py --all` includes ALREADY-EXACT functions**; cross-check with `residuals.py`.

⚠ v117 note: v116's pickup item #1 (work the container-call-form seam) was worked and the
Worldgen CHEAP BAND is now CLOSED — 11 variants across five functions, all flat, with the
byte-exact sibling ZoneHasIzxItemMaybe confirming `.GetAt()`. Item #3's ParsePuz2 remains
open. ⛔ AND v116's own last commit (165b365) was found to have silently cost DrawRect 327
bytes + its length match: the three-way revert was right for ParsePuz2 and AddItemToInv but
wrong for Generate, because it judged each conversion by the EDITED function alone — the very
failure lesson #48 warns about. Fixed at v117; see lesson #49.

### ⏮ v117 (2026-09-04) — LENGTH FIRST: the vacuous `lenmis` column, and the seam it had been hiding

**255 exact, held (+0/−0).** No new byte-match, but two functions were fixed STRUCTURALLY for a
net −654 bytes of residual, and an entire ORACLE was found silenced.

- ⭐ **`residuals.py`'s `lenmis` column had been VACUOUS since it was written** — it compared our
  length against a slice taken AT our own length, so it could never be True. Fixing it against
  Ghidra's extents created the `--lenmis` seam and **lesson #49**: a residual whose emitted LENGTH
  is wrong is a STRUCTURAL defect, and the register difference you see is its consequence.
  The eleventh harness bug of the project, and the first one that had been HIDING the best
  remaining target list rather than merely misreporting it.
- **`ZoneProvidesItem` 0x41c3b0: 214 B → 17, length 239 → 214 = the extent.** A year-old park note
  describing a register permutation turned out to be describing a SYMPTOM: an arm that RETURNS must
  not also ASSIGN (the extra `found = 1` is what spilled `found` out of EDI), and an inner early
  exit is `break;` not `return x;` (which duplicated the epilogue).
- **`Generate` 0x41f960's `Add` form restored → `DrawRect` 0x424010: 387 B → 60**, length 651 → 654
  = the extent. v116's last commit had judged that revert on the EDITED function alone — exactly
  the failure lesson #48 warns about — and silently cost DrawRect 327 bytes for four commits.
- New: **`tools/formsweep.py`** (one-at-a-time TU sweep, k+1 compiles, whole marker vector per row).
- ⚠ The raw length census MANUFACTURES targets: 18 Ghidra extents are stubs reading `1`, and a
  trailing switch JUMP TABLE sits inside our COMDAT but outside the extent. Unfiltered it put
  Tick/Run/Generate on top — all three artifacts. Filtered: 44 residuals / 408 B of real
  structural error, 48 length-EXACT, 31 not comparable.
- Measured negatives recorded in the functions' source notes: the container ACCESS form is CLOSED
  on the Worldgen cheap band (11 variants across 5 functions, all flat); `RefreshZone` 0x403ae0's
  6-byte deficit is localised to two `movsx` at the loop bottoms but not solved (the increment
  spelling is inert, and it is NOT a call-argument promotion — `short destX` is confirmed by all
  21 call sites).

### ⏮ v118 (2026-09-04) — 255 → 257 exact, REAL MATCHES; +2/−0 (condensed; demoted at v119)

Two new standing lessons, one new dial (#50), two new tools, and the closure of a decl axis
no tool in the project could reach. All oracles green at 257.

- **`AddHealth` 0x427690 byte-EXACT (49 B → 0, length 517 → 520 = the extent).** Found by
  `residuals.py --lenmis`. The whole residual was ONE missing `mov ecx,[edi+0x44]`: the original
  RELOADS the `pWorld` member between two `= 1` stores because the first store may alias it,
  while our cached `CDeskcppDoc *pW = pWorld;` alias could not be invalidated. Removing the alias
  is only half of it — the death tail's inner decl order `pTile,bFound,i` is load-bearing.
  **+2/−0: `DetonateAdjacentTiles` 0x428680 came back for free**, one of the three functions the
  v114 re-baseline cost. (0x41f830 and 0x423d20 are still out.) ⇒ standing lesson #50.
- **`BlitViewportDither` 0x428e30: 125 B → 55**, length 237 → 238 against an extent of 242,
  purely by declaring the inner `prod` before `x`.
- **`UseWeapon` 0x427d20: 1536 B → 1068, LENGTH 2383 → 2390 = the extent.** `pOldPal` belongs
  LAST in the head block; `nAX` before `nAY` in the step-3 block stacks on top. This REFUTED two
  claims in the function's own v15 note ("the rest of the head is order-insensitive"; "proven NOT
  steerable by decl order/placement, 9 probes") — both true only of the 3 decls those probes covered.
- **New tools:** `declorder.py --inner` (permutes EVERY brace-block's decl run, not just the
  leading function-scope one — the axis `hoisttest.py` structurally cannot reach; it also SKIPS
  permutations that move a decl ahead of one its initializer needs, strips MEMBER names when
  deriving those dependencies, and SELF-CHECKS that the source's own order is legal) and
  `tools/aliasscan.py` (READ-ONLY target list for lesson #50; 61 hits, 6 also length-mismatched).
- **Measured negatives — do NOT re-tread:** `TextDialog::Position` 0x417570 is NOT the alias dial
  (7 variants dead flat at 100 B — its `pW` uses are pure READS, so no store invalidates the CSE;
  this is the counter-example that bounds #50). The cheap band stays closed on the inner-block axis
  too (0x41c200, 0x403aa0 flat; 0x423d20 / 0x405330 / 0x423dc0 have no permutable inner block).
  Flat on `--inner`: 0x41a1c0, 0x408e70, 0x409c10, 0x40ec30, 0x40f060, 0x41d260. Below the landing
  bar: `BlitTile` 0x40a320 13 B → 12, `ZoneProvidesItem` 0x41c3b0 17 B → 15.
- **The lead v118 handed forward — `AddItemToInv` 0x428f50, 381 B → 141 on `nInv,i` but HELD BACK
  because its length moved 505 → 502, away from the extent of 506 — was correct to hold and was
  CRACKED at v119** (see lesson #51: the missing 4 bytes were the arm order, and the decl order was
  only one of four composed dials). Same for the three functions v118 flagged as "cuts diff but
  shortens the length away from the extent, same verdict: not yet" (0x41c580 / 0x41c730 / 0x41cf10)
  — all three moved at v119, by a constant-fold lever rather than the axis v118 guessed.


---

### ⏮ v119 PICKUP (demoted at v120) (2026-09-04 v119 — **257 → 258 exact, REAL MATCH; +1/−0**. One new
standing lesson (**#51, the COMPOSITE LEVER**) that overturns two of v116's three reverts and
both of v118's held-back leads, plus **1029 bytes of residual cut across four more functions,
four of which now sit at EXACTLY their Ghidra extent**.
All oracles green: 258 exact / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (known benign)
0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches / build-sdl links.
v118 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST:** standing lesson **#51 — a lever that measures WORSE alone is not refuted;
cross it with the ARM ORDER and re-measure the 4 cells**, and the bullet under it on the
zero-init constant fold. Both are cheap procedures, not insights you have to have.

**▶ WHAT LANDED** (3 commits; anchor re-measured with `exactset.py` + `comm` after every one,
and again after each source note, per lesson #23; all five oracles + build-sdl at the end).
1. **`ParsePuz2` 0x422fd0 byte-EXACT (165 B → 0, len 310 → 317 = the extent).** THE headline.
   Its own note had named both halves of the answer since G1 and still could not land it:
   arm-swap alone 94 B at len 318, `Add` alone 164 B at len 309, **together 0**.
2. **`AddItemToInv` 0x428f50: 381 B → 6, len 505 → 506 = the extent**, insns 161/161, reg_pen 0.
   FOUR composed dials — decl order `nInv,i`, the inner arm order, `Add`, and a
   `int nCur = GetSize()` named local before the scrollbar if (lesson #43). v118's held-back
   141 B variant was the first dial only.
3. **`WorldgenFillQuestItemSpot` 0x41c580 218 B → 11 (len = extent) and `WorldgenFillSpawn`
   0x41c730 227 B → 26 (len = extent)** — `int j = 0;` must come AFTER `paSpots.SetSize(0, -1);`
   or cl folds the zeroed register into the literal `0` argument. `WorldgenFillQuestItemSpot2Maybe`
   0x41cf10 carries the same form (295 → 292) on clone consistency + length, not on its own merit.

**▶ NEXT — concrete, in priority order.**
1. **⭐ SWEEP THE WHOLE `Add`/`SetAtGrow` SEAM AGAIN, CROSSED WITH THE ARM ORDER.** Lesson #48's
   seam (~80 `SetAtGrow` sites) was worked ONCE, in isolation, and that is exactly the mistake
   #51 identifies — two of v116's three reverts were recoverable. 4 compiles per function.
   Start with the third v116 revert, **`Generate`** (6 B out of 5710), and with every non-exact
   function that has BOTH a `SetAtGrow(X.GetSize(), …)` site and an `armscan.py` hit.
   ⚠ `Add` is INERT on `CWordArray` value arrays (measured) — aim it at CObArray members only.
2. **⭐ `WorldgenFillQuestItemSpot2Maybe` 0x41cf10 (292 B) — the lead is already diagnosed** in
   its source note: we RELOAD the `itemId` parameter inside the genCandidateA scan loop
   (`mov ax,[ebp+0x14]` at +0x5f) where the original hoists it above the loop. Stage it in a
   named local (lesson #43) and re-measure before touching any dial.
3. **Grep for the zero-init constant fold** (the second v119 bullet): a `= 0` local declared
   just before a call taking a small literal, in a function that is 1–2 bytes SHORT. It is worth
   200 B a time and `residuals.py --lenmis` ranks the candidates for free.
4. **FINISH THE INNER-BLOCK SEAM** (v118's item, still open): 45 non-exact functions / 521 legal
   permutations, ~19 swept. Run per TU with `run_in_background` to a log, never concurrently
   with another sweep. ⚠ Worldgen.cpp compiles are slow. Already swept flat or below the bar —
   do not re-tread: 0x4260e0, 0x421930, 0x424fc0, 0x423df0, 0x412250, 0x40a710, 0x40f4b0,
   0x4270f0, 0x41a1c0, 0x408e70, 0x409c10, 0x40ec30, 0x40f060, 0x41d260, 0x41c200, 0x403aa0,
   0x41d480, 0x41c490, 0x40a320, 0x41c3b0, and (v119) **0x428f50 and 0x41c580**.
5. **Keep working `residuals.py --lenmis` top-down.** Unworked, negative first:
   `ScrollZoneTransition` 0x411180 (−62), `Layout` 0x4176f0 (−35), `DrawHealthNeedle` 0x4278a0
   (−17), `DrawHealthDial` 0x427490 (−16), `OnUpdate` 0x408e70 (−11), `WorldgenPlacePuzzles`
   0x421930 (−11); then positive: `ShowWinMessage` 0x40f4b0 (+36), `IactProbeMove` 0x406550
   (+26), `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 (+13). **OFF this list now:** 0x428f50,
   0x422fd0, 0x41c580, 0x41c730. ⚠ hand-disassemble two hits before investing (v117's own first
   census ranked 3 artifacts).
6. **`aliasscan.py` ∩ `--lenmis`** (lesson #50's strong form) is still unworked apart from
   AddHealth: best is **`OnNewDocument` 0x41bb10 (−29)**. Read-only recon done this session — the
   original caches `&pCanvas` in a frame slot ([ebp-0x24]) and RELOADS `pCanvas` through it three
   times, and it stores the Canvas ctor's return DIRECTLY (`mov [ecx],eax`) rather than via a
   `pNew` temp, which our `Canvas *pNew = NULL; TRY {...} pCanvas = pNew;` cannot produce.
7. **`dtorscan.py`'s remaining live hit**: `Load` 0x422670 (1634 B). (ParsePuz2 is now closed.)
8. **⛔ Do NOT run another per-function decl sweep on the cheap band** (< ~25 B) — that verdict
   now rests on ~20 functions across v111–v119, inner blocks included. Lesson #51 does NOT
   reopen it: both v119 wins were on LARGE residuals with a wrong LENGTH.
9. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note). **Phase-H goals 2-5 untouched** this session.

**▶ MEASURED NEGATIVES this session — do NOT re-tread.**
- **`AddItemToInv`'s scrollbar if/else arm order is INERT** (56 B either way, measured TWICE
  before `nCur` landed) — and then costs 56 AFTER it, so the `> 7`-first order is confirmed.
  `< 8` and `>= 8` each cost 1 B, pinning the literal 7; the member `SetScrollRange` form is inert.
- **A hand-written `{ int nIdx = GetSize(); SetAtGrow(nIdx, pNew); }` is NOT MFC's `Add`** —
  189 B at len 491 on AddItemToInv vs `Add`'s exact-length result. The evidence is for the
  library inline, not for the idea of naming the index.
- **`Add` is inert on `CWordArray`** (0x41c580 / 0x41c730, identical both ways).
- **0x41c580 is closed on the decl axis**: all 6 leading-decl permutations are worse, and the
  {i,nCount} × {nObjs,j} product reaches 9 B via `nCount,i` — 2 bytes of tuning on an
  already-exact length, deliberately NOT landed under lesson #48's bar.
- **`AddItemToInv`'s last 6 B are TU-joint phase**, not a body defect: one instruction's schedule
  slot, flat across the arm's whole spelling space and the inner-decl axis, at matching length,
  matching save set, reg_pen 0 (the lesson-#44 signature).

**▶ HOW TO WORK THE DIAL SAFELY (v104–v118 rules all stand and were all re-used).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps concurrently,
or one while `progress.py`/`exactset.py`/`residuals.py`/`jointdecl.py`/`formsweep.py`/`armscan.py`/
`dtorscan.py`/`declorder.py` is in flight (they share `build/*.obj`). Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone. A comment rewrite IS a line-count
change (lesson #23) — **re-measure AFTER writing the note**; v119 did this for all five notes and
each was clean.
⭐ **v119 additions:**
- **Do not judge a lever by its own row.** Before recording a conversion as refuted, cross it
  with the arm order (4 cells). The length rule applies to the COMBINATION.
- **Decompose the LENGTH deficit instruction by instruction before choosing a dial.** On both
  v119 wins the byte count added up EXACTLY (ParsePuz2: +4 +4 +2 −2 −1 = 7), which is what turned
  "some register thing" into a specific, falsifiable source hypothesis.
- ⚠ **In this environment wall-clock only advances while a command is actually running.** Polling
  a background job in a tight loop of quick calls makes a live sweep look hung. Either run the
  measurement in the FOREGROUND with a long `timeout`, or block on a real `until … sleep` wait.


---

### ⏮ v120 PICKUP (demoted at v121)

### ⏭ NEXT SESSION PICKUP (2026-09-04 v120 — **258 → 255 exact, a DELIBERATE, USER-APPROVED
re-baseline DOWN for a real ARITY BUG** (see the ⛔ block near the top of this file — it is
the SECOND deliberate drop in the project's history and must not be reverted). No new
byte-match, but the session was the most productive in a while by every other measure:
**1050 bytes of residual cut across three functions, two more LENGTHS landed exactly on their
Ghidra extents, one genuine transcription bug found and fixed, and TWO new oracles built**
(`aritycheck.py`, `epiloguescan.py`).
All oracles green: 255 exact / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (known benign)
0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches / **arity 0 mismatches** /
build-sdl links. v119 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST — the one method that produced everything this session.**
⭐ **DECOMPOSE THE LENGTH DEFICIT INSTRUCTION BY INSTRUCTION, THEN NAME THE SOURCE CONSTRUCT
THAT PRODUCES THE MISSING BYTES.** Lesson #49 said "check the length first"; v120 is what
happens when you then *account for it exactly* before touching a dial. Three functions fell
to the same three steps — (1) `residuals.py --lenmis` gives a signed delta; (2) find the
instructions that make up that delta in the two disassemblies; (3) ask what C construct emits
them. It worked on 148 B, 98 B and 100 B residuals in one session:
- **`PreCreateWindow` 0x419210, 148 B → 4, length onto the extent.** +4 = the original's
  `mov [esp+0x24],eax` / `mov eax,[esp+0x20]` pair (8 B) vs our `mov edi,eax` / `mov eax,edi`
  (4 B). The spill was the SYMPTOM: the original register-homes the window WIDTH in a named
  local (`lea edi,[eax*2+0x20d]` instead of our `add eax,eax; add eax,0x20d` straight into the
  `rc.right` slot), and that extra long-lived value is what pushes `bRet` out of EDI.
- **`PlaySound` 0x409060, 98 B → 6, length onto the extent.** −5 = a DUPLICATED EPILOGUE (7 B)
  minus our 2-byte `jmp`. See the new lesson bullet below.
- **`TextDialog::Position` 0x417570, 100 B → 62.** Its −3 decomposed into a missing `push 0`
  (the Layout arity bug) and a member load the original makes TWICE.
⚠ **All three had park notes that read the residual BACKWARDS** — "allocator/scheduling
tie-break", "not source-steerable, G1 fodder", "cmp-direction flips the C source can't steer".
Every one described a register or jcc symptom while the LENGTH was sitting there saying
"structural". ⇒ **treat a park note that names a register permutation as UNREAD if the length
is wrong.** That is now three sessions running (v117's ZoneProvidesItem, v118's AddHealth, and
these three).

**▶ NEW STANDING LESSONS (fold into the numbered list when convenient).**
1. ⭐ **A `jle K` against our `jl K+1` is NOT a codegen tie-break — cl 10.20 encodes the
   comparison CONSTANT exactly as written, so it is the SOURCE that says `<= 0x11c` where we
   said `< 0x11d`.** Read it straight off the byte diff: the constant differs by one right
   next to the jcc. Four such boundaries were corrected in Position for 7 B. This retires a
   whole family that has been dismissed as "lesson #6, not steerable" for years.
2. ⭐ **THE DUPLICATED EPILOGUE.** cl lays the *then* arm out as the fallthrough, so a bare
   `return;` in that arm gets a LOCAL `pop.../add esp,N/ret N` instead of a branch to the
   shared epilogue. If the author wrote the arms in the other order, the original carries ~7
   extra bytes exactly where we emit a 2-byte `jmp`. **`tools/epiloguescan.py`** is the target
   list — and it is already MINED OUT (PlaySound was the only instance; 0 hits over 21
   length-short residuals now).
3. ⭐ **AN `armscan.py` HIT TAGGED "one-armed" CAN STILL BE AN ARM-ORDER DEFECT ONE LEVEL
   OUT.** PlaySound was on armscan's list the whole time; the tag describes the INNER
   `if (...) return;`, which is precisely why nobody ever tried swapping the OUTER arms.
4. ⚠ **A COMPOSITE IS NOT GUARANTEED TO INHERIT ITS HALVES' GAINS** — the converse of lesson
   #51. On `PlacePuzzle` 0x421620 the arm swap alone gives 35 B → 32 and `Add` alone 35 → 29,
   but TOGETHER they give **49**. #51 says "a lever that measures worse alone is not refuted";
   it does NOT say a composite is better. Measure the 4 cells.

**▶ WHAT LANDED** (5 commits; `exactset.py` + `comm` after every edit AND after every note
rewrite, per lesson #23; all oracles + build-sdl at the end).
1. **`PreCreateWindow` 0x419210: 148 B → 4**, length 180 → 184 = the extent, insns 63/63,
   reg_pen 0. +0/−0. Residual is one 2-instruction schedule swap.
2. **`PlaySound` 0x409060: 98 B → 6**, length 171 → 176 = the extent, insns 50/50. +0/−0.
   Residual is a pure EAX↔EDX scratch bijection (lesson #44).
3. **`TextDialog::Position` 0x417570: 100 B → 62** — four comparison constants, one arm swap
   worth 39 B, and `if (ay == 0) ay += 0x22;` (the original emits `add ebx,0x22`; `ay = 0x22`
   compiles to the 5-byte `mov`). The .cpp-local part measured +0/−0.
4. **⭐ `TextDialog::Layout` 0x4176f0 ARITY: three int params, not two** — the re-baseline.
   Proven twice over (`ret 0xc`; the call site pushes `push 0; push ebx; push esi`); the third
   arg is never read. USER-APPROVED. ⚠ the parameter NAME is not the dial input — an unnamed
   `int` measures the identical −4/+1.
5. **Two new tools**, both positive-controlled in both directions.

**▶ NEXT — concrete, in priority order.**
1. **⭐ KEEP RUNNING THE v120 METHOD DOWN `residuals.py --lenmis`.** It is 3-for-3. Unworked,
   biggest signal first: **`ScrollZoneTransition` 0x411180 (−62, 703 B)** — recon done, the
   original SPILLS `this` to a frame slot ([esp+0x10]) and reloads it ~8 times where we keep it
   in ESI, and spills the constant 0x10 too, i.e. it has one more long-lived value than we do
   (the `PreCreateWindow` shape exactly — look for a named local we inlined). Then
   **`Layout` 0x4176f0 (−35, 999 B)**, **`OnNewDocument` 0x41bb10 (−29)**,
   **`CDeskcppView::CDeskcppView` 0x408710 (−18)**, `DrawHealthNeedle` 0x4278a0 (−17),
   `DrawHealthDial` 0x427490 (−16), `OnMouseMove` 0x413580 (−13), `OnUpdate` 0x408e70 (−11).
2. **Try to recover the three the re-baseline cost** — `CyclePalette` 0x415af0,
   `ZoneHasIzxItemMaybe` 0x41bfa0, `ParseZax2` 0x423210, `DetonateAdjacentTiles` 0x428680
   (0x423d20 was gained). ⚠ Note the 0x41bfa0/0x423210/0x428680/0x423d20 cluster flipped on
   EVERY Worldgen-visible perturbation this session, so it is phase, not body defects.
3. **Re-run `aritycheck.py` on newly-transcribed functions** — 96 of 359 markers are still
   "unreadable" (no terminal ret: EH funclets, tail `jmp`s). Widening that coverage is cheap
   and the payoff is proven.
4. **⛔ CLOSED THIS SESSION — do not re-tread.** (a) Lesson #48's CObArray `SetAtGrow` seam is
   now swept on EVERY remaining non-exact site: `ReadSavedState` 0x405bd0 (12 → 19) and
   `WorldgenAddZoneEntry` 0x41d800 (27 → 28) have their current spelling POSITIVELY CONFIRMED,
   `OnLoadWorld` 0x424fc0 is inert, `PlacePuzzle` 0x421620 is a net loss (see lesson 4 above).
   (b) The `ReadZax2`/`ReadZax3`/`ReadZaux` `mov ax`/`movsx` idiom (4 sites, worth +3 exact) is
   re-swept on four axes v99/v109 never touched — container call form, decl SET (a 6th `short`
   at all 6 positions), forced truncation (10 spellings), and decl TYPE. All flat. `short i` is
   the informative negative: it buys the 16-bit load but keeps the counter 16-bit. ~45
   spellings are now spent; do not re-open without a new mechanism. (c) `0x41cf10`'s decl axis
   (`declorder.py --inner`, 9 legal permutations).
5. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note). **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v119 rules all stand and were all re-used).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps concurrently,
or one while `progress.py`/`exactset.py`/`residuals.py`/`jointdecl.py`/`formsweep.py`/`armscan.py`/
`dtorscan.py`/`declorder.py`/`aritycheck.py`/`epiloguescan.py` is in flight (they share
`build/*.obj`). Measure with `tools/exactset.py` + `comm`, never progress.py's total alone. A
comment rewrite IS a line-count change (lesson #23) — **re-measure AFTER writing the note**.
⭐ **v120 additions:**
- ⚠ **`--expect-exact` on `formsweep.py`/`jointdecl.py` is PER-TU, not project-wide** (Worldgen.cpp
  is 44, not 255). The tool hard-fails loudly, so this costs one wasted run, not a wrong result.
- ⚠ **A vartest/declorder run RESTORES the file to whatever it read at START**, so if you applied
  an edit by hand first, "restored" means "back to your edited state", not to HEAD. Two `assert
  s.count(old)==1` failures this session came from exactly that. Always `git diff --stat src/`
  before assuming.
- ⚠ **An edit to a HEADER is not a per-TU change.** The Layout arity fix moved four functions
  across two TUs. Header edits need a full `exactset.py` comparison, never a per-TU one.

---

### ⏮ v121 (2026-09-05) — 255 → 256 exact, a REAL MATCH (condensed from CLAUDE.md's pickup)

**Landed:** `CMainFrame::OnPaletteChanged` 0x4193f0 **54 B → 0** (105 B = its extent) and its
twin `OnPaletteIsChanging` 0x419460 **54 B → 1** with its LENGTH onto the extent (112/112), both
via the new **CROSS-JUMPED IF/ELSE lever, lesson #52** (standing bullet in CLAUDE.md). Both had
been parked since G1 as a "cmp-direction/sbb-vs-branch instruction-selection tie-break" — a note
naming a SYMPTOM. `OnPaletteIsChanging`'s last byte is the compare's ENCODING DIRECTION (orig
`39 /r`, ours `3b /r`); 8 condition spellings measure 1 B, and the byte-EXACT twin uses `3b`, so
the two 1997 sources really do differ there.

**Also delivered v121:** `tools/vartest.py`'s VACUOUS `origlen` column (it echoed our own length,
the same bug v117 fixed in residuals.py) replaced with `ext=<extent> <signed delta>` from
app_funcs.txt.

**Two measured NEGATIVES (also written into the source notes):**
- ⛔ `ScrollZoneTransition` 0x411180 (−62, 702 B) is a TU-JOINT-PHASE park, not a decl problem.
  The −62 was decomposed instruction by instruction and is entirely the original's `this`/`n2`
  spill (36 B prologue, ~10-12 B per arm's BitBlt segment, 10 B epilogue). Swept flat: the
  arm-local coordinate hypothesis (6 spellings, 705-708 B) and the whole decl dial (23
  configurations → 702 B at length 851, dead flat). Lesson #44's signature.
- ⛔ `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 (+13, 117 B): 8 declaration placements for
  `nOk`/`item1` flat at 117 B (one is 367). Its existing note was right — the this=EDI-vs-ESI
  callee-save cascade, joint-pass territory.

**Harness trap caught before it cost anything:** "non-exact functions where OURS has more
`sbb`/`setcc` than the ORIGINAL" fabricated 8 targets, because it decoded the original from a
buffer sliced to OUR length and masked at OUR reloc offsets, desyncing the stream (orig column
read 0 for functions that visibly have 32). Rule: scan the ORIGINAL from raw bytes at its own
extent, never through the candidate's mask.

**Oracles at v121:** 256 exact / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (known benign) 0 SHIFT
/ vt 10 CLEAN / msg 11 CLEAN / arity 0 mismatches / savescan 0 mismatches / build-sdl links.


---

## ⏮ v122 PICKUP (demoted at v123)

### v122 pickup, as written (2026-09-05 v122 — **held at 256 exact; no new byte-match, but
one PARKED FUNCTION'S LENGTH LANDED EXACTLY ON ITS EXTENT.** `CDeskcppDoc::OnNewDocument`
0x41bb10 went **537 B @ −29 → 422 B @ ±0 (975/975)** on a RECOVERED MISSING SOURCE
CONSTRUCT — the house `CATCH_ALL` + `THROW_LAST()` around the Canvas allocation — found by
the new **lesson #53** method (read the construct out of your own byte-exact code). Two
large, well-bounded NEGATIVES recorded so nobody re-treads them, and one new mechanism
named. All oracles green: **256 exact** / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (known
benign) 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / arity 0 mismatches / build-sdl links.
v121 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST — the pattern is now FIVE sessions deep and it is the most reliable thing
here.** v117 ZoneProvidesItem, v118 AddHealth, v120's three, v121's two, and now v122's
OnNewDocument: **every time a park note named an "instruction-selection tie-break" or a
"register permutation" while `residuals.py --lenmis` said the LENGTH was wrong, the real
defect was a source-level construct.** Treat such a note as UNREAD. Run `--lenmis` first,
decompose the length delta instruction by instruction, then name the C construct.

**▶ WHAT LANDED** (2 commits; `exactset.py` + `comm` after the edit AND after every note).
1. **`OnNewDocument` 0x41bb10: 537 B @ −29 → 422 B @ ±0**, length exactly on the extent.
   The `TRY { } END_TRY` around `new Canvas` is really the house hand-expanded CATCH_ALL:
   `catch (CException *e) { _afxExceptionLink.m_pException = e; THROW_LAST();
   AfxMessageBox(IDS_ERR_UNRECOVERABLE,0,-1); AfxAbort(); }`. Three shapes refuted and
   recorded in the source note (direct assignment inside the TRY: 523 @ −6; `= NULL` on the
   temp — it is what made our frame 4 B bigger; the catch without THROW_LAST: 518 @ −20).
   ⚠ this is a FIDELITY fix, not a dial: the original rethrows on an OOM Canvas allocation
   and we were swallowing it.
2. **Notes only, no count change:** the self-movsx cluster's mechanism + DrawHealthDial's
   proven cause (both below).

**▶ TWO MEASURED NEGATIVES — do not re-tread (both written into the source notes).**
1. ⛔ **The SELF-MOVSX cluster** — `DrawLocatorMap` 0x423df0 (−6), `RefreshZone` 0x403ae0
   (−6), `ZoneTransitionStep` 0x409650 — is **5 sites in 3 functions, project-wide** (scan
   of all 410 extents for `movsx r32,r16` on the same register followed by a 16-bit add of
   it), and none is exact. ⭐ The MECHANISM is named now: a self-extension is cl 10.20
   MAINTAINING a 32-bit incarnation of a `short` in the same register — proven by
   ZoneTransitionStep's `lea ecx,[ebx+edi]` three instructions earlier, and by the POSITIVE
   CONTROL that byte-exact `Canvas::BlitFast` 0x408110 carries `movsx edx,dx` from
   `height = canvasH - destY;` + `int rows = height;`. **16 spellings refuted** on
   DrawLocatorMap (int copies in seven placements — they movsx into a SCRATCH register and
   never coalesce; `register`; long/unsigned increments; for-loop form; increment order).
   Decl SCOPE cuts its diff 96 → 88 dead-flat across four configurations but NEVER moves the
   length off 336 — recorded, not landed.
2. ⛔ **`DrawHealthDial` 0x427490 (−16): the cause is COORD MEMORY-RESIDENCY and nothing
   else.** Forcing the four coords into memory via the rect's address (`CRect rc = ...;
   rc.InflateRect(2,2);`) collapses reg_pen 35 → **3** and identity_miss 54 → **3** — every
   register role snaps to the original's. That is a PROBE, not the answer (those are real API
   calls the original does not make; it emits inline `sub eax,2`/`add eax,2`). 13 spellings
   refuted: the whole DECL axis is flat at 346; a plain `RECT rc;` is **SCALARISED** by cl
   when its address never escapes; `int c[4]` likewise; `RECT rc = <member>;` + member
   adjustment hits len 509 == the extent EXACTLY and 301 B but emits a BLOCK COPY where the
   original fuses load/adjust/store per field — rejected as number-chasing under lesson #48.
   ⇒ The open question is narrow: **what 1997 spelling puts four ints in the FRAME with
   inline load/adjust/store?** Answering it also almost certainly lands the sibling
   `DrawHealthNeedle` 0x4278a0 (−17).

**▶ NEXT — concrete, in priority order.**
1. **⭐ KEEP RUNNING THE LENGTH-FIRST METHOD DOWN `residuals.py --lenmis`.** Unworked,
   biggest signal first: **`ShowWinMessage` 0x40f4b0 (+36, 1670 B)**, **`IactProbeMove`
   0x406550 (+26, 495 B)**, `Layout@TextDialog` 0x4176f0 (−35, 999 B — note this moved when
   v120 fixed its ARITY, so its old numbers are stale), `WorldgenPlacePuzzles` 0x421930
   (−11), `OnUpdate` 0x408e70 (−11), `UpdateDragCursor` 0x412cc0 (+9), `PlaceZone` 0x4260e0
   (−7), `ReadZaux` 0x406270 (−6, only 111 B of diff).
2. **The `DrawHealthDial`/`DrawHealthNeedle` memory-residency question above** — it is the
   most sharply-posed open item on the list, worth 33 bytes across two siblings, and
   everything except that one construct is already solved on the dial.
3. **Near-misses worth one pass each** (length off by ONE, small diff): `ParseZax2` 0x423210
   (+1, 78 B — also a v120 re-baseline casualty), `HitEntityAt` 0x4059d0 (+1, 206 B),
   `TransitionZoneXWing` 0x40e7c0 (−1, 167 B), `WorldgenPlaceItemOnLock` 0x41cdc0 (−1),
   `OnDraw` 0x409110 (−1).
4. **Try to recover the three the v120 re-baseline cost** — `CyclePalette` 0x415af0,
   `ZoneHasIzxItemMaybe` 0x41bfa0, `ParseZax2` 0x423210, `DetonateAdjacentTiles` 0x428680.
   ⚠ that cluster flips on EVERY Worldgen-visible perturbation, so it is phase, not body.
5. **Re-run `aritycheck.py` on newly-transcribed functions** — 96 of 359 markers are still
   "unreadable" (no terminal ret). Widening that coverage is cheap and the payoff is proven.
6. **⛔ CLOSED — do not re-tread.** (a) The `push 0xe01e` catch-funclet census: **17 sites,
   OnNewDocument was the ONLY omission** — mined out. (b) The self-movsx cluster's 16
   spellings and DrawHealthDial's 13 (above). (c) The lesson-#52 diamond census (6
   project-wide, 5 accounted for; only 0x413df0 unworked, inside an 84 %-differing
   function). (d) The "ours has more `sbb` than the original" scan — a HARNESS TRAP, see the
   ⚠ in lesson #52. (e) ScrollZoneTransition's decl + arm-local axes, and 0x41d0c0's decl
   axis. (f) Everything v120 closed: the CObArray `SetAtGrow` seam on the remaining non-exact
   sites, the `ReadZax2/3/Zaux` `mov ax`/`movsx` idiom (~45 spellings), `0x41cf10`'s inner
   decl axis.
7. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note). **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v121 rules all stand and were all re-used).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run
long sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps
concurrently, or one while `progress.py`/`exactset.py`/`residuals.py`/`jointdecl.py`/
`formsweep.py`/`armscan.py`/`dtorscan.py`/`declorder.py`/`aritycheck.py`/`epiloguescan.py`
is in flight (they share `build/*.obj`). Measure with `tools/exactset.py` + `comm`, never
progress.py's total alone. A comment rewrite IS a line-count change (lesson #23) —
**re-measure AFTER writing the note** (done three times this session; all were line-safe).
⚠ **`--expect-exact` on `formsweep.py`/`jointdecl.py` is PER-TU, not project-wide.**
⚠ **A vartest/declorder run RESTORES the file to whatever it read at START** — if you
applied an edit by hand first, "restored" means back to YOUR edited state, not to HEAD.
⚠ **An edit to a HEADER is not a per-TU change** — it needs a full `exactset.py` compare.
⭐ **`vartest.py` prints the REAL extent** — `ext=<extent> <signed delta>`. Read the delta on
every row: it refutes a variant before you look at a single register, and it is what told us
`OnNewDocument` was solved (the row that hit `+0`). ⚠ **`jointdecl.py` still carries the same
vacuous `orig_len`** — a cheap, worthwhile chore for the next session.
⭐ **A scratch compile is a legitimate instrument** (v122, new): `toolchain/bin/cl` on a tiny
throwaway .cpp in the scratchpad, with several candidate loop/decl shapes as separate
functions, answers "does cl 10.20 emit X for spelling Y?" in ONE compile and never touches
`src/` or `build/*.obj`. That is how the int-copy hypothesis for the self-movsx cluster was
killed before spending a vartest sweep on it.

---

### ⏮ v123 PICKUP (2026-09-06 — demoted at v124)

**Held at 256 exact; no new byte-match.** The session's result was a NEGATIVE with high
leverage: the six closest-to-exact functions in the project (11 bytes total, including the
only DIFF(1) function) were PROVEN unreachable from the source and CLOSED — **lesson #54, the
compare-encoding peephole** (now a standing bullet in CLAUDE.md; v124 added a 7th function and
a third guise). No code changed; every edit was a source note. All oracles green.

- ⛔ **Lesson #54.** Proven POSITIONAL (reordering the three textually-identical
  `LoadStoryHistory*` clones moves the `39/jg` form with the 3rd SLOT in all 4 permutations;
  injected decoy clones make it vanish at 6+), proven NOT-the-condition (positive control on
  the byte-exact twin `OnPaletteChanged` 0x4193f0 stays exact under every operand order), and
  moved ONLY by the ❌ forbidden file-scope symbol-count dial. Closed `OnPaletteIsChanging`
  0x419460 (1 B), `LoadStoryHistoryNevada` 0x401ac0, `SaveStoryHistory{Nevada,Alaska,Oregon}`
  and `FindObjectAt` 0x405330.
- **`FindObjectAt` 0x405330** — the early-return shape is REFUTED BY LENGTH (72 B against the
  extent's 79, all three spellings), which positively CONFIRMS the `result` + `break` form.
- **`IactProbeMove` 0x406550: the +26 decomposed exactly** — one 12-byte frame in both images
  holding {savedY, savedX, this, ONE int}, and the two pick a different int: `found` in memory
  costs +33, `r` in a register saves -14, +2 for two `cmp [bForce],0` where the original folds
  the live zero as `cmp [bForce],ebp`, -6 for our inline `return 1` epilogue. Decl SET+ORDER
  (10 configs) and statement order around `r` (6 configs) both CLOSED.
- **`DrawHealthDial` 0x427490: the -16 decomposed exactly, and the question re-framed.** Both
  frames are `sub esp,0x3c` with seven slots — neither image is short of stack. ⇒ "how do I
  force the coords into memory" was the wrong question; the question is why cl demotes `this`.
  **v124 answered both**: lesson #55 (the EH frame is the `this` dial) plus the `GetSysColor`
  import-address CSE in EBX, which is the third long-lived value that evicts the coords.
- ⭐ The v123 method note that paid off again at v124: **a THROWAWAY PROBE SCRIPT beats a
  general tool for a one-off question** — ~60 lines of scratch Python that edits the TU,
  compiles once, disassembles with capstone and prints ONE derived fact. `vartest.py` reports
  a byte COUNT; these questions were about WHICH byte.


---

### ⏮ v124 PICKUP (2026-09-07 v124 — **held at 256 exact; no new byte-match.
The session's product is a NEW STANDING RULE (lesson #55, the `this`-residency rule +
`tools/thisscan.py`) that answers the v123 pickup's #1 open item, plus a SEVENTH function
closed under lesson #54 and a statement-order axis positively CONFIRMED from the machine
code.** All oracles green: **256 exact** / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (known
benign) 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / arity 0 mismatches. Only NOTES + one new
READ-ONLY tool changed; no game code. v123 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST — two triage rules now, not one.** (1) v123's still stands: if a residual's
`kinds` are only `cmp-swap`/`jcc-mirror`, park it (lesson #54). (2) **New: run
`tools/thisscan.py` before reading ANY negative-length residual as a register mystery.** If
the original has an EH frame, it almost certainly SPILLS `this` and the missing bytes are its
reloads — that is a fact about `/GX`, not about your source.

**▶ WHAT LANDED** (notes + `tools/thisscan.py`; `progress.py` re-run after every edit).
1. ⭐ **LESSON #55 — the `this`-residency rule, a new standing bullet.** Over all 213
   byte-exact `__thiscall` functions: **no EH frame ⇒ ENREG (71/78), with the only 3
   exceptions being bodies that need 5+ long-lived values and all 3 sharing an IDENTICAL
   prologue shape, 0 counterexamples; EH frame ⇒ SPILL (68/75), even with registers to
   spare.** This answers v123's #1 item ("what makes cl keep `this` in a register?") for the
   common case and RE-FRAMES both target functions.
2. ⛔ **`GetFrameTile` 0x404850 CLOSED — lesson #54's third guise, the LEA SIB base/index
   swap.** Both images hold the same values in the same registers; only the commutative
   address's encoding differs. 8 spellings dead flat. ⇒ v123's pickup called this "the
   cheapest remaining real target"; it was wrong.
3. ⭐ **`DrawHealthDial` 0x427490 — the statement order is now PROVEN, and the mechanism
   named.** The EH STATE STORE `mov byte [ebp-4],3` sits immediately before the original's
   coord block, proving the coords come AFTER all four GDI ctors = our current order. The
   "coords-first" variant (len -16 -> -6, diff 346 -> 336) is NUMBER-CHASING and is refuted by
   that store. The real cause of the coords living in memory is a **CSE of the `GetSysColor`
   import address in EBX** (`mov ebx,[__imp__]` + four 2-byte `call ebx`) — a third long-lived
   value that leaves no register for the coords. Closed this session: the CDC::Chord member
   form (inert), 20 coord-position x object-order x decl-order cells.
4. **`ScrollZoneTransition` 0x411180 relocated into lesson #55**: it is the 4th and only
   unsolved member of the no-EH/saturated class, and **the other three are byte-exact in our
   tree** — so lesson #53's read-it-off-your-own-source method now applies to it.

**⛔ v125 CORRECTION TO THE ABOVE — item 3's MECHANISM is retracted (items 1, 2 and 4 stand).** v124's item 3
named "a CSE of the `GetSysColor` IMPORT ADDRESS in EBX" as the mechanism behind
`DrawHealthDial`'s -16 and asserted "we do not make that CSE"; the v124 NEXT list promoted
hunting the spelling that triggers it to the #1 item, "worth 3 functions". **We make the
identical CSE, in EDI.** Measured on both sides by `tools/impcse.py` (v125). The claim was
inferred from the byte diff and never checked against our own compiled output, and the v123
ledger in the same source note already recorded `ours edi=CSE then x1`. See lesson #56 in
CLAUDE.md. What survives from v124: lesson #55 itself, the `GetFrameTile` closure, and the
EH-state-store statement-order confirmation — all independent of the retracted claim.


---

## ⏮ v125 PICKUP (demoted at v126)

### ⏮ (was: NEXT SESSION PICKUP, 2026-09-05 v125 — **held at 256 exact; no new byte-match. The
session's product is a RETRACTION: v124's named mechanism for `DrawHealthDial` is FALSE, and
it had already become this pickup's predecessor's #1 priority. Plus the project's LAST 2-byte
residual is CLOSED, a new both-sides census tool, and a REAL target list replacing a refuted
one.** All oracles green: **256 exact** / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH (the
documented benign `StartGame` 0x4037a0 finding) 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN / arity
0 mismatches. Only NOTES + `tools/impcse.py` + one `residuals.py` classifier rule changed; no
game code. v124 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST — THREE triage rules now.** (1) v123's: if a residual's `kinds` are only
`cmp-swap`/`jcc-mirror`/`lea-sib-swap`/`operand-reassoc`, park it (lesson #54). (2) v124's:
run `tools/thisscan.py` before reading any NEGATIVE-length residual as a register mystery —
under an EH frame cl SPILLS `this` by default. (3) ⭐ **NEW (lesson #56): a park note saying
"the original does X and we don't" is a claim about BOTH binaries. Census our own compiled
output before believing it — a differing REGISTER is not a differing CONSTRUCT.**

**▶ WHAT LANDED** (notes + one new tool + one classifier rule; anchor re-measured after every
edit, still 256).
1. ⛔ **RETRACTED — the `GetSysColor` import-address CSE was the v124 pickup's #1 item and
   there is nothing there.** v124 said the original CSEs the import address in EBX "where we
   emit four 6-byte `call dword ptr [__imp__]`"; **we emit the identical construct in EDI**
   (`8b 3d <imp>` + four `ff d7`). Never measured on our side, and the same note's v123 ledger
   six lines above already said so. `DrawHealthNeedle` 0x4278a0 is the same story. ⇒ Both
   functions' residuals are ONE allocation decision (the original ranks `this` above every
   coord), i.e. purely a lesson-#55 question. See lesson #56.
2. ⭐ **NEW REAL TARGET LIST — `tools/impcse.py` finds the 5 functions where the CSE'd import
   set GENUINELY differs**, three of them with corroborating length deltas. Best first:
   **`UpdateDragCursor` 0x412cc0 (+9 — OURS CSEs `SetPixel` x1, the original does not)** and
   **`DrawWeaponBox` 0x428ac0 (+5 — the ORIGINAL CSEs `GetNearestPaletteIndex` x1, we do
   not)**; then `DrawWeaponIcon` 0x428c40, `OnTimer` 0x40d470 (ours CSEs `SetScrollRange` x3),
   `WorldSizeDlg::OnHScroll` 0x418560 (ours CSEs `GetScrollPos` x2). The sign of the length
   delta agrees with the direction of the extra/missing CSE in both of the top two, which is
   what makes them worth a sweep.
3. ⛔ **`DrawTextA` 0x40f060 CLOSED — lesson #54's fourth guise, OPERAND REASSOCIATION.**
   It was the project's last 2-byte residual. cl normalises `(A - i) != nScroll` into
   `(A - nScroll) != i` itself; 22 spellings across 5 axes are dead flat at len 877 = the
   extent. `residuals.py` now classifies the shape as `operand-reassoc` so the triage rule
   fires instead of `UNCLASSIFIED`. ⚠ no positive control exists (the shape occurs ONCE in the
   image), so this is a strong park, not a proof.
4. ⭐ **Lesson #55's exception set refined (read-only, already done).** Of the 7 EH-frame ENREG
   exceptions, **5 have a LOOP** — where `this` wins on loop-weighted use count and so says
   nothing about a loopless function. The two WITHOUT a loop are `OnNewWorld` 0x424450 (9 `this`
   uses) and `WorldgenPushZoneEntry` 0x41d6b0 (4 uses); **0x41d6b0 is the closest analogue
   `DrawHealthDial`'s original has** (EH frame, no loop, 4 uses, ENREG) and it is BYTE-EXACT,
   so lesson #53's dictionary method applies. ⚠ but both no-loop exceptions save only TWO
   registers where DrawHealthDial's original saves THREE — a hypothesis to measure, not a rule.

**▶ NEXT — concrete, in priority order.**
1. **⭐ The 5 `impcse.py` targets** (item 2 above). Start with 0x412cc0 (+9) and 0x428ac0 (+5):
   both have a length delta whose SIGN matches the extra/missing CSE, which is the lesson-#49
   corroboration the refuted GetSysColor lead never had.
2. **`DrawHealthDial` 0x427490 (-16) / `DrawHealthNeedle` 0x4278a0 (-17) — pursue via
   `WorldgenPushZoneEntry` 0x41d6b0**, not via the CSE. The question is narrow: what makes cl
   rank `this` above the coords under an EH frame with no loop?
3. **Keep running `residuals.py --lenmis`, skipping the four TIE kinds.** Unworked, biggest
   first: **`ShowWinMessage` 0x40f4b0 (+36, 1670 B)**, `Layout@TextDialog` 0x4176f0 (-35),
   **`IactProbeMove` 0x406550 (+26)** ⚠ (its decl + statement-order axes are already closed),
   `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 (+13, only 117 B of diff — cheap),
   `WorldgenPlacePuzzles` 0x421930 (-11), `OnUpdate` 0x408e70 (-11), `PlaceZone` 0x4260e0 (-7),
   `ReadZaux` 0x406270 (-6, only 111 B of diff), `RefreshZone` 0x403ae0 (-6, only 70 B).
4. **`ScrollZoneTransition` 0x411180 (-62)** — still the largest single structural residual;
   diff its register roles against the three byte-exact siblings sharing its prologue shape
   (`DrawEntities` 0x40b160, `SaveZoneRecursive` 0x4033b0, `LoadZoneRecursive` 0x403450).
   ⚠ decl axes are CLOSED here (v121, 23 configs).
5. **Near-misses worth one pass each** (length off by ONE, small diff): `ParseZax2` 0x423210
   (+1, 78 B), `HitEntityAt` 0x4059d0 (+1, 206 B), `TransitionZoneXWing` 0x40e7c0 (-1, 167 B),
   `WorldgenPlaceItemOnLock` 0x41cdc0 (-1), `OnDraw` 0x409110 (-1). ⚠ check `kinds` first.
6. **Try to recover the four the v120 re-baseline cost** — `CyclePalette` 0x415af0,
   `ZoneHasIzxItemMaybe` 0x41bfa0, `ParseZax2` 0x423210, `DetonateAdjacentTiles` 0x428680.
   ⚠ that cluster flips on EVERY Worldgen-visible perturbation, so it is phase, not body.
7. **Re-run `aritycheck.py` on newly-transcribed functions** — 96 of 359 markers are still
   "unreadable" (no terminal ret). Cheap, and the payoff is proven.
8. **⛔ CLOSED — do not re-tread.** (a) ⭐ **The `GetSysColor` import-address CSE hunt (v124's
   #1 item) — REFUTED, we already make it.** (b) `DrawTextA` 0x40f060 (22 spellings).
   (c) Lesson #54's census, now **8** functions: v123's six, `GetFrameTile` 0x404850, and
   `DrawTextA` 0x40f060. (d) `DrawHealthDial`'s coord POSITION (refuted by the EH state store),
   its Chord call form, and the 20-cell cross. (e) `ScrollZoneTransition`'s decl + arm-local
   axes (v121). (f) `IactProbeMove`'s decl and statement-order axes. (g) The `push 0xe01e`
   catch-funclet census (17 sites). (h) The lesson-#52 diamond census (6 project-wide, only
   0x413df0 unworked). (i) The "ours has more `sbb`" scan — a HARNESS TRAP. (j) Everything
   v120 closed.
9. **Still open from v98:** de-hex leftovers (`0x68`->PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note). **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v124 rules all stand and were all re-used).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run
long sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps
concurrently, or one while `progress.py`/`exactset.py`/`residuals.py`/`jointdecl.py`/
`formsweep.py`/`armscan.py`/`dtorscan.py`/`declorder.py`/`aritycheck.py`/`epiloguescan.py`/
`impcse.py` (no `--orig`) is in flight (they share `build/*.obj`). `thisscan.py` and
`impcse.py --orig` are READ-ONLY and safe during a sweep.
⚠ **v125 HIT THE CONCURRENCY TRAP AND IT COST A RUN:** a `vartest.py` sweep was launched while
a backgrounded `residuals.py` was still going. Both were salvageable only because vartest's
`--expect` guard reproduced the baseline and its `finally` restored the file. **Check that the
previous background job has actually EXITED — an empty output file means still running, not
finished.**
Measure with `tools/exactset.py` + `comm`, never progress.py's total alone. A comment rewrite
IS a line-count change (lesson #23) — **re-measure AFTER writing the note** (this session is
notes + one tool + one classifier rule; re-measured, still 256, all oracles re-run).
⚠ **A 2-minute foreground `vartest.py` WILL time out and leave the TU MUTATED.** Background it
from the start, and `git status` before doing anything else.
⚠ **`--expect-exact` on `formsweep.py`/`jointdecl.py` is PER-TU, not project-wide.**
⚠ **A vartest/declorder run RESTORES the file to whatever it read at START** — if you
applied an edit by hand first, "restored" means back to YOUR edited state, not to HEAD.
⚠ **An edit to a HEADER is not a per-TU change** — it needs a full `exactset.py` compare.
⭐ **`vartest.py` prints the REAL extent** — `ext=<extent> <signed delta>`. Read the delta on
every row; it refutes a variant before you look at a single register. ⚠ **`jointdecl.py`
still carries the same vacuous `orig_len`** — a cheap, worthwhile chore.
⭐ **A THROWAWAY PROBE SCRIPT beats a general tool for a one-off question (v123/v124/v125).**
v125's import-CSE census started as ~40 lines of scratch Python and only became
`tools/impcse.py` once it had overturned a published mechanism. ⚠ but give the throwaway a
positive control too — and note that v125's control (three byte-exact functions that MUST show
the construct on both sides) is what made the retraction trustworthy rather than just another
confident scan.



---

### ⏮ v126 PICKUP (2026-09-06 — 256 → 257 exact; demoted at v127)

+3 REAL byte-matches minus a 2-function user-approved phase trade. Landed: `OnDraw` 0x409110
386 B → 0 (lesson #52 call duplication, a TWO-push diamond v121's census could not see);
`OnUpdate` 0x408e70 283 B → 0 (lesson #47 arm order, then #52 — the original spells
`::ReleaseDC` twice); `CyclePalette` 0x415af0 and `DrawTextA` 0x40f060 fell out downstream
(the latter retiring v125's lesson-#54 park — it was PHASE, not the compiler bound).
`DrawWeaponBox` 0x428ac0 301 B → 16 with its length onto the extent, in three composing steps
(#42 member call form → #57/#39 statement order → #48 `tiles[i]`). New: lesson #57 (the PUSH
FORM, `tools/pushscan.py`), and v121's lesson-#52 census REOPENED 6 → 12 (`tools/xjumpscan.py`)
because it hard-coded a one-push arm. ⛔ The −2: landing OnUpdate rotated DeskcppView.cpp's
joint phase and cost `FindEntityAt` 0x40b210 and `StepDetonatorEffect` 0x40e400; all 6
legitimate exact spellings cost exactly 2, line-neutral padding does not help, and the only
zero-cost variant kept a dead `HWND hWnd;` declaration and was REJECTED as the v96 padding dial.
⇒ v127 recovered `FindEntityAt` and DECLINED `StepDetonatorEffect` — see lesson #58.

⏮ v127 pickup (demoted 2026-09-06 by v128) — **257 → 258 exact.** `FindEntityAt` 0x40b210
recovered BYTE-EXACT (16 B → 0 at len 85 = the extent) by re-permuting its four existing
declarations to `pZone/n/i/nCharId`; kept only because v126's rotation is the
better-supported phase, and the match is PHASE-BOUND (lesson #58 — v125's `DeskcppView.cpp`
compiles both it and `StepDetonatorEffect` byte-exact under the OTHER spellings).
`StepDetonatorEffect` 0x40e400's analogous refit was DECLINED (32 B → 24, no match, and its
x-first spelling is positively confirmed by v125's exactness). New oracle
`tools/widthscan.py` — the parameter-TYPE gap `aritycheck.py` is structurally blind to, 12
hits, positive-controlled; ⚠ its first draft repeated v126's xjumpscan mistake by requiring
an aligned instruction pair (1 hit instead of 12). Two residual families decomposed and
closed on every axis the project owns: `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 (+13,
all six decl configs flat) and the `Zone::ReadZa*` trio (9 B, one construct, one lesson-#53
dictionary entry). ⚠ **v128 RETRACTED v127's reading of the 0x41d0c0 hit** — it called the
widthscan finding a false lead because "the callee reads that slot as a DWORD at four
sites", but both reads are FORWARDING PUSHES, which are byte-identical for `int` and
`short` and discriminate nothing; the parameter really is a `short`, and fixing it plus the
nOk both-arms form took the function to 33 B at exactly its extent.

### ⏮ v128 PICKUP (2026-09-06) — condensed at v129

**258 → 257, a user-approved TRADE for a proven form.** `WorldgenPlaceItemForLockChainMaybe`
0x41d0c0: 117 B @ +13 → 33 B at its extent EXACTLY, on (a) `nOk` assigned in BOTH ARMS of an
if/else (lesson #59, the LIVE-RANGE dial) and (b) `WorldgenPlaceItemOnLock` 0x41cdc0's 3rd
parameter being `short`, not `int` (the forwarding-push retraction — a `mov eax,[esp+N]; push
eax` is byte-identical for both widths, so only a CONSUMING use types a parameter). (b) was
free; (a) cost `SetCurrentToIntroZone` 0x423d20, a phase weathervane re-swept flat at 5.
Shipped: `tools/framescan.py` (the LOCAL FRAME SIZE oracle, 15 hits, positive-controlled) and
`tools/sbs.py` (full side-by-side disassembly). Both standing lessons live in this file.
Measured and NOT landed: 0x41cf10's `int nSpots` (breaks two other functions' exact lengths).

### ⏮ v129 PICKUP (2026-09-06) — condensed at v130

Held at **257 exact**; no new byte-match. Headline: `LoadWorld` 0x421fd0 went **1047 B @ +6 →
485 B at LENGTH 1684 = the Ghidra extent EXACTLY, +0/−0 collateral**, on the house guarded
COUNTDOWN in its zone delete loop — a defect that had been sitting in the function's own park
note as a SYMPTOM ("delete-loop countdown (dec/jne) vs up-count+spill") attributed to "a
reg-pool cascade seeded by nRet". Found by `framescan.py` and `pushscan.py` AGREEING.

- ⭐ **`tools/loopform.py` GENERALISED** past the `for` spelling (new `DOWHILE_CMP` matcher +
  `our-dowc` column): **14 → 22 candidates**, five with no `for` loop at all. Its positive
  control had ROTTED ("0x403070 exact? expected False" — exact since v116) and was replaced
  with one that cannot rot. ⇒ lesson #40's v129 addition: **match the MECHANISM, not one
  instance of its output** (same family as v126's xjumpscan and v127's widthscan first drafts).
- ⭐ **The decompiled body can contain the countdown variable spelled out**: `OnLoadWorld`
  0x424fc0's questItems loops test `if (nCount - i == 1)`, and `<limit> - <index>` inside a
  loop body IS direct evidence the original counted down.
- **Three functions written up with measured NEGATIVES**: `OnLoadWorld` 0x424fc0,
  `ZoneTransitionStep` 0x409650 (its frame delta decomposed; the original spends its three
  callee-saved registers on {i, sy, &pWorld} and HOMES `pTile->pixels`), and `LoadWorld`'s own
  remaining 485 B.
- ⭐ **Triage rule 7 born here: NEVER hand-read a raw extent delta — always go through
  `residuals.py --lenmis`.** v129 hand-read `app_funcs.txt` for `WorldgenPlaceBlockades`
  0x41e350, got "+13 = structural!", and spent a target on it; the 13 bytes are the switch
  JUMP TABLE inside our COMDAT but outside the extent (the v117 confound `--lenmis` filters).

### ⏮ v130 (2026-09-06) — the SHORT->INT PROMOTION census; held at 257

- **`tools/movsxscan.py`** — every `movsx r32,<16-bit>` per function, ORIGINAL and OURS, split
  REG-form (source in a register) vs MEM-form (source in a frame slot), with the SELF subset
  (`movsx eax,ax`) reported separately and flagged as register-dependent. **132 CONFIRMED /
  23 ORIG-MORE / 13 OURS-MORE.** It independently re-derives v127's `ReadZa*` finding without
  being told about it.
- ⭐ **Triage rule 8 born here: A CENSUS KEYED ON A *REGISTER-DEPENDENT* PROPERTY IS NOT A
  STRUCTURAL CENSUS.** The tool's first draft counted only SELF-form promotions and confidently
  ranked `WorldgenFillQuestItemSpot2Maybe` 0x41cf10 the sharpest target in the tree; that site
  is orig `movsx eax,ax` vs ours `movsx esi,ax` — same construct, same 3 bytes, different
  register. Third guise of lesson #56, and the first where the TOOL'S KEY was the thing lying.
- **`RefreshZone` 0x403ae0** — 16 more spellings refuted across four axes (strength reduction,
  increment ORDER, int casts/aliases, `short t`), all written into its source note. `short t`
  moves the length -6 -> -3, i.e. it really does add one promotion, but on the wrong variable
  and the diff explodes 70 -> 227. The deficit is confirmed to be two promotions on the
  ACCUMULATORS and nothing else.
- **`OnLoadWorld` 0x424fc0** — the countdown pair reproduced exactly (2851 B at len 3602 =
  extent -4); recorded that at 79 % of bytes differing a 4-byte cumulative-offset decomposition
  is not tractable.
