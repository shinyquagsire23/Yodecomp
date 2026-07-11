# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#33 + MFC-matching lessons** (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-11, v84)

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
  Still broken (live-confirmed, next session): F8 debug-info dialog (Ctrl+F8), Statistics menu
  item — see pickup. A pre-existing DEMO-variant `CFile::Read` crash found in v83 is still
  unisolated.

## ⭐ CURRENT GOALS (user-set 2026-07-10)

1. **Indy ifdef stragglers** — the remaining `GAME_INDY` deltas, in rough priority:
   - **Startup theme: Indy uses a MID, not a WAV** (user finding). Indy's music is MIDI; the Yoda engine
     already carries MIDI support even though Yoda doesn't ship MIDs (`MIDILoad` registry flag, docs/sound.md;
     `CDeskcppView.pMusicThread@0x2fc`). Route Indy's startup/theme music through the MIDI path.
   - Hero-HP tail: `IndyGenerate` tail sets entity+0x90=120 in DESKADV; we set only doc fields.
   - Still-uncertain IACT opcodes: cmd 0x13 rect arg-order vs DrawZoneCellRect; condition specials
     0/8/9/0xb/0x14–0x16 — verify vs DESKADV jump tables.
   - INI replay persistence.
   - OPTIONAL: Indy menu resources (menu id 2; command IDs match — extend `tools/make_res.py --indy`; risks
     command-dispatch mismatch, deferred).
2. **H4 — the SDL portable target** (largest lift; spec below).
3. **Indy Ghidra RE sweep** — comb `DESKADV.EXE` (`program=DESKADV.EXE`) for behavioral differences we've
   missed, naming functions + defining structs along the way (same conventions as YodaDemo; 16-bit NE,
   segmented addresses — recover LOGIC, not codegen).

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

### ⏭ NEXT SESSION PICKUP (2026-07-11 v84 — INDY×SDL LIVE PLAYTEST: a real GAME_INDY crash found +
fixed + USER-CONFIRMED, plus Hide Me!/P-pause wired + USER-CONFIRMED; F8 debug dialog and the
Statistics menu item are LIVE-CONFIRMED STILL BROKEN — top of next session's list, see below)

**▶ v84 — ✅ Indy indoor-talk/bump crash FIXED, USER-CONFIRMED** (docs/engine-bugs.md #16). Root
cause found via a live backtrace, NOT guesswork: `CDeskcppView::ShowWinMessage` (called from
`OnBumpTile` on every interactive-cell bump — includes bumping/talking to an NPC) has two
Yoda-hardcoded tile-catalog indices (780, 2034) that are never `GAME_INDY`-ifdef'd; Indy's tile
catalog is far smaller (1144 entries in the reproducing world) so `pWorld->tiles.GetAt(2034)` reads
OOB on nearly every bump (the `780` arm almost never matches, so control always reaches the `2034`
arm's `GetAt`, evaluated BEFORE the `goalItemTileId==0xbd` check due to left-to-right `&&`). Fixed
with a `YODA_SIC_FIX`-guarded short-circuit (`tiles.GetSize() > 2034 || (BUGLOG(...), 0)) &&`
prepended to each condition, line-neutral — see the new ⭐ standing lesson in THE ANCHOR section
above for the exact shape (this is the first v84 edit that touched `src/` directly; anchor
re-verified 211/99.17% + all 5 oracles green after). ⭐ **Debugging method worth reusing:** my FIRST
guess (unbounded `MapEntity::charId` in `Tick`/`DrawEntities`, docs/engine-bugs.md #15) was WRONG —
plausible-looking, but a real bug. What actually found the true cause was adding a temporary
`MfxArrayOOBTrap(i, n, "what")` call into `CObArray`/`CWordArray`'s `GetAt`/`SetAt`
(`microfx/include/afxwin.h`, KEPT — cheap, `#ifndef NDEBUG` gated, prints array/index/size + a real
`backtrace()`/`backtrace_symbols_fd` to stderr AND appends to `yoda_crash.log` in cwd) and just
reproducing the crash once. Next OOB assert hunt: reach for this FIRST instead of reading code and
guessing — it named the exact function (`ShowWinMessage`) and exact array in one shot. (#15's guard
is kept as harmless defense-in-depth but was confirmed NOT the fix.)

**▶ v84 — ✅ Hide Me! wired, USER-CONFIRMED.** `WM_SYSCOMMAND SC_MINIMIZE` (posted by
`CDeskcppView::OnCmdMinimize`) reached `CWnd::OnSysCommand`, which was a pure no-op stub
(`microfx/src/app/mfxstubs.cpp`) — the game-logic pause (`bBusy=1`) worked but the window itself
never minimized. Fixed by adding `MfxPlatMinimize()` to the platform-backend contract
(`microfx/include/mfxplat.h`) — implemented in all three backends (sdl3/sdl2 call
`SDL_MinimizeWindow`, null no-ops) — and calling it from `CWnd::OnSysCommand` on `SC_MINIMIZE`. A
new port (DS) just gets a no-op here for free via the null-shaped default.

**▶ v84 — ✅ P pause hotkey wired, USER-CONFIRMED** (was already scoped as a v83 deferred item).
`MfxTranslateAccel` (mfxpump.cpp) only matched FVIRTKEY+FCONTROL chords; broadened to also match
plain (non-modifier) FVIRTKEY entries by comparing `bWantCtrl == bCtrl` instead of requiring both
`fFlags&FCONTROL` AND ctrl-held. This ALSO wires F1→`ID_HELP_INDEX` (0xe142, "How to Play") since it
shares the same code path — **not yet live-verified**, check next session (low priority per user).

**▶ ⚠ STILL BROKEN, live-confirmed 2026-07-11 — investigate next session (was previously assumed
fine-on-code-read or "not a bug"; both assumptions were WRONG, don't repeat them):**
- **F8 debug-info dialog (Ctrl+F8).** Code read (VK_F8 case in `CDeskcppView::OnKeyDown`,
  `GetAsyncKeyState(VK_CONTROL)`, `CTextDialog(0).DoModal()` with template 0xbf) shows nothing
  obviously wrong, and headless synthetic tests (`YODA_AUTOMOD=<start>:<vk>:<dur>` — new this
  session, mfxpump.cpp, holds a modifier's `g_mfxKeyState` without a WM_KEYDOWN dispatch, for
  chord-testing) didn't reproduce a crash either — but the user confirms it's still non-functional
  live. Next step: figure out what "doesn't work" means concretely (dialog never appears? appears
  garbled? wrong template data?) — ask for a screenshot or use `YODA_SHOT` timed around a live
  Ctrl+F8, or check whether dialog template 0xbf actually exists/is well-formed in the Indy .res
  (built via `make_res.py --indy` — the Yoda .rsrc base with only identity resources overridden;
  0xbf is NOT an identity resource, so it's whatever Yoda's own template contains — may not apply
  to Indy's dialog geometry/DDX at all).
- **Statistics menu item.** Previously dismissed as "not a bug — demo limitation" (v80/v83 notes),
  but that reasoning was for the base YODA_VARIANT=DEMO case; GAME_INDY builds ALSO default to
  YODA_VARIANT=DEMO (both `build-indy` and `build-sdl-indy`'s CMakeCache — nobody has ever passed
  `-DYODA_VARIANT=FULL` for an Indy config) which is semantically wrong (DESKADV.EXE has no
  demo/full split — it's the one retail Indy game), so `OnUpdateStatsUi`'s
  `#ifdef YODA_FULL ... #else disabled #endif` (src/DeskcppView.cpp ~8128) disables Stats for Indy
  too. Compare against the established pattern used by `OnUpdateLoadWorld`/`OnUpdateReplayStory`
  (src/WorldgenHelpers.cpp ~665/680): `#if defined(GAME_INDY) || defined(YODA_FULL)`. Adding the
  same `GAME_INDY` branch to `OnUpdateStatsUi` would enable it, BUT: v80's own notes flag the Stats
  dialog TEMPLATE (id 0xe1) as "corrupt in the shipped .res" — enabling the menu item might just
  swap "grayed out" for "crashes/renders garbage" on click. RE the DESKADV.EXE ground truth first
  (does Indy even have a working Stats feature?) before touching the ifdef — don't guess.
- **INDY menu bar** — mfxmenu.cpp only parses Yoda's RT_MENU id=2; Indy's own menu resource (if any)
  in DESKADV.EXE hasn't been checked against this session's parser (unchanged from v83).

**▶ Research note (from the user, unexplored):** SDL3 has a native file-dialog API
(`SDL_ShowOpenFileDialog`, https://wiki.libsdl.org/SDL3/SDL_ShowOpenFileDialog) — worth splitting
`CFileDialog::DoModal`'s current custom row-list picker (mfxdlg.cpp, v80) so a backend can override
it with the OS-native picker where available, falling back to the current custom implementation on
platforms without one (e.g. a future DS port). Not started; would touch the `mfxplat.h` contract
(a new `MfxPlatShowFileDialog`-shaped hook) rather than `mfxdlg.cpp` directly, following the same
backend-split precedent as v82/this session's `MfxPlatMinimize`.

**▶ Carried over, still open:** a DEMO-variant `CFile::Read` assertion crash (`m_pStream` null,
mfxcore.cpp:326) reproduces with ZERO user interaction ~10-15s into idling on the title/intro;
found in v83, not yet isolated to a call site. Reproduce: `cd YodaDemo && ./yoda`, wait ~10s.
`build-sdl`'s default is `YODA_VARIANT=FULL` (not DEMO) — check `build-sdl/CMakeCache.txt` before
assuming.

**▶ GOAL 1 — Indy (main thread candidate):** MIDI audibility now CONFIRMED (v82's backend split
works end-to-end). Remaining stragglers: IACT cmd 0x13 rect arg-order + cond specials
0/8/9/0xb/0x14–0x16 vs DESKADV jump tables; INI replay persistence; OPTIONAL Indy menus
(`make_res.py --indy` extension) — candidate for the menu-bar parser once F8/Stats above are done.

- ⚠ worldgen needs Terrain∈{1,2,3} in the INI (Terrain=-1 ⇒ infinite Generate retry). Harness INIs:
  `<exebase>.INI` next to the binary; doc ctor re-picks the planet EVERY run and writes it back — reset
  before A/B runs. `worldgen_smoke <seed>` · `zone_view <seed> [--zone id] [--dump x.bmp] [--show]` ·
  `game_walk <seed>` · `YODA_SHOT=<pfx>[:n] ./yoda` (captures the composited window incl. menu bar)
  · `YODA_AUTOKEY=<startms>:<vk>:<durms>` · `YODA_AUTOMOD=<startms>:<vk>:<durms>` (v84, holds a
  modifier's key-state only, no dispatch — for chord testing) · `YODA_AUTOCMD=<startms>:<cmdhex>` ·
  `YODA_AUTOCLICK=<ms>:<x>:<y>[,...]` (up to 4 clicks; y<19 hits the menu bar, y>=19 rides normal
  capture-aware routing) · `YODA_DLGSHOT=<path>` · `dlg_smoke` (headless CFileDialog logic unit
  test) · `YODA_ACCEL=1` (OPT-IN SDL_Renderer+texture present path — still not live-verified) ·
  `YODA_VSYNC=1` · `yoda_crash.log` (v84, cwd — `MfxArrayOOBTrap`'s backtrace dump on any
  `CObArray`/`CWordArray` OOB `GetAt`/`SetAt`, appends across runs, deleted/absent = no OOB hit).

**▶ GOAL 3 — Indy Ghidra sweep (agent fodder):** `program=DESKADV.EXE`, ~214 app-code unnamed (seg 1010
=91 doc/parse/worldgen/IACT, 1018=109 view/UI/sound/dialogs, 1020=14 cmd handlers; segs 1000/1008 =
MFC/CRT library, SKIP). Method: twin-rich area → string/import xrefs or caller structure → name `Indy*`
+ plate-comment the Yoda twin. ⚠ match twins by STRUCTURE not 16-bit offsets; data xrefs may be
unresolved (search PUSH of negative DGROUP offset); check gaps for undiscovered functions. This sweep
would also directly help the Stats/F8 investigation above (DESKADV's real special-item tile ids /
debug-dialog wiring, if any).

**▶ Anchor:** 211 exact / 99.17 % (progress.py + bugscan 0 HIGH + vtcheck 10 CLEAN + msgcheck 11
CLEAN + link_exe.sh 0 unresolved/duplicate, ALL re-verified after v84's `src/DeskcppView.cpp` edit —
the FIRST v84 edit to touch `src/` this arc; done via the new line-neutral short-circuit macro shape,
see THE ANCHOR section). All Indy work GAME_INDY-guarded; all H4 work YODA_PORTABLE-guarded; debug
rig YODA_DEBUG-guarded (committed builds OFF). H4 rule of thumb: fix portability in microfx
headers/stubs first; touch a game TU only for __asm / pointer-width casts / old-for-scope / a genuine
crash-class bug, always guarded via `YODA_SIC_FIX`, always re-oracled — and NEVER add an unguarded
include to a byte-matched TU (lesson 6).
