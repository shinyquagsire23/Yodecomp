# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#40 + MFC-matching lessons** (later lessons #41–#47 are standing bullets in this file) (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-11, v87; re-baselined 217→234 at v100 (MEASUREMENT FIX); 234→237 at v102, 237→240 at v103, 240→244 at v104, 244→247 at v105, 247→249 at v106, 249→250 at v107, 250→251 at v108, 251→255 at v110 (REAL MATCHES); **255→252 at v114 — a DELIBERATE, user-approved re-baseline DOWN**, see below; held at 252 at v115)

⛔ **v114 RE-BASELINED THE ANCHOR DOWN, 255 → 252, ON PURPOSE (user-approved).** This is the
first deliberate DECREASE in the project's history and it is not a regression to bisect. A
length-PROVEN source fix to `WorldgenShuffleList` 0x41ef90 (the unrotated loop form + decl
order, **269 B → 16 B**; see lesson #46 below) re-rolled the v105 TU-joint phase and cost three
DOWNSTREAM functions their byte-exactness: `CheckZoneItemsAvailable` 0x41f830 (9 B),
`SetCurrentToIntroZone` 0x423d20 (2 B), `DetonateAdjacentTiles` 0x428680 (60 B) — all pure
register bijections, none of them a defect in its own body. The trade was taken because the
gained form is refuted-from-the-other-side by LENGTH (Ghidra's extent for 0x41ef90 is 396 B; the
old `while` spelling emits 400, the new one emits 396) and because recovery was MEASURED to be
impossible: 12 decl configs on 0x423d20 and 5 on 0x41f830 bottom out at their current values,
0x428680 was proven phase-only at v39 and v105, and all 8 unrotated spellings give
BYTE-IDENTICAL downstream damage. `PlaceQuestNode` 0x41f120 also GAINED (432 B → 405 B).
⇒ **252 is the new floor. Do not "fix" it back to 255 by reverting 0x41ef90.**

⚠ **v115 held at 252** — no new byte-match, but two residuals were cut and a NEW dial named:
`StartGame` 0x4037a0 went 79 B → 69 B on the IF/ELSE ARM ORDER (lesson #47, new) and `IactRun`
0x406780 went 1548 B → 1538 B on a second in-condition assignment. Both functions are far too
large to reach exact, so a flat count here means "the wins landed in big functions", not
"nothing happened". v115 also CORRECTED a stale oracle expectation: bugscan's green state is
**1 HIGH (known benign)**, not 0, and it had been wrong for an unknown number of sessions.

⚠ v111, v112 and v113 all held at **255** — no new byte-match. v113 was still productive:
`RemoveEmptyZonesFromPlacedList` 0x403070 went 26 B → 24 B on the LOOP FORM (lesson #40) and the
fix was structural, not cosmetic — `this` moved into ESI as the original has it. Three cheap-band
residuals were swept to a measured floor with zero gain (135 configurations total), and the
`tools/loopform.py` scan that found the win is now checked in. Read a flat exact count as "the
cheap band is search-limited", not "nothing happened".

⚠ v112 note:
`WriteSavedState` 0x405f30 went 20 B → 7 B (lesson #45), and the joint search v111 asked for is
built and returned a measured NEGATIVE. Read a flat exact count as "the cheap band is
search-limited", not "nothing happened".

Phases A–G (byte-matching YodaDemo.exe's app region): **252 functions byte-exact / 99.17 % coverage** (v100's
+17 was a **MEASUREMENT CORRECTION, not new matching** — `progress.py` had been under-counting; v102's +3 IS
real matching — the `CWnd::SendMessage` member-form find, see the standing lesson below),
every function transcribed (exact or annotated-EFFECTIVE), a runnable `/OPT:REF`-linked image, all
oracles green.

⛔ **CLOSED AGAIN at v101 (2026-09-02) — the v96 re-opening was a HARNESS ARTIFACT.** v96 claimed
`tools/dialsweep.py` reached "215 exact project-wide with FOUR GAINED AND ZERO LOST" from ~7 missing
file-scope symbols, and set the standing quest "find the real seven". **That is retracted.**
`dialsweep.exact_set()` still carried the PRE-v100 COMDAT filter (no `hinted` exception) → the same
28-mis-pair positional cascade v100 fixed elsewhere; and `membertest.py`/`headersweep.py`/
`enumfieldtest.py` ALL measure through it, so **every sweep number ever published here inherited the
bug** (v97's "members are INERT" included — treat as UNVERIFIED; v99 had already dented it).
Fixed in the one shared place; dialsweep's baseline now agrees with the anchor at **234** (pre-fix it
said 211). Re-measured: extern/struct/typedef all agree with each other (the dial IS pure symbol
count — that part of v96 survives) but **no position beats baseline** — n=7 gives 227 with **+0
gained / −7 lost**, not 215/+4/−0. The two functions v96 said DeskcppView.cpp was "6-8 symbols short"
of gaining (`0x40ebe0`, `0x40fca0`) are **already byte-exact at the corrected baseline** — they were
among the 17 the v100 fix recovered; and `ParseZaux` 0x423110 differs in **78 of 116 bytes**, not a
one-declaration near-miss. The only ever-gained function (`0x40a320`) always costs 8 losses — a TRADE,
i.e. the fingerprint of padding. ⇒ **Do not resume the "missing symbols" hunt.** 234 is the honest
plateau; the residuals are ordinary source-fidelity work (`tools/residuals.py`). Detail:
docs/compiler-hunt.md v101.

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
4. **⭐ WASM port (user-set 2026-07-11) — ✅ CORE SHIPPED v88** (playable in Chrome + Firefox,
   user-tested): `emcmake cmake -B build-wasm -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL`. The
   feared blocking-modal-loop lift dissolved: **ASYNCIFY + zero game-code restructuring** —
   every blocking wait already funnels through `MfxPlatDelay()`, which the sdl3 backend routes
   to `emscripten_sleep()` under `__EMSCRIPTEN__` (plus a yield after each present so busy-wait
   animation loops display mid-handler). `mfxplat_sdl3.cpp` IS the wasm backend (emscripten
   ships an SDL3 port, `--use-port=sdl3` — the browser is just another SDL3 platform; no
   separate mfxplat_wasm.cpp needed). Audio = new `mfxsnd_sdl3stream.cpp` (SDL3-core streams;
   no SDL3_mixer port exists; Yoda ships no .mid so SFX ≈ full audio — Indy-wasm MIDI needs a
   soft-synth later, e.g. TinySoundFont + GM .sf2). Two asset modes (user-set):
   `YODA_WASM_PRELOAD=ON` default (DTA/INI/sfx baked into yoda.data — automation/self-testing) /
   `OFF` (SHIPPABLE page, zero game data; `--pre-js` picker `microfx/web/mfx_asset_picker.pre.js`
   copies the user's folder into MEMFS pre-main). Remaining tails: INI/save persistence across
   reloads (IDBFS), Indy-wasm MIDI, user-found gameplay deltas. See "WASM build/debug" below.
5. **⭐ Android port (user-set 2026-07-18) — ✅ CORE SHIPPED v92** (touch, emulator-tested):
   `cmake --preset android-demo && cmake --build --preset android-demo` → one `.apk` (needs
   `ANDROID_NDK_HOME` + SDK). Same "phone = just another SDL3 platform" story as wasm — the v82
   backend split meant NO new backend file, just `#ifdef __ANDROID__` deltas in `mfxplat_sdl3.cpp`
   (+ a data-dir branch in `mfxstubs.cpp`); anchor & game TUs untouched. Deliberately ONE CMake
   target (user's ask, improving on OpenJKDF2's shell dance): SDL3+mixer **static-linked into a
   single `libmain.so`** (`getLibraries()={"main"}`), and `--target apk` cross-compiles it, builds
   sibling ABIs, stages libs+baked-assets+SDL-Java+icons into a build copy of `packaging/android/`,
   and drives gradlew (a dumb packager — no externalNativeBuild). Touch: fullscreen letterbox
   renderer, multitouch on-screen **Push/Pull (Shift)** + **Attack (Space)** buttons, tap=mouse
   walk; APK assets extracted to internal storage on first launch (saves/INI persist there). See
   "Android build/debug" below + BUILDING.md "Build: Android `.apk`". Tails: x86_64 second ABI
   (building), Indy MIDI (timidity needs patches, like wasm), on-device playtest, icon/UX polish.

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

⭐ **Named-constant DIALS (v95/v96, measured) — the three ways "just naming a magic number" costs
byte-exactness.** All confirmed by A/B with `tools/progress.py`; none are theoretical:
1. **A new `#include` FILE in a byte-matched TU's chain costs a function — even if the file is
   EMPTY** (v95: `Worldgen.cpp` 34→33, −80 B). ⇒ new shared constants go at the **TAIL of an
   ALREADY-INCLUDED header**, never in a new file. (Canonical home: the "═══ Resource ids ═══"
   block at the tail of `GameObjectClasses.h`; TUs that can't see it carry their few ids at their
   own header's tail — `Deskcpp.h`, `TextDialog.h`, `MainFrm.h`, `IactScript.h`, `DeskcppView.h`.)
2. **An `enum` in a header a byte-matched TU can see is a dial input** (v96: `enum ArtooHint` at
   `DeskcppView.h`'s tail cost **6** functions, 211→205 — it reaches `Worldgen.cpp`/
   `WorldgenHelpers.cpp` via `Worldgen.h`). The same constants as `#define`s cost nothing.
   ⇒ **pure `#define` for anything a byte-matched TU sees.** Enums are fine only in headers no
   anchor TU includes. (Note this partially overrides the general "prefer enums" convention —
   inside the anchor, defines win.)
3. **`sizeof(T)` swapped in for the equivalent literal is a dial input too**, despite folding to
   the same constant (v96: `h->biSize = sizeof(BITMAPINFOHEADER)` cost 1 function, `Canvas.cpp`
   9→8). See the DIAL NOTE at `Canvas.cpp` EOF — the literal there is deliberate, not sloppy.
⇒ And the anchor is **not the only oracle**: naming MFC/Win32 constants can break the portable
build while `progress.py` stays green (v95 did exactly that — microfx lacked `CLR_INVALID`,
`WS_MAXIMIZE`, `SM_CXDLGFRAME`, `ES_NOHIDESEL`, `OFN_EXPLORER`, …). Build `build-sdl` too.

⭐ **THE INLINE-MEMBER IDIOM (v99 — a MATCHING lever, not a dial trick; full write-up =
PLAN_COMPLETED.md lesson #34).** A redundant `mov reg,reg` feeding a thiscall, where the vtable
load uses the ORIGINAL register instead of ECX — `mov eax,[esp+4]; push 0; mov ecx,eax;
mov edx,[eax]; call [edx]` — is the fingerprint of **inlining a non-static MEMBER** (its implicit
`this` nominally owns ECX, so the arg must be staged in EAX). `static __inline` free functions,
`static` members, local copies, casts and references ALL fold to the tight `mov ecx,[esp+4]` form.
Reproduce it by making the helper a real member. This landed all five demo-grayed `OnUpdate*`
stubs (0x403510/0x403600/0x403610, 0x4165a0/0x416800) that had been parked as "allocation
artifact" since v34. ⚠ such a diff shows up as idiomscan **class D**, not class B.

**Anchor oracles — run after ANY shared-code edit, all must hold:**
| oracle | command | green state |
|---|---|---|
| exact count | `python3 tools/progress.py` | **252 exact / 99.17 % transcribed** |
| full link | `tools/link_exe.sh` | 0 unresolved / 0 duplicates / exit 0 |
| field/slot bugs | `python3 tools/bugscan.py --all` | **1 HIGH (known benign) / 0 SHIFT** |
| vtables | `python3 tools/vtcheck.py` | 10 classes CLEAN (+13 skipped, unanchorable) |
| message maps | `python3 tools/msgcheck.py` | 11 maps CLEAN |

⚠ **The bugscan green state is 1 HIGH, not 0 — corrected at v115.** The finding is
`StartGame` 0x4037a0 `@+0x14a lea base=esi orig=0x4b4 ours=0x4b0`, and it is a PROVEN
FALSE POSITIVE that pre-dates v115 (measured at HEAD too, so earlier pickups recording
"0 HIGH" were stale). bugscan compares the (base, displacement) SPLIT of a field access;
the original anchors the grid loop's `mz` induction pointer at `&zones[0].id` (0x4b4,
because it hoists `mz++` above the first store and uses negative displacements) while we
anchor at `&zones[0]` (0x4b0). Every one of the 30 grid stores has `ours_disp ==
orig_disp + 4`, exactly cancelling the anchor difference — the effective addresses are
IDENTICAL. ⇒ Do not "fix" it, and do not add a suppression list to bugscan (hiding a
finding is the wrong direction for an instrument — see the harness-can-lie lessons);
the verdict is recorded in the function's source note.

⚠ **252 is the CURRENT baseline (234 at v100, +3 REAL at v102, +3 REAL at v103, +4 REAL at v104, +3 REAL at v105, +2 REAL at v106, +1 REAL at v107, +1 REAL at v108, +4 REAL at v110, **−3 DELIBERATE at v114**; all five oracles
re-run in the same pass).** ⭐ **v100's +17 was a MEASUREMENT CORRECTION, not 17 new byte-matches** — those functions
were ALREADY byte-exact and were being scored against the WRONG addresses; do not read it as progress on
matching. **v102's +3 and v103's +3 ARE matching** (v102: the TextDialog scroll family, via `CWnd::SendMessage`;
v103: `ParseSnds` via a buffer SIZE, `OnEraseBkgnd` + `CyclePalette` via the member-call form;
v104: four via DECLARATION SCOPE, lesson #37; v105: the decl SET+ORDER refinement, lesson #38;
v106: the statement-ORDER-around-a-materialized-constant lever, lesson #39; v107: SetCurrentToIntroZone
via lesson #38, plus SaveZoneRecursive via the new LOOP-FORM lever, lesson #40; v108: ParseTilesMaybe
via decl ORDER at function scope, lesson #38 — found only because the loop-form sweep came back FLAT). The project-wide per-TU count is
the thing that must never drop; re-baseline deliberately, never let it drift.

⭐ **THE MFC MEMBER-CALL FORM IS A MATCHING LEVER (lesson #35; v102 — the argument-ordering
guise of lesson #34).** `pWnd->SendMessage(msg, w, l)` and `::SendMessage(pWnd->m_hWnd, msg, w, l)` are
semantically identical and MFC's member is a thin inline — but they are NOT codegen-identical when
the object expression is non-trivial. The member's implicit `this` (here the `pParentView->ctrl`
address) must be evaluated BEFORE the argument pushes; the global form lets cl fold the `m_hWnd`
load in among them. Fingerprint: **same instruction count, same registers, one load sitting 1–2
bytes earlier/later around a run of `push`es** (`align 12`, `reg_pen 0`). That signature had three
TextDialog functions parked as "EFFECTIVE — schedule shift" since G1; switching to the member form
made all three exact (+3, zero regressions). ⇒ when a residual is a load shifting across argument
pushes, **try the MFC member wrapper before touching the schedule**. Corollary measured the same
session: where the object expression is a bare `m_hWnd` (this-relative) or a plain `CScrollBar*`
with a constant `SB_CTL`, the two forms fold to identical bytes — inert, so don't churn those.

⭐ **v103 EXTENDS #35 — the lever is far broader than SendMessage, and the "non-trivial object"
rule is now measured twice more.** It is NOT a scroll/window-message idiom; it applies to any MFC
inline wrapper. Landed at v103: `pDC->PatBlt(...)` (OnEraseBkgnd, +1) and
`pWorld->pPalette->AnimatePalette(...)` (CyclePalette, +1); improved DrawDirectionArrows 28 B→21 B
via `pDC->FillRect(&rc, &br)`. ⚠ **Two surprises worth carrying forward:**
1. **It can drive things that look nothing like argument order.** OnEraseBkgnd's entire 6 B residual
   was the *tail funclet ORDER* (orig emits the `__ehhandler` thunk before the `~CBrush` unwind
   stub; ours emitted them swapped). The old note there called that axis "not source-steerable" —
   wrong. So a residual in EH/funclet layout is NOT automatically a park; probe the call forms.
   (A binary-wide survey confirms both orders occur in both images: 89 eh-first / 6 unwind-first in
   the original, near-identical in ours — so cl chooses, and source can steer the choice.)
2. **Selectivity is real and not intuitable — MEASURE, don't reason.** In CyclePalette, converting
   BOTH `AnimatePalette` calls to the member form gives 6 B, converting only the FIRST gives 0 B,
   and converting both *plus* GetDC/RealizePalette/ReleaseDC also gives 0 B. Enumerate the
   combinations with `tools/vartest.py`; don't assume a conversion is monotonic.
⇒ Targeting rule that pays: scan for global-form calls whose object expression is a **pointer chain**
(`pWorld->pPalette->m_hObject`, `pInvScrollBar->m_hWnd`, `pBitmap->m_hObject`). Ones rooted at
`m_hWnd`/`pDC->m_hDC` are usually inert — re-measured inert at v103 on both MainFrm palette handlers
(all 8 variants gave 54 B) and on `DrawIcon`.

⭐ **A LOCAL BUFFER'S DECLARED SIZE IS A DIAL — and it is invisible in the padded frame (v103,
lesson #36).** `ParseSnds` 0x4233f0 sat at 5 B since v36 as a "frame-slot order" park: the original
lays the four char buffers out ascending {ext,fname,name,path}, ours put name before fname. v36
compiled ALL 24 decl-order permutations, found every one identical, and correctly concluded slot
order is decl-order-INVARIANT — then wrongly generalised that to "irreducible from the source side".
It never varied the **sizes**. `char fname[9]` (a DOS 8.3 basename + NUL) instead of `char fname[12]`
makes it byte-exact. ⇒ **When a residual is a set of `lea reg,[esp+N]` displacement diffs, solve the
original's frame layout from the disassembly and treat every buffer's DECLARED size as a free
variable.** Padding hides it: 9, 10, 11 and 12 all occupy the same 12-byte slot, yet only 9–10 match.
⚠ The oracle pins a FAMILY, not a point (exact for fname 9–10 × ext {5,6,8} × name 14–16), and the
underlying cl 10.20 key stays opaque — name 13 and 16 have identical padded slots but only 16
matches. So pick the idiomatic value inside the measured window and say so; don't invent a sort rule.
(Method note: solve the frame by tracking `esp` through the prologue/pushes and normalising every
`[esp+N]`; every byte of the original's 0xac frame accounted for exactly.)

⭐ **A LOCAL'S DECLARATION SCOPE IS A REGISTER-ALLOCATION DIAL (v104, lesson #37) — the lever that
cracks the "pure register permutation" class.** cl 10.20 assigns registers differently depending on
whether a local is declared at FUNCTION scope or inside an inner block (loop body / if body /
for-init), even when the emitted work is instruction-for-instruction identical. Fingerprint:
**the residual is explained ENTIRELY by a consistent register renaming** (esi↔edi, ebx↔ebp, a
3-cycle) — same mnemonics, same schedule, same operand order, only register NAMES swapped. A
permutation census found **10 of 138 residuals** in that class; the lever landed 4 of them.
- ✅ Landed at v104: `IactScript::~IactScript` 0x4187e0 (7 B→0, hoist `CObject *p`),
  `Zone::~Zone` 0x4054d0 (12 B→0, same), `PlaceZoneObjectTiles` 0x403140 (22 B→0),
  `LoadZoneRecursive` 0x403450 (fell out exact alongside), `FindObjectAt` 0x405330 (11 B→2 B).
- ⚠ **It is DIRECTIONAL, not a free knob.** `CDeskcppDoc::~CDeskcppDoc` 0x41b2f0 really does scope
  `p` to the loop body — hoisting DOUBLES its residual (6→12). `WorldgenCollectZoneRefs` 0x41f8e0
  likewise (9→16). That asymmetry is what makes a win EVIDENCE about the 1997 source rather than a
  number to game: the author wrote C-style "declare at the top" in some functions and not others.
- ⭐ **Two refinements, both measured.** (1) **Decl ORDER at function scope matters**:
  PlaceZoneObjectTiles is exact with `Zone *z` declared BEFORE `ZoneObj *o`, and 22 B with it after.
  (2) **The original sometimes reused ONE variable where we had two** — PlaceZoneObjectTiles needed
  `t2` merged into a single `t` serving both switch arms; hoisting alone only got it 22→10.
- Sometimes only a PAIR of hoists moves anything (`Zone::WriteSavedState` 0x405f30: every single
  hoist = 13 B, any pair = 7 B), because the count/order of function-scope decls is itself the input.
⇒ Tool: **`tools/hoisttest.py <tu.cpp> <0xADDR> --expect N`** enumerates the hoist subsets (and both
decl orders) and delegates measurement to `vartest.py` verbatim, inheriting the anchor's exact
predicate + the `--expect` baseline guard. ⚠ Its variants are line-neutral by construction (decls are
crammed onto the first body line); when you APPLY a win, re-check the WHOLE TU — a +4-line natural
reformat of `FindObjectAt` cost a DIFFERENT function in the same TU (lesson #23 in the wild), while
the line-neutral spelling of the same change cost nothing.

⭐ **v105 EXTENDS #37 into LESSON #38 — the dial is the SET of function-scope locals AND their
ORDER, and it can be ASYMMETRIC between two identical branches.** `hoisttest.py` asks a yes/no
question per declaration; that is too coarse. For `ZoneHasIzxItemMaybe` 0x41bfa0 the winning
configuration is: `nCount` at function scope (BOTH branches assign the one variable), the **else**
branch's `i` at function scope, and the `sel!=0` branch keeping its **own** `i` that SHADOWS it.
Descent: 17 → 10 (`nCount` declared before `i`) → 8 (3-name subset) → 7 (only `nCount`, from both
branches) → **0**. Plain all-hoist never gets below 8. ⚠ a flat `--max-hoist 1` probe is NOT a
dead end — escalate to a hand-written set×order sweep before parking.

⭐ **A TU HAS A JOINT REGISTER-ALLOCATION PHASE — residuals in one TU are NOT independent (v105).**
`ParseZaux` 0x423110 / `ParseZax3` 0x423190 / `ParseZax2` 0x423210 are TEXTUALLY IDENTICAL (only
the callee name differs; the three declarations match too) — yet each image allocates registers
differently, and a DIFFERENT one deviates in each: `ParseZax3` in the original, `ParseZaux` in
ours. Our `ParseZax3`'s codegen IS the original `ParseZaux`'s allocation. So a PERM residual can
be unreachable from its own function's source (0x423190 survived 18 spellings). Two more
confirmations: `DetonateAdjacentTiles` gained on a 5-line shift 5000 lines earlier, and
`ZoneFindInIzxList`'s exact spelling costs exactly 4 downstream functions **even when the edit is
line-neutral** — so it is the TOKEN change re-rolling the phase, not lesson #23.
⇒ Three consequences: (1) a park note saying "intrinsic / not source-steerable" is a record of
which axis was probed, never proof — v39's on 0x428680 was retracted this way; (2) before grinding
a PERM residual, look for a textually identical SIBLING in the same TU — if one is exact and the
other is not, the source is not the variable; (3) a greedy one-function-at-a-time search WILL hit
trades, so a saturated TU needs a JOINT search over several functions' decl configurations.
⇒ Measure with **`tools/exactset.py` + `comm`**, never progress.py's total alone: the total
reports a +2/−4 as "−2" and never says WHICH functions moved.

⭐ **STATEMENT ORDER RELATIVE TO A MATERIALIZED CONSTANT IS A DIAL (v106, lesson #39).**
`LoadWorldStateFile` 0x423850 and `Serialize` 0x423b30 each sat at DIFF(2), parked since G1 as an
"inc-vs-add tie-break": ours `inc [nDone]`, the original `mov ecx,1; add [nDone],ecx`. The cause
is NOT the increment's spelling — `nDone++`, `nDone += 1`, `nDone = nDone + 1` and `++nDone` ALL
fold to the identical `inc`. It is the increment's POSITION among the neighbouring `= 1` stores:
the block writes `bHidePlayer = 1; bWorldReadyMaybe = 1; nMapChangeReason = 1;`, so cl
materialises 1 into ecx for those, and an increment sequenced AFTER the first such store folds
into that live register as `add mem,ecx`. Measured: position 0 = 2 B, positions 1–6 ALL EXACT,
`nDone = 1` = 9 B. +2 / −0.
⇒ **When a residual is an immediate-vs-register form** (`inc mem` vs `add mem,reg`; `mov mem,imm`
vs `mov mem,reg`), the lever is the STATEMENT ORDER around the constant's other users, not the
arithmetic spelling. The oracle pins a FAMILY — pick the idiomatic member and say so (lesson #36
discipline). ⚠ Mined out for now: all 29 remaining `inc/add` sites are in 78 B+ residuals.

⭐ **THE TU JOINT PHASE IS DOWNSTREAM-ONLY (v106 — sharpens v105).** Every function that moved
under the ZoneFind edit sits AFTER it in file order, and an edit confined to the TU's LAST
function (`RemoveItem` 0x429150) moves NOTHING (247→247, +0/−0). ⇒ a joint search IS tractable:
an upstream edit can never break a fix earlier in the file. ⚠ **Corollary: per-function byte
counts in a pickup are PHASE-RELATIVE, not absolute** — after landing any change, every stale
residual number for a LATER function in the SAME TU is invalid. `vartest --expect 13` on
`WriteSavedState` hard-failed immediately after the ReadSavedState fix (it had moved to 20 B);
the v100/v101 baseline guard caught it automatically, which is exactly what it is for.

⭐ **THE LOOP FORM IS A DIAL — AND A FLAT DECL SWEEP IS THE SIGNAL FOR IT (v107, lesson #40).**
`SaveZoneRecursive` 0x4033b0 sat at DIFF(6) since G1, annotated as a "walker/counter ebx<->ebp
2-cycle" — the canonical lesson-#37 fingerprint. It is not one: **all 9 hoist / decl-order /
decl-set variants measured 6 B, dead flat.** The lever is the shape of the loop. The original
writes the house countdown `int n = X.GetSize(); if (n > 0) { int i = 0; do { ...; i++; n--; }
while (n != 0); }` rather than `for (int i = 0; i < n; i++)` — the recipe PlacePuzzle's delete-loop
note already documented, now recognised as a general probe. All five spellings of the idiom give
0 B (so the oracle pins a FAMILY — pick the house member, lesson #36); the `n > 0` GUARD is
load-bearing (an unguarded do-while emits 145 B vs the original's 149).
⇒ **When a residual looks like a walker/counter register 2-cycle and every declaration variant is
flat, stop treating it as a register problem and vary the LOOP FORM.** A flat decl sweep is a
POSITIVE signal for this lever, not a dead end.
⚠ **Not universal — and the emitted LENGTH refutes it instantly where it is wrong**, which makes
it cheap to probe: countdown gives 165 B vs 177 on `LoadZoneRecursive`, 77 B vs 79 on
`FindObjectAt`, 204 B vs 206 on `RemoveEmptyZones` loop 2, and is byte-IDENTICAL (inert) on that
function's loop 1. `WorldgenCollectZoneRefs` 0x41f8e0 reaches 9 B → 7 B at identical length
(parked — see its source note). So textual mirrors can genuinely differ in loop form.
⚠ **VC 4.2 has OLD for-scope**: `for (int i ...)` leaks `i`, so two such loops in one function is
a redefinition ERROR. That bounds what the 1997 author could write in every multi-loop function
(and is why `RemoveEmptyZonesFromPlacedList` uses `i` and `j`).

⭐ **THE CALLEE-SAVE SET IS THE CHEAPEST DIAGNOSTIC IN THE PROJECT (v110, lessons #42/#43) —
`tools/savescan.py`.** A function's prologue `push ebx/esi/edi` set is a one-line summary of how
many long-lived values its body needs, and it sits at the TOP: one extra save shifts EVERY later
byte, so a handful of real decisions present as hundreds of differing bytes. **Fix the save set
first** — most of the residual is an artifact of the shift. This single scan drove all four of
v110's wins, and it now reports **0 mismatches across 123 residuals** (seam mined out; re-run it
on newly-transcribed functions). Read the two directions differently:
- ⭐ **Ours saves MORE ⇒ WE materialised something the original recomputes (lesson #42, the
  CSE-TEMP / LICM dial).** `SaveStoryHistory*` wrote `int idx = k + lineNo * 10;` before an
  if/else and used `idx` in both arms. Naming a subexpression whose multiply is loop-INVARIANT
  let cl hoist `lineNo*10` into EBX before the loop; that cost a callee-saved register, leaked
  into the OTHER loop's arms (they landed the buf pointer in different registers), broke the
  cross-jump of their shared `[lea buf; inc esi; push buf; call]` tail (+16 B) and rotated three
  frame slots into a 3-cycle. Repeating the subscript INLINE in both arms makes cl CSE it to
  just above the branch and recompute per iteration — the original's shape. **611 B → 22 B ×3.**
  Same family, different guise: a global-form call on a POINTER CHAIN
  (`::GetNearestPaletteIndex((HPALETTE)pWorld->pPalette->m_hObject, GetSysColor(...))`) let cl
  keep `pWorld` alive in EDI across the inner call as a CSE; the MFC member form
  (`pWorld->pPalette->GetNearestPaletteIndex(...)`) forces the reload. **DrawTextA 663 → 60.**
- ⭐ **Ours saves FEWER ⇒ the ORIGINAL kept a value alive across a call that we spell as an
  ARGUMENT EXPRESSION (lesson #43, the named-local lever).** Give it a name and assign it
  BEFORE the neighbouring call. `CheckCheat` 0x415820 (parked since G1 as "a whole-function
  register-role swap / G1 dial territory") went **372 → 0** on
  `int x = pWorld->playerX * LOCATOR_CELL_SIZE + 18;` / `int y = ...;` placed ahead of the
  `str = "Invincible!"` assignment, at BOTH call sites. `ConfirmExit` 0x416030 went **10 → 0**
  the same way: `CWinApp *pApp = AfxGetApp();` and `CWinThread *pT = (CWinThread *)pMusicThread;`
  as named locals. The shape is pinned from several sides, which is what makes it evidence and
  not tuning: x must precede y (y first = 4 B), the pair must precede the `str =` assignment
  (after it = strictly worse than doing nothing), and `pApp` must be INSIDE the `if` body
  (hoisted out = 19 B, worse than baseline). ⇒ **Scan target list: non-exact functions
  containing `f()->m(...)`, `((T *)p)->m`, `a->b->c(...)` or `->m_hXxx,` as an argument.**
- ⭐ **The original saves NOTHING at all ⇒ suspect a THUNK, and CHECK THE EXTENT before touching
  the body.** `OnAppExit` 0x416110 is FIVE BYTES — `jmp ?ConfirmExit@...`, the /O2 tail call for
  a one-line forwarder — and we had transcribed the twin's whole 220-byte body there, scoring it
  against a different function and manufacturing a 163 B "residual". ⚠ progress.py compares OUR
  length's worth of bytes at the marker VA, so a too-short original yields a plausible byte
  count instead of an error. A follow-up extent scan over all 378 markers found no second case.
- ⭐ **A DESTRUCTOR CALL'S POSITION PROVES AN INNER SCOPE.** DrawTextA's original emits
  `mov [ebp-4],-1; call ~CBrush` BEFORE `y += 0x20; slot++; i++`, which a `CBrush` declared
  directly in the do-body cannot produce — its dtor must run at the body's closing brace, after
  the increments. Wrapping the drawing work in an explicit nested block: **24 B → 2 B.** ⇒ when
  a dtor/EH-state store sits earlier than your source allows, the original had a narrower scope.
⚠ **Implement the scan from the PROLOGUE, never the EPILOGUE.** The obvious "set of registers
`pop`'d in the body" version is wrong and silently so — a linear sweep desyncs on an embedded
jump table or EH data, so large functions report an empty set. It flagged six functions whose
originals visibly DO push ebx+esi+edi in their first 0x20 bytes. Another instance of the v100
"the instrument itself can lie" family; savescan.py's positive control is the guard.

⭐ **THE IF/ELSE ARM ORDER IS A DIAL, AND IT IS NOT THE CONDITION'S SPELLING (v115,
lesson #47).** A `test/cmp` followed by the WRONG-POLARITY jcc, where the two arms appear in
the opposite order to the original's, is not a scheduler tie-break and not a negation-spelling
problem — it is which arm the 1997 author wrote FIRST. cl 10.20 emits the *then* arm as the
FALLTHROUGH and branches over it to the *else*, so the arm order is directly readable off the
disassembly. On `StartGame` 0x4037a0's generate loop the original emits `test eax,eax; je;
inc edi; jmp` — the `ok = ok + 1` arm falling through — which pins the source as
`if (Generate(seed) != 0) ok = ok + 1; else seed = Randomize();`. We had the arms the other way
round. **79 B → 69 B.**
- ⚠ **The negation/increment SPELLING is inert and will mislead you**: `!Generate(...)` and
  `ok++` both measure 79 B, i.e. identical to baseline. Only the ORDER moves anything.
- The oracle pins a FAMILY (`!= 0`, bare truth-test and `while (!ok)` all give 69), so pick the
  idiomatic member — and the rivals are refuted by LENGTH: `ok = 1` emits 671 B and
  `while (ok < 1)` 668 against the original's 667, positively confirming `ok = ok + 1` /
  `while (ok == 0)`.
⇒ **When a residual is a jcc POLARITY flip with both arms present, swap the arms in the source
before touching anything else.** It is the cheapest probe in the family and it is orthogonal to
the loop dials (#40/#46) and the decl dials (#37/#38/#45). ⚠ Distinguish it from an
`if (!x) return;` early-out, which has only one arm and no order to vary.

⭐ **THE LOOP FORM IS TWO DIALS, NOT ONE — ROTATION IS THE SECOND, AND LENGTH SETTLES IT
(v114, lesson #46) — `tools/unrotscan.py`.** Lessons #40/#43 covered the backedge SHAPE
(countdown vs compare). The independent axis is whether cl 10.20 ROTATES the loop at all:
- A plain `while (c) { body }` compiles to **entry guard + body + DUPLICATED bottom test**.
- `for (;;) { if (!c) break; body }` compiles to **test at the TOP + body + an unconditional
  `jmp` BACK to that test** — no bottom test.
On `WorldgenShuffleList` 0x41ef90 the second form is the original's, and it is provable from
OUTSIDE the byte diff: **Ghidra's extent for the function is 396 B; the `while` spelling emits
400, the `for(;;)+break` spelling emits 396.** The rotated spellings all sit at 269–286 B of
diff, the unrotated one at 22. ⇒ **When a residual's back edge is an unconditional `jmp` in the
original but a compare in ours, the lever is ROTATION — and the emitted LENGTH decides it before
you look at a single register.**
⭐ **The decl dial then finished the job (second confirmation of lesson #45).** With the loop
fixed, the whole remaining residual was three `cmp mem,reg` vs `cmp reg,mem` backedge mirrors.
All **8** combinations of spelling those three conditions the other way round are DEAD FLAT at
22 B — and simply declaring `short i` BEFORE `short nSize` killed all three at once (22 → 16).
The mirror is a decl-BLOCK symptom; it is never the condition's spelling.
⭐ **`tools/unrotscan.py`** is the (read-only, positive-controlled) target list. ⚠ It reports
**exactly ONE hit project-wide** — 0x41ef90 itself — so this lever is now MINED OUT; re-run it
only on newly-transcribed functions. ⚠ A RELAXED version of the rule adds one hit, 0x40fca0,
which has no loops at all in its source: that is an embedded switch JUMP TABLE decoding as
instructions, the exact trap that produced v100's "8 fake class-D targets". Keep the strict rule.
⚠ **This lever cost three exact functions downstream** — see the re-baseline note at the top of
this file before reaching for it again.

⭐ **THE LOOP FORM CAN FIX A REGISTER THE LOOP DOESN'T OWN — AND LENGTH DOES NOT ALWAYS
REFUTE (v113, sharpening lesson #40) — `tools/loopform.py`.** v107 gave two rules for the loop
dial that both needed narrowing:
- **"Wrong forms are refuted instantly by emitted LENGTH" is not universal.** On
  `RemoveEmptyZonesFromPlacedList` 0x403070 the `for` and the guarded countdown emit **exactly
  the same 206 bytes**, because cl 10.20 already strength-reduces `for (i = 0; i < n; i++)` into
  `add edi,2; dec ebx; jne`. The emitted LOOP was never the problem. Length still refutes where
  it differs (loop 2's register-guarded countdown: 204 vs 206), so keep using it — just don't
  read "same length" as "same form".
- **What the source form actually moved was `this`.** Writing loop 1 as the house
  `if (n > 0) { int i = 0; do { ...; i++; n--; } while (n != 0); }` put `this` in ESI, matching
  the original, and four unrelated **this-relative load diffs** disappeared with it
  (+0x03e/+0x069/+0x084/+0x093). 26 B → 24 B, +0/−0. ⇒ a loop-form probe is worth running even
  when the loop bytes already match, and its payoff can land nowhere near the loop.
⭐ **`tools/loopform.py`** is the target list: it disassembles the ORIGINAL, classifies every
BACKWARD branch as countdown (`dec`/`sub` feeding the test) or compare (`cmp`), and reports the
non-exact functions whose original uses a countdown while OUR source spells
`for (x = 0; x < n; x++)`. It is **READ-ONLY** — no compile, no `build/*.obj` — so it is safe to
run while a `vartest.py` sweep is in flight (pass a cached `--exact` file from `exactset.py`).
⚠ **A hit is a CANDIDATE, not a defect**: cl lowers most of our `for` loops to countdowns by
itself, so confirm with `bytediff.py` before investing. Strongest signal = **orig-cmp 0 with
our-for > 0** (the original uses no compare loop at all). Current list, best first: `~CDeskcppDoc`
0x41b2f0 (7 cd / 0 cmp — but at DIFF(6) with 405/405 insns matching, its loops already agree;
this is the known register-phase park), `BlitMasked` 0x408240, `OnNewDocument` 0x41bb10,
`WorldgenCollectZoneRefs` 0x41f8e0.

⭐ **THE DECL DIAL IS AN INTERACTION, NOT TWO SEPARATE KNOBS — SWEEP SET x ORDER JOINTLY
(v112, lesson #45).** Lesson #38 already said "the SET of function-scope locals AND their
ORDER", but every tool the project owns asks only ONE of those questions: `hoisttest.py` asks
yes/no per inner-block decl at a FIXED order, and `declorder.py` permutes the EXISTING leading
block without hoisting anything. Neither can reach the configuration that landed
`WriteSavedState` 0x405f30 **20 B -> 7 B**, which needs BOTH at once: several inner locals
hoisted to function scope AND the loop index declared **LAST**.
- The descent is two levers, each killing a distinct, identifiable defect. (1) `i` before
  `count` fixed the backedge compare FORM in all three count loops simultaneously (orig
  `cmp [esp+count],reg; jg`, ours `cmp reg,[esp+count]; jl`): 20 -> 14. (2) Hoisting the three
  object pointers with `i` last fixed the iactScripts loop's ebx<->edi bijection: 14 -> 7.
- ⚠ **Each lever is INVISIBLE to the other's sweep.** All 16 hoist subsets were dead flat at
  the original decl order, and all 4 decl orders were flat with no hoists. Only the product
  moves. A flat one-axis sweep is therefore NOT evidence the decl dial is closed.
- ⭐ **The compare-operand order is NOT the lever for a cmp mirror** — `count > i` vs
  `i < count` folded identically in all 15 combinations across four loops. When the backedge
  compare is mirrored, reach for the decl block, not the condition's spelling.
- The oracle pins a FAMILY (any >=5 function-scope decls with `i` last measure 7; WHICH names
  are hoisted is irrelevant), so pick the idiomatic member and say so (lesson #36). What keeps
  this evidence rather than tuning is that every rival shape is **refuted by LENGTH**: inline
  cast chains instead of the named `o` emit 808 B, `objects.GetAt(i)` 826 B, and staging the
  `GetSize()` earlier costs 53 B, against the original's 830. A separate index variable for the
  first loop is inert, so the dial is not "one more declaration".
- ⚠ **It is function-specific, not a recipe.** The same 51-configuration sweep on the write
  mirror's read twin (`ReadSavedState` 0x405bd0) is flat at its floor of 12. Two mirror
  functions genuinely respond differently to the same dial.

⭐ **THE TU-JOINT PHASE IS NOT REACHABLE BY DECL CONFIGURATION, AND NOT POSITIONAL (v112 —
two measured negatives that BOUND v105, plus `tools/jointdecl.py`).** v111's one identified
path for the cheap band was the joint search v105 called for. It is now built and run, and it
comes back empty in the TU it was aimed at:
- **`tools/jointdecl.py`** applies a combination of whole-function source variants, compiles
  the TU ONCE, and prints the byte-diff for EVERY marker in it. Two facts make it ~10x cheaper
  than an `exactset.py` run per combination and still exact: TUs compile SEPARATELY (an edit
  here cannot move another TU's codegen, so scoring the edited TU's own markers is sufficient),
  and the phase propagates downstream. `--expect-exact` inherits the v100/v101 baseline guard;
  its baseline vector reproduces `verify.py`'s exactly.
- **Result on Iact.cpp (54 combinations of ReadIzon x ReadSavedState x WriteSavedState decl
  configurations): every marker column is CONSTANT except the edited function's own.** Zero
  cross-function coupling, including into the downstream ReadZaux/ReadZax2/ReadZax3 block.
- **And the phase is not positional.** `RemoveZoneEntry` 0x41d740 / `RemoveZoneEntry2` 0x41d7a0
  are character-identical apart from the container member; the sibling is byte-EXACT and this
  one is a 13 B bijection. Physically SWAPPING the two definitions in the file changed
  NOTHING — both functions kept their verdicts. So the allocation follows the BODY, not file
  position, and v105's coupling needs a bigger perturbation than reordering declarations.
⇒ Read together with lesson #44: the cheap band's remaining bijections are not going to fall to
a search over decl configurations. Do not spend another session building one.

⭐ **THE SCRATCH-REGISTER BIJECTION IS A DISTINCT, SOURCE-CLOSED CLASS (v111, lesson #44) —
and it is what most of the cheap band now IS.** v110 gave the callee-save set as an instrument;
its blind spot is that it only sees `ebx/esi/edi`. A large share of the remaining SMALL residuals
are pure bijections among the SCRATCH registers `eax/ecx/edx` — same mnemonics, same schedule,
same operand order, matching LENGTH, matching save set, only register NAMES rotated. `savescan.py`
reports 0 mismatches on every one of them BY CONSTRUCTION, so a clean savescan is NOT evidence
that a residual is untouched work.
Six functions were swept to a measured floor this session with **zero** gain, across every dial
the project knows — decl SCOPE (#37), decl SET+ORDER (#38), loop FORM (#40), buffer SIZE (#36),
the MFC member-call form (#35), the named-local lever (#43), and compare/arithmetic spelling:
`ReadSavedState` 0x405bd0 (12 B; 12 orders x 32 hoist subsets x 9 loop spellings),
`ReadIzon` 0x405ae0 (7 B; tag[5..16] x 6 orders), `ZoneRequiresItemMaybe` 0x41c0b0 (8 B; 10
spellings), `OnHScroll` 0x417fa0 (6 B; the full 2x2x2), plus `FindObjectAt` 0x405330 and
`LoadStoryHistoryNevada` 0x401ac0 re-confirmed against their existing notes by byte-diff.
⇒ Two rules follow. (1) **A flat sweep across ALL THREE decl dials PLUS the loop form is no
longer a signal to change axis (v108's lesson #41) — at this point it is the signature of the
v105 TU-JOINT PHASE**, i.e. the lever is not in that body at all. Recognise it early: matching
length + matching save set + a clean register bijection = stop after one sweep, not five.
(2) The remaining path for this class is the **joint search** v105 called for, made tractable by
v106's downstream-only property — vary several functions' decl configurations in ONE TU at once
and score with `exactset.py` + `comm`. Per-function spelling sweeps are mined out below ~25 B.
⚠ Do NOT read this as "the cheap band is worthless" — read it as "the cheap band needs a
different SEARCH, not more spellings".

⭐ **THE DIALS COMPOSE — A FLAT SWEEP IS A SIGNAL TO CHANGE AXIS, NEVER A PARK (v108,
lesson #41; and the tooling gap it closed).** v107 taught "flat DECL sweep ⇒ vary the LOOP
FORM". v108 is the converse, measured: `ParseTilesMaybe` 0x41a030 sat at DIFF(3) since G1,
annotated "backedge cmp direction … operand flip proven inert". All **8** guarded loop-form
spellings measured 3 B dead flat — and the answer was **decl ORDER at function scope**:
declaring `int i;` BEFORE `int n = nBytes / 0x404;` makes it byte-exact, while every order with
`n` before `i` costs 40–43 B. +1 / −0, no regressions.
⇒ **Cycle the three dials before parking anything**: decl SCOPE (#37) → decl SET+ORDER (#38) →
loop FORM (#40). A flat result on one is evidence the lever is one of the others.
⭐ **The gap: `hoisttest.py` only ever asks "should this INNER-BLOCK local be hoisted?" — it
never permutes the decls ALREADY at function scope**, which is precisely the axis that landed
this. New **`tools/declorder.py <tu.cpp> <0xADDR> --expect N`** permutes the leading
function-scope decl block (line-neutral by construction, measurement delegated to `vartest.py`
verbatim so the anchor's predicate and the `--expect` guard are inherited, per v100/v101).
⚠ **Mined out on the small residuals**: a 17-target sweep over every residual ≤13 B produced
**0 improvements** — a verdict v109 RE-CONFIRMED after fixing the tool. ⛔ but the parenthetical
"11 of them have no permutable block at all" was **partly the tool lying**: its `DECL` regex did
not match ARRAY declarators, so any function whose first local is a buffer (`char buf[32];`)
reported an EMPTY block. Fixed at v109 — it unblocked 9 residuals, all of them LARGE
(SaveStoryHistory* ×3, ReadZax2/3, ReadIzon, 0x41f960). Aim the tool at those, not at the
≤13 B list. Re-run it on newly-transcribed
functions, not on the current census. Confirmed re-flat this session and NOT worth re-treading:
`FindTile` 0x403aa0 (5 perms, `r` before `i` pinned at 4 B), `GetZoneIndex` 0x423dc0 (10 loop/
compare spellings, all 2 B — the guarded do-while form is confirmed by LENGTH), `BlitMasked`
0x408240 (11 spellings, all 4 B; the two movsx loads are genuinely not source-steerable — and
the 47 B results for a moved `s = src` positively confirm the current statement order).

⭐ **A NET-ZERO FIDELITY TRADE IS SOMETIMES RIGHT — AND IT IS NOT THE v96 PADDING CASE (v107).**
Landing SaveZoneRecursive's countdown costs `LoadZoneRecursive` (immediately downstream) its last
byte: +1/−1, total unchanged at 250. Taken deliberately, because before the change Save carried a
form now PROVEN wrong and after it both functions carry their most-likely-correct form. The
distinction from the ❌ "a trade is the fingerprint of padding" rule: **that rule governs adding
filler DECLARATIONS to move the dial.** A trade is defensible when the gained form has INDEPENDENT
structural proof (here: the wrong variants emit the wrong LENGTH) and matches a house idiom already
documented in the source. Precedent: the v45 `~CDeskcppDoc` message-map trade.
⚠ Confirmed TOKEN-driven, not lesson #23 — a LINE-NEUTRAL spelling trades identically. Second
clean confirmation of v105's joint phase.

⭐ **THE DUPLICATED-LOCAL SEAM IS DOWNGRADED (v107).** v106 called it "the richest untouched
seam" (48 functions). Worked three of its top candidates; **all three closed without a gain**:
`DrawDirectionArrows` 0x4270f0 (13 spellings, floor 21 — merging x/y costs +7), `PlacePuzzle`
0x421620 (12 spellings, 32 → 29 via hoisting `i`, merge inert), and `GetFrameTile` 0x404850 where
the one-variable hypothesis is **structurally REFUTED** (merging `bank` into `idx` collapses the
`dy == -1` test → 177 B vs the original's 183). Keep the scan as a TARGET LIST — it is how
SaveZoneRecursive surfaced — but do not expect the merge probe itself to pay.

⭐ **"ONE VARIABLE WHERE WE HAD TWO" IS A REPEATABLE PROBE (v106; lesson #37 refinement 2).**
`Zone::ReadSavedState` 0x405bd0 went 21 B → 12 B because the original used ONE `ZoneObj *o` for
both its grow loop (`new ZoneObj`) and its read loop (`(ZoneObj *)objects[i]`). It killed two of
three diff sites — a register swap AND a cmp-operand mirror — which is what makes it evidence
rather than coincidence. ⇒ **A scan finds 48 non-exact functions carrying a repeated same-name
local declaration** (`Tick` 0x40b270, `OnBumpTile`, `Generate`, `IactRunCommands`, `~CDeskcppDoc`
lead it); that is the richest untouched seam. ⚠ not universal — merging across BOTH branches of
`WorldgenAssignTransitItemMaybe` costs 21 B. ⚠ `hoisttest.py` reaches this configuration only via
its name-DEDUP, so `--max-hoist 1` cannot see it.

⭐ **PROBE SPELLINGS, DON'T REASON ABOUT SCHEDULES (v102 method).** Both wins this session came
from enumerating ~10 ways the 1997 author could have SPELLED one statement and letting
`tools/vartest.py` measure each against the anchor's byte oracle. Most spellings fold to identical
codegen — that "source-inert" verdict is a real result, and it retires a residual honestly instead
of leaving it open. Proven inert this way so far: `FindTile` 0x403aa0 (cast placement, `void**`
walk, cmp operand order, decl order, for-vs-do-while), `BlitMasked` 0x408240 (every associativity
of `pData + destX + canvasW * destY`), and the `savedId != child` compare in `LoadZoneRecursive`.
⚠ keep variants LINE-NEUTRAL — a line-count change mid-TU rotates the dial on its own (lesson #23)
and confounds the measurement.

⭐ **THE INSTRUMENT ITSELF CAN LIE — verify the harness before chasing its targets (v100, two real
tool bugs found in one session).** Both silently manufactured work that did not exist:
1. **The marker-pairing CASCADE.** `verify.py` has always kept a lib-owned COMDAT that a marker
   EXPLICITLY names by mangled hint (the `hinted` set — CDeskcppView's CGdiObject/CBitmap trios are
   genuinely ours to match). `progress.py` and `idiomscan.py` had drifted and dropped them. A marker
   whose COMDAT was filtered away then fell back POSITIONALLY in `match.pair_by_name`, consuming a
   COMDAT the NEXT marker wanted — cascading **28 mis-pairs through DeskcppView.cpp**, scoring 17
   already-exact functions against wrong addresses and fabricating a whole phantom "`??_G`
   scalar-deleting-dtor idiom family" that looked exactly like the v99 five-stub cluster. Fixed by
   porting verify.py's exception + fixing 3 stale marker hints (`??_GGameView`/`?DrawTextA@GameView`
   → `CDeskcppView`; the two stacked InvScrollBar markers both derived `??1InvScrollBar@@`).
   `pair_by_name` now WARNS on every positional fallback (`YODA_PAIR_WARN=0` to silence) — a warning
   means a stale marker hint to FIX, never noise to ignore.
2. **Two different definitions of "exact".** `idiomscan` skipped a function on `asmscore`'s
   DISASSEMBLY-based verdict; the anchor's definition is `progress.py`'s reloc-masked BYTE compare.
   A function carrying an embedded switch JUMP TABLE decodes the table as instructions, so asmscore
   reports a phantom `byte_diff` on provably byte-exact code — **8 fake class-D targets**
   (OnUpdateGameSpeedUi, OnUpdateDifficultyUi, ClassifyTile, StepDetonatorEffect, OnChar, …).
   idiomscan now uses the byte test. ⇒ **before investing in any residual, confirm it is non-exact
   with a raw reloc-masked byte diff**, and cross-check a per-TU number against `verify.py`
   (it disagreed with progress.py for 3 sessions and verify.py was right).
3. **v101 — THE SAME BUG, A THIRD TIME, in `tools/dialsweep.py`.** Its `exact_set()` still had the
   pre-v100 filter, and `membertest.py`/`headersweep.py`/`enumfieldtest.py` all measure THROUGH it —
   so every sweep result the project ever published was computed on the cascade. It had been silently
   under-reporting the baseline by the same 23 (211 vs 234) and, worse, **manufacturing a free
   +4/−0 gain that does not exist** — the entire v96 "find the seven missing symbols" quest. Fixed;
   dialsweep now reproduces the anchor's 234 exactly.
4. **v103 — an AD-HOC scan lied too, the same way.** A one-off scan of "which non-exact functions
   contain global-form `::Call(` sites" reported **zero**, and I nearly closed the whole member-call
   seam on it. The bug: `tools/residuals.py --csv` writes the `va` column in **DECIMAL**, and the
   scan parsed it with `int(va, 16)` — so no address ever matched and every function looked exact.
   The corrected scan found **28** such functions, and three of this session's wins came out of them.
   ⇒ The baseline rule applies to throwaway greps too: **print a positive control** (here: "141
   residual addrs parsed, e.g. 0x403450") before believing an empty result. An empty result from a
   scan you just wrote is a bug hypothesis, not a finding.
5. **v109 — a SIXTH, in `tools/declorder.py`, and it manufactured a PARK rather than a target.**
   Its `DECL` regex had no clause for an array extent, so `char buf[32];` was not a declaration;
   `leading_block()` starts at the first body line and breaks on the first non-declaration, so a
   function whose first local is a buffer reported **"no permutable block"** instead of an 8-decl
   one. That is precisely the sentence v108 published ("11 of 17 have no permutable block"). The
   failure mode to generalise: **a harness that reports NOTHING TO DO is as suspect as one that
   reports a finding** — the v103 positive-control rule applies to emptiness too. Caught by
   hand-checking one function (`SaveStoryHistoryNevada`) whose source visibly HAS a decl block.
6. **v110 — a SEVENTH, caught BEFORE it published anything.** The v110 callee-save scan was first
   written to read the save set from the EPILOGUE (the set of `pop ebx/esi/edi` in the body).
   Capstone's linear sweep desyncs on an embedded jump table or EH data, so large functions
   reported an EMPTY set — it confidently flagged six functions (ScrollZoneTransition, Layout,
   Position, OnUpdate, …) whose originals visibly DO push ebx+esi+edi in their first 0x20 bytes.
   Caught only by hand-disassembling four of the six. Reading the PROLOGUE instead (stop at the
   first call/branch) keeps the decode inside real prologue bytes and reports 0 mismatches.
   ⇒ Same family as #2: **two plausible implementations of "the same" measurement can disagree,
   and the one that decodes MORE of the function is the one that will lie.**
7. **v111 — an EIGHTH and NINTH, both "nothing to do" lies, found in one session.**
   (a) `tools/declorder.py`'s `DECL` regex demanded `;\s*$` — ONE declaration per line. But the
   line-neutral idiom this project uses everywhere to avoid lesson #23 crams several onto one
   line (`ZoneObj *o; int count;`), so such a line matched NOTHING, `leading_block()` broke on
   it, and the tool reported **"0 leading function-scope decls — nothing to permute"** for the
   very function being worked. Same shape as the v109 array-extent bug it was supposed to have
   fixed. Fixed with a non-anchored `ONE_DECL` + a `decl_names()` that accepts a whole line of
   declarations as ONE permutable unit. ⚠ Scope check first: only **4** functions project-wide
   have a multi-decl leading line and only ONE (`ReadSavedState`) had enough units to permute —
   so the bug was real but narrow, which is worth measuring before assuming a big seam.
   (b) `tools/residuals.py` called `main()` at MODULE level (unguarded). Any tool that
   `import`s it ran the whole census, printed the table, and parsed the IMPORTER's `sys.argv` —
   which silently swallowed `chainscan.py`'s own flags. Now under `if __name__ == "__main__"`.
   ⇒ Generalise: **"nothing to permute" / "no hits" / "no mismatches" all need the same positive
   control as a finding.** Both of this session's new-tool bugs (see `chainscan.py` below) were
   caught by testing the scan against strings whose answer was already known.
8. **v111 — the headline dashboard itself, for MONTHS, in plain sight.** `progress.py` printed
   ">>> 124.88 % transcribed; −24.88 % left to decompile <<<" because its numerator (our COMDAT
   lengths, EH funclets + jump tables IN) and denominator (Ghidra body sizes, funclets OUT) were
   different bases. The mismatch was DESCRIBED IN A COMMENT directly beneath the offending print,
   and the correct figure was computed a few lines further down and reported separately as
   "marker coverage" — nobody reconciled the two. Fixed onto one extent basis, with an assert
   that the tiers partition the total. ⇒ **A visibly impossible number is a bug, not a quirk of
   the metric** — and a known-wrong line you keep printing next to a right one will outlive
   every session that reads past it. Give a tool an invariant it can assert about itself.
⇒ A cluster of functions sharing an identical residual signature is the productive seam (v99's five
stubs were real) — but confirm the cluster is not a pairing artifact FIRST. Audit script pattern:
re-derive `_want_key` per marker and assert it appears in the paired COMDAT name.
⇒ ⭐ **A measurement tool must AGREE WITH THE ANCHOR AT BASELINE before any of its deltas mean
anything.** All three bugs would have been caught on day one by one assert: run the tool with a
null/zero perturbation and check it reports the same number as `progress.py`. Any new harness gets
that check first. (`bytediff.py`/`residuals.py`/`vartest.py`/`exactset.py` share progress.py's filtering + pairing code
for this reason — copy that block, never re-derive it.)

⭐ **THE DIAL IS AN INSTRUMENT, NOT A KNOB (v96 — the rule that keeps this honest).** The exact
count is steerable by ambient declaration state, which means it can be *gamed*. Do not.
- ✅ **Legitimate:** find a REAL missing declaration (a header the original included, an enum whose
  true field count we mis-transcribed, a class we never modelled) and add it. Signature of a correct
  fact: **gains with ZERO regressions** — the `afxcmn.h` pattern (v37: +3 TUs, no losses) and the v96
  +4/−0 plateau. Such a change is justifiable on its own merits, dial or no dial.
- ❌ **Forbidden:** adding filler declarations, padding an enum, or keeping a wrong field count
  because it happens to score better. That encodes a NUMBER, not a fact, and poisons the source as a
  reference. Every position in the v96 sweep that *traded* (+3/−3) is that kind of position — a trade
  is the fingerprint of padding, a free gain is the fingerprint of truth.
`tools/dialsweep.py` MEASURES the mechanism (pure symbol count; identifier length irrelevant;
enum = tag + field count; macros are free — they never enter the symbol table). ⛔ **v101: do NOT use
it to hunt "missing symbols" any more.** On the fixed tool every position LOSES and the baseline is
the best known, so there is no deficit to go find — the v96 "+4/−0 / seven symbols" reading was the
cascade bug talking (docs/compiler-hunt.md v101). The ✅/❌ rule above still governs if a real
declaration ever turns up on independent evidence.

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

### WASM build/debug (GOAL 4 — v88)

- **Build:** `emcmake cmake -B build-wasm -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL [-DYODA_DEBUG=ON]
  [-DYODA_WASM_PRELOAD=OFF] && cmake --build build-wasm` → `yoda.html/.js/.wasm[/.data]`. Homebrew
  emscripten; SDL3 via `--use-port=sdl3` (no find_package under EMSCRIPTEN — the CMake branch handles
  it); `-fexceptions` is REQUIRED (microfx CFile throws CFileException; JS-EH is the Asyncify-safe
  choice). Keep two trees: `build-wasm` (preload + YODA_DEBUG — automation) and `build-wasm-pick`
  (`-DYODA_WASM_PRELOAD=OFF`, shippable, no baked game data — the in-page folder picker).
- **Node harnesses = the fast oracles (no browser):** the same worldgen_smoke/zone_view/game_walk/
  dlg_smoke build as `.js` with NODERAWFS — run `node build-wasm/worldgen_smoke.js <seed>` from a
  folder holding the DTA + `yoda.INI` (wasm exe base is always "yoda": `GetModuleFileNameA` returns
  `<cwd>/yoda`). v88 parity: 5/5 seeds byte-identical `yoda_debug.log` vs native, zone_view BMP
  pixel-identical. ⚠ cross-libc A/B needs BOTH pins: `YODA_SEED` AND `YODA_PLANET` (the planet
  re-pick spins an unseeded `rand()` — macOS/musl/msvcrt disagree; both pins are YODA_DEBUG-only).
- **⭐ Puppeteer browser oracle (how to debug wasm WITHOUT eyes):** `npm install puppeteer-core` in
  any scratch dir (uses installed Chrome, no download), serve the build
  (`cd build-wasm && python3 -m http.server 8777 &`), then from that scratch dir run
  `node tools/wasm_boottest.js [url] [shotPrefix] [assetDirForPickerBuilds]` — boots the page,
  clicks through the title, walks, screenshots the canvas (`<pfx>{0,1,2}.png` — READ these as
  images), and prints canvas-pixel + audio-graph stats (PASS = painted canvas + AudioContext
  'running'). Patterns inside worth reusing for ad-hoc probes: `page.evaluateOnNewDocument` to
  instrument JS APIs before the app loads (that's how the audio graph is proven), `canvas.screenshot`
  per phase, `page.on('pageerror')` for wasm traps (an Asyncify stack overflow or a missing FS file
  shows up there), `input.uploadFile(<dir>)` drives the `webkitdirectory` picker. Env vars for the
  page (YODA_SHOT etc.) do NOT pass through the browser — instrument via JS instead.
- **Architecture facts:** ASYNCIFY makes the blocking loops legal — the ONLY yield points are
  `MfxPlatDelay` (→`emscripten_sleep`) and the post-present yield in `mfxplat_sdl3.cpp`; a new
  busy-wait that never presents nor delays will freeze the tab. `ASYNCIFY_STACK_SIZE=1MB` (deep
  modal-in-handler stacks). MEMFS is CASE-SENSITIVE (the DTA's `Door.wav` only loads via the snd
  layer's lowercase retry) and non-persistent (INI/saves lost on reload — IDBFS is the open tail).
  wasm32 `long`=32-bit but emscripten `time_t`=64-bit → the 1997 `long time(long*)` decls in
  Score/MainFrm are renamed to `mfx_time32` wrappers by the `MFX_TIME32_SHIM` tails of Worldgen.h/
  MainFrm.h (keep the two copies synced).

### Android build/debug (GOAL 5 — v92)

- **Build:** `export ANDROID_NDK_HOME=~/Library/Android/sdk/ndk/<ver>` then `cmake --preset
  android-demo && cmake --build --preset android-demo` → `build-android-demo/<AppName>.apk`
  (presets `android-{demo,full,indy}`, build the `apk` target). First configure clones + first
  build compiles **static SDL3 3.4.12 + SDL3_mixer** per ABI (slow; arm64 cached after). The whole
  flow is ONE CMake target — cmake/Android.cmake defines `main` (the single static `libmain.so`)
  and the `apk` target that runs `tools/android_apk.sh` (stages a build copy of `packaging/android/`
  + gradlew). `--target apk-install` also adb-installs + launches. Requires NDK+SDK (platform-34,
  build-tools 34) and JDK 17+ (JDK 20 works; Gradle 8.7/AGP 8.4 fetched by the wrapper).
- **Emulator oracle (debug WITHOUT a device — the counterpart to wasm's puppeteer):** this Mac has
  AVD `Pixel_3a_API_33_arm64-v8a` (Apple-Silicon-native arm64 — boots the arm64 APK directly; the
  x86_64 ABI is for Intel emulators). Recipe: `$SDK/emulator/emulator -avd Pixel_3a_API_33_arm64-v8a
  -no-window -no-audio -no-snapshot -gpu swiftshader_indirect &`; `adb wait-for-device` +
  poll `getprop sys.boot_completed`; `adb install -r "<apk>"`; `adb shell am start -n
  <pkg>/org.yodecomp.app.GameActivity`; then **`adb exec-out screencap -p > shot.png`** (READ it as
  an image — the game renders under swiftshader) and `adb shell input tap <x> <y>` to drive touch
  (coords are DEVICE pixels; the emulator display is 2220×1080 landscape). `adb logcat | grep SDL`
  shows `SDL_main from libmain.so` + `microfx: Android data dir = …` + `Low latency audio enabled`.
  A one-time "Viewing full screen" SYSTEM toast may appear — tap "Got it"; it is NOT our app.
  `GameActivity.java` forces true immersive fullscreen: ⭐ the load-bearing part is
  `setDecorFitsSystemWindows(false)` (edge-to-edge LAYOUT) applied in **onCreate** — SDL's SurfaceView
  is otherwise measured to the content area (screen MINUS nav bar, 2220×948) and STAYS there even once
  the bars hide, so the letterbox is computed against the short surface → a navbar-sized dead strip
  ("still cropping as if the navbar is there"). With edge-to-edge the surface is full-height from the
  first frame (SDL logcat `Window size: 2220x1080`); bars hidden via `WindowInsetsController.hide`
  (API30+) / deprecated immersive flags (28–29), cutout ALWAYS. Verify by grepping logcat for
  `SDL.*Window size`. ⚠ emulator: a stray `input tap` off the game backgrounds it to the launcher —
  force-stop + relaunch, minimal taps.
- **Architecture facts / traps:** SDL3 is static in libmain (`getLibraries()={"main"}`); the entry
  is `-include SDL3/SDL_main.h` renaming harness main()→SDL_main (SDL's Android JNI resolves it).
  Data dir = `SDL_GetAndroidInternalStoragePath()` (`MfxAndroidDataDir` in mfxplat_sdl3.cpp);
  GetModuleFileNameA reports `<internal>/yoda`; baked APK assets (in `assets/`, listed in
  `manifest.txt`) EXTRACT to internal storage on first launch (skip-if-exists → INI/saves persist).
  Present = fullscreen letterbox renderer, logical size adapts to the presented DIB; touch↔game via
  `SDL_RenderCoordinatesFromWindow`. Touch: `SDL_HINT_TOUCH_MOUSE_EVENTS=0`, `MfxHandleFinger`
  multitouch (mouse/Shift/Space finger roles), the two corner buttons (bottom-right cluster:
  Attack ◆ = Space, Push/Pull ⇕ = Shift diagonally ↖ of it) synthesize VK_SHIFT/VK_SPACE (feed
  `g_mfxKeyState` like hardware keys). The overlay is **input-modality adaptive** (`s_bTouchActive`):
  shown on touch, hidden once a key/mouse/gamepad event arrives, re-shown on next touch. ⚠ do NOT
  `SDL_StartTextInput` on Android (raises the soft keyboard over the game — guarded out).
  `ndkVersion` is passed to gradle so AGP strips the lib. ⚠ **cursor scale**: on Android the soft
  cursor composites in the renderer's game-pixel LOGICAL space, so it uses `MfxCursorScale()`==1
  (not s_nScale — that double-scaled it: the "2× position and size" bug).
- **⭐ Modal dialogs on Android (v92 fix — two bugs, ONE was shared root cause; all in `mfxdlg.cpp`,
  device-verified on an AYN Thor):** (1) **empty About + slider-dialog SIGSEGV.** `CDialog::DoModal`'s
  DLGTEMPLATE parser DWORD-aligned each control entry off `p`'s ABSOLUTE address; the spec aligns
  relative to the TEMPLATE start. It only worked where the embedded `.res` blob happened to land
  4-aligned (desktop/wasm link layouts); Android's blob base isn't, so every control read shifted →
  wrong ids/classes → controls never created. Symptom pair: template dialogs render frame+caption
  only (About was empty) AND `GetDlgItem(0x67)` returned NULL → `DifficultyDlg::OnInitDialog`'s
  `pCtrl->m_hWnd` null-derefed at +0x10 (Combat Difficulty/Game Speed/World Control all crashed). Fix:
  `p = pT + (((size_t)(p - pT) + 3) & ~3)` — identical output when pT is 4-aligned, so no desktop
  change. (2) **save = 0-byte file + write error.** SDL3's Android file picker uses the Storage
  Access Framework: it PRE-CREATES the chosen file (0 bytes) and hands back a `content://` URI, which
  the engine's `fopen`-based `CFile` can't open. Fix: `MfxPlatShowFileDialog` returns -1 on
  `__ANDROID__` (like `__EMSCRIPTEN__`) → CFileDialog's in-window row-list picker, rooted at
  `MfxAndroidDataDir()` (writable internal storage where assets extract + saves persist; cwd "." is
  "/" and unwritable). Save writes a real .wld and Load lists it. ⚠ `fprintf(stderr)` is INVISIBLE on
  Android — SDL doesn't redirect stdio to logcat; use `SDL_Log`/`__android_log_print`, or read the
  native crash via `adb logcat | grep DEBUG` (the tombstone backtrace names the crashing function).
- **Game controller (all SDL3 platforms, not just Android — `mfxplat_sdl3.cpp`):** `SDL_INIT_GAMEPAD`;
  both sticks + D-pad → 8-way movement mapped to the game's own arrow/diagonal VKs (a held direction
  is auto-repeated every ~33ms since the game needs a fresh WM_KEYDOWN per tick to keep walking);
  A(South)=Space/Attack, B(East)=Shift/Push-Pull, X(West)=Enter/dismiss, Select(Back)='L'/Locator.
  Synthesized transitions go through a small ring queue (`MfxPadPush`/`MfxPadPop`, drained at the
  top of `MfxPlatPollEvent`).

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
1018:8e40, IACT cmd dispatcher `FUN_1010_2eb6`, `IndyCacheSpecialTilePtrsMaybe` 1010:42be (≡ CacheUiTilePtrs
0x41a5d0 — 20 locator/UI tile ptrs, idx=srcOff/4), `IndyDrawLocatorMap` 1010:bb60 (≡ DrawLocatorMap 0x423df0),
`IndyGetLocatorIcon` 1010:402e (≡ GetLocatorIconMaybe 0x41a1c0). Full tables in docs/phase-h3-indy.md.

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

Byte-match harness (anchor checks): **`progress.py`** (headline dashboard. ⚠ v111 fixed its
PERCENTAGES: numerator was our COMDAT lengths (EH funclets + jump tables IN), denominator was
Ghidra body sizes (funclets OUT), so it printed "124.88 % transcribed; −24.88 % left" for many
sessions — flagged in a comment beside the print and shipped anyway. Everything is now on the
ONE extent basis (`toolchain/test/app_funcs.txt`, 410 funcs / 156054 B) and the tool ASSERTS
exact+partial+todo == total. The EXACT COUNT and the exactness predicate never changed) ·
**`savescan.py [--all]`** (⭐ v110 — the CALLEE-SAVE SET of every non-exact residual vs the
original's; the cheapest "what is actually wrong here" read in the project, and the instrument
behind all four v110 wins. Currently 0 mismatches — re-run on newly-transcribed functions.
Its docstring carries the two readings + the epilogue-pops trap) · **`bytediff.py <src.cpp>
[0xADDR...]`** (⭐ the ANCHOR's own definition of exact for ONE function — reloc-masked BYTE diff +
hexdump of each differing run; run this BEFORE investing in any residual, per v100) ·
**`vartest.py <tu.cpp> <0xADDR> <variants.py> --expect N`** (⭐ batch-A/B a set of source SPELLINGS for
ONE function against the anchor's byte oracle — the instrument that landed both v102 wins; ALWAYS pass
`--expect`, it hard-fails when its own baseline disagrees with the anchor. v110 added a `saves=`
column flagged `(orig X) <-` on a mismatch: a variant that changes the callee-save set has changed
how many long-lived values the body needs, which is often the real find even when the byte count
barely moves) ·
**`chainscan.py [--max-diff N] [--lines]`** (⭐ v111 — the LESSON #43 target list: non-exact
functions whose source passes a POINTER CHAIN as an argument / call receiver / assignment RHS,
each hit tagged `arg`/`recv`/`other`. Reproduces the v110 hand-derived list exactly. ⚠ its own
first draft had TWO bugs, both caught by a known-answer positive control: the `cast()->` regex
missed the canonical PARENTHESIZED `((T *)p)->m` form, and restricting hits to argument lists
dropped both actual v110 wins, which were a receiver and an assignment RHS) ·
**`unrotscan.py [--exact <file>|--all]`** (⭐ v114 — READ-ONLY target list for the loop ROTATION dial, sibling of loopform.py; currently 1 hit project-wide, i.e. MINED OUT) · **`loopform.py [--exact <file>|--all]`** (⭐ v113 — READ-ONLY target list for the LOOP-FORM dial: the original's countdown backedges vs our `for` spellings. Safe to run during a sweep; a hit is a candidate, not a defect) · **`jointdecl.py <spec.py> --expect-exact N`** (⭐ v112 — the JOINT search: applies a combination of WHOLE-FUNCTION source variants, compiles the TU once, prints the byte-diff for EVERY marker in it. Cheap because TUs compile separately, so an edit here cannot move another TU. ⚠ its first run is a NEGATIVE result — 54 combinations over Iact.cpp moved no column but the edited function's own; see lesson #45's second half before reaching for it) · **`declorder.py <tu.cpp> <0xADDR> --expect N`** (⭐ v108 — permutes the LEADING FUNCTION-SCOPE
decl block; the axis `hoisttest.py` structurally cannot reach, and the one that landed
ParseTilesMaybe. ⚠ v111 fixed it AGAIN: it could not see a line holding SEVERAL declarations) · **`hoisttest.py <tu.cpp> <0xADDR> --expect N`** (decl SCOPE: hoist
inner-block locals. ⚠ v110: it only sees decls WITH an initializer, so a bare `Tile *pTile;`
is invisible to it — its "1 spelling" answer on DrawTextA was a tool limit, not a result;
sweep bare decls by hand) ·
**`residuals.py`** (⭐ census of the non-exact functions RANKED by byte-diff, with a
commutative/tie-break classifier; the trustworthy replacement for sorting idiomscan's class D) ·
**`verify.py <src.cpp>`** /
**`match.py`** (per-TU marker compare, reloc-masked; best-fit can mis-pair clones — confirm name-keyed) ·
**`asmscore.py <src.cpp> 0xADDR [--dump]`** (graded disasm scorer; `--dump`: LEFT=original, RIGHT=ours;
recompiles the TU itself) · **`bugscan.py`** / **`vtcheck.py`** / **`msgcheck.py`** (correctness oracles —
wrong vtable slot / field disp / message-map entry; see anchor table) · **`link_exe.sh`** (full-image link
oracle) · `permute.py`, `survey.py`, `frontier.py`, `g2_link.sh`, `g2_diff.py`, `g2_order.py`,
`exactset.py`, `libfingerprint.py` (parked byte-match era; see PLAN_COMPLETED.md).
Resources: **`make_res.py`** (+`reslib.py`), `extract_res.py`.

## 📋 Session protocol

1. **Orient:** read the ⏭ pickup block below; run `python3 tools/progress.py` to confirm the anchor (252)
   reproduces BEFORE changing anything (if not, a header drifted — bisect first).
2. **Work** the pickup goals. Ghidra writes: always `program=`. Anchor rule for every shared-TU edit
   (ifdef fall-through = original tokens); re-run the anchor oracles after shared-code changes.
3. **Agents** for read-only RE sweeps (naming/xref surveys); keep build-and-test iterations in the main thread.
   Escalation: spawn a `fable`-model agent with the disasm + relevant lesson numbers for novel mechanisms.
4. **Session end:** update the ⏭ pickup block (findings → instincts, done items removed, next steps concrete);
   demote the old pickup to a condensed ⏮ block APPENDED to PLAN_COMPLETED.md; distill new mechanisms into
   the lessons lists (PLAN_COMPLETED.md) or the standing-lesson bullets here; sync new struct fields/renames
   to Ghidra (or list as PENDING); `save_program`; commit with a descriptive message.

### ⏭ NEXT SESSION PICKUP (2026-09-04 v115 — **252 exact, unchanged (+0/−0, verified by
`exactset.py` + `comm`)**. Two residuals cut on the `loopform.py` list the v114 pickup pointed
at — `StartGame` 0x4037a0 **79 B → 69 B** on a NEW dial (IF/ELSE ARM ORDER, lesson #47) and
`IactRun` 0x406780 **1548 B → 1538 B** via a second in-condition assignment. Neither function
is small enough to reach exact, so the count did not move. Also CORRECTED a stale oracle
expectation: bugscan's green state is **1 HIGH (known benign)**, not 0. All oracles green at
the recorded state (252 exact / 99.17 % / link 0-0-exit0 / bugscan 1 HIGH known-benign, 0 SHIFT
/ vt 10 CLEAN / msg 11 CLEAN / savescan 0 mismatches over 126 residuals / build-sdl links,
worldgen_smoke converges). v114 log demoted to PLAN_COMPLETED.md.)

**▶ READ FIRST:** the new standing lesson **"THE IF/ELSE ARM ORDER IS A DIAL, AND IT IS NOT
THE CONDITION'S SPELLING"** (#47), and the ⚠ bugscan note under the oracle table — a future
session must not "fix" that HIGH or suppress it in the tool.

**▶ WHAT LANDED.**
1. **`StartGame` 0x4037a0: 79 B → 69 B.** The generate loop's arms were the other way round
   from the original. `if (Generate(seed) != 0) ok = ok + 1; else seed = Randomize();` — the
   original emits the `inc edi` arm as the FALLTHROUGH. Family pinned by LENGTH (`ok = 1` = 671
   B, `while (ok < 1)` = 668, original 667); `ok++` and `!Generate(...)` are inert at 79.
2. **`IactRun` 0x406780: 1548 B → 1538 B.** `(tx = dx + x)` in the COND_BumpTile condition,
   the twin of the `(ty = y + dy)` trick that landed at G1 and which the header note had
   flagged as untried for x. Kills the add-vs-sub form at +0x14b.
3. **Oracle correction.** bugscan reports 1 HIGH — `StartGame @+0x14a lea orig=0x4b4
   ours=0x4b0` — and it PRE-DATES v115 (measured at HEAD). Proven false positive: all 30 grid
   stores have `ours_disp == orig_disp + 4`, exactly cancelling the induction-anchor
   difference, so every effective address is IDENTICAL. Recorded, deliberately NOT suppressed.

**▶ MEASURED NEGATIVES — do NOT re-tread** (all written into the functions' source notes):
- **`StartGame`'s remaining 69 B is source-closed.** 10 spellings of the grid loop's two
  induction increments' PLACEMENT plus the pg/mz decl ORDER are flat at 68–72 B with identical
  length and save set — lesson #44. Decl swap is strictly WORSE (72), positively confirming the
  current order.
- **`IactRun`'s +0x15e** (orig adds `y` then `dy`, ours `dy` then `y`): all 7 ty spellings dead
  flat at 1538; dropping `ty` costs 252 B. Third confirmation of lesson #45 — a mirrored
  compare is never the condition's spelling.
- **`IactRun`'s tx is NOT reused in the tile subscript.** Every variant substituting `tx` there
  emits 2404 B against the original's 2408 — refuted by length.
- **`WorldgenPlacePuzzles` 0x421930's +0x05f movsx site: THREE hypotheses refuted.** The
  lesson-#43 named local (5 spellings, all 1294–1295 B vs the original's 1299 — refuted by
  length, diff doubles to ~1100); an explicit `(int)` cast (inert at 633); and `a4` being `int`
  in `PlaceQuestNode`'s signature — that one DOES emit the movsx but 0x421930 gets worse
  (633 → 636) and 0x41f120's own body more than doubles (405 → 880). **a4 is `short`.**
- **`ReadZaux` 0x406270 is the known `mov ax`/`movsx` park**, already double-swept (v99: 20+
  statement forms; v109: all 120 decl permutations) across its ReadZax2/ReadZax3 clones. Its
  111 B is that +3 B pair × 2 plus the resulting tail misalignment. Don't re-open without a
  genuinely new mechanism.

**▶ NEXT — concrete, in priority order.**
1. **Lesson #47 is BRAND NEW and has not been swept project-wide.** Build the target list the
   way `loopform.py`/`unrotscan.py` were built: READ-ONLY, positive-controlled — disassemble
   each non-exact residual's ORIGINAL, find `test`/`cmp` + jcc pairs whose two arms are both
   present, and flag the ones where OUR jcc has the opposite polarity with the same arm
   contents. `StartGame` was found by eye; a scan should find the rest. This is the cheapest
   unexploited lever right now.
2. **The dtor-position probe (v110 item 2) is STILL the best unexploited seam** — an `[ebp-4]`
   EH-state store among the DIFFERING instructions, ~20 real candidates. ⚠ `[ebp-4]` holding a
   REGISTER is a spill/EH-state-zero reuse, not a dtor-position tell; look for
   `mov [ebp-4], imm` or a dtor CALL sitting earlier than our source allows. ⚠ "a call appears
   in the diff" is NOT a filter. ⚠ `Puzzle::Puzzle` 0x4042b0 already refuted.
3. **`loopform.py`'s remaining untouched hits**, confirmed with `bytediff.py` FIRST:
   `0x4070e0` IactRunCommands (1401 B), `0x41bb10` OnNewDocument (537 B), `0x403c80`
   BuildQuestPathMaybe (1121 B). ⚠ `0x4037a0`, `0x406780` and `0x406270` are now worked/parked.
4. **`TriggerHotspotsMaybe` 0x40ec30 (17 B)** is a clean pWorld/nCount ESI↔ECX role swap in the
   PROLOGUE only (orig loads `this->pWorld` BEFORE `mov edi,ecx`). Its note already records the
   `World*` local probe and n/i decl order as inert; what is NOT yet probed is the ORDER of the
   opening statements (`int nResult = 0;` position, `bBusy = 1;` position, tx/ty vs the early
   return). Cheap.
5. **⛔ Do NOT run another per-function decl sweep on the cheap band.** v111 swept six to a
   floor, v113 three, v114 three, v115 re-confirmed two more. Below ~25 B the decl dials are
   mined out; that verdict now rests on fourteen functions.
6. **Worldgen.cpp still holds the biggest residual mass** (`Generate` 0x41f960 at 5710 B,
   `OnInitialUpdate` 0x426c40, `WorldgenPlacePuzzles` 0x421930 at 633 B whose bulk is the OPEN
   block-sinking family) — transcription-level work, not a dial.
7. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note).
8. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v114 rules all stand and were all re-used this session).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE and gate on BOTH `pgrep` and the driver's
own DONE marker; restore a single function from `git show HEAD:<file>`, never `git checkout
<file>` mid-sweep; never run two sweeps concurrently, or one while `progress.py`/`exactset.py`/
`residuals.py`/`jointdecl.py` is in flight (they share `build/*.obj`). A variants file that reads
the SOURCE to build its BASE must read `git show HEAD:<file>`. `vartest.py` stops cleanly on
SIGINT (`pkill -INT -f tools/vartest.py`), which runs its restore. Measure with
`tools/exactset.py` + `comm`, never progress.py's total alone. A comment rewrite IS a line-count
change (lesson #23) — this session rewrote three function notes and verified +0/−0 afterwards.
⭐ **v115 additions:**
- **A stale GREEN STATE is as dangerous as a broken tool.** The oracle table said bugscan was
  0 HIGH; it had been 1 HIGH for an unknown number of sessions. Before treating an oracle
  disagreement as YOUR regression, re-measure it at HEAD — `git stash push -- src/`, run, pop.
  That one step turned a scary-looking regression into a documentation fix.
- **`residuals.py --csv` needs a PATH argument** (`--csv out.csv`); bare `--csv` throws an
  IndexError. Its columns are `cpp,va,name,L,ndiff,span,lenmis,kinds,tie` and **`va` is
  DECIMAL** (the v103 trap — parse with `int(r['va'])`, never `int(va, 16)`).
- **`loopform.py --all` includes ALREADY-EXACT functions**; without `--all` it is the
  non-exact list. Nine of the 25 rows in its `--all` output are exact, so cross-check against
  `residuals.py` before picking a target.



### ⏮ PRIOR PICKUP (2026-07-18 v93 — four Indy playtest fixes shipped; see below.)

**▶ v93 (2026-07-18) — Indy playtest round (all GAME_INDY-guarded; anchor 211/99.17% + all
oracles green after each; commits b86f62b/c1ea012/7fb50c0/c709aa6):**
1. **Locator/overview MAP tiles** — `CacheUiTilePtrsMaybe` hardcoded Yoda UI tile idx 817-837
   (2128-tile catalog); Indy DAW has 1144 tiles → garbage. Recovered Indy idx from DESKADV
   `IndyCacheSpecialTilePtrsMaybe` 1010:42be (idx=srcOff/4); `IndyDrawLocatorMap` 1010:bb60 fills
   0x4c + draws NO per-cell bg tile (Yoda 0x344). DeskcppDoc.cpp + Worldgen.cpp branches.
2. **Overview map wouldn't open** (L-key/Select) — checks inventory[0]==tiles[**0x1a5**] (Yoda
   locator item); Indy's is **0x1bb** (DESKADV IndyPlacePuzzlesPass 1010:9ebc order-1 anchor; our
   worldgen already places it). Token-neutral macro `IDX_LOCATOR_ITEM` (Worldgen.h tail).
3. **Exit dialog "Leave Yoda Stories?"** — `ConfirmExit`'s `AfxMessageBox(0xe01b)`; make_res.py
   `--indy` now overrides string 0xe01b → "Leave Desktop Adventures?".
4. **⭐ SAVE/LOAD — retail Indy is a 16-bit INDYSAV44 format** (Yoda YODASAV44 is 32-bit). (A)
   "loads OK but specific scripts broken" = our loader restored per-object type/x/y + zone
   globalVar/planet that retail DROPS → clobbered regenerated state; fixed Zone::Read/WriteSavedState
   (Iact.cpp) to the Indy 16-bit record. (B) full retail read+write via new IndyWrite/ReadWorldState
   (Worldgen.cpp tail) + OnSave/OnLoadWorld branch + magic→INDYSAV44 + recursive full-flag 2B.
   VERIFIED by new `save_smoke` harness (round-trip seeds 1/42/7 preserve tail scalars). ⚠ retail-.sav
   READ has a few medium-confidence field identities (self-consistent for OUR saves; needs a real
   retail .sav to fully validate). Full spec: docs/phase-h3-indy.md "INDYSAV44"; memory [[h3-indy-load]].
   ⚠ Indy save FORMAT CHANGED — old YODASAV44-hybrid Indy saves won't load (none existed on disk).

---

### ⏮ PRIOR PICKUP (2026-07-16 v91 — self-contained macOS `.app` packaging landed;
v88 WASM core still the headline GOAL-4 work below. v87/v86 detail condensed → PLAN_COMPLETED.md ⏮.)

**▶ v91 (2026-07-16) — ✅ self-contained macOS `.app` build (all cmake/tools; anchor & game TUs
UNTOUCHED — no src/ edits, no anchor-oracle run needed).** THE footgun (OpenJKDF2 lineage):
Homebrew ships SDL3 **dylib-only**, so a normal build bakes `/opt/homebrew` linkage that breaks on
other Macs. New `YODA_SDL_FETCH=ON` (cmake/PortableSDL.cmake) FetchContent-builds **SDL3 3.4.12 +
SDL3_mixer 3.2.4 STATIC from source** (`SDL_SHARED OFF`+`BUILD_SHARED_LIBS OFF`+`SDLMIXER_VENDORED
ON`, heavy codecs OFF) → `otool -L` shows ONLY `/usr/lib/*` + system frameworks. The `app` target
(`tools/make_macos_app.sh`, APPLE-guarded) assembles `<AppName>.app` and **fails the build** on any
non-system dylib (the otool gate). Assets stage into `Contents/MacOS/` (= `_NSGetExecutablePath`
data dir; zero code change): DTA/DAW + `sfx/` [Yoda] or loose `*.WAV` [Indy] + starter `yoda.INI`.
Icon = game's GROUP_ICON 2 via `tools/make_icns.py` (decodes the 32×32 DIB, **nearest-neighbour**
upscale — sips interpolation ghosted the light icon, user-flagged). Presets
`macos-app-{demo,full,indy}` (build the `app` target). Build+boot USER-CONFIRMED (Yoda-full .app
opens, plays); crisp icon confirmed. ⚠ writable state (INI/saves) still lives in the bundle → fine
locally, NOT read-only `/Applications` — the deferred **InstallHelper/XDG pass** (OpenJKDF2-style,
writable state → `~/Library`/`$XDG_*`) is the natural next step the user already flagged. Docs:
BUILDING.md "Build: macOS `.app` bundle"; memory [[h4-microfx]] v91.

**▶ v88 — ✅ WASM port core (GOAL 4).** Full recipe + architecture facts live in the "WASM
build/debug" section above — the load-bearing findings:
- **ASYNCIFY dissolved the modal-loop lift**: every blocking wait already funnels through
  `MfxPlatDelay()` → route it to `emscripten_sleep()` and the nested `while(GetMessageA)`
  loops (DoModal/CFileDialog/AfxMessageBox/intro) just WORK in a browser. Plus one yield after
  each present (busy-wait animation loops present via the clock hook and never touch Delay).
  NO game-code restructuring, NO emscripten_set_main_loop conversion.
- **`mfxplat_sdl3.cpp` IS the wasm backend** (`--use-port=sdl3`; tiny `__EMSCRIPTEN__` deltas:
  file dialog returns -1, delay/present yields). The planned separate mfxplat_wasm.cpp was
  unnecessary — the browser is just another SDL3 platform.
- **New `mfxsnd_sdl3stream.cpp`** (SDL3-core streams bound to one device — SDL mixes; no
  SDL3_mixer port exists): fixed the user-reported perma-mute + dead Sound/Music checkboxes
  (null backend ⇒ the game's own no-sound-card path). Yoda ships NO .mid (audio = 66 sfx WAVs),
  so this is full Yoda audio; also the native fallback now when SDL3_mixer isn't installed.
  Browser audio-graph PROVEN via puppeteer AudioContext instrumentation (state=running@48kHz).
- **Cross-libc portability fixes**: emscripten time_t is 64-bit vs the 1997 `long time(long*)`
  decls (Score/MainFrm) → `MFX_TIME32_SHIM` name-redirect at the tails of Worldgen.h/MainFrm.h
  (token-neutral; keep the 2 copies synced) + wrappers in mfxcore. And the planet re-pick spins
  an UNSEEDED rand() ("first rand() of the process" — macOS/musl/msvcrt all disagree!) → new
  YODA_DEBUG-only `YODA_PLANET` env pin in LoadWorld (sibling of YODA_SEED; BOTH pins are
  required for any cross-host A/B).
- **Oracles all green**: node worldgen_smoke 5/5 seeds byte-identical yoda_debug.log vs native;
  zone_view BMP pixel-identical; browser boot/interact/audio via `tools/wasm_boottest.js`
  (checked in — see the CLAUDE.md recipe; puppeteer-core + system Chrome; screenshots proved
  title → worldgen → Dagobah + bubble + inventory + walking IN THE BROWSER).
- **Two asset modes (user-set)**: `build-wasm` (YODA_WASM_PRELOAD=ON default + YODA_DEBUG=ON —
  assets baked, for automation) and `build-wasm-pick` (OFF — SHIPPABLE, zero game data,
  `microfx/web/mfx_asset_picker.pre.js` folder picker; smoke-tested via puppeteer directory
  upload, boots to gameplay). Serve: `cd build-wasm && python3 -m http.server 8777`.

**▶ v89 (same day) — deploy + user-feedback fixes, all verified via the puppeteer oracle:**
`tools/deploy_wasm.sh` builds + copies the GitHub Pages layout (user-set): `yodecomp/index.html`
= chooser (microfx/web/chooser.html — detects WHICH game a picked folder holds by data-file
name, stashes the Files in IndexedDB, redirects to per-game PRELOAD=OFF builds in
`yodecomp/{full,demo,indy}/`; the picker pre-js consumes the stash so no re-pick — survives
refresh too) + `yodecompdemo/` = PRELOAD=ON demo build (freely-distributable assets baked,
instant play). Chooser flow tested end-to-end (upload YodaFull → redirect → boots). ⭐ AUDIO
LAG ROOT CAUSE (user found the key clue: "later click = more lag"): browsers keep AudioContext
SUSPENDED until a user gesture; the sdl3stream/ScriptProcessor path queued samples the whole
time and drained the backlog at realtime rate after the first click — a PERMANENT shift equal
to the pre-click wait. Fix = new `mfxsnd_webaudio.cpp` (wasm default): each SFX is an
AudioBufferSourceNode `start(0)` (immediate; renders on the browser's audio thread, immune to
Asyncify stalls), and plays attempted while the context isn't 'running' are DROPPED, not
queued. Also: custom shell `microfx/web/mfx_shell.html` (no emscripten branding/console
textarea, dark centered canvas) and the window/tab title now falls back to the
AFX_IDS_APP_TITLE string 0xE000 ("Yoda Stories") in CWinThread::Run, mirroring the
AfxMessageBox caption chain. Pages repo deploy is copy-only — the user reviews + commits
shinyquagsire23.github.io themselves.

**▶ v89 tail (user-driven polish, all USER-CONFIRMED):** (1) YodaDemo app icon = page favicon
(GROUP_ICON 2 via reslib → .ico → data-URI in mfx_shell.html + chooser.html; ⚠ emcc bakes shell/
pre-js at LINK time — LINK_DEPENDS added so edits actually relink yoda.html). (2) Boot renders
Win95-correct now: the screen DIB color table starts ZEROED, so pre-realize paints were wrong —
presents black, and OnEraseBkgnd's COLOR_BTNFACE nearest-matched to a BLUE once a partial palette
existed ("interface draws on a blue background until worldgen finishes"). Fix: seed the table with
the 20 Win95 STATIC colors (GetSystemPaletteEntries, slots 0-9/246-255) at pump start — realizes
overwrite it after, identical to before; do NOT pin statics through realize (game indexes assume
identity — pinning would break confirmed-correct sprites). Shell html bg = 3DFACE gray too.
(3) Health-dial 3D rim was missing because ::Chord was a SILENT STUB (mfxstubs) — DrawHealthDial
draws two Chord halves (hilite/shadow) under the green Pie disc. Real Chord now in mfxgdi.cpp
(Pie's scanline + arc-side-of-chord-line half-plane test); sunken look confirmed. ⭐ grep
mfxstubs.cpp for remaining silent GDI stubs when a UI element "draws flat/missing".
(4) Win95 WINDOW CHROME around the web page (user request): mfx_shell.html draws the full
frame in CSS — teal desktop, navy titlebar (favicon icon + title synced from document.title),
disabled min/max, and a WORKING X: it sets `window.__mfxCloseReq` (a JS handler must NOT call
into wasm mid-Asyncify-suspend) which MfxPlatPollEvent polls under __EMSCRIPTEN__ and turns
into MFXPLAT_EV_QUIT → the game's own "Leave Yoda Stories?" modal. Screenshot-verified. A
C++-side compositor version (extend the v83 menu-bar strip) is the path for homebrew ports
(user floated Wii U).

**▶ ⚠ Open watch-items:** (1) the v86 one-off Replay SIGSEGV (exit 139, never reproduced —
lldb `bt 25` if it recurs). (2) wasm INI/save persistence: MEMFS is lost on reload — IDBFS
(mount + sync on write) is the natural next wasm milestone. (3) picker mode: a user INI with
Terrain=-1 (Indy-shared bottle INI) would infinite-retry worldgen — consider sanitizing in the
pre-js. (4) F1→How to Play + YODA_ACCEL=1 still never live-verified.

**▶ NEXT (likely session shape):** (1) user feedback from wasm play sessions (audio now in —
needs EARS; the picker page; anything weird under Asyncify timing) and the still-pending v87
Indy playtest items (real menu bar, Replay persistence across restarts, corrected opcodes,
live confirm boxes; user copies build-sdl-indy/yoda to YodaIndy/ themselves). (2) wasm tails:
IDBFS persistence · Indy-wasm corner (DESKTOP.DAW preload exists in the CMake branch already;
MIDI needs a soft-synth — TinySoundFont + a GM .sf2 in the preload, ~2 MB) · maybe host the
picker build somewhere static. (3) GOAL 3 Indy Ghidra RE sweep as fill.

- ⚠ worldgen needs Terrain∈{1,2,3} in the INI (Terrain=-1 ⇒ infinite Generate retry). Harness INIs:
  `<exebase>.INI` next to the binary (wasm harnesses: always `<cwd>/yoda.INI`); doc ctor re-picks the
  planet EVERY run and writes it back — reset before A/B runs, and pin `YODA_PLANET` alongside
  `YODA_SEED` for any cross-host/cross-libc diff (v88; both YODA_DEBUG-only) (v85: persistence tests
  must also snapshot/restore `[GameData]`!). `worldgen_smoke
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

**▶ Anchor:** 211 exact / 99.17 % — ALL 5 oracles re-verified in v88 after the shared-code edits
(Worldgen.cpp YODA_DEBUG planet-pin block; MFX_TIME32_SHIM tails on Worldgen.h/MainFrm.h): link
0 unresolved/0 dup, bugscan 0 HIGH/0 SHIFT, vt 10 CLEAN, msg 11 CLEAN.
All Indy work GAME_INDY-guarded; all H4 work YODA_PORTABLE-guarded;
debug rig YODA_DEBUG-guarded (committed builds OFF). H4 rule of thumb: fix portability in microfx
headers/stubs first (v85's CFile fix is the model case); touch a game TU only for __asm /
pointer-width casts / old-for-scope / a genuine crash-class bug, always guarded via
`YODA_SIC_FIX`, always re-oracled — and NEVER add an unguarded include to a byte-matched TU
(lesson 6).
