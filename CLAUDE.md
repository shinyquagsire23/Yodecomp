# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#33 + MFC-matching lessons** (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-11, v87)

Phases A–G (byte-matching YodaDemo.exe's app region) are **COMPLETE & PARKED**: **211 functions byte-exact /
99.17 % coverage**, every function transcribed (exact or annotated-EFFECTIVE), a runnable `/OPT:REF`-linked
image, all oracles green. The residual byte-identity gap is a compiler-intrinsic register-coloring wall —
**do NOT re-chase it** (docs/compiler-hunt.md, docs/g2-layout.md; every lever proven dead — body, header,
emission order, PCH, COMDAT set, compiler options: PLAN_COMPLETED.md lessons #26–#30).

Phase H (extension — functional correctness, not byte-matching) status:
- **H1 CMake build** ✅ (docs/cmake-build.md) — config matrix `YODA_GAME`(YODA|INDY) × `YODA_VARIANT`(DEMO|FULL)
  × `YODA_PLATFORM`(WIN32|SDL).
- **H2 full Yoda Stories** ✅ (docs/phase-h2-full-game.md) — all 3 planets generate + play; Save/Load/Replay work.
- **H3 Indy 32-bit port** ⏳ broadly PLAYABLE (docs/phase-h3-indy.md) — DAW load, worldgen, ACTN scripts,
  doors, HUD, palette, resources (Indy icon/title/About) all done + user-confirmed; minor tails remain.
- **H4 SDL portable target** — ⏳ M0–M5 CORE COMPLETE (docs/phase-h4-sdl.md): ⭐ the game RUNS
  NATIVELY on macOS (`build-sdl/yoda`) — title, intro, game loop, input, walking, zone/door
  transitions, item drag, weapons, FULL AUDIO (WaveMix+MCI over SDL2_mixer), FULL UI CHROME
  (HUD/health dial/arrows, MS Sans Serif text, MODAL speech bubbles, .res strings/icons/cursors,
  scrollbar, teardown — all v79 user-confirmed), and (v80) REAL DIALOGS: CDialog::DoModal parses
  RT_DIALOG templates → controls → modal loop → DDX (About + option sliders screenshot-verified),
  plus menu commands via the game's real accelerator table (Ctrl-chords → WM_COMMAND), and
  (v80 tail) a real save/load CFileDialog (SDL has no native picker — lists *.wld files as
  clickable rows; unit-tested via new `dlg_smoke` harness, not yet live-screenshot-verified).
  v81: deferred-present perf fix (user: "MUCH snappier"). v82: platform-BACKEND split
  (mfxplat.h contract; neutral pump/snd + swappable backend TUs — sdl3 (new default,
  user-confirmed), sdl2, null; a DS port = two new files). v83: a REAL VISIBLE MENU BAR
  (user-confirmed live) — chrome strip composited above the game's screen DC (own DC/palette,
  zero game-coordinate changes), dropdown popups riding the existing dialog child/capture
  machinery, full CN_UPDATE_COMMAND_UI wiring; found + fixed 4 real bugs along the way
  (CDeskcppDoc was missing from WM_COMMAND routing entirely — see PLAN_COMPLETED.md ⏮ v83).
  v84: INDY×SDL live playtest — MIDI audible (v82 backend split confirmed working), P pause
  hotkey wired + user-confirmed, Hide Me! wired (new `MfxPlatMinimize` platform-contract hook)
  + user-confirmed, and a real GAME_INDY-only crash fixed + user-confirmed
  (`ShowWinMessage`'s Yoda-hardcoded tile ids 780/2034 read OOB against Indy's smaller tile
  catalog on nearly every bump/talk interaction — docs/engine-bugs.md #16; a `MfxArrayOOBTrap`
  diagnostic, kept in `microfx/include/afxwin.h`, pinpointed it via a live backtrace after an
  initial guess — `charId` bounds in `Tick`/`DrawEntities`, #15 — proved to be a red herring).
  v85: the whole v84 "still broken" list CLEARED — the F8 dialog and the roaming CFile::Read
  crash were ONE bug (microfx `CFile` ops ASSERTED on a never-opened stream where real MFC
  THROWS the `CFileException` that `LoadWorldStateFile`'s CATCH is designed to swallow; fixed in
  mfxcore.cpp, F8 user-confirmed working + Yoda-SDL 30s idle clean); Statistics resolved by
  GROUND TRUTH (retail Indy has NO Stats feature — no menu item, no dialog 0xe1) via importing
  Indy's REAL menu (`make_res.py --indy` now converts DESKADV's NE RT_MENU → Win32 template;
  live-rendering, update-UI working, every command id already in our dispatch space); INI replay
  persistence implemented ([GameData] Wyoming/Hawaii — see GOAL 1 notes); all 9 uncertain IACT
  condition opcodes + cmd 0x13 re-derived from DESKADV's REAL condition switch — 6 entries were
  WRONG (incl. one with 142 uses in DESKTOP.DAW).

## ⭐ CURRENT GOALS (user-set 2026-07-10)

1. **✅ Indy ifdef stragglers — GOAL 1 CLOSED (v87).** `GAME_INDY` deltas all done:
   - ✅ Startup theme MIDI (v72) · ✅ IACT opcodes fully verified (v85) · ✅ INI replay
     persistence (v85) · ✅ Indy menu resources (v85) · ✅ Hero-HP tail (v87 — was a misread:
     DESKADV's "entity+0x90=120" is actually `view->nTargetZoneId=120`; Indy health is
     doc+0x1096/0x1098 already reset 1/1 by StartGame. No field to wire; added the real missing
     tail writes — timeBase/unk50/unk2e34/camera 0x160,0xa0. See v87 pickup.).
2. **H4 — the SDL portable target** (largest lift; spec below).
3. **Indy Ghidra RE sweep** — comb `DESKADV.EXE` (`program=DESKADV.EXE`) for behavioral differences we've
   missed, naming functions + defining structs along the way (same conventions as YodaDemo; 16-bit NE,
   segmented addresses — recover LOGIC, not codegen).
4. **⭐ WASM port (user-set 2026-07-11)** — compile the H4/microfx SDL3 target to WebAssembly via
   Emscripten, still SDL3 (Emscripten ships an SDL port; SDL3 supports the web backend). The pump's
   main loop must convert to `emscripten_set_main_loop` (a browser can't own the loop the way
   `CWinThread::Run`'s `while(!quit)` does — see the modal-loop caveat below). Assets (YODESK.DTA /
   DESKTOP.DAW / .res / WAV+MID) ship in the Emscripten virtual FS (`--preload-file`); audio via
   SDL3's web audio; the SDL3 native file dialog (v85) has no web equivalent → `MfxPlatShowFileDialog`
   returns -1 in a wasm backend so CFileDialog's in-window row-list picker is used (already the
   fallback). ⚠ The blocking modal loops (CDialog::DoModal, CFileDialog, the intro) use a nested
   `while(GetMessageA)` — those DON'T work under a browser's cooperative event loop and are the main
   porting lift (either Asyncify, or restructure the modal loops into state machines). New backend =
   `microfx/src/platform/mfxplat_wasm.cpp` + a CMake/emcmake toolchain config; keep it a config-matrix
   corner like the others. Reference: OpenJKDF2 has an Emscripten build.

### H4 spec — Beyond Win95: portable SDL target via "microfx" (full design: docs/phase-h4-sdl.md)
- **Strategy (user-set 2026-07-10): implement a source-compatible MFC SUBSET ("microfx"), not per-call
  ifdefs.** All game TUs get MFC solely via `<afxwin.h>/<afxext.h>/<afxcmn.h>/<afxcoll.h>/<mmsystem.h>`;
  the SDL config puts `microfx/include/` first on the include path so those SAME directives resolve to OUR
  drop-in headers (MFC+Win32 subset over SDL2). The 13 TUs compile UNMODIFIED → anchor preserved by
  construction (no token or line-number changes; the lesson-#23 hazard never arises). Same shape as
  OpenJKDF2's Win95-API shim, one level up: keep the MFC-shaped code, reimplement MFC. Existing message
  maps/DYNCREATE/afx macros keep working — the pump synthesizes WM_* from SDL events into EXISTING handlers.
- Milestones (each with an oracle — docs/phase-h4-sdl.md): M0 core classes + logic TUs native, worldgen-log
  diff vs wine · M1 Canvas→SDL_Surface (8-bit DIBSection ≈ paletted surface) · M2 event pump/timers/input ·
  M3 SDL2_mixer audio (WaveMix + MCI MIDI) · M4 resources/dialogs/menus.
- **References:** `~/workspace/DesktopAdventures` (SDL patterns, NOT behavior truth), `~/workspace/OpenJKDF2`
  (shim precedent). SDL2, macOS/Linux/Windows.
- **Done when:** a native SDL build of Yoda Stories runs on macOS AND the Win32/MFC byte-match anchor still passes.

## 🛡 THE ANCHOR (never regress this)

The byte-exact demo build (GAME_YODA + YODA_DEMO + WIN32/MFC + /O2) is the **preserved default corner** of the
config matrix. Every extension is ADDITIVE — ifdefs / a platform HAL — and **any ifdef must leave the default
config's PREPROCESSED TOKENS identical** (guard so the Yoda/demo/Win32 path is the fall-through). When editing
shared TUs, watch #line provenance: adding/removing source LINES mid-file can rotate a TU's codegen dial even
when tokens are neutral — prefer end-of-file additions / same-line decls (lesson #23).

⭐ **YODA_SIC_FIX inside a boolean expression (v84):** `YODA_SIC_FIX(x)` expands to EMPTY in anchor
builds — embedding it mid-expression (e.g. `if (A && YODA_SIC_FIX(B) && C)`) leaves a dangling `&&`
and fails to COMPILE the anchor. The safe shape is a short-circuit clause PREPENDED as its own
complete `(bool) &&`/`||` term: `if (YODA_SIC_FIX((cond || (BUGLOG((...)), 0)) &&) A && B)` — in
anchor mode this collapses to `if ( A && B)` (identical tokens, harmless whitespace); in bugfix mode
it adds a real short-circuiting guard term before the original condition ever evaluates. Used to fix
docs/engine-bugs.md #16 (`ShowWinMessage`'s hardcoded tile ids OOB on Indy's smaller catalog) without
touching a single original token or line count.

**Anchor oracles — run after ANY shared-code edit, all must hold:**
| oracle | command | green state |
|---|---|---|
| exact count | `python3 tools/progress.py` | **211 exact / 99.17 %** |
| full link | `tools/link_exe.sh` | 0 unresolved / 0 duplicates / exit 0 |
| field/slot bugs | `python3 tools/bugscan.py --all` | 0 HIGH / 0 SHIFT |
| vtables | `python3 tools/vtcheck.py` | 10 classes CLEAN |
| message maps | `python3 tools/msgcheck.py` | 11 maps CLEAN |

⚠ Objects live in **`build/`** (repo root), not next to sources: compile with
`cd src && ../toolchain/bin/cl /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS /Fo../build/<File>.obj <File>.cpp`.
progress.py's count is name-keyed and robust; a lone verify.py per-TU number is a lower bound (lesson #30).
Original engine bugs are reproduced, not fixed, in the ANCHOR — `docs/engine-bugs.md` + `// sic:`
comments. Non-anchor configs default **`YODA_BUGFIX=ON`**: crash/UB/leak sic-sites are fixed via the
line-neutral `YODA_SIC_FIX`/`YODA_SIC_RETURN`/`BUGLOG` macros (tail of Worldgen.h/DeskcppStub.h/
DeskcppDoc.h — 3 identical copies, keep synced) and hits log to `yoda_bugfix.log`; behavior-shaping
bugs (worldgen quirks, script scheduling) stay faithful so seed-parity holds (digest A/B verified).
Per-bug status table: docs/engine-bugs.md.

## 🔨 Build / run / debug (Phase H)

- **Configs:** `cmake -B build-<cfg> -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake [-DYODA_GAME=INDY] [-DYODA_VARIANT=FULL] && cmake --build build-<cfg>`
  — existing trees: `build-cmake` (demo anchor), `build-full` (retail Yoda), `build-indy` (Indy).
  ⚠ `JOB_POOL wine=1` serializes wine cl — parallel wine cl deadlocks the wineserver. Kill stale wine between
  runs: `pkill -9 -f yoda.exe`.
- **Run folders:** `./run_indy.sh` → `YodaIndy/` (DESKTOP.DAW + assets); `./run_full.sh` → `YodaFull/`
  (YODESK.DTA); `run.sh` → demo. GUI runs go through CrossOver (user does visual confirmation).
- **⭐ Headless debug oracle:** CrossOver wine reaches `Load()`/worldgen/`OnTimer` headless (window timer
  fires). Build with `-DYODA_DEBUG=ON`, `#include "DebugLog.h"`, `YDBG((...))` → logs to
  `YodaIndy/yoda_debug.log`. FAST logic-bug oracle for worldgen + game loop. ⚠ YODA_DEBUG perturbs
  byte-matched TUs — keep YDBG guarded under GAME_INDY/YODA_DEBUG and git-revert before an anchor check;
  `-DYODA_DEBUG=OFF` for committed builds.
- **Resources:** `tools/make_res.py <yoda_exe> <out.res> --indy <DESKADV.EXE> | --full <Yodesk.exe>` builds
  extended-config .res = Yoda's `.rsrc` base (our code depends on YodaDemo's integer resource IDs — never
  wholesale-swap) with only identity resources overridden (icon/title/About). Pure demo anchor uses
  `extract_res.py`. `IDR_MAINFRAME==2` in this app (not 128). `tools/reslib.py` parses both PE and 16-bit NE.

## Reference binaries & key RE facts

| binary | what | where |
|---|---|---|
| `YodaDemo.exe` | Yoda Stories demo — THE byte-match target | repo root + `YodaDemo/`; Ghidra `program=YodaDemo.exe` |
| `Yoda Stories/Yodesk.exe` | retail full Yoda (same engine, 4 days newer) | Ghidra 2nd program (H2 diffs) |
| `INDYDESK/DESKADV.EXE` | 16-bit NE Indy — GROUND TRUTH for every Indy delta | Ghidra `program=DESKADV.EXE` |
| data | `YODADEMO.DTA` / `YODESK.DTA` (4.6 MB) / `DESKTOP.DAW` (2.36 MB) | run folders / `~/workspace/DesktopAdventures` |

`YodaDemo.exe`: PE32 x86 MFC app, **MSVC 4.2** (cl 10.20/link 3.10, 1997-02-18), static CRT (`/MT`) + static
MFC (NAFXCW), `/GX /O2`, imports WAVMIX32 et al. App region 0x401000–~0x429000 (534 funcs, all claimed);
0x429000+ is MFC/CRT library code (never hand-written). Original project name: **"Deskcpp"**.

**DESKADV.EXE named anchors (Ghidra):** `IndyGenerate` 1010:8524, `IndyPlaceQuestNode` 1010:7f0c
(param_3=gridOrder/tag, param_4=reqItem, param_5=step-1/orderSlot, param_6=nodeType), `IndySelectPuzzle`
1010:7b58, `IndyPopulateGoalZone` 1010:5dac, `IndyParseActn` 1010:b5d4 (≡ our ParseActn), `IndyCyclePalette`
1018:8e40, IACT cmd dispatcher `FUN_1010_2eb6`. Full tables in docs/phase-h3-indy.md.

**⭐ Indy-delta lessons (standing):** IACT condition+command OPCODES are RENUMBERED Yoda↔Indy — audit the
remap tables (`kIndyCmdToYoda` in src/IactScript.cpp) case-for-case against DESKADV's real jump tables. A Yoda
HUD/UI element may simply NOT exist in Indy (RE the DESKADV draw list before "fixing" a broken-looking one).
`~/workspace/DesktopAdventures` is a REIMPLEMENTATION — a where-to-look map, NOT behavior truth; its
`if(!is_yoda)` gates can be wrong (e.g. it falsely says Indy doesn't cycle the palette). Confirm every "Indy
differs" claim against DESKADV.EXE.

### External references
- **`~/workspace/DesktopAdventures`** — the user's own engine recreation (both games). Asset-format and
  game-logic semantics for naming: `scrdoc.txt` (script opcode format), `SCRIPTS.md`, `README.md`. Also the
  H4 architecture reference. See the caveat above — verify against binaries.
- `~/workspace/OpenJKDF2` — style/naming conventions, CMake layout.
- `docs/` — per-subsystem findings: dta-format, game-logic, worldgen, sound, engine-bugs, settings,
  phase-h2-full-game, phase-h3-indy, cmake-build, compile-units, link-audit, g2-layout, compiler-hunt.

## Source map (src/ — single flat folder, real AppWizard-style names)

13 .cpp TUs in address/link order: `GameTypes`(0x401000) → `Score` → `WorldgenHelpers` → `GameObjects` →
`Iact` → `Canvas` → `DeskcppView` → `IactScript` → `TextDialog` → `MainFrm` → `Deskcpp` → `DeskcppDoc` →
`Worldgen`. Headers: `Deskcpp.h`, `DeskcppDoc.h` (the real CDeskcppDoc/World struct), `DeskcppView.h`,
`MainFrm.h`, `TextDialog.h`, `GameObjects.h`+`GameObjectClasses.h`, `IactScript.h`, `DeskcppStub.h`,
`Canvas.h`, `MapZone.h`, `Worldgen.h`, `DebugLog.h`. Functions carry `// FUNCTION: YODA 0xADDR` markers.
Classes use their ORIGINAL names (`CDeskcppDoc`/`CDeskcppView`, from CRuntimeClass strings); variables keep
readable game-concept names (`pWorld`, `pView` — original variable names are unknown).

## Naming conventions (Ghidra + source)

Loose-Hungarian variables: `p`=pointer, `pa`=pointer-to-array, `b`=bool, `n`=int. Name a pointer after the
struct it points to (`pWorld`, `pView`, `pZone`).

**Function naming = C++ `Namespace::Method`**, namespace = the class, bare method name (`Canvas::BlitMasked`,
`Zone::GetTile`). ⚠ **The Ghidra namespace MUST equal a same-named Structure** — that's how a `__thiscall`
function's auto-`this` gets typed; a namespace with no matching struct degrades `this` to `void*`.
`set_function_this_type X*` types AND moves the function into namespace `X` in one act. Sub-modules
("Dta", "Worldgen") are documentation concepts, never namespaces.

**Uncertainty ladder:** `FUN_<addr>` (unread) → `Related` (touches subsystem X, role unknown) → `Maybe`
(honest hypothesis) → certain name. Struct fields: `Unk0xNN` placeholders → promoted as readers/writers are
found. Prefer a descriptive `Maybe` guess over an anonymous `FUN_*`, but read the body first — never
confidently-wrong names. Grep `Maybe` to find open hypotheses.

**Struct discipline (applies to the DESKADV sweep):** define structs in Ghidra FIRST so the decompiler emits
`this->field` instead of pointer math — transcription/reading becomes trivial. Pin a struct's size from its
allocation site (`operator_new(N)`), not from observed access extent. One canonical definition: Ghidra DB +
src/ headers (docs/structs.md is deprecated/history-only). Non-idiomatic decompiled C++ (raw casts, wrong
field types) is a signal a type is still missing — model it, don't transcribe mess. Prefer enums over
magic-value comments (they transfer to Ghidra).

## Ghidra access

MCP-backed decompiler at `http://localhost:8089` (bethington/ghidra-mcp) + the richer `mcp__ghidra__*` tools.
**⚠ CRITICAL: many programs are open (JK.EXE, KOTOR, YodaDemo.exe, Yodesk.exe, DESKADV.EXE …). ALWAYS pass
`program=` on EVERY request — reads AND writes.** With it set, writes route to the named program regardless
of which is active (fixed v51); omitting it targets whatever's active. `switch_program` does not persist.
Example: `http://localhost:8089/decompile_function?program=YodaDemo.exe&address=0040b270`.

**Write recipes/gotchas (battle-tested):**
- `run_script_inline` = POST JSON `{"code":"..."}` built with json.dumps; NO import statements — fully-qualify
  every Ghidra class. Finish with POST `save_program`. Compile-error noise from old `~/ghidra_scripts/*.java`
  is normal.
- `modify_struct_field` silently NO-OPs field renames — use `run_script_inline` `setFieldName`.
  `modify_struct_field_type` clobbers the field NAME (restore after). NEVER use `recreate_struct` (ignores
  offsets) or `remove_struct_field` (shifts the tail) on offset-precise structs — `replaceAtOffset` is the
  tool; growing a field over neighbors needs `getComponentAt`+`clearComponent` per byte range first.
- Grow structs with `while (getLength() < size) growStructure(size - getLength());` (one-shot arithmetic
  leaves it 1 byte short). `deleteAll()` leaves a phantom length-1.
- Renames into class namespaces: `f.getSymbol().setName(...)` + `f.setParentNamespace(...)` — auto-retypes
  `this` when a same-named Structure exists. Clear stray params with `f.replaceParameters(DYNAMIC_STORAGE_ALL_PARAMS,
  true, USER_DEFINED, new Parameter[0])`.
- Audit for `-BAD-` dangling field types after struct surgery — they silently degrade dependent decompiles.
- Bulk `this`-typing: scan untyped `__thiscall` funcs for DISTINCTIVE field offsets (Zone 0x7ac/0x7c0/0x844,
  World 0x4b4/0x2e20/0x3330, Canvas 0x438); avoid common offsets (0x44/0x98) — they false-positive; require
  corroboration for weak signals.
- HTTP raw writes: JSON bodies; rename key `"function_address"`, plate key `"address"`; `program=` in the
  QUERY string.

## Tooling (`tools/`, Python, run from repo root)

Byte-match harness (anchor checks): **`progress.py`** (headline dashboard) · **`verify.py <src.cpp>`** /
**`match.py`** (per-TU marker compare, reloc-masked; best-fit can mis-pair clones — confirm name-keyed) ·
**`asmscore.py <src.cpp> 0xADDR [--dump]`** (graded disasm scorer; `--dump`: LEFT=original, RIGHT=ours;
recompiles the TU itself) · **`bugscan.py`** / **`vtcheck.py`** / **`msgcheck.py`** (correctness oracles —
wrong vtable slot / field disp / message-map entry; see anchor table) · **`link_exe.sh`** (full-image link
oracle) · `permute.py`, `survey.py`, `frontier.py`, `g2_link.sh`, `g2_diff.py`, `g2_order.py`,
`exactset.py`, `libfingerprint.py` (parked byte-match era; see PLAN_COMPLETED.md).
Resources: **`make_res.py`** (+`reslib.py`), `extract_res.py`.

## 📋 Session protocol

1. **Orient:** read the ⏭ pickup block below; run `python3 tools/progress.py` to confirm the anchor (211)
   reproduces BEFORE changing anything (if not, a header drifted — bisect first).
2. **Work** the pickup goals. Ghidra writes: always `program=`. Anchor rule for every shared-TU edit
   (ifdef fall-through = original tokens); re-run the anchor oracles after shared-code changes.
3. **Agents** for read-only RE sweeps (naming/xref surveys); keep build-and-test iterations in the main thread.
   Escalation: spawn a `fable`-model agent with the disasm + relevant lesson numbers for novel mechanisms.
4. **Session end:** update the ⏭ pickup block (findings → instincts, done items removed, next steps concrete);
   demote the old pickup to a condensed ⏮ block APPENDED to PLAN_COMPLETED.md; distill new mechanisms into
   the lessons lists (PLAN_COMPLETED.md) or the standing-lesson bullets here; sync new struct fields/renames
   to Ghidra (or list as PENDING); `save_program`; commit with a descriptive message.

### ⏭ NEXT SESSION PICKUP (2026-07-11 v87 — the two remaining v86 work items DONE + all
user-confirmed live: the LAST GOAL-1 item (Indy hero-HP tail — turned out to be a nothing,
see below) and the proper in-window `AfxMessageBox` modal. Bonus user-found redraw-residue
bug fixed. v86 detail retained below; v85 condensed → PLAN_COMPLETED.md ⏮.)

**▶ v87 — ✅ Proper `AfxMessageBox` in-window modal (USER-CONFIRMED live).** Replaced the
M-era headless stderr stub (auto-answered IDYES/IDOK, so every confirm was INVISIBLE) with a
real Win95-style modal: `MfxShowMessageBox` in microfx/src/app/mfxdlg.cpp reuses the M5 control
kit (CK_DLGFRAME bevel + word-wrapped CK_LABEL lines + a CK_DEFBUTTON/CK_BUTTON row mapped from
the MB_ type: OK/OKCANCEL/YESNO/YESNOCANCEL → return IDOK/IDCANCEL/IDYES/IDNO), its own
GetMessageA modal loop (Enter=default, Esc=cancel where valid, Y/N accelerators, click→
WM_COMMAND scoped to msg.hwnd==dialog). Exposed via a new `microfx.h` prototype so BOTH core/
entry points (AfxMessageBox in mfxcore.cpp, MessageBoxA in mfxstubs.cpp) call it; caption =
app name else AFX_IDS_APP_TITLE string 0xE000. ⭐ Headless-safe fallback preserved: returns -1
when `MfxPumpIsUp()` (new pump accessor) is false or no root window, and the callers keep the
old auto-answer — so worldgen_smoke/game_walk/dlg_smoke stay non-blocking. Screenshot-verified
("Leave Yoda Stories?" YESNO over the game, correct caption).

**▶ v87 — ✅ Debug oracles now pump inside modal loops too (enabler for the above).** Factored
the AUTOMOD/AUTOKEY/AUTOCMD/AUTOCLICK/SHOT block out of `CWinThread::Run` into a shared
`MfxDebugOracles()` (mfxpump.cpp) that the modal `GetMessageA` wait also calls each spin — so a
YODA_AUTOCLICK can drive/answer a dialog (the v86 "modal loop owns the thread → headless
dialog-trigger oracles never fire" limitation, now lifted). Also fixed AUTOCLICK to emit
WM_LBUTTONUP after the DOWN (dialog-kit buttons commit on the UP — a DOWN-only synthetic click
pushed but never fired them). Per-phase statics keep double-pumping harmless. ⚠ Note: the
in-game speech BUBBLE is itself a nested modal that swallows non-bubble clicks, so an AUTOCLICK
fired while a bubble is up can't reach a box behind it — dismiss the bubble first (real game
gates New/Replay/Load during a bubble anyway).

**▶ v87 — ✅ Redraw residue on New/Replay/Load World FIXED (USER-FOUND + USER-CONFIRMED).**
The confirm box overlaps the inventory panel; on close it left box pixels over the inventory.
Root cause = the "out-of-main-loop redraw" class (same family as the level-transition junk):
those three commands return to a handler that IMMEDIATELY runs a world regen + zone transition,
and the transition redraws only the game-AREA region in a busy-wait loop (clock-hook presents,
NOT a full OnDraw), so the box's `MfxSetDirty()` was preempted — its full repaint never ran and
the inventory region (which a transition never touches) kept the box pixels. Fix: force
`MfxPaintIfDirty()` synchronously at EVERY modal close (MfxShowMessageBox + CDialog::DoModal +
both CFileDialog paths, mfxdlg.cpp) so a full erase+OnDraw clears the whole window while the
game is still in its pre-transition drawable state. About/sliders never tripped it because
nothing redraws after them. ⭐ LESSON: a modal that returns into a partial-redraw operation
must repaint ON CLOSE, not just mark dirty.

**▶ v87 — ✅ LAST GOAL-1 item (Indy hero-HP tail) CLOSED — it was a misread, nothing to wire.**
Re-RE'd DESKADV IndyGenerate's tail (1010:8524) + the StartGame twin `IndyStartNewGameMaybe`
(1020:0ed0): the v84 "sets hero entity+0x90=120, clears +0x2c" reading was WRONG — `local_3e`
is the VIEW (from the GetFirstViewPosition/GetNextView vtable pair at the head), so
"entity+0x90=0x78" is `view->nTargetZoneId = 120` and "entity+0x2c=0" is the view busy flag —
both of which our shared code already sets. Indy's HEALTH lives at doc+0x1096/0x1098
(healthLo/healthHi, same lo/hi scheme as Yoda; IACT cond 0x16 at 1010:2e6a reads
hi*-100-lo+0x191) and is reset to 1/1 by the StartGame twin (doc+0x1096 = 0x10001) — which our
shared StartGame already does. So NO per-entity hero-HP field exists to wire. While there,
added the two genuinely-missing DESKADV tail writes (Worldgen.cpp IndyGenerate tail, all
GAME_INDY-guarded): `timeBase = time(NULL)` (doc+0x58 clock stamp; DESKADV does NOT clear
timeOffset, kept faithful), `unk50 = startZoneId`/`unk2e34 = 0` (doc+0x44/0xc34), and the
camera init `cameraX/Y = 0x160/0xa0` (doc+0x10a2, was Yoda's 0x140/0x140). GOAL 1 is now fully
CLOSED. Anchor: Worldgen.obj recompiled, all 5 oracles GREEN (change is GAME_INDY-guarded).

**▶ v86 — ✅ Statistics dialog opens now (USER-CONFIRMED path via menu).** Root cause was NOT
the .res or the enable-gate — dialog 0xe1 is the game's ONE **DLGTEMPLATEEX** resource
(dlgVer=1/sig=0xFFFF), and microfx's `CDialog::DoModal` only parsed the classic DLGTEMPLATE
layout → it read the EX header's fields at the wrong offsets → bogus cx/cy → early IDCANCEL
(that's the real meaning of v80's "corrupt in the .res" note). Added an EX branch to the
header + control parse (microfx/src/app/mfxdlg.cpp); the classic path is untouched (EX branch
only fires on the 0xFFFF signature). Parse validated against the real 0xe1 bytes (cx=147,cy=71,
9 controls, exact 482 B). ⚠ Testing note learned this session: the game sits in a **blocking
`while(GetMessageA)` modal loop the whole run** (the title/intro owns the thread from ~3s in —
heartbeat proof), so `YODA_AUTOCMD`/`YODA_AUTOCLICK` (evaluated only in the main `CWinThread::Run`
loop) NEVER fire once the intro starts; only real input, which the modal loop's own GetMessage
drains, gets through. That's why headless dialog-trigger oracles fail — NOT the fix. This same
blocking-modal-loop structure is THE WASM porting lift (GOAL 4).

**▶ v86 — ✅ SDL3 native file dialog for Save/Load World.** New `MfxPlatShowFileDialog` hook on
the mfxplat.h contract; the SDL3 backend drives the async `SDL_ShowOpen/SaveFileDialog` to
completion (pump events until the callback lands). SDL2/null/(future DS/WASM) return -1 →
`CFileDialog::DoModal` falls back to microfx's in-window row-list picker (unchanged; dlg_smoke
still green). Signatures verified against /opt/homebrew/include/SDL3/SDL_dialog.h. ⭐ Focus nit
(user-found, FIXED): the panel steals keyboard focus → queues FOCUS_LOST that the spin loop
never polls → the pump later drains a LONE stale FOCUS_LOST → `CMainFrame::OnActivate` pauses
the game (nFrameMode=0/bBusy=1 = stuck on STUP) with no matching wake (SDL doesn't reliably
re-emit FOCUS_GAINED on panel close). Fix: after the panel closes, `SDL_FlushEvent` both focus
events + `SDL_RaiseWindow` so the game stays in its pre-dialog active state. Whether the SAVE
path (native panel returns a path) writes correctly is still user-playtest-pending.

**▶ v86 — ✅ Ctrl+D opens the F8 debug dialog (USER-CONFIRMED working live).** macOS reserves
the **Ctrl+F8 chord** as a system shortcut ("move focus to status menus") and eats it before SDL
(user-confirmed: plain F8 reaches the app, only the Ctrl+F8 combo is grabbed). Fix in the neutral
pump (mfxpump.cpp): a plain `Ctrl+D` (unused by the game, not a macOS shortcut) injects a synthetic
`VK_F8` WM_KEYDOWN while real Ctrl is held → the game's own OnKeyDown VK_F8 handler opens the dialog
with NO key-state faking. Windows' native Ctrl+F8 unaffected. `YODA_KEYLOG=1` (SDL3 backend, kept)
logs sdlkey/scancode/mod→vk per key event — the probe that framed this + future key issues.

**▶ v86 anchor note:** all v86 edits are microfx-only (mfxdlg/mfxpump/mfxplat + 3 backends) — the
byte-match anchor build never compiles microfx, so 211/99.17% is untouched by construction (not
re-run). Binaries copied to YodaFull/ and YodaIndy/ for the user.

**▶ ⚠ Open watch-item:** ONE replay run exited SIGSEGV (exit 139) after the world regenerated;
NOT reproduced since (3 fixed-INI re-runs + 2 lldb runs all clean, no yoda_crash.log). If a
crash shows up around Replay Story live, get `lldb -b -o run -k "bt 25"` on it. Also still not
live-verified: F1→How to Play (low priority), YODA_ACCEL=1 present path.

**▶ NEXT (likely session shape):** GOAL 1 is CLOSED and the v86 TODO (AfxMessageBox) is DONE.
Remaining open work: (1) user live-playtests Indy — the REAL menu bar, Replay Story persistence
across app restarts, gameplay under the corrected condition opcodes, and now the live confirm
boxes (Replay/New/Load) with the real modal (NOTE: the user runs `cd YodaIndy && ./yoda` — copy
`build-sdl-indy/yoda` there first, or ask; an unsolicited cp was declined mid-v85, the user
copies it themselves). (2) GOAL 2 (H4 SDL polish) / GOAL 4 (WASM port — the blocking modal
loops incl. the new AfxMessageBox loop are the porting lift). (3) GOAL 3 Indy Ghidra RE sweep
as fill.

**▶ Research note (from the user, unexplored):** SDL3 has a native file-dialog API
(`SDL_ShowOpenFileDialog`, https://wiki.libsdl.org/SDL3/SDL_ShowOpenFileDialog) — worth splitting
`CFileDialog::DoModal`'s custom row-list picker (mfxdlg.cpp, v80) behind a new
`MfxPlatShowFileDialog`-shaped `mfxplat.h` hook (same precedent as v82/v84 `MfxPlatMinimize`),
falling back to the custom picker on backends without one (null/DS).

- ⚠ worldgen needs Terrain∈{1,2,3} in the INI (Terrain=-1 ⇒ infinite Generate retry). Harness INIs:
  `<exebase>.INI` next to the binary; doc ctor re-picks the planet EVERY run and writes it back — reset
  before A/B runs (v85: persistence tests must also snapshot/restore `[GameData]`!). `worldgen_smoke
  <seed>` · `zone_view <seed> [--zone id] [--dump x.bmp] [--show]` · `game_walk <seed>` ·
  `YODA_SHOT=<pfx>[:n] ./yoda` (composited window incl. menu bar) · `YODA_AUTOKEY=<startms>:<vk>:<durms>`
  · `YODA_AUTOMOD=<startms>:<vk>:<durms>` (modifier key-state only, for chords) ·
  `YODA_AUTOCMD=<startms>:<cmdhex>` (0x8008 New World / 0x800b Replay — the v85 persistence oracle) ·
  `YODA_AUTOCLICK=<ms>:<x>:<y>[,...]` (y<19 = menu bar) · `YODA_DLGSHOT=<path>` · `dlg_smoke` ·
  `YODA_ACCEL=1` · `YODA_VSYNC=1` · `yoda_crash.log` (MfxArrayOOBTrap backtrace dump, cwd) ·
  microfx `AfxMessageBox` is now a REAL in-window modal when the pump is up (v87) — headless it
  still prints to stderr + auto-returns IDYES/IDOK (`MfxShowMessageBox` returns -1) · the v87
  debug oracles (AUTOCLICK etc.) now fire inside modal loops too (shared `MfxDebugOracles`),
  but a nested speech-bubble modal swallows non-bubble clicks — dismiss it first · run-from
  `build-sdl-indy/` works (DESKTOP.DAW + yoda.INI copied there in v85) — the data path is the EXE's
  OWN folder (`GetModuleFileName`), not cwd.

**▶ GOAL 3 — Indy Ghidra sweep (agent fodder):** `program=DESKADV.EXE`, ~210 app-code unnamed (seg 1010
doc/parse/worldgen/IACT, 1018 view/UI/sound/dialogs, 1020 cmd handlers; segs 1000/1008 = MFC/CRT
library, SKIP). Method: twin-rich area → string/import xrefs or caller structure → name `Indy*`
+ plate-comment the Yoda twin. ⚠ match twins by STRUCTURE not 16-bit offsets; data xrefs may be
unresolved (find strings by scanning the raw NE file + search PUSH imm16 of the const-seg offset —
worked perfectly for the v85 INI-keys hunt); Ghidra flow-splits big 16-bit functions (1010:9684 is
a FRAGMENT of IndyGenerate's tail, plate-commented) — "No function found" from the decompile
endpoint near a known function usually means you're in such a fragment; disassemble instead.

**▶ Anchor:** 211 exact / 99.17 % — ALL 5 oracles re-verified TWICE in v85 (after the
WorldgenHelpers/IactScript/Worldgen mid-file GAME_INDY ifdef edits, and again after the final
IactScript comment pass). All Indy work GAME_INDY-guarded; all H4 work YODA_PORTABLE-guarded;
debug rig YODA_DEBUG-guarded (committed builds OFF). H4 rule of thumb: fix portability in microfx
headers/stubs first (v85's CFile fix is the model case); touch a game TU only for __asm /
pointer-width casts / old-for-scope / a genuine crash-class bug, always guarded via
`YODA_SIC_FIX`, always re-oracled — and NEVER add an unguarded include to a byte-matched TU
(lesson 6).
