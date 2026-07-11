# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#33 + MFC-matching lessons** (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-10, v72)

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
- **H4 SDL portable target** — ⏳ M0–M4 CORE COMPLETE (docs/phase-h4-sdl.md): ⭐ the game RUNS
  NATIVELY on macOS (`build-sdl/yoda`) — title, intro, game loop, input, walking, zone/door
  transitions, item drag, weapons, FULL AUDIO (WaveMix+MCI over SDL2_mixer), and (v79) FULL
  UI CHROME: HUD panel/health dial/arrows, MS Sans Serif text, MODAL speech bubbles w/ real
  GetMessage + CEdit + buttons, .res strings/icons/cursors, scrollbar, clean teardown — all
  user-confirmed (drag-heal, locator bubbles, close-on-press fidelity). Next: M5 dialogs
  (DoModal from RT_DIALOG) + menus; INDY×SDL in-game playtest.

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

### ⏭ NEXT SESSION PICKUP (2026-07-10 v79 — H4 M4 UI CHROME CORE COMPLETE, USER-CONFIRMED; next M5 dialogs/menus + Indy playtest; anchor untouched)

**▶ v79 this session (H4 M4, commits v79a-g — ZERO src/ edits, all microfx/cmake/tools):**
resources + GDI chrome + text + modal UI + cursors + scrollbar + teardown. ⭐ USER-CONFIRMED:
item click-drag heals via BOTH drop targets (player + health circle), locator textboxes (the
v78 test case) work, and button close-on-PRESS matches the original ("accuracy of the MFC
implementation is actually really surprising"). Full inventory + new lessons 9-13 in
docs/phase-h4-sdl.md. Headlines:
- **res/mfxres.cpp**: embedded yoda.res (tools/bin2c.py; PortableSDL.cmake mirrors the
  demo/full/indy make_res variants) → LoadString (ALL bubble/HUD text), LoadIcon+DrawIcon
  (HUD arrows), LoadCursor, named RT_BITMAPs (bubble buttons); MfxImg decode + MFXIMG API.
- **gdi real**: pens/brushes/fonts + per-DC draw state + FillRect/PatBlt/Pie/RoundRect/
  Polygon/lines/pixels/GetClipBox + Win95 GetSysColor (feeds sysPalette[1..3]). ⭐ Lesson 9:
  DrawGameArea's GetPixel probe needs WM_ERASEBKGND before WM_PAINT or every blit becomes a
  full-redraw storm. ⭐ Lesson 10 (user-detected): color matching must SKIP PC_RESERVED
  cycling-ring entries (health-dial flash); LineTo EXCLUDES its endpoint (bevel off-by-one).
- **text**: genuine MS Sans Serif 13px/16px FNT strikes committed (microfx/src/gdi/
  mfxfont_data.c via tools/fon2c.py from a real SSERIFE.FON); strike mapping -8/-14 +
  synthesized bold; TextOut/GetTextMetrics/GetTextExtentPoint32.
- **modal UI**: GetMessageA/DispatchMessageA REAL — SDL events + timers become MSGs in the
  ONE posted queue that both CWinThread::Run and the game's own modal loops drain (msg.pt =
  cursor pos; mouse hit-tests to visible child windows, capture-aware). Word-wrapping CEdit
  + CBitmapButton + window registry; children re-composite via a gdi OVERLAY hook fired on
  every screen write (one shared surface — DrawGameArea would erase them), presents batched
  by MfxTouchHold/Release. Lesson-11 gotchas: buttons stay WM_SETREDRAW(0) forever yet must
  draw; a redundant ShowWindow(SW_SHOW) must still dirty the frame (black-boot regression).
  Lesson 12: GetMessageA returns 0 when no SDL window exists — game_walk hung at 100% CPU
  in a modal wait until that guard landed.
- **cursors** (user-detected regression → redesign, lesson 13): SOFTWARE cursor composited
  over the window surface at present time (chunky at window scale; the DS-port path);
  YODA_HWCURSOR=1 opts into SDL hardware; never-set/IDC_ARROW keep the OS arrow;
  SetCursor(NULL) hides (faithful keyboard-move/drag behavior).
- **M4e**: Win95-chrome SB_CTL scrollbar (bevel arrows/checker track/thumb drag →
  WM_VSCROLL(SB_*) to the parent → the reflected InvScrollBar handler) + real teardown on
  pump exit (view dtor → WaveMixCloseSession; YODA_SNDLOG=1 shows "[snd] session closed").

**▶ GOAL 2 — H4 next = M5 dialogs + menus (the main thread):**
- CDialog::DoModal real: parse RT_DIALOG templates from the embedded .res
  (tools/reslib.py parse_dlg32 = the format reference), create child controls, run a modal
  loop (GetMessage already real). Order: F8 StatsDlg first (simple), then About,
  Difficulty/GameSpeed/WorldSize sliders (1 CScrollBar + OnHScroll each — plumbing exists),
  save/load last (CFileDialog needs a picker; consider fixed-slot fallback).
- Menus: ON_COMMAND ids are ints — keyboard map or minimal menu bar can post them
  (WM_COMMAND routing view→frame→app works). The .res blob still carries menu id 2.
- USER-CONFIRMED post-v79g: bubble UP/DOWN scroll buttons work held AND tapped; inventory
  scrolling fixed in v79h (::GetParent was still a stub returning 0 → InvScrollBar's
  FromHandle(GetParent())->DrawText(0) repaint never ran; scrollbar moved, items stale).
  Audit hint for future symptoms of the shape "X updates but Y doesn't": grep mfxstubs.cpp
  for the USER/GDI call the Y-path depends on — a silent 0-returning stub is the pattern.
- ⏳ USER-VERIFY: health-dial flash + bevel fixes (v79b); cursor feel (v79f).
- ⚠ worldgen needs Terrain∈{1,2,3} in the INI (Terrain=-1 ⇒ infinite Generate retry).
  Harness INIs: `<exebase>.INI` next to the binary; doc ctor re-picks the planet EVERY run
  and writes it back — reset before A/B runs. `worldgen_smoke <seed>` · `zone_view <seed>
  [--zone id] [--dump x.bmp] [--show]` · `game_walk <seed>` · `YODA_SHOT=<pfx>[:n] ./yoda` ·
  `YODA_AUTOKEY=<startms>:<vk>:<durms>`. Bubbles in automated runs: dismiss via
  `osascript` (activate the "yoda" process, then `key code 36`) — v79 technique; the pump's
  shot counter stalls while a modal loop runs (expected — Run is blocked inside GetMessage).
- Perf note (fine to ignore): scripted intro flights present per BitBlt (~55% CPU during
  busy-wait IACTs). If it ever bothers: rate-limit the present hook (~8ms), keep BitBlt
  immediate for transitions.

**▶ GOAL 1 — Indy (after M5 or user-led):** ⏳ INDY×SDL in-game playtest (`YodaIndy/yoda_sdl`,
rebuilt v79 — the .res pipeline now feeds it Indy resources incl. Indy cursors/strings); then
the stragglers: IACT cmd 0x13 rect arg-order + cond specials 0/8/9/0xb/0x14–0x16 vs DESKADV
jump tables; INI replay persistence; OPTIONAL Indy menus (`make_res.py --indy` extension).

**▶ GOAL 3 — Indy Ghidra sweep (agent fodder):** `program=DESKADV.EXE`, ~214 app-code unnamed (seg 1010
=91 doc/parse/worldgen/IACT, 1018=109 view/UI/sound/dialogs, 1020=14 cmd handlers; segs 1000/1008 =
MFC/CRT library, SKIP). Method: twin-rich area → string/import xrefs or caller structure → name `Indy*`
+ plate-comment the Yoda twin. ⚠ match twins by STRUCTURE not 16-bit offsets; data xrefs may be
unresolved (search PUSH of negative DGROUP offset); check gaps for undiscovered functions.

**▶ Anchor:** 211 exact / 99.17 % (progress.py re-verified during v79). v79 made ZERO src/ or
shared-header edits (microfx/cmake/tools only — the anchor build never sees those trees), so the
remaining oracles cannot have moved; run the full table around any future shared-TU edit as
usual. All Indy work GAME_INDY-guarded; all H4 work YODA_PORTABLE-guarded; debug rig
YODA_DEBUG-guarded (committed builds OFF). H4 rule of thumb: fix portability in microfx
headers/stubs first; touch a game TU only for __asm / pointer-width casts / old-for-scope,
always guarded, always re-oracled — and NEVER add an unguarded include to a byte-matched TU
(lesson 6).
