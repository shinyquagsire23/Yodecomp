# Yodecomp — decompilation cheat sheet

In general, you can adhere to patterns found in `OpenJKDF2`, located at `~/workspace/OpenJKDF2`. CMake should be used, and the assumed build platform is macOS and Linux. Claude is permitted to modify this file with any useful notes that will aid other/later Claudes. Use `wine` to invoke Windows toolchains and executables.

### External references
- **`~/workspace/DesktopAdventures`** — the user's own engine recreation of the *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures). Invaluable for asset-format and game-logic semantics when naming functions/structs. Notably `scrdoc.txt` = reverse-engineered **script opcode format** (pre-script conditions like `BumpTile`, `CheckEndItem`, `EnemyDead`, `HasItem`, `HealthLs`…), plus `SCRIPTS.md`, `README.md`. Use it to name `.DTA`/zone/script parsing code. The 10×10 grid at World+0x4B4 (stride 0x34/zone) is the map's zone grid.
- `~/workspace/OpenJKDF2` — style/naming conventions (`Module_Function`, loose-Hungarian), CMake layout.

## Naming Convention and Decompiling Tips

In general, variable names should follow a loose-Hungarian Notation, where pointers start with `p` (ie, `pThing`), pointers to arrays are prefixed with `pa` (ie `paIndices`), booleans are prefixed with `b` (ie, 'Main_bMotsCompat'). Name a pointer after the **struct it points to**, not its MFC role. **⭐ v50: the doc and view classes were
RENAMED to their ORIGINAL names — `CDeskcppDoc` (was aliased `World`) and `CDeskcppView` (was `GameView`) —
known with certainty from the binary's `CRuntimeClass.m_lpszClassName` strings ("CDeskcppDoc"/"CDeskcppView";
the original MFC project was "Deskcpp" = Desktop Adventures). Source + Ghidra struct + Ghidra namespace all
renamed.** VARIABLE names keep the readable game-concept form (we don't know the original's variable names):
the `CDeskcppDoc` pointer is still **`pWorld`**, NOT `doc`/`pDoc`/`pCDeskcppDoc` (e.g. `CDeskcppView.pWorld@0x44`);
a `CDeskcppView` pointer is `pView`, etc. (So class = original `CDeskcppDoc`; pointer = readable `pWorld`.)

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

**WRITE GOTCHA (worse than the read one) — STILL LIVE as of 2026-07-08:** `program=` is honored only for
**reads**. **Mutations** (rename, set-comment, set-prototype, etc. — both `mcp__ghidra__*` and the HTTP
endpoints) always act on the **currently-active** program, and `switch_program` returns `success` but does
NOT persist. Passing `program=X` to a write is silently ignored → you will corrupt whatever is ACTIVE.
**To rename/annotate YodaDemo you must make YodaDemo.exe the ACTIVE program in the Ghidra GUI first**
(open/focus it in the CodeBrowser). Confirm with `list_open_programs` → `current_program` must read
`YodaDemo.exe` before any write. Then writes land correctly. ⚠ Other programs ARE open now (v50:
libKOTOR.so/KOTOR2sub/libkotor2.so — the user's KOTOR project), so a mis-routed write corrupts THEIR work.
- **⭐ v51 DIAGNOSIS — the PLUGIN is fixed, the BRIDGE is not.** Ghidra runs plugin **5.15.0** (verified:
  `curl localhost:8089/get_version`) from `feature/program-param-write-routing`. v51 tested post-restart:
  - **Direct HTTP with `program=` in the QUERY string ROUTES CORRECTLY** — `curl -s
    "localhost:8089/set_plate_comment?program=libkotor2.so" -X POST -d '{"address":"0x400e70","comment":"..."}'`
    lands on libkotor2.so and leaves the active YodaDemo untouched (proven: the call returned libkotor2.so's
    own "No function at 0x401000" and YodaDemo's OnTimer comment was unchanged).
  - **The `mcp__ghidra__*` tools STILL mis-route** — `mcp__ghidra__set_plate_comment(program="libkotor2.so",…)`
    hit the ACTIVE YodaDemo. The Python MCP BRIDGE sends `program` where the plugin's regular @McpTools don't
    read it (body vs the required QUERY), or the running bridge predates the fix. Restarting Ghidra did NOT fix
    the bridge (separate process).
  - **⭐ v51 ROOT CAUSE (the MCP server points at an OUTDATED bridge script):** `~/.claude.json` →
    `mcpServers.ghidra` runs `/opt/homebrew/bin/python3 /Users/maxamillion/Programs/bridge_mcp_ghidra.py` — the
    OLD monolithic bridge (1326 lines, Apr 10) whose `dispatch_post` sends EVERY POST param (incl. `program`)
    in the JSON BODY, with NO schema-driven query/body split. The plugin's regular `@McpTool`s read `program`
    only from the QUERY → never see it on writes → fall back to active program. The UPDATED package bridge
    `~/workspace/ghidra-mcp/python/bridge_mcp_ghidra/` FIXES this (registry.py:99 puts `source==query` params in
    the query; dispatch.py:182 POSTs them as query params). **FIX = repoint the MCP config** at the new bridge:
    `{"command":"/opt/homebrew/bin/python3","args":["-m","bridge_mcp_ghidra"],
    "env":{"PYTHONPATH":"/Users/maxamillion/workspace/ghidra-mcp/python"}}` (default stdio + 127.0.0.1:8089, same
    as now) — then RESTART Claude Code (MCP servers launch at startup). Verify deps (`mcp`, `requests`) import
    under that python3. AFTER repoint+restart, re-run the cross-program comment test; if it routes, relax these
    warnings.
  - **v51 APPLIED the fix (pending a Claude Code RESTART to take effect):** the old bridge needed `mcp>=1.28.1`
    but system python3 had `mcp 1.5.0`, so built a dedicated venv `~/workspace/ghidra-mcp/.venv`
    (`python3 -m venv` + `pip install -e ~/workspace/ghidra-mcp`, mcp 1.28.1) and repointed
    `~/.claude.json`→`mcpServers.ghidra` to `{"command":".../.venv/bin/python","args":["-m","bridge_mcp_ghidra"]}`
    (backup at `~/.claude.json.bak-v51`). Smoke-tested: the new bridge starts + registers tools. ⚠ It prefers
    UDS AUTO-DISCOVERY and connected to instance **"JK_re"**, NOT YodaDemo/8089 — after the CC restart, use
    `list_instances`/`connect_instance` to reach the YodaDemo instance if it didn't auto-pick it. THEN re-run
    the cross-program comment test; if writes route by `program=`, relax the WRITE GOTCHA warnings.
  - **⇒ SAFE-WRITE RULES until the bridge is repointed+restarted:** (a) write to a NON-active program via
    **direct HTTP `?program=<name>` in the query** (routes correctly); (b) for `mcp__ghidra__*` writes keep the
    TARGET program **ACTIVE** (`list_open_programs`→current) — they ignore `program=`.

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
  names unknown).**
  Full per-session milestone history in PLAN_COMPLETED.md.
  ~100 % = G2's byte-identical whole-image build. Track effective-match bytes separately (G, not %).

### 📋 SESSION PROTOCOL (follow this shape every session)
1. **Orient:** read the ⏭ NEXT SESSION PICKUP block below; `cd src/<TU> && rm -f ../../build/<TU>.obj &&
   ../../toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
   /Fo../../build/<TU>.obj <TU>.cpp` then `python3 tools/verify.py src/<TU>/<TU>.cpp` from repo root
   — confirm the recorded exact-count reproduces BEFORE changing anything (if not, a header drifted;
   bisect first). **⚠ OBJS LIVE IN `build/` (repo root), not next to the .cpp** (v32 hygiene change):
   all tooling (verify/match/progress/asmscore + link_exe.sh) reads/writes `build/<TU>.obj` via `/Fo`.
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

### ⏭ NEXT SESSION PICKUP (2026-07-08 v50 — PHASE G2: DYNCREATE verify + CLASS RENAME to CDeskcppDoc/CDeskcppView; 211 exact)
**▶ RECONFIRM STATE FIRST (fresh session):** `git log --oneline` tops out at the **v50** commit (class rename +
CRuntimeClass); below: bea006a v49 (msgcheck), b6bb67e v48, e961bf9 v47, ec4015c v46, 4f56b58 v45. Tree CLEAN
(USER gitignored YodaDemoCopy/ = their wine runtime copy — don't touch). ⭐ **v50 RENAMED the doc/view classes to
their ORIGINAL names: `World`→`CDeskcppDoc`, `GameView`→`CDeskcppView`** (in source AND Ghidra struct+namespace) —
known from the binary's CRuntimeClass strings. So the CLASS is now `CDeskcppDoc`/`CDeskcppView` everywhere;
VARIABLES stay `pWorld`/`pView` (readable). Baselines (compile EVERY TU into `build/` first — `cd src/<TU> &&
rm -f ../../build/<TU>.obj && ../../toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
/D _MBCS /Fo../../build/<TU>.obj <TU>.cpp`, or `tools/link_exe.sh`): `link_exe.sh` → **0/0/exit 0**;
`progress.py` → **211 exact / 99.17%** (⚠ was 212 through v44 — v45 traded ~World for the msgmap; v46-v50
neutral); `bugscan.py --all` → **0/0/0**; **`vtcheck.py` → 10 CLEAN**; **`msgcheck.py` → 11 CLEAN**. Per-TU exact:
AppData 14/14, App 11/12, Canvas 9/11, Dlg 5/5, Frame 14/18, GameData 13/27, GameView(now CDeskcppView TU) 73/124,
Iact 2/10, IactScript 11/12, Records 26/33, World 6/8, WorldDoc 7/13, Worldgen 34/91. ⚠ verify.py per-TU can
UNDERCOUNT ~10 (lesson #30): trust progress.py's 211. **G2: `g2_link.sh && g2_diff.py` → LAYOUT 39/378, CONTENT
225/378.** **REF-drop oracle (lesson #32): 5 dropped.** Ghidra: current=YodaDemo.exe, SAVED (struct+namespace
rename done). ⚠ BUDGET: Fable weekly reset 2026-07-09 23:00 America/Boise (main-thread).

**▶ v45–v50 RESULTS — ref graph 22→5 + ALL .rdata content validated + classes renamed to original:**
- **v45 World msgmap** (22→7; cost ~World 212→211). **v46 vtable overrides** (7→5). **v47/48 vtcheck** — 10
  vtables CLEAN. **v49 msgcheck** — 11 maps CLEAN (fixed CTheApp incomplete + GameView WM_HSCROLL→VSCROLL bug).
- **v50** — DYNCREATE CRuntimeClass sizes verified (World 0x33c0 / GameView 0x310 / CMainFrame 0xd8, no heap
  bug); classes RENAMED World→CDeskcppDoc, GameView→CDeskcppView (source: 323 tokenizer edits; Ghidra: struct +
  namespace) → DYNCREATE macro now emits correct .rdata strings. Codegen-neutral (211 held). USER-directed.

**▶ START HERE (v51) — content phase is DONE; the two live threads are both PAUSED pending EXTERNAL action (the
user is obtaining things). Don't manufacture busywork — check the two threads, else ask the user.**

**THREAD 1 — Ghidra write-routing: PLUGIN fixed, BRIDGE not (v51 diagnosed; see the WRITE GOTCHA block above).**
Ghidra runs plugin 5.15.0 (`feature/program-param-write-routing`). v51 proved: **direct HTTP with `?program=`
in the QUERY routes correctly** (write to a non-active program lands there, active untouched), but the
**`mcp__ghidra__*` bridge still mis-routes** to the active program (it doesn't put `program` in the query the
plugin reads). So until the BRIDGE is fixed/restarted: use direct HTTP `curl "localhost:8089/<ep>?program=<name>"`
for cross-program writes, OR keep the target program ACTIVE for mcp writes. NEXT: if the user updates/restarts
the MCP bridge, RE-RUN the cross-program comment test via the mcp tool; if it routes, relax the WRITE GOTCHA
block. ⚠ KOTOR programs share this Ghidra — a mis-routed mcp write corrupts THEIR work.

**THREAD 2 — the compiler hunt (PAUSED; waiting on a candidate build).** CORRECTION to the old "unobtainable cl"
framing: **we HAVE + use a genuine VC++ 4.2, cl `10.20.6166` (CL.EXE + C1XX.EXE + C2.EXE) at `toolchain/vc42/`**
— 211 exact proves it's the right major compiler. The ~48 reg-coloring residuals are most likely a slightly
DIFFERENT 4.2 sub-build (binary dated 1997-02-18, between VC 4.2/1996 and VC 5.0/1997-04-28; linker 3.10 = 4.2
family; base 4.2=`10.20.6166` ours, VC 4.2b=`10.20.6312` per KB Q164951). **INFRA READY (v50):**
`toolchain/bin/{cl,link,lib}` honor a `VCDIR` env override. When the USER supplies a candidate build (they're
hunting 4.2b / MSDN 1996-97 subscription refreshes on Internet Archive), drop it at `toolchain/vc42b/` and A/B:
`VCDIR=/abs/path python3 tools/progress.py` (exact **>211** = closer; if it's THE build, many reg-coloring funcs
flip at once). Fast probe: `VCDIR=<cand> python3 tools/asmscore.py src/Worldgen/Worldgen.cpp 0x428680`
(DetonateAdjacentTiles ESI↔EDI bellwether; reg_pen/identity_miss→0 = match). Full plan: **docs/compiler-hunt.md**.
If a better build lands, re-baseline everything (g2 absolute layout likely unblocks too — same wall). **Do NOT
grind reg-coloring funcs by hand (source-side exhausted v37-v40) — the compiler build is the only lever.**

**If both threads are still blocked and the user wants incremental work:** the content phase is genuinely
complete (211 exact + effective; runnable /OPT:REF image; ref-graph 22→5; ALL .rdata content — vtables,
msgmaps, DYNCREATE sizes+names — validated; classes named as original). Only low-value remains: the 5 hard-tail
REF-drops (each ≤2 funcs, risks a regression) or PE timestamp/checksum + EH-funclet cosmetics. Prefer asking the
user over manufacturing work.
- **PARKED / blocked (unchanged):** AppData CObject-trio -0x30 + WorldgenZoneEntry ??_G (self-correct).
  **Canvas-gap mini** BLOCKED on owning dialog class (msgmap 0x44b1d8, combo ctrl 0x9e). **De-dup step 6**
  (World, ~102 fields) — docs/dedup-plan.md.
- **Map non-exact:** `tools/survey.py` + `tools/frontier.py` (need build/*.obj) — remaining non-exact are
  lesson-#29 intrinsic/park class, NOT content-fixable and (when same-length) layout-neutral.


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


### Tooling (`tools/`, all Python; run from repo root)
- **`match.py <src.cpp> [--exe ...]`** — compile the `.cpp`'s object (must exist next to it), then for each
  `// FUNCTION: YODA 0xADDR` marker, best-fit the COMDAT function to its address and byte-compare with
  relocations masked. Reports MATCH / DIFF(n) per function. (Best-fit can mis-pair near-identical funcs —
  confirm with a direct disasm diff. It also exposes `coff_functions`/`trim_pad`/`mask` for reuse.)
- **`progress.py`** — completion dashboard: compiles every `src/**/*.cpp`, sums matched bytes ÷ 128158
  total app-region function bytes (534 funcs). One number to track progress.
- **`bytematch.py --va 0x.. --obj ..`** — single-function reloc-masked compare (the original harness).
- **`survey.py`** (v36) — ranks EVERY non-exact marker across all TUs by asmscore closeness (align→reg_pen
  →byte_diff). Top rows = nearest to exact; align=0 = pure reg-alloc/TU-phase, align>0 = insn-sel/sched.
  The G1 target picker. Needs build/*.obj (compile all first).
- **`frontier.py`** (v36) — per TU, the FIRST non-exact fn in .text/emission order (the joint-pass
  frontier). ⚠ v36 proved the "first is steerable" hope FAILS for the reg-tie-break class (header-phase,
  not body-steerable) — use it to MAP, not as a free-win list. Both reuse match+asmscore.
- **`bugscan.py [src/TU/TU.cpp ...] [--all]`** — ⭐ the **static #24/#25 correctness-bug oracle** (v35;
  the committed form of v34's job-tmp vtscan). Needs `build/<TU>.obj` built (compile every TU first, like
  progress.py). For each `// FUNCTION: YODA` marker it NW-aligns our stream vs the original (relocs masked,
  reuses asmscore._decode/_align) and flags aligned same-mnemonic/same-opkind pairs with matching base reg
  + index but DIFFERING memory-operand disp — the fingerprint of #24 (wrong `call [reg+disp]` vtable slot)
  and #25 (wrong inlined-accessor field, e.g. AfxGetInstanceHandle+0x8 vs AfxGetResourceHandle+0xc). Three
  buckets: **HIGH** (align==0 drift-free ⇒ real field/slot bug), **LOW** (drifted, same reg NAME may be a
  different pointer — manual review, `--all`), **FRAME** (ebp/esp stack = frame noise). **SYSTEMATIC**
  section groups identical (func,mnem,base,dispA,dispB) ≥3×: one-directional = `SHIFT(bug?)` (real — every
  call site mis-transcribed the same way); bidirectional = `SWAP(sched)` (two writes REORDERED, NW-crossed,
  NOT a bug). Exit 1 iff HIGH or SHIFT>0. Absolute [disp32]/reloc operands skipped. ⚠ inherits
  match.pair_by_name's positional-fallback mis-pairing when run over ALL TUs at once (a marker with no
  name-matching COMDAT pairs against the wrong function) — for a specific finding, RE-RUN on the single
  owning TU (bugscan.py src/<owner>/<owner>.cpp) to confirm. Validated: clean tree = 0 findings; reverting
  the v34 Afx fix re-flags it 10×+4× as SHIFT.
- **`vtcheck.py`** (v47; auto-locating v48) — ⭐ the **.rdata vtable-content oracle** (data-side complement to
  bugscan). FULLY AUTOMATIC: for every `??_7<Class>@@6B@` data COMDAT we emit, it reads the relocations →
  per-slot target symbol, looks up each override target's ORIGINAL address from our `// FUNCTION` markers
  (match.pair_by_name), LOCATES the original vtable by scanning .rdata for ≥2 of those addresses at consistent
  relative offsets, then compares override patterns. A **MISSING override** = a virtual we forgot to declare
  (the base runs — a runtime bug; caught the `World::IsModified` class of gap). Exit 1 on any real mismatch. Two
  baked-in gotchas: (a) ICF folds trivial base methods to app-region addrs → `FOLDED_BASE` whitelist (CObject
  defaults 0x401060/70/80, DisableSelfWindow/Enable 0x401090/a0, CView no-op 0x40e3f0) treats them as base; (b)
  the compare is BOUNDED to our vtable's real slot extent (max reloc offset) — reading past walks into adjacent
  .rdata (embedded sub-vtables, ??_GCPalette) and false-flags. v48: 10 classes CLEAN (all UI/dialog/frame/view/
  doc); 13 single-??_E-dtor data classes SKIPPED (un-anchorable, <2 override addrs — low-risk). No config needed.
- **`msgcheck.py`** (v49) — ⭐ the **.rdata MESSAGE-MAP content oracle** (vtcheck sibling; lesson #33). For each
  class with a `?GetMessageMap@Cls@@` marker it reads the ORIGINAL AFX_MSGMAP_ENTRY array from the exe
  (GetMessageMap `mov eax,&messageMap;ret` → messageMap {pBaseMap,lpEntries} → 24-byte entries till zero
  nMessage) and OURS from the obj's `?_messageEntries@Cls@@` COMDAT, and compares the fixed fields
  (nMessage/nCode/nID/nLastID/nSig) + handler identity (pfn addr via markers) entry-for-entry. A wrong/missing
  entry = a menu command or window message silently mis-dispatched. Exit 1 on any mismatch. v49: all 11 maps
  CLEAN after fixing CTheApp (incomplete) + GameView #11 (WM_HSCROLL→VSCROLL bug). Imports vtcheck for
  sections/va_to_off/build_name2addr.
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
