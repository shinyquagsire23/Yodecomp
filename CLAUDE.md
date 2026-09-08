# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#40 + MFC-matching lessons** (later lessons #41–#47 are standing bullets in this file) (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-09-07, v135; re-baselined 217→234 at v100 (MEASUREMENT FIX); 234→237 at v102, 237→240 at v103, 240→244 at v104, 244→247 at v105, 247→249 at v106, 249→250 at v107, 250→251 at v108, 251→255 at v110 (REAL MATCHES); **255→252 at v114 — a DELIBERATE, user-approved re-baseline DOWN**, see below; held at 252 at v115; **252→255 at v116 (REAL MATCHES — the CONTAINER CALL FORM, lesson #48)**; held at 255 at v117, which landed no new match but cut 654 bytes of residual STRUCTURALLY via the new LENGTH census, lesson #49; **255→257 at v118 (REAL MATCHES — the MEMBER-ALIAS/CSE-reload dial, lesson #50, plus the INNER-BLOCK decl axis no tool could reach)**; **257→258 at v119 (REAL MATCH — the COMPOSITE LEVER, lesson #51: two dials that each measure WORSE alone, including on LENGTH, landing together; it also cut 1029 bytes of residual across four more functions and put four more LENGTHS exactly on their Ghidra extents)**; **258→255 at v120 — the SECOND DELIBERATE, user-approved re-baseline DOWN, for a real ARITY BUG in `TextDialog::Layout`, see below; v120 also cut 1050 bytes of residual across three functions and put two more LENGTHS exactly on their extents**; **255→256 at v121 (REAL MATCH — the CROSS-JUMPED IF/ELSE, lesson #52: a constant argument materialized by a BRANCH in the original is two duplicated CALLS tail-merged, not an expression)**; **held at 256 at v122 — no new byte-match, but `OnNewDocument` 0x41bb10's LENGTH went 946 -> 975 = its extent EXACTLY (537 B -> 422) on a RECOVERED MISSING SOURCE CONSTRUCT, the house CATCH_ALL+THROW_LAST, found by lesson #53's read-it-out-of-your-own-exact-code method**; **held at 256 at v123 — no new byte-match, but the SIX closest-to-exact functions in the project (11 bytes total) were PROVEN unreachable from the source and closed: lesson #54, the compare-encoding peephole**; **held at 256 at v124 — no new byte-match, but the v123 pickup's #1 open question was ANSWERED for the common case by lesson #55, the `this`-RESIDENCY rule (`tools/thisscan.py`): an EH frame is the dial, and cl 10.20 SPILLS `this` by default under one (68 of 75) while ENREGISTERING it without one (71 of 78, 0 counterexamples). v124 also closed a SEVENTH function under lesson #54 (the LEA SIB guise) and positively CONFIRMED `DrawHealthDial`'s statement order from the EH state store**; **held at 256 at v125 — no new byte-match; the session's product is a RETRACTION: v124's named mechanism for `DrawHealthDial` ("the original CSEs the GetSysColor import address and we do not") is FALSE — we emit the identical construct in a different register — and it had already become the v124 pickup's #1 priority. Also `DrawTextA` 0x40f060, the project's last 2-byte residual, CLOSED under lesson #54's new REASSOCIATION guise, and a new both-sides census `tools/impcse.py` whose 5-function output REPLACES the refuted target list**; **256→257 at v126 — +3 REAL (`OnDraw` 0x409110 386 B→0 and `OnUpdate` 0x408e70 283 B→0, both on lesson #52 CALL DUPLICATION found by the new `tools/pushscan.py`; `CyclePalette` 0x415af0 fell out downstream) MINUS 2 to the TU joint phase, a USER-APPROVED trade — plus `DrawWeaponBox` 0x428ac0 301 B→16 with its LENGTH onto the extent, and v121's lesson-#52 census REOPENED as too narrow (`tools/xjumpscan.py`, 6 hits→12)**; **257→258 at v127 — `FindEntityAt` 0x40b210 recovered (+1/−0), one of the two functions v126's phase rotation cost; plus a NEW ORACLE `tools/widthscan.py` (the parameter-TYPE gap `aritycheck.py` is structurally blind to, 12 hits) and lesson #58: a decl-order match on a PHASE VICTIM is not evidence about the source, because BOTH spellings are byte-exact — each at a different phase**)

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
⇒ **252 was the floor from v114; v116 climbed back to 255 by a DIFFERENT route** (the
container call form — see lesson #48), NOT by reverting 0x41ef90. The v114 trade still
stands and must not be undone: `WorldgenShuffleList` keeps its unrotated loop, and the
three functions it cost are still non-exact. Do not read "back to 255" as "v114 was wrong".

⛔ **v120 RE-BASELINED THE ANCHOR DOWN AGAIN, 258 → 255, ON PURPOSE (user-approved).** The
second deliberate decrease in the project's history, and unlike v114 it is not a codegen
trade at all — it is a **fixed TRANSCRIPTION BUG**. `TextDialog::Layout` 0x4176f0 takes
**THREE** int parameters, not the two we had declared. Proven from two independent sides:
the callee ends **`ret 0xc`**, and its ONLY call site (`Position` 0x417570 at +0x172) pushes
**three** args, `push 0; push ebx; push esi`. The third is always the literal 0 and is NEVER
READ — an argument-slot scan over Layout's whole body finds reads of `[esp+0x34]` and
`[esp+0x38]` only — which is exactly why nothing downstream ever noticed. `Layout` is
declared in **`DeskcppView.h`, which every TU includes**, so changing the type list re-rolls
the shared codegen dial: it cost `CyclePalette` 0x415af0, `ZoneHasIzxItemMaybe` 0x41bfa0,
`ParseZax2` 0x423210 and `DetonateAdjacentTiles` 0x428680, and gained `SetCurrentToIntroZone`
0x423d20 (−4/+1). **None of the four is a defect in its own body.** ⚠ The parameter NAME is
not the dial input — an unnamed `int` third parameter measures the IDENTICAL −4/+1, so this
could not be had for free. ⇒ The trade was taken because **a wrong signature in a
decompilation is a defect in the artifact, not a tuning choice**; the exact count is an
instrument, and a correct source that scores 255 beats a wrong source that scores 258. Do not
revert it to "recover" the three. Detail: the source notes at 0x417570 and 0x4176f0.
⭐ **Generalise the FIND, not the trade: `ret N` at the callee vs the push count at the call
site is a two-sided ARITY oracle, and nothing in the project checks it.** A same-arity scan
over all 410 markers is the obvious next instrument (see the v120 pickup).

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

Phases A–G (byte-matching YodaDemo.exe's app region): **257 functions byte-exact / 99.17 % coverage** (v100's
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
| exact count | `python3 tools/progress.py` | **257 exact / 99.17 % transcribed** |
| full link | `tools/link_exe.sh` | 0 unresolved / 0 duplicates / exit 0 |
| field/slot bugs | `python3 tools/bugscan.py --all` | **1 HIGH (known benign) / 0 SHIFT** |
| vtables | `python3 tools/vtcheck.py` | 10 classes CLEAN (+13 skipped, unanchorable) |
| message maps | `python3 tools/msgcheck.py` | 11 maps CLEAN |
| **arity** | `python3 tools/aritycheck.py` | **0 mismatches / 264 comparable** (v120) |
| **frame size** | `python3 tools/framescan.py --exact <set>` | **15 hits / control CLEAN** (v128) |
| **instruction mix** | `python3 tools/mixscan.py` | **27 hits / control CLEAN over 226** (v131, new) |

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

⚠ **257 is the CURRENT baseline (234 at v100, +3 REAL at v102, +3 REAL at v103, +4 REAL at v104, +3 REAL at v105, +2 REAL at v106, +1 REAL at v107, +1 REAL at v108, +4 REAL at v110, **−3 DELIBERATE at v114**, +3 REAL at v116, +2 REAL at v118, +1 REAL at v119, **−3 DELIBERATE at v120**, +1 REAL at v121, **+1 NET at v126 (+3 REAL / −2 phase, user-approved — see the v126 note below)**, +1 REAL at v127 (a v126 phase victim recovered — but read lesson #58 before treating it as evidence), **−1 DELIBERATE at v128 (a user-approved trade for a PROVEN form — see the v128 note below)**, held at 257 at v129 (no new byte-match, but `LoadWorld` 0x421fd0 went 1047 B → 485 B with its LENGTH onto the extent EXACTLY, +0/−0, on the generalised loop-form census — see lesson #40's v129 addition), held at 257 at v130 (no new byte-match; the product is `tools/movsxscan.py`, the both-sides short→int PROMOTION census — 132 CONFIRMED / 23 ORIG-MORE / 13 OURS-MORE — plus 16 measured negatives closing `RefreshZone` 0x403ae0's last named axis), held at 257 at v132 (no new byte-match; `PlaceZone` 0x4260e0 went ext−7 → ext−6 FREE on v119's zero-init fold, and the session's product is a TOOL FIX — `declorder.py --inner` was seeing only each block's LEADING decl run, and generalising it takes the seam 685 → 1162 permutable decl lines across 48 residuals — plus lesson #61, the vtable-store construction-order oracle, which REFUTED a plausible 9-byte length win on `DrawHealthNeedle` 0x4278a0), ****held at 257 at v133 (no new byte-match, but `UpdateDragCursor` 0x412cc0 went 379 B @ ext+9 → 112 B @ len 1246 = the extent EXACTLY, +0/−0, on the NEW `for`/`do`-while ALLOCATION-RANK dial, lesson #62 — the converse of #40, and invisible to every loop census because the emitted loop is the SAME SHAPE either way; landing it knocked `CyclePalette` 0x415af0 out and it was RECOVERED FOR ONE TOKEN by re-running its own prescribed sweep, the first time a phase victim cost nothing)**, held at 257 at v131 (no new byte-match, but `WorldgenPlacePuzzles` 0x421930 went 617 B @ ext−11 → 629 B @ ext−8 FREE on the loop ROTATION dial — the original tests its retry loop at the BOTTOM and shares one `return 0` with the entry guard; found by the new `tools/mixscan.py` INSTRUCTION-MIX census, lesson #60, which also triages 45 of the ~92 residuals as source-CLOSED `PURE-REG`)**; held at 257 at v134 (no new byte-match, but `ShowWinMessage` 0x40f4b0 — the richest
residual in the tree — went 1671 B @ ext+36 → 817 B @ len 1980 = ext−9, +0/−0, on FOUR
COMPOSING structural fixes found by the NEW `tools/jseqscan.py` conditional-jump SEQUENCE
census, lesson #63; `CyclePalette` 0x415af0 was recovered for ONE TOKEN for the second
session running); all oracles
re-run in the same pass); **held at 257 at v135 (no new byte-match, but the LARGEST SINGLE
STRUCTURAL LANDING in the project's history: `ScrollZoneTransition` 0x411180 — the biggest named
unknown in the tree, parked since G1 behind an explicit "do not open this function without a
candidate" — went 702 B @ ext−62 → 761 B @ len 912 = ext−1, +0/−0 with NO phase victims, on ONE
call-form change, lesson #64: the four scroll blits are the CDC MEMBER form
`pDC->BitBlt(x,y,w,h,pDC,xSrc,ySrc,SRCCOPY)`, the source DC being pDC ITSELF. A `push` is a memory
write, so it kills a member-load CSE; the global form therefore loads each duplicated rect field
TWICE while the member inline precomputes the whole argument DAG and loads it once. v135 also
DIAGNOSED the whole of `Layout` 0x4176f0's −35 (lesson #65: a `cmp` with no consumer is SOURCE,
not codegen — cl cross-jumps identical arm BODIES and strands the test, so the author repeated the
bx range ladder inside each nTailDir arm; the mnemonic census closes on it exactly) and refuted
that function's G1 park note. The `--lenmis` census fell 33 residuals / 321 B → 27 / 184 B.)**; **held at 257 at v136 (no new byte-match, but `Layout` 0x4176f0 — the top of `--lenmis`, and the function v135 had fully DIAGNOSED — went 999 B @ ext−35 → 1010 B @ ext−23, +0/−0, on TWO constructs read straight off the original's BLOCK LAYOUT, lesson #66: its bx ladder is a NESTED `if/else` and not a flat `else if` chain (a flat chain lets cl cross-jump the identical trailing arms and DELETE the `jle` outright), and its nTailDir dispatch is a `switch` and not an if-chain (`je / je / jmp`, both arms out of line). SEVEN mnemonic columns went from wrong to EXACT. `CyclePalette` 0x415af0 was recovered for the FOURTH session running by re-running its own prescribed sweep, and its winning cell has swung BACK to v103's original spelling. `--lenmis` fell 184 B → 172 B.)**, held at 257 at v137 (no new byte-match; a CLOSING session — `Layout` 0x4176f0's park note was SPLIT INTO TWO PROBLEMS by the free store test, lesson #67, and the only source lever for the aliasing half was REFUTED on shape and length; `IactProbeMove` 0x406550 was read out end to end and its `n`-SCOPE axis closed; FIVE of the v132 decl-run seam's named targets swept to a measured floor; and the `movsx` self-extension dictionary went from one byte-exact entry to two); **held at 257 at v138 (no new byte-match, but `Layout` 0x4176f0 went 1396 B @ ext−23 → 1412 B @ **ext−7**, +0/−0, on TWO COMPOSING fixes — lesson #68, the converse of #59: a variable REDEFINED ON EVERY PATH of an if/else cannot be CSE'd from its initializer, which is what forces the original's ADDRESS CSE + per-block reloads; landing it then UN-REFUTED the case-2 inner ladder that had been a standing MEASURED NEGATIVE, for a further 12. `CyclePalette` 0x415af0 was recovered for the FIFTH session running — and TWICE inside this one. `--lenmis` fell 172 B → 156 B)** ⭐ **v100's +17 was a MEASUREMENT CORRECTION, not 17 new byte-matches** — those functions
were ALREADY byte-exact and were being scored against the WRONG addresses; do not read it as progress on
matching. **v102's +3 and v103's +3 ARE matching** (v102: the TextDialog scroll family, via `CWnd::SendMessage`;
v103: `ParseSnds` via a buffer SIZE, `OnEraseBkgnd` + `CyclePalette` via the member-call form;
v104: four via DECLARATION SCOPE, lesson #37; v105: the decl SET+ORDER refinement, lesson #38;
v106: the statement-ORDER-around-a-materialized-constant lever, lesson #39; v107: SetCurrentToIntroZone
via lesson #38, plus SaveZoneRecursive via the new LOOP-FORM lever, lesson #40; v108: ParseTilesMaybe
via decl ORDER at function scope, lesson #38 — found only because the loop-form sweep came back FLAT;
**v116: ParseChar, RemoveEmptyZonesFromPlacedList and ParseChwp, all three via the CONTAINER CALL FORM,
lesson #48**). The project-wide per-TU count is
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
⭐ **v129 — THE SEAM WAS NEVER MINED OUT; THE CENSUS COULD ONLY SEE ONE SPELLING OF IT.**
`tools/loopform.py` matched `for (x = 0; x < n; x++)` only, so the *same defect* written as a
guarded do-while (`do { ...; i++; } while (i < n);`) was invisible — and that is what
`LoadWorld` 0x421fd0's zone delete loop was. Writing the house countdown there took it
**1047 B @ +6 → 485 B at length 1684 = the Ghidra extent EXACTLY, +0/−0 collateral**, and it
also removed the HOMED loop count (`cmp edi,[ebp-0x3c]` per iteration) that `framescan.py`
had reported as half of a +8 frame delta. Same failure family as v126's `xjumpscan` and v127's
`widthscan` first drafts: **match the MECHANISM, not one instance of its output.** Generalised
(`DOWHILE_CMP`, deliberately not matching the countdown `!= 0` or a flag `== 0`); the seam goes
**14 → 22 candidates**, five of them with no `for` loop at all. ⚠ its positive control had also
ROTTED — it printed "0x403070 exact? expected False" for a function byte-exact since v116.
⭐ **THE DECOMPILED BODY CAN CONTAIN THE COUNTDOWN VARIABLE SPELLED OUT.** `OnLoadWorld`
0x424fc0's questItems loops test `if (nCount - i == 1)` — `nCount - i` IS the countdown, and
with `nCount` counted down the test is just `nCount == 1`. ⇒ **an expression of the form
`<limit> - <index>` inside a loop body is direct evidence the original counted down.** Each
conversion alone cuts ~90 B at the length still EXACTLY on the extent; the pair costs 4 bytes
of length, so it is measured-but-not-landed (see the source note) — the finding to carry
forward is the READING, not the trade.

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

⭐ **READ AN UNKNOWN CONSTRUCT OUT OF YOUR OWN BYTE-EXACT CODE (v122, lesson #53) — the
cheapest identification method in the project, and it works on EH FUNCLETS, which nothing
else here ever looked at.** When the original emits a call sequence you cannot name, do NOT
guess and do NOT go to Ghidra first: **scan every function's extent for the same sequence,
then intersect with the exact set.** A hit inside a byte-exact function means OUR OWN SOURCE
already contains the construct — read it off and copy it.
- Landed `CDeskcppDoc::OnNewDocument` 0x41bb10: **537 B @ −29 → 422 B @ ±0, the LENGTH
  landing exactly on the 975-byte extent.** Its G1 note called the residual "the
  reg-rename/schedule family"; the length said −29, so per lesson #49 that note named a
  symptom. The real defect: the Canvas allocation's `TRY { } END_TRY` is really the house
  hand-expanded CATCH_ALL. The catch funclet at +0x390 reads `push -1; push 0; push 0xe01e;
  call AfxMessageBox` (0xe01e = `IDS_ERR_UNRECOVERABLE`) preceded by an unidentifiable
  `call <x>; push 0; push 0; call <y>` PAIR. That pair occurs at **6 sites**, two of them in
  functions we already match byte-exactly (`ParseChar` 0x421e70, `ParsePuz2` 0x422fd0) —
  where our source spells it **`THROW_LAST();`**. Descent: 537 @ −29 → 438 @ −14 (catch block
  + `Canvas *pNew;` UNINITIALISED) → 422 @ ±0 (THROW_LAST).
- ⚠ **`= NULL` on a temp is a real dial, and it inflates the FRAME.** `Canvas *pNew = NULL;`
  cost the `xor edi,edi`, a frame store, turned the original's `cmp dword [eax],0` into
  `cmp [eax],edi`, and made our frame 4 bytes bigger (`sub esp,0x34` vs `0x30`). The
  byte-exact `ParseChar` uses the uninitialised form — again readable from our own tree.
- ⚠ The obvious follow-up census (does every `push 0xe01e` catch funclet in the original
  match our source's THROW_LAST/AfxMessageBox count?) is **CLEAN — 17 sites, OnNewDocument
  was the only omission**, so this particular seam is MINED OUT. ⚠ but write the scan to
  extract a function's body between markers carefully: counting identifier occurrences in a
  naive slice picks up SOURCE-NOTE prose and fabricates three mismatches (0x412cc0's note
  literally says "no THROW_LAST here").
⇒ Generalise the METHOD, not the find: the exact set is a **dictionary from machine code back
to source**, and it grows every session. Any time you can localise a construct to a byte
sequence, grep the image for it and look up the answer instead of guessing.

⭐ **THE COMPARE-ENCODING PEEPHOLE IS DIAL-BOUND, NOT SOURCE-BOUND — PARK IT ON SIGHT (v123,
lesson #54). This is the first residual class the project has proven UNREACHABLE from the
source, and it accounts for the SIX CLOSEST-TO-EXACT functions in the tree.** The class: a
residual whose only differing bytes are a comparison's ENCODING — `cmp mem,reg` (39) versus
`cmp reg,mem` (3b) with the jcc mirrored, or `test r,r` versus `cmp r,<register that holds 0>`
— at IDENTICAL length, identical registers and identical schedule. `residuals.py`'s `kinds`
column already names it (`cmp-swap`, `jcc-mirror`).
- **The census is 6 functions / 11 bytes, and it is the whole class:** `OnPaletteIsChanging`
  0x419460 (**1 B** — the closest function in the project), `SaveStoryHistory{Nevada,Alaska,
  Oregon}` 0x402670/0x4029c0/0x402d10 (2 B each), `LoadStoryHistoryNevada` 0x401ac0 (2 B) and
  `FindObjectAt` 0x405330 (2 B, the `test`-vs-`cmp` guise).
- ⭐ **It is not the condition's spelling, and that is now a POSITIVE CONTROL rather than an
  assertion.** Run the operand-order sweep on the BYTE-EXACT twin `OnPaletteChanged` 0x4193f0:
  `pFocusWnd != this`, `this != pFocusWnd`, both `!(... == ...)` forms and the cast all stay
  EXACT. So cl 10.20 normalises the compare whatever the source order says, and the sibling's
  exactness proves the sweep can detect a change if there is one to detect.
- ⭐ **It is POSITIONAL.** The three `LoadStoryHistory*` clones are textually identical apart
  from the planet name. Physically REORDERING their definitions moves the `39/jg` form with the
  **3rd SLOT**, never with the function — all four non-identity permutations give
  slot1=3b, slot2=3b, slot3=39. Injecting N identical extra clones ahead of them keeps `39` on
  the 3rd clone for N=1,2 and makes it **vanish entirely at 6+ clones**, i.e. a bounded
  optimiser-state effect. Neither the string literal nor the member array offset changes it.
- ⛔ **The one axis that DOES move it is the TU's file-scope SYMBOL COUNT — the ❌ forbidden
  padding dial.** A single `static int` or bare `extern int` ahead of the block flips the form
  (and a static decoy FUNCTION flips `SaveStoryHistoryAlaska`'s two sites 3b,3b -> 39,39); a
  comment line is inert. That is exactly the mechanism `dialsweep.py` measures, and v101 closed
  the missing-symbols hunt, so **these six are CLOSED, not open** — do not "recover" them by
  adding declarations.
- ⭐ **What makes it evidence about the COMPILER rather than the 1997 source:** the ORIGINAL
  binary oscillates across its OWN clones too (Load = jg/jl/jg; ours = jl/jl/jg), and our
  `LoadStoryHistoryOregon` already emits the `39` form from the identical text.
⚠ **TRIAGE RULE: when `residuals.py` reports a residual whose kinds are only `cmp-swap` and/or
`jcc-mirror`, park it immediately.** A spelling sweep there is guaranteed waste — v123 spent
~35 compiles across four of these re-confirming flat, which is the cost this rule now saves.
⭐ **v124 ADDS A THIRD GUISE AND A SEVENTH FUNCTION: the LEA SIB base/index swap.**
`GetFrameTile` 0x404850 is DIFF(2) and the two bytes are the SIB byte of its two direction
LEAs — ours `8d 54 02` = `[edx+eax+6]`, the original `8d 54 10` = `[eax+edx+6]`. It is NOT a
register bijection: **both images hold the same values in the same registers** (edx=bank,
eax=dy), so only the commutative address's base/index ENCODING differs. Eight source
spellings — every operand order, both parenthesisations, constant-first — are DEAD FLAT at
DIFF(2)/len 183. ⚠ and the tempting "base = the younger temp" rule (which the byte-EXACT
`AddHealth` 0x427690 obeys, emitting `[eax+esi-1]` from `nScaled / -3 - 1 + nLo`) DESCRIBES
cl's output without being REACHABLE from the input — no spelling reproduces it. ⇒ extend the
triage rule: **a commutative-operand ENCODING difference at identical length, registers and
schedule is dial-bound whatever the operator** — compare, test-vs-cmp, or LEA addressing.
The pickup that called this "the cheapest remaining real target" was wrong; it is closed.

⭐ **`this` HAS A RESIDENCY RULE, AND THE EH FRAME IS THE DIAL (v124, lesson #55) —
`tools/thisscan.py`.** A `__thiscall` receives `this` in ECX; cl 10.20 then parks it in a
callee-saved register (ENREG), stores it to a frame slot and RELOADS it before nearly every
use (SPILL), or leaves it in ECX (small leaf bodies). Which one it picks is worth 10-60 bytes,
because each SPILL reload is a 3-4 byte `mov ecx,[...]` — and those reloads are precisely the
bytes a NEGATIVE length delta is missing (lesson #49). Measured over all 213 byte-exact
`__thiscall` functions:
- **NO EH frame ⇒ ENREG is the default (71 of 78).** The ONLY exceptions are the **three**
  functions needing FIVE OR MORE long-lived values, and all three spill with the IDENTICAL
  prologue shape: `sub esp,N` / `mov [esp+k],ecx` / `push ebx` / `push esi` / `push edi` /
  `push ebp` — all four callee-saved registers already committed, so `this` has nowhere to go.
  **0 counterexamples in either direction.** ⭐ All three (`DrawEntities` 0x40b160,
  `SaveZoneRecursive` 0x4033b0, `LoadZoneRecursive` 0x403450) are BYTE-EXACT in our tree, so
  lesson #53 applies: our own source already spells this construct. `ScrollZoneTransition`
  0x411180 (-62) is the fourth instance and the only unsolved one.
- **EH FRAME (`/GX` + any object with a dtor, or a TRY) ⇒ SPILL is the default (68 of 75).**
  ebp is the frame pointer so only 3 registers are available, and `this` loses **even with
  registers to spare** — 53 of the 68 spills are not saturated at all (mostly ctors/dtors
  saving one register or none). ⇒ **an EH frame is itself the strongest predictor that the
  original reloads `this`**, which retires a whole family of "why is the register allocation
  different" park notes.
⚠ **The 7 ENREG-under-EH exceptions are NOT explained by use count, and claiming they are
would be the vacuous-column mistake**: `~Zone` 0x4054d0 SPILLS at 21 `this` uses while
`WorldgenPushZoneEntry` 0x41d6b0 ENREGS at 4. All 7 do share `nsaved >= 2`. Treat the EH rule
as a strong default with a genuinely OPEN exception set — `DrawHealthDial` 0x427490's original
is an 8th exception, and v124 traced ITS cause to a **CSE of the `GetSysColor` IMPORT ADDRESS**
in EBX (one `mov ebx,[__imp__]` + four 2-byte `call ebx`, against our four 6-byte
`call dword ptr [__imp__]`) — a third long-lived value that forces all four coords to the
frame. The coords-in-memory question and the `this` question there are ONE question.
⚠ **Implement it from the PROLOGUE and stop at the first `call`** — the same v110 trap
savescan.py hit; a linear sweep of a whole body desyncs on an embedded jump table or EH data.
⚠ **And watch the letters**: the first draft mapped both `ebx` and `ebp` to "b", silently
merging two different saturation states — another instrument that would have lied.
⭐ **Free corollary — the EH STATE STORE is a statement-order oracle.** `mov byte [ebp-4],N`
marks exactly how many EH-protected objects have been constructed, so its POSITION reads the
original's statement order straight off the machine code. On `DrawHealthDial` the state-3
store sits immediately BEFORE the coord block, which POSITIVELY CONFIRMS that the coords are
computed after all four GDI objects (our current order) and refutes the hoisted variant that
otherwise looked like a 10-byte length win. Use it before sweeping a statement-order axis.

⛔ **v128 RE-BASELINED THE ANCHOR DOWN, 258 → 257, ON PURPOSE (user-approved).** The third
deliberate decrease. `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 goes **117 B @ len +13 →
33 B @ len 401 = the Ghidra extent EXACTLY** on two independent structural fixes (lesson #59
below + a corrected parameter TYPE); landing them rotates the Worldgen.cpp joint phase and
costs `SetCurrentToIntroZone` 0x423d20. That function is a **phase weathervane, not a
defect** — byte-exact v107–v113, DIFF(2) after v114, exact again from v120, DIFF(5) now —
and its own floor was RE-SWEPT at the new phase (6 configurations dead flat at 5, the
countdown refuted by LENGTH 53 vs 60). ⚠ Line-neutral spelling does NOT avoid the rotation
(v126's finding again: it is the TOKEN change), and all three legal both-arms spellings
measure identically, so it could not be had for free. Its residual is now the SAME ESI↔EDI
bijection 0x41d0c0 carries, so the two may fall together to a later TU-joint pass.

⭐ **DIFF THE INSTRUCTION MIX BEFORE YOU READ A SINGLE BYTE (v131, lesson #60) —
`tools/mixscan.py`.** A byte diff shows the ORIGINAL clearly and OURS only as "the other
column", and once the schedule shifts it stops being readable at all: `WorldgenPlacePuzzles`
0x421930 differs in 47 % of its 1299 bytes and `sbs.py` prints 600 lines of noise. Decode both
sides, count MNEMONICS and subtract, and the same function collapses to five numbers —
`test -1, jge -1, jmp -1, movsx -1, mov +1`. The `test`+`jge` pair IS the find, and nothing
else in the toolbox could see it.
- ⭐ **It landed the v131 fix.** Those two missing instructions say the original tests its
  retry loop at the BOTTOM (`test edi,edi; jge <body>`) and falls through to a `return 0` that
  the entry guard's `jl` also targets — one shared exit, both edges directly readable. Our
  `for (;;) { if (nZoneId < 0) return 0; ... }` structurally cannot emit that. **117 B of
  length deficit closed: -11 -> -8, FREE (+0/-0), with the mix then agreeing EXACTLY on
  test/jge/jmp/ret/pop.** Only one of four spellings does it (see below).
- ⭐ **AN EMPTY DELTA WITH A NONZERO BYTE DIFF IS THE STRONGEST SIGNAL IT GIVES** — the streams
  agree instruction-for-instruction and ONLY registers/encodings differ, i.e. the lesson-#44
  scratch bijection / lesson-#54 compare-encoding class, which is source-CLOSED. It prints
  those as `PURE-REG`: **45 of the ~92 residuals**, half the census triaged as closed in one
  run. Park them instead of sweeping spellings at them.
- ⭐ **THE ROTATION DIAL HAS FOUR SPELLINGS, NOT TWO, AND ONLY ONE IS RIGHT** (sharpens #46).
  Measured on 0x421930: `for (;;) { if (!c) return 0; ... }` = top test + `jmp` backedge;
  plain `while (c) { ... } return 0;` is **BYTE-IDENTICAL to it** (cl does NOT rotate a loop
  with `goto` exits — #46's "while rotates" rule is not universal); an UNGUARDED
  `if (!c) return 0; do { ... } while (c); return 0;` gets the bottom test right but gives the
  second `return 0` its OWN epilogue (+12 B, overshooting the extent); only
  **`if (c) { do { ... } while (c); } return 0;`** shares the exit block and lands the length.
- ⚠ **Land it on the LENGTH, not the diff.** The byte diff ROSE 617 -> 629 here; those 12 bytes
  are downstream shift from an already-open block-layout family. Lesson #48's "large diff cut"
  bar governs call-form conversions, where the number is the only evidence; a construct read
  directly out of the original's control flow is evidence in its own right (the v107 rule).
- ⚠ **Its own first two drafts lied, and the positive control caught both** — reloc-MASKING the
  original corrupts a shifted stream (the v121 trap this very docstring warns about), and
  padding a short extent out to our length with zeros resurrected four byte-EXACT functions as
  phantom residuals. Give every new census the "byte-exact functions must report nothing" check
  BEFORE reading its output, not after.

⭐ **THE `for` / `do`-WHILE CHOICE IS AN ALLOCATION-RANK DIAL, AND IT IS THE CONVERSE OF
LESSON #40 (v133, lesson #62).** #40/#129 taught one direction: the original's house
`do`-countdown where we wrote a `for`. v133 is the other, and it is invisible to every loop
census the project owns because **the emitted loop is the same shape either way** — `mixscan`
shows no `jmp`/`test`/`jcc` delta at all. What changes is the loop index's ALLOCATION RANK: cl
10.20 ranks a `for`'s index above the same variable driving a `do`-while backedge, and that one
rank change decides who gets the last callee-saved register.
- **`UpdateDragCursor` 0x412cc0: 379 B @ len 1255 (ext+9) → 112 B @ len 1246 = the extent
  EXACTLY, +0/−0.** Its hi-colour pixel loop was `int y2 = 0; ... do { ... y2++; } while (y2 <
  32);`. As a `do`, y2 lost EDI to a **CSE of the `SetPixel` import address**; as
  `for (y2 = 0; y2 < 32; y2++)` y2 outbids the import, which returns to the 6-byte memory call.
- ⭐ **The +9 decomposed exactly, which is what makes it evidence** (the "decompose the length"
  method): +6 `mov edi,[__imp__SetPixel]`, −4 the shortened `call edi`, +3 for
  `inc dword [ebp-0x28]`/`cmp dword [ebp-0x28],0x20` over the register forms, +3 the pre-push
  reload, +1 the init.
- ⭐ **THE FINGERPRINT IS AN `impcse.py` HIT ∩ A `framescan.py` OURS-LARGER HIT.** We CSE an
  import the original does not *and* we home a local — those are one decision, not two, because
  the CSE is what evicted the local. Check that intersection first; at v133 0x412cc0 CLEARED
  both, leaving `DrawWeaponIcon` 0x428c40 and `WorldSizeDlg::OnHScroll` 0x418560 as the
  remaining members.
- ⚠ **Only the OUTER loop is evidence** — `for`/`for` and outer-`for`-only measure IDENTICAL
  (112 @ 1246), so the inner one's form is unobservable and was left as the `do` it was.
- ⭐ **Two neighbouring axes fell out with it, both pinned:** the index `i` must be declared
  OUT beside y2 rather than inside the `if` (in-block 119, out 112), and **its zero must be a
  STATEMENT at the loop, never part of its declaration** — `int i = 0;` hoisted measures 303 B
  at len 1255, i.e. REFUTED BY LENGTH. The comma in `for (i = 0, y2 = 0; ...)` is NOT the lever
  (a separate `i = 0;` statement scores the same); within the comma `i` must precede `y2`.
⚠ **The collateral is the lesson-#58 case in the wild, and it cost nothing only because a park
note had predicted it.** Landing this re-rolled DeskcppView.cpp's joint phase and knocked
`CyclePalette` 0x415af0 out on UNCHANGED text (6 B, an eax↔ecx bijection). v110's finding there
— "mgmm is the unique 0" over 16 call-form combinations — was **PHASE-BOUND, not a fact about
the source**, and the function's own note said so and prescribed re-running the sweep. Widened
to 32 cells (GetDC/ReleaseDC added as a 5th axis) it has **three** exact cells at the new phase,
all sharing a2=global + sel=member; the minimal one was taken (RealizePalette → global form, one
token). ⇒ **Before accepting a phase victim as the price of a proven form, re-run whatever sweep
originally landed it.** A victim is often recoverable at the new phase for one token, and that
turns a −1 re-baseline into +0/−0. (SelectPalette is NOT phase-bound there: the global form
costs 18 bytes of LENGTH in all 16 cells, which positively confirms the member form.)

⭐ **ALIGN THE JUMP *SEQUENCES*, NOT THE ADDRESSES — `tools/jseqscan.py` (v134, lesson #63).**
`mixscan.py` (#60) reduced `ShowWinMessage` 0x40f4b0 to eleven numbers, two of which were
`jne -5 / je +5`: five conditional branches whose POLARITY is inverted. That is a COUNT, not
a location, and in a 2025-byte residual differing in 82 % of its bytes there was no way to
find them — `sbs.py` prints 880 lines and **`armscan.py` structurally cannot help, because it
compares jcc's at the same ALIGNED instruction boundary and nothing after the first differing
byte is aligned any more.** Decode both sides, keep ONLY the conditional jumps, and LCS-diff
the two mnemonic lists: control flow is the one property that survives a schedule shift.
- **It drove the whole v134 landing.** All five flips localised to ONE construct — the
  `field30 == 1` quest-list selector — and **cl INVERTS the source polarity wherever it
  TAIL-MERGES the two arms**, so the original's `jne`-to-B / A-fallthrough is emitted from
  `if (field30 != 1) B; else A;`. ⭐ That is the identical spelling `OnBumpTile` 0x413df0's
  note has carried since G1, i.e. the find was already written down in our own tree for a
  neighbouring function (lesson #53's dictionary method, applied to a source note).
- ⭐ **The polarity is PER SITE and that is what makes it evidence**: the three
  `GetAt(GetSize() - 1)` selectors in the same function are NOT tail-merged and keep the
  `== 1` spelling. A file-wide rewrite would have been the ❌ padding move.
- **Three classes, and the class decides whether to invest.** `POLARITY` = arm order
  (#47), SOURCE-STEERABLE. `MIRROR` = compare-operand exchange (#54), DIAL-BOUND — park on
  sight; **20 of the 25 hits are pure MIRROR**, so the census is mostly a triage instrument.
  `SHAPE` = a branch on one side only: a real control-flow difference, or a MOVED BLOCK.
- ⛔ **THE LCS IS LOAD-BEARING, NOT A REFINEMENT — and the positional first draft LIED.**
  Compared by ORDINAL, `LoadWorld` 0x421fd0 reports **FOURTEEN** polarity flips and reads as
  the richest target in the tree. It has NONE: a single `je` moves from index 41 to index 77
  (the loop-exit cleanup block, which that function's own note has recorded since v129), and
  a moved block rotates the entire tail of the sequence. Caught by hand-disassembling the top
  hit before investing — the v117 rule — NOT by the positive control, which passed either
  way. ⇒ **a positive control proves a tool is not wrong about the null case; it says nothing
  about whether the tool's ALIGNMENT MODEL matches reality.**

⭐ **THE CALL FORM DECIDES WHETHER ARGUMENTS ARE PRECOMPUTED, BECAUSE A `push` IS A MEMORY
WRITE THAT KILLS A MEMBER-LOAD CSE (v135, lesson #64) — the sharpest guise of lesson #35 yet,
and it was worth 61 bytes in one edit.** `ScrollZoneTransition` 0x411180 had been the project's
biggest named unknown since G1 (−62, "one register-budget decision", "do not open this function
without a candidate"). Its four scroll blits are the **CDC MEMBER form** —
`pDC->BitBlt(x, y, w, h, pDC, xSrc, ySrc, SRCCOPY)`, the source DC being pDC ITSELF — where we
had written the global `::BitBlt(pDC->m_hDC, ..., pDC->GetSafeHdc(), ...)`.
**702 B @ 851 (ext−62) → 761 B @ 912 (ext−1), +0/−0, no phase victims.**
- ⭐ **THE MECHANISM, and it generalises past GDI.** In the global form cl evaluates each
  argument lazily and pushes it, so two occurrences of the same member (`rectUnk3274.top`, and
  `.left`) straddle a `push` — a memory write — and cl reloads, 6 bytes each. Expanding the
  member inline makes cl evaluate the whole argument DAG into pseudo-registers BEFORE the first
  push, so each field is loaded ONCE and duplicated with a 2-byte register copy. ⇒ **when the
  original loads a member ONCE and COPIES it while we load it twice around a push, the lever is
  the CALL FORM, not a named local.**
- ⭐ **It is also what sets the register budget**: precomputing lifts each arm's live scratch
  demand from 1 to 5 (top, top-copy, left, left+16, safehdc), which is *why* the original
  commits ebx AND ebp to per-arm temps and then has nowhere left to keep `this` (lesson #55).
- ⭐ **FREE TWO-SIDED ORACLE, NO COMPILE: the source-DC null test decides the call form.**
  `CDC::BitBlt` expands `pSrcDC->GetSafeHdc()`, so the member form emits a
  `test <reg>,<reg> / je / mov <reg>,[<reg>+4]` diamond whenever the source DC is a POINTER —
  and none at all when it is `&someLocalCDC` (cl folds the test). The DESTINATION hdc is the
  last push and never has one. On 0x411180 the diamond is present ⇒ member form; on
  `UpdateDragCursor` 0x412cc0 all four blits load the source hdc with NO diamond ⇒ the global
  spelling is POSITIVELY CONFIRMED there. Read it before spending a compile.
- ⚠ **The seam is SMALL and now MINED OUT — 9 global-form CDC/CWnd call sites tree-wide**: 2 in
  byte-exact functions (confirmed), 4 converted at v135, 3 refuted by the missing diamond.
  Re-run only on newly-transcribed functions.
- ⛔ **The arm-local coordinate hypothesis is REFUTED AT BOTH BASELINES** (707 B @ 853 under the
  new call form; 705–708 @ 853 across 6 spellings at v121). Reproducing the v121 number under
  the NEW form is what proves the two axes independent — cf. lesson #51.

⭐ **A LADDER'S *NESTING* AND A DISPATCH'S *STATEMENT KIND* ARE BOTH DIALS, AND THEY ARE READ
STRAIGHT OFF THE BLOCK LAYOUT (v136, lesson #66) — the constructive half of lesson #65.** #65
said a dead `cmp` proves the source evaluated it. v136 is how you then WRITE it, and both halves
were worth 12 bytes on `Layout` 0x4176f0 (999 B @ ext−35 → 1010 B @ ext−23, +0/−0), taking SEVEN
mnemonic columns from wrong to exact.
- ⭐ **A FLAT `else if` CHAIN AND A NESTED `if/else` ARE DIFFERENT BLOCK LAYOUTS.** cl 10.20
  cross-jumps the trailing arms of a FLAT chain whose bodies are identical — which deletes one
  arm *and its jcc*. Nest the same conditions one level and it keeps both arms as separate
  blocks, each with its own `jmp`. On 0x4176f0 the original has TWO identical `sub eax,0x10`
  blocks and a `jle`; our flat chain had one block and no `jle` at all. The fix:
  `if (bx >= 144) { if (bx > 0x100) H; else M; } else { if (bx < 0x20) A; else A; }`.
  Landing it put `jge` 2/2, `jle` 1/1, `jg` 0/0, `sub` 11/11 and one `jl` exactly on target.
- ⭐ **THE COMPARE'S IMMEDIATE TYPES THE OPERATOR, FOR FREE, BEFORE ANY COMPILE.** `bx < 0x101`
  emits `cmp 0x101`; `bx <= 0x100` emits `cmp 0x100` — and only the second can ever produce the
  `jle` the original emits. ⇒ **read the original's immediate and match it exactly**; a
  semantically identical off-by-one spelling is refutable at zero cost. This is the same
  free-oracle move as v135's `GetSafeHdc` diamond.
- ⭐ **A `switch` AND AN `if/else if` CHAIN LOWER DIFFERENTLY, even with two cases.** A switch
  emits `cmp eax,1 / je / cmp eax,2 / je / jmp` with EVERY arm out of line; the if-chain emits
  `cmp eax,1 / jne` with arm 1 INLINE. On 0x4176f0 the switch was FREE in length and cut 30 B of
  diff, landing `je` 3/3, `jne` 3/3 and `jmp` 11/11. ⇒ **`je / je / jmp` in the original is the
  fingerprint of a `switch`**; an if-chain can never produce it.
- ⚠ **The byte diff ROSE 999 → 1010 across this landing.** Per lesson #60, a construct read out
  of the original's CONTROL FLOW is evidence in its own right — land it on the LENGTH.
⚠ **AND THE COMPANION NEGATIVE, which is where the method's limit is:** the inner nTailDir
ladders that #65 diagnosed are REFUTED BY MEASUREMENT at both baselines (+44 B at ext−35, +64 B
at ext−23; flat and nested spellings identical). The `nTailDir == 2` arm reproduces the original
instruction for instruction — it is the `nTailDir == 1` arm that mis-merges, because our
`rectBox.bottom` CSE survives the `point[]` stores and the original's does not. ⇒ a correct
diagnosis can still be unlandable while a NEIGHBOURING defect (here a member-RELOAD/residency
decision) blocks it; fix the blocker first and re-measure, don't respell the diagnosed construct.

⭐ **A VARIABLE REDEFINED ON EVERY PATH CANNOT BE CSE'd FROM ITS INITIALIZER — THE
CONVERSE OF LESSON #59 (v138, lesson #68).** #59 said: assign in BOTH arms of an if/else so
cl can re-use a register whose value has just died. v138 is the same construct aimed at the
opposite outcome — making a value die *on purpose* — and it solved the x-ladder half of
v137's split on `Layout` 0x4176f0 (**ext−23 → ext−19**, and the ladder landing that followed
took it to **ext−7**, +0/−0).
- **The two spellings, and why they are not codegen-equivalent.**
    ours  `int bx = nBoxX; if (nMode == 0) bx -= pW->nViewLeft;`
    orig  `int bx; if (nMode == 0) bx = nBoxX - pW->nViewLeft; else bx = nBoxX;`
  Ours has ONE definition of the `nBoxX` temp dominating the whole body, so cl keeps that
  temp (the VALUE CSE) in a register for the later `nBoxX` reads and spends a SECOND register
  on a copy for `bx` — `mov ecx,[esi+0x18]; mov edx,ecx; sub edx,...`. The original redefines
  `bx` on every path, so no single register holds `nBoxX` across the merge: the value CSE is
  unavailable and cl falls back to an **ADDRESS CSE**, `lea edx,[esi+0x18]` plus a 2-byte
  `[edx]` reload per BLOCK. Same register COUNT on both sides — this is not pressure.
- ⭐ **THIS IS THE MISSING HALF OF LESSON #67, AND IT RETIRES ITS OPEN QUESTION.** #67 taught
  the discriminator (a store between load and reload ⇒ aliasing; no store ⇒ the CSE lost its
  register, fingerprinted by cl degrading to an address CSE) and then asked "what gives it one
  more long-lived value?". **Wrong question.** The address-CSE half is not about how many
  values are live, it is about WHERE THE LIVE RANGE STARTS — and that is spelled in the
  source, for free.
- ⚠ **The oracle pins a FAMILY** (lesson #36): then-first, else-first, the ternary and a
  two-statement then-arm all measure IDENTICALLY. Pick the member the original's FALLTHROUGH
  pins (lesson #47) — here `cmp [esi+0x54],0 / jne` skips the subtract, so `nMode == 0` is the
  *then* arm.
- ⚠ **The byte diff ROSE 1010 → 1036 across it.** Land it on the LENGTH and the SHAPE
  (lessons #60/#66, triage rule 13); the extra diff is downstream shift.
- ⛔ **THE SEAM LOOKS EMPTY OUTSIDE 0x4176f0, and the probe that says so is UNSOUND — do not
  rebuild it.** A census of "who holds an address CSE where the other side holds a value"
  (`lea rD,[rB+disp]` whose rD is later dereferenced at displacement 0) reports 10 differing
  functions, but its KEY IS NOT REGISTER-BLIND: whether some later instruction happens to
  dereference *that same register* with no displacement is allocation, not source. It rates
  `DrawHealthNeedle` 0x4278a0 at orig 4 / ours 26 when the two sides' `lea` lists are
  near-identical CPen/CBrush object addresses. Same failure as `movsxscan.py`'s first draft
  (see "census key must be register-blind"). Its two REAL sub-findings are worth keeping,
  both caught by the positive control: (1) our COMDATs have reloc fields ZEROED, so
  `mov [eax+0x459558],0` decodes as `[eax+0]` and masquerades as a base-only deref — key on
  `insn.disp_size == 0`, the ENCODING, never on `disp == 0`; (2) the 39 jump-table functions
  must be EXCLUDED (their reloc-zeroed entries decode as instructions on our side only).
  What survives on the merits: all four ORIG-more candidates are refuted BY LENGTH before any
  compile, because the address-CSE form ADDS bytes and none of them is length-short.

⭐ **TWO RELOADS ARE NOT ONE MECHANISM — SEPARATE THEM BEFORE YOU NAME A LEVER (v137,
lesson #67).** When the original re-reads a member that we hold in a register, there are TWO
different causes and they need opposite fixes. The discriminator is free, needs no compile, and
is decisive: **look for a STORE between the first load and the reload.**
- **A store is present AND the register still holds the value at the reload ⇒ a STORE-KILLED
  memory CSE**, i.e. an ALIASING question. `Layout` 0x4176f0 case 2 is the clean proof:
  `+0x1fb mov eax,[esi+0x60]` / `+0x1fe mov [esp+0x14],eax` (eax still = rectBox.top) /
  `+0x202 mov eax,[esi+0x60]` — a redundant reload of a live register value. cl only does that
  because the store killed the CSE.
- **No store in between ⇒ the value CSE simply LOST ITS REGISTER**, i.e. a register-pressure
  question. Its fingerprint is cl degrading to an **ADDRESS CSE**: `lea edx,[esi+0x18]` once,
  then a 2-byte `mov r,[edx]` per BLOCK. 0x4176f0's x-ladder is this — there is no store at all
  between +0x176 and +0x191, so no aliasing lever can ever reach it.
⛔ **AND THE ALIASING LEVER IS ALL-OR-NOTHING, which is why it was refuted here.** The only
source construct that kills a member CSE is an INDIRECT store (`TriPoint *pp = point;` and store
through `pp`). Measured on 0x4176f0: it DOES produce the missing reloads of `[esi+0x18]`,
`[esi+0x60]` and `[esi+0x68]` — the mechanism is confirmed — but it kills after **every** store,
so we reload 3x per arm where the original reloads ONCE per block, and the length goes
1396 (ext−23) → **1424 (ext+5)** for a 4-byte diff gain. REFUTED on SHAPE and on LENGTH.
⇒ **A park note that lists several reloads as one symptom is hiding two problems.** Split them
by the store test first; otherwise you will aim one lever at both and it will overshoot.

⭐ **THE SELF-EXTENSION `movsx r32,r16` NOW HAS TWO DICTIONARY ENTRIES, AND ITS CENSUS KEY IS A
TRAP (v137).** Read-only scan of all 410 extents: **85 sites in 27 functions**, but the great
majority are the ordinary `movsx eax,ax` widening of a short-returning call. The construct that
matters is the NON-eax form — the value stays in the register it was produced in — and **two of
those sit in BYTE-EXACT functions**, so lesson #53 gives two readings instead of the one the
0x423df0 note carried: `Canvas::BlitFast` 0x408110 +0x5d (`height = canvasH - destY;` then
`int rows = height;`) and `WorldgenShuffleList` 0x41ef90 +0x79 (`rand()`'s int remainder narrowed
into a `short`, then used as an int SUBSCRIPT). ⇒ generalised shape: **a value PRODUCED INTO a
16-bit register — a `mov r16,[mem]` load, 16-bit arithmetic, or a narrowing assignment — that is
then used as an int.** ⚠ but do NOT census on self-ness: whether a promotion is `self` is
REGISTER ALLOCATION, not source (movsxscan.py's own first draft died of this). Read the TOTAL
count per side; use the split only as a locator.
⭐ **AND THE FORWARDING-PUSH TRAP RECURRED, in the parameter-TYPE guise.** 0x423df0's note
confirmed `short destX/destY` because "all 21 blit call sites push a plain register with no
sign-extension" — that is not evidence: a plain `push edi` is exactly what an `int` parameter
looks like too, whenever cl maintains the extension IN-REGISTER (which is precisely what that
function's loop-bottom movsx is doing). The sound proof is the register CONTENT: `mov bx,4` /
`mov di,4` never write the high halves, which still hold the caller's values, so the int
incarnation is provably never consumed. Same verdict — but now for a reason that holds.

⭐ **A `cmp` WITH NO CONSUMER IS *SOURCE*, NOT CODEGEN (v135, lesson #65) — the converse of
lesson #52, and it corrects a G1 park note that had stood for the life of the project.**
`Layout` 0x4176f0's note blamed its extra range tests on "cl's TRACE-DRIVEN DUPLICATION … a
range test whose result is unused. Clean source emits none." **cl does not invent a compare
with no consumer.** What it does do is CROSS-JUMP two arms whose BODIES are identical — which
deletes the bodies and strands the condition evaluation. ⇒ **a dead compare is positive
evidence that the 1997 source evaluated it**, and the arms it guarded were textually identical.
- Decoding 0x4176f0's ladder end to end shows each `nTailDir` arm carries a full dead
  `cmp bx,0x20 / jl / cmp bx,0x100` and the low-x arm a dead `cmp bx,0x20`, i.e. the author
  repeated the bx range ladder INSIDE the tail-direction arms. **The arithmetic closes exactly**,
  which is what makes it a diagnosis and not a story: the mnemonic census reads cmp −5 / jl −3 /
  jle −1, and the original has 7 compares to our 2, 3 `jl` to our 0, 1 `jle` to our 0 — nothing
  else in the mix is missing. That accounts for the entire −35.
- ⭐ Same read positively CONFIRMED a spelling we might have "cleaned up": the outer ladder's
  middle and high arms are BOTH `sub eax,0x10`, so the two textually identical `else if`/`else`
  arms in our source are real, not a transcription slip.
- ⚠ `mixscan.py` EXCLUDES this function (trailing jump table). Census it with a throwaway
  `Counter(mnemonic)` over `residuals.paired()` — ~15 lines, and it is what localised the find.

⭐ **A PROBE THAT IMPROVES LENGTH CAN STILL BE A NUMBER RATHER THAN A FACT — VERIFY THE
STRUCTURE, NOT THE SCORE (v135, sharpening the v134 method note).** On 0x4176f0 the member-
pointer axis reads: **A** `int *pbx = &nBoxX;` used at all six point-store sites = 1015 B @
ext−15 (**+20 bytes of length**); **B** the pointer used only for the `bx` read = 1003 @ ext−27;
**C** `int v = *pbx;` read ONCE per arm = **byte-IDENTICAL to baseline**. C is the decisive
cell: reading through a member pointer is CODEGEN-INVISIBLE, so the original's
`lea edx,[esi+0x18]` is a cl ADDRESSING artifact and **not** a source address-of — and A's 20
bytes are SPURIOUS extra reloads (A emits 2–3 loads per arm where the original emits exactly
one). **A was NOT landed.** ⇒ a length gain earns its landing only once the emitted SHAPE
matches; add a third cell that predicts *inertness* and let it referee.

⭐ **A LEVER REFUTED BY LENGTH CAN BE UN-REFUTED BY A LATER LANDING — RE-RUN THE MEASURED
NEGATIVES AFTER EVERY STRUCTURAL FIX (v134, sharpening lesson #51).** #51 said a lever that
measures worse ALONE is not refuted and must be crossed with the arm order. v134 is the
sequential form of the same thing, and it paid twice in one function: `WORD id` on
0x40f4b0 measured **2028 = ext+39** at v133 and was written up as a ⛔ MEASURED NEGATIVE
("REFUTED BY LENGTH"); after the arm-order fix landed it measures **2011 = ext+22**, an
8-byte GAIN — and it was that gain which collapsed the frame 0x20 -> 0x18, homed `tx`, and
made the head-locals fix reachable at all. The chain was 1671 B @ +36 -> 1681 @ +30 -> 1678
@ +22 -> **817 @ -9**, and every link but the first is refuted or inert at the baseline
before it. ⇒ **A park note's ⛔ list is PHASE-RELATIVE in exactly the way lesson #106's byte
counts are.** Before starting a session on a function, re-measure the two or three cheapest
entries on its own ⛔ list; they cost one compile each and one of them may have flipped.

⭐ **A DEFAULT-CONSTRUCTED C++ OBJECT WRITES ITS CLASS *AND* ITS CONSTRUCTION ORDER INTO THE
MACHINE CODE — READ IT BEFORE LANDING ANY DECL-ORDER "WIN" (v132, lesson #61).** Constructing
a `CPen`/`CBrush` (or any MFC leaf) emits **three vtable stores to the same slot** — CObject's,
CGdiObject's, then the LEAF's — so the leaf constant identifies the CLASS and the slot sequence
gives the ORDER, exactly and for free. This is the sibling of lesson #55's EH-state-store
oracle: that one counts how many objects exist, this one says WHICH and IN WHAT ORDER.
- ⛔ It killed a plausible v132 find outright. `DrawHealthNeedle` 0x4278a0's newly-visible decl
  run `penA,brA,penB,brB,cx,cy` has five permutations that move `penB` to the END and measure
  **len 1131 = ext−8 against our 1122 = ext−17, diff 803 → 771** — 9 of 17 missing bytes and a
  32-byte diff cut, which reads like a lesson-#49 structural win. It is not: the original's four
  leaf vtables run `0x44cfbc @[ebp-0x20], 0x44cfec @[-0x28], 0x44cfbc @[-0x30], 0x44cfec @[-0x38]`
  = **X,Y,X,Y**, i.e. pen,brush,pen,brush — our CURRENT order. The permutation would emit X,Y,Y,X.
- ⭐ **Census our own side too (lesson #56)**: our COMDAT alternates `edi,ebx,edi,ebx` through the
  same four slots, so the construct is confirmed present on both sides and the current spelling is
  POSITIVELY CONFIRMED rather than merely un-refuted.
- ⇒ **A decl-order permutation reorders CONSTRUCTION, which is observable.** When a decl sweep's
  best cell moves object decls past each other, the vtable stores adjudicate it in one read — do
  that before believing the length. Reloc masking zeroes the constants on OUR side, so identify
  the classes from the ORIGINAL and match ours by the *pattern* of registers/immediates.

⭐ **A LOCAL'S LIVE RANGE IS A DIAL, AND `sub esp,N` IS THE INSTRUMENT THAT FINDS IT (v128,
lesson #59) — `tools/framescan.py`.** cl 10.20 will re-use a register whose value has just
died — the classic shape is `f(..., x, ...)` where `x`'s last use is a `push`, immediately
followed by `mov <that reg>,0` for the NEXT variable, *inside the same argument setup*. It
can only do that if the next variable's live range STARTS THERE. Writing
`int nOk = 0; if (c) nOk = f();` starts nOk before the push and the two ranges overlap, so
nOk is HOMED TO A FRAME SLOT instead. Assigning in **both arms of an if/else** —
`int nOk; if (c) nOk = f(); else nOk = 0;` — starts it at the join and reproduces the
original exactly. On 0x41d0c0 that spill WAS the whole `sub esp,8` vs `sub esp,4`:
**113 B → 33 B, length onto the extent.**
- ⚠ **The ARM ORDER is inert here** (if/else, else-first and the ternary all measure 33/401),
  so pick the member pinned by the original's FALLTHROUGH (lesson #47), not by the oracle.
- ⛔ **The DECL dial cannot reach this** — it is not about where `nOk` is declared. All six
  decl SET × ORDER configurations were flat at the OLD phase and all three `nOk`-hoist
  positions are flat at the NEW one. A flat decl sweep is the signal (lesson #41), and the
  answer is the ASSIGNMENT's position, not the declaration's.
- ⭐ **`tools/framescan.py` is the census**: the prologue's reserved local bytes, ORIGINAL vs
  OURS, for every residual. **15 hits / 378 markers, positive control CLEAN** (a byte-exact
  function may not report one; it exits 1 if any does). Read it in two directions —
  **OURS LARGER = we homed a local the original enregisters**; **OURS SMALLER = the original
  homed something we do not have.** It is the third member of the prologue-instrument family
  (`savescan.py` = the callee-save SET, `thisscan.py` = `this` residency) and the only one
  that sees an ordinary local.
- ⭐ **It CONFIRMED lesson #55's last open case from a second side**: `ScrollZoneTransition`
  0x411180's frame is 16 against our 12, and the missing 4 IS the `this` slot. Two tools
  measuring different things agree that its whole −62 is ONE allocation decision, which
  re-scopes that function — the search is for one more long-lived value, not for a spelling.
- ⚠ **A hit is a CANDIDATE.** `DrawWeaponIcon` 0x428c40 (+4) is `nAmmo` homed because we
  spend EBX on a `this->pWorld` CSE the original does not make — a lesson-#42 question, not
  a live-range one (and its `impcse.py` GetSysColor hit is a SYMPTOM of that same decision,
  not a second defect). `DrawWeaponBox` 0x428ac0 (+4) is a slot that is NEVER REFERENCED,
  with rc and pOldPal swapped between the images — frame slot ORDER is decl-order INVARIANT
  (v36's ParseSnds finding) and a RECT has no size knob, so it is genuinely closed.

⭐ **A FORWARDING PUSH DISCRIMINATES NOTHING — v127's `widthscan` DISMISSAL IS RETRACTED
(v128).** v127 called 0x41d0c0's parameter-TYPE hit a false lead because "the callee
0x41cdc0 reads its 3rd argument slot as a DWORD at four sites, so `int nVal` is POSITIVELY
CONFIRMED". Both of those reads are **`mov eax,[esp+N]; push eax` into ANOTHER parameter**,
and that sequence is **byte-identical for `int` and for `short`** — it is not evidence about
the type at all. The real proof runs the other way and is two-sided: `PlaceQuestNode`
0x41f120 loads its own `nOrder` with `mov bx, word [ebp+0x1c]` (garbage upper half) and
pushes EBX **RAW** into 0x41d0c0, which pushes it **RAW** again into 0x41cdc0 — cl widens
whenever a `short` feeds an `int` parameter, and neither side widens, so the parameter is a
`short`. Fixed; **FREE, +0/−0**. ⇒ **Generalise the METHOD: to type a parameter, find a use
that is not a forward.** Only a read that CONSUMES the value (an arithmetic use, a compare,
a store to a wider destination) distinguishes the widths; a pass-through never does. And
the same lesson-#56 discipline applies — v127's claim was about the callee, and checking it
meant reading the callee's own accesses, which nobody did.

⭐ **A DECL-ORDER MATCH ON A *PHASE VICTIM* IS NOT EVIDENCE ABOUT THE 1997 SOURCE (v127,
lesson #58) — the oracle pins a family whose MEMBERSHIP MOVES WITH THE PHASE.** Lesson #36
already said the byte oracle pins a FAMILY of spellings and you must pick the idiomatic
member. v127 found a sharper version: when the TU's joint phase rotates (v105/v106), the
member that is byte-EXACT changes, so a recovered match certifies nothing on its own.
- **The measurement.** v126's `OnUpdate` landing cost `FindEntityAt` 0x40b210 and
  `StepDetonatorEffect` 0x40e400 their exactness. Re-permuting FindEntityAt's four EXISTING
  declarations recovers it exactly (16 B → 0 at len 85 = the extent, +1/−0). But compiling
  **v125's `DeskcppView.cpp` verbatim** shows BOTH functions byte-exact there under the OTHER
  spellings — `pZone/nCharId/n/i` then, `pZone/n/i/nCharId` now. Neither spelling is wrong C
  and no declaration was added or removed, so this is NOT the ❌ v96 padding case; it is
  simply two members of one family, each exact at one phase.
- ⇒ **The rule: refit a phase victim only when the NEW phase is independently better
  supported, and say in the note that the match is phase-bound.** Here it is — v126's OnDraw
  and OnUpdate each landed byte-exact AND with their length on the extent, i.e. the rotation
  moved the TU toward the original source. Without that, refitting is tuning.
- ⭐ **Corollary, and it is why the second victim was DECLINED: a spelling that was byte-exact
  at a previous phase is POSITIVE EVIDENCE, and 8 bytes of diff does not outrank it.**
  `StepDetonatorEffect`'s analogous refit (`int y` before `int x`, which matches the
  original's +0x160-first load order) cuts 32 B → 24 and lands NO match. Left unlanded.
  Its residual is then a clean ebx↔esi 2-cycle on {y, this} = the lesson-#44 class.
- ⚠ **A stale park note will send you down the wrong road here**: FindEntityAt's note still
  read "EFFECTIVE MATCH … 6 byte diff" from G1 while the function was byte-exact, and the
  v126 pickup's per-function byte counts were already phase-relative (v106's corollary). Check
  the CURRENT `bytediff.py` before believing any number in a note.

⭐ **THE PARAMETER-TYPE ORACLE — `tools/widthscan.py` (v127).** `aritycheck.py` (v120) catches
a wrong parameter COUNT from the callee's `ret N`. It is structurally blind to a wrong
parameter TYPE: `short n` and `int n` occupy the same 4-byte slot and clean the same argument
bytes, so the linker, bugscan, vtcheck, msgcheck and aritycheck ALL pass. The only trace is the
ACCESS WIDTH — a caller passing a `short` into an `int` parameter must widen it
(`movsx eax, word [esp+N]`, 5 B); one passing an `int` copies the raw slot (4 B).
- **12 functions differ**, positive-controlled (0 of the byte-exact set may report a hit, and
  it fails loudly if one does). Best unworked: `ApplyHotspotCamera` 0x40e500 (orig 2 / ours 0),
  `TransitionZoneXWing` 0x40e7c0 (0/2, ext−4), `WorldgenFillQuestItemSpot2Maybe` 0x41cf10
  (1/0, ext−3), `PlaceZone` 0x4260e0 (1/0, ext−7).
- ⚠ **A HIT IS A CANDIDATE, NOT A DEFECT — v127's own top hit was a FALSE signature lead.**
  `WorldgenPlaceItemForLockChainMaybe` 0x41d0c0 widens `nOrder` where the original copies the
  raw slot, which reads exactly like `short` vs `int`. It is not: the callee 0x41cdc0 reads
  that argument slot as a DWORD at four sites, so `int nVal` and `short nOrder` are BOTH
  positively confirmed and cl widens because it must. ⇒ **always read the CALLEE's own
  accesses to the slot before touching a signature** — a parameter type lives in a shared
  header and changing it re-rolls every TU's dial (the v120 `Layout` precedent).
- ⚠ **Its first draft repeated v126's xjumpscan mistake exactly.** It required an ALIGNED
  instruction pair, and so could not see the very site that motivated it (the two instructions
  align to different neighbours) — 1 hit instead of 12. Censusing each side STRUCTURALLY
  (lesson #56) fixed it. **Match the mechanism, not one instance of its output.**

⭐ **DECOMPOSE THE LENGTH, THEN NAME THE CONSTRUCT — two more functions fully accounted for
(v127).** The method (lesson #49 + the "decompose the length" memory) paid twice more:
- **`WorldgenPlaceItemForLockChainMaybe` 0x41d0c0, +13**, decomposed instruction by
  instruction: +3 `mov dword [esp+0x14],0` vs `mov ebx,0`, +3 `cmp dword [esp+0x14],0` vs
  `test ebx,ebx`, +2 `mov eax,[esp+0x14]` vs `mov eax,ebx`, +2 the nOrder movsx, +3 frame
  fallout. ⭐ The mechanism is READABLE IN THE ORIGINAL: EBX holds `item1a`, and cl re-uses it
  for `nOk` with a `mov ebx,0` placed INSIDE the argument setup — i.e. after item1a's last use
  (`push ebx`). Our `nOk = 0` is scheduled BEFORE that push, so the ranges overlap and nOk is
  homed. ⛔ all six decl SET × ORDER configurations are DEAD FLAT at 117 B (including the v106
  one-variable merge of the two inner `int next`), so per lesson #41 the lever is elsewhere.
- **The `Zone::ReadZa*` family — 3 functions, 9 bytes, ONE construct.** `ReadZax2` 0x406410
  (−3), `ReadZax3` 0x406490 (−3) and `ReadZaux` 0x406270 (−6, two sites). Each function's
  ENTIRE residual is the original's `mov ax, word [esp+0x12]` + `movsx ebp, ax` (8 B) where we
  emit a single `movsx ebp, word [esp+0x12]` (5 B), plus the downstream shift. ⭐ The lesson-#53
  dictionary has EXACTLY ONE entry for the two-step form — `Canvas::BlitFast` 0x408110's
  `int rows = height;`, where a C-level assignment (`height = canvasH - destY;`) reaches the
  short on one path and the parameter slot on the other — so the shape is the **MERGE of two
  definitions of a short**, and `count` here has only one. ⛔ closed and not to be re-tread:
  120 decl permutations (v109), 20+ statement forms (v99), and v127's `n = count; i = n;` /
  `i = n = count;` (REFUTED BY LENGTH — 119 vs the 114 extent), `i = n` alone, `int i` scoped
  inside the if, both casts, `count = count;`, and a `short t = count;` temp (all flat).

⭐ **THE PUSH FORM IS A TWO-SIDED SOURCE ORACLE (v126, lesson #57) — `tools/pushscan.py`.**
A call argument is either materialised as a literal (`push 0`, 2 bytes) or folded into a
register that already holds the value (`push ebx`, 1 byte). Which one cl picks is NOT a
spelling tie-break: it reports on the source, and it reads in BOTH directions. Both of v126's
wins came out of this one scan, in opposite directions, which is what makes it a rule.
- ⭐ **ORIG pushes a REGISTER, ours an IMMEDIATE ⇒ our STATEMENT ORDER destroyed a fold the
  original had (lesson #39, the v119 fold in the joining direction).** `DrawWeaponBox`
  0x428ac0 writes `bReleaseDC = 1;` BEFORE the `SelectPalette(pal, 0)` call, so cl must
  materialise `push 0`; the original writes the flag AFTER and pushes the still-zero EBX.
  **270 B → 106, and the LENGTH lands exactly on the 375-byte extent.**
- ⭐ **ORIG pushes an IMMEDIATE, ours a REGISTER ⇒ WE merged into one call something the
  original spelled as TWO duplicated calls (lesson #52).** Our version must compute the
  argument into a variable; the original's arm knows it is a constant. `OnUpdate` 0x408e70's
  `::ReleaseDC(hWnd, hdc)` is exactly this — the original spells ReleaseDC TWICE, once per
  arm, and pushes a literal 0 for hWnd. **103 B → 0, BYTE-EXACT at len 479 = the extent.**
- The census is **8 functions**, positive-controlled (0 hits among the 259 byte-exact
  functions — a hit there would mean the instruction alignment is lying). Best unworked:
  `ShowWinMessage` 0x40f4b0 (2 ORIG-reg sites), `LoadWorld` 0x421fd0 (2 ORIG-imm),
  `ZoneTransitionStep` 0x409650 (one of each), `OnDragItem` 0x4102d0, `FireWeaponStep`
  0x40a710.

⭐ **A PATTERN CENSUS THAT HARD-CODES AN INSTANCE'S *SHAPE* SILENTLY UNDER-REPORTS — v121's
lesson-#52 seam is REOPENED, 6 hits → 12 (v126) — `tools/xjumpscan.py`.** v121 built a census
for the cross-jump diamond, found 6 functions, and closed the seam as "essentially mined out".
It scanned for the literal byte shape `jcc / push imm / jmp / push imm` — i.e. a **ONE-push
arm**. `OnDraw` 0x409110's diamond pushes **two** values per arm and was invisible to it;
duplicating that call took the function **386 B → BYTE-EXACT**. Generalising the arm to "one or
more pushes plus argument-setup, joining at one shared call" doubles the census.
⇒ **Match the MECHANISM, not one instance of its output.** This is the same family as the
harness-can-lie lessons, but the bug is in the PATTERN rather than the plumbing: the tool was
correct about everything it looked at, and the thing it could not see was the find.
⭐ **And census OUR side too (lesson #56):** a diamond present in both images is CONFIRMED, not
a target. Of the 12, exactly **2** are ours to fix — `OnBumpTile` 0x413df0 (arms 1/1, inside an
84 %-differing 4635-byte function) and **`OnKeyDown` 0x4150f0 (arms 3/3, NEW at v126)**.
⚠ On OnKeyDown the ARM ORDER is refuted (both spellings dead flat at 1247 B), so its diamond is
not simply inverted — its dominant defect remains the parked shared-tail placement.

⭐ **THE THREE-LEVER DESCENT ON `DrawWeaponBox` 0x428ac0 (v126) — a worked example of lesson
#51's "the dials COMPOSE", and of how a park note names a SYMPTOM.** 301 B @ +5 → **16 B at
len 375 = the extent EXACTLY**, in three independent steps, each a different standing lesson:
1. **#42** — the global form `::GetNearestPaletteIndex((HPALETTE)pWorld->pPalette->m_hObject,
   ...)` let cl keep `pWorld` alive in EBP as a memory CSE across the whole if/else; the MFC
   MEMBER form forces the reload the original makes. That freed EBP, which let `bReleaseDC`
   reach EBX instead of a frame slot. 301 → 270. (The identical find as `DrawTextA` at v110.)
2. **#57/#39** — `bReleaseDC = 1;` must come AFTER the SelectPalette call. 270 → 106, length
   onto the extent.
3. **#48** — `pWorld->tiles[i]`, not `pWorld->tiles.GetAt(i)`. 106 → 16.
⚠ **The ARM LAYOUT was NOT the defect** — the G1 note had blamed it for the life of the
project. It is a SYMPTOM that follows the body (the v116 ParseChwp trap, lesson #41): both
if-spellings are byte-identical at ALL THREE baselines above, i.e. cl canonicalises the
condition here. Measured flat and not worth re-treading: all 6 decl orders of
{bReleaseDC,pOldPal,rc}, `pTile` at function scope. Refuted BY LENGTH: `short bReleaseDC`
(377), inlining the tile expression (379).
⚠ **The seam is MIXED, which is what makes it evidence:** the sibling `DrawWeaponIcon`
0x428c40 is REFUTED on both of the first two axes by LENGTH (member form −10, statement swap
worse), positively CONFIRMING its current global-form / flag-first spelling. Convert per
function, never per file.

⛔ **v126 TOOK A −2 PHASE TRADE TO LAND A PROVEN FORM (user-approved).** `OnUpdate` 0x408e70's
fix is proven from two sides (byte-exact AND its length lands on the extent), but landing it
rotates the DeskcppView.cpp joint phase and costs **`FindEntityAt` 0x40b210 (→16 B) and
`StepDetonatorEffect` 0x40e400 (→32 B)** — so the session is +3 REAL / −2 phase = **257, where
parking OnUpdate would have scored 259**. Neither victim is a defect in its own body.
Measured, so nobody re-treads it: **line-neutral padding does NOT avoid the rotation** (it is
the TOKEN change re-rolling the phase, v105/v107 — not lesson #23), and **all 6 legitimate
exact spellings cost exactly 2**; folding the GetDC into `CDC::FromHandle` merely swaps
FindEntityAt for `ClassifyTile` 0x40fca0 at 896 B, which is why the two-line `HDC hdc =` form
was chosen — the victims are the smallest and most recoverable.
❌ **The ONLY zero-cost variant keeps a DEAD `HWND hWnd;` declaration and was REJECTED** — that
is precisely the v96 forbidden padding dial (a filler declaration kept because it scores
better). Recovering the two victims is the top item in the v126 pickup.

⭐ **A PARK NOTE THAT SAYS "THE ORIGINAL DOES X AND WE DON'T" IS A CLAIM ABOUT *BOTH*
BINARIES — MEASURE OUR SIDE BEFORE BELIEVING IT (v125, lesson #56) — `tools/impcse.py`.**
Every harness lesson in this file is about a TOOL lying. v125 is the first case of the
SOURCE NOTES lying, and notes are read far more often than tools are re-run — this one had
already been promoted to the next session's #1 priority, budgeted at "worth 3 functions".
- **The false claim.** v124 named `DrawHealthDial` 0x427490's -16 as: the original commits
  EBX to a CSE of the `GetSysColor` IMPORT ADDRESS (`mov ebx,[0x45eb94]` + four 2-byte
  `call ebx`) "where we emit four 6-byte `call dword ptr [__imp__GetSysColor]`". **We emit
  the identical construct** — `8b 3d <imp>` at +0x6b + four `ff d7` — in EDI. Its sibling
  `DrawHealthNeedle` 0x4278a0 likewise CSEs `CreatePen` x6 and `CreateSolidBrush` x4 on both
  sides, in rotated registers.
- ⭐ **WHY THE BYTE DIFF INVITES THE ERROR, which is the transferable part.** A byte diff
  shows the ORIGINAL clearly and OURS only as "the other column". When a construct lands at a
  different OFFSET and uses a different REGISTER, its two instructions (`8b 1d` vs `8b 3d`,
  `ff d3` vs `ff d7`) appear in the diff as unrelated bytes and the construct reads as ABSENT.
  ⇒ **A differing REGISTER is not a differing CONSTRUCT.** Census the feature structurally on
  both sides; do not infer our side from the diff.
- ⚠ **The note had already contradicted itself and nobody reconciled it** — the v123 register
  ledger SIX LINES ABOVE the v124 claim reads `ours edi=CSE then x1`. Same failure family as
  v111's "visibly impossible number printed next to the right one for months": *a known-wrong
  line next to a right one will outlive every session that reads past it.*
- ⇒ **What survives:** the residual is ONE ALLOCATION DECISION, not a missing construct. Both
  images save ebx+esi+edi, both CSE the import, both frames are `sub esp,0x3c`; the original
  ranks `this` above every coord and we rank it below. That is purely a lesson #55 question.
- ⭐ **And the corrected census pays immediately: `tools/impcse.py` reports the 5 functions
  where the CSE'd import set GENUINELY differs**, three with corroborating length deltas —
  `UpdateDragCursor` 0x412cc0 (+9; OURS CSEs `SetPixel`, the original does not),
  `DrawWeaponBox` 0x428ac0 (+5; the ORIGINAL CSEs `GetNearestPaletteIndex`, we do not),
  `DrawWeaponIcon` 0x428c40, `OnTimer` 0x40d470 (ours CSEs `SetScrollRange` x3) and
  `WorldSizeDlg::OnHScroll` 0x418560. That is a real target list replacing a refuted one.
- ⚠ Both forms occur in BOTH images, so the CSE is a compiler CHOICE, not a toolchain
  property: byte-exact `Canvas::~Canvas` 0x407eb0 repeats `call [DeleteObject]` FOUR times
  and caches nothing, identically on both sides.

⭐ **LESSON #54 GAINS A FOURTH GUISE: OPERAND REASSOCIATION ACROSS TWO INSTRUCTIONS (v125),
and `residuals.py` now names it `operand-reassoc`.** `DrawTextA` 0x40f060 was the project's
last 2-byte residual and its only defect was which operand landed in the `sub` versus the
`cmp`: orig `sub eax,[i]; cmp eax,[nScroll]`, ours the mirror. cl 10.20 normalises
`(A - i) != nScroll` into `(A - nScroll) != i` by itself, so **the operand the source names is
not the operand you get**. TWENTY-TWO spellings are dead flat at len 877 = the extent exactly
(v110's ten, plus v125's twelve which added three untouched axes: a NAMED TEMP for the
difference, HOISTING the member load into a local, and isolating `i` on the right); the only
two that move are refuted by LENGTH (`short` temp 884, comparing against `slot` 880).
⚠ **It read `UNCLASSIFIED`, because `classify()` compares ONE instruction against its
counterpart and structurally cannot see an exchange BETWEEN two neighbours** — so the
lesson-#54 triage rule never fired on it. Fixed. ⚠ **No positive control exists here**: the
shape `sub <r>,mem ; cmp <r>,mem` occurs EXACTLY ONCE in the whole image, so there is no
byte-exact twin for the v123 control and no lesson-#53 dictionary entry. That makes this a
park on strong evidence, not a proof — say so rather than over-claiming.

⭐ **A CONSTANT ARGUMENT THAT THE ORIGINAL MATERIALIZES WITH A *BRANCH* IS TWO
DUPLICATED CALLS THAT cl CROSS-JUMPED — NOT AN EXPRESSION (v121, lesson #52).** The
fingerprint is unmistakable once named: the original emits `cmp a,b; je L0; push <imm>;
jmp L1; L0: push <imm>; L1: <ONE shared call>` where we emit the branchless boolean
idiom (`sub/cmp/sbb/inc`, or a `setcc`). It is NOT an instruction-selection tie-break.
cl 10.20 tail-merges two source call statements down to the single instruction that
differs — the push — so a diamond whose arms are *just* two `push imm` is the shadow of
a full `if (c) f(..., A); else f(..., B);`.
- Landed `CMainFrame::OnPaletteChanged` 0x4193f0 **54 B → 0** (105 B = the extent) and
  its twin `OnPaletteIsChanging` 0x419460 **54 B → 1** with its LENGTH onto the extent
  (112/112). +1 / −0. Both had been parked since G1 as a "cmp-direction/sbb-vs-branch
  instruction-selection tie-break" — a park note that named a *symptom*, again.
⚠ **EVERY expression spelling folds to the sbb form, so an expression sweep will tell you
the function is closed when it is not.** Measured flat at 54 B on OnPaletteChanged:
`c ? TRUE : FALSE`, `c ? 1 : 0`, `(BOOL)c`, `!!c`, parenthesised, `!(a == b)`, both
operand orders, both pointer casts — AND the two obvious statement forms that keep ONE
call (`BOOL b; if (c) b = TRUE; else b = FALSE; f(..., b);` and the `b = FALSE; if (c)
b = TRUE;` variant). **Only duplicating the CALL moves it.** ⇒ when you see the diamond,
do not sweep the condition — duplicate the call.
⚠ The arm order is load-bearing on top of it (lesson #47): `!=`-first is exact,
`==`-first with the arms swapped is 3 B.
⭐ **The seam is SMALL and now essentially MINED OUT — a census, not a tool.** A
read-only scan of all 410 Ghidra extents for the `jcc / push imm / jmp / push imm`
diamond finds **exactly 6 functions**: 0x4193f0 and 0x419460 (this session's win),
and 0x4270f0 `DrawDirectionArrows`, 0x412250, 0x411730 — all three of which **already
emit the identical diamond at the identical offset**, i.e. positively confirmed. The
sixth, 0x413df0 `OnBumpTile` (+0x11c4, `push 5`/`push 1` into a 3-arg call), is the one
real unworked instance, and it sits inside an 84 %-differing 4635-byte function. Re-run
the scan on newly-transcribed functions only.
⚠ **AND THE OBVIOUS GENERALISATION IS A HARNESS TRAP — it fabricated 8 targets (v121).**
"Non-exact functions where OURS has more `sbb reg,reg`/`setcc` than the ORIGINAL" looks
like the right scan and is not: it must decode the original, and decoding it from a
buffer **sliced to OUR length and masked at OUR reloc offsets** desyncs the stream, so
the orig column reads 0 for functions that visibly have 32 of them. It confidently
ranked `Load` 0x422670 (ours=16 / "orig=0"; the original really has 32), `LoadWorld`,
`Generate`, `OnTimer` and `OnBumpTile`. Caught by re-disassembling five hits from the
RAW bytes at their true Ghidra extents before investing — the v117 rule ("disassemble
two hits by hand before you trust a new seam's ranking") paying for itself again. ⇒ scan
the ORIGINAL from raw bytes at its own extent; never through the candidate's mask.

⭐ **A LEVER THAT MEASURES WORSE ALONE IS NOT REFUTED — COMPOSE IT WITH THE ARM ORDER
(v119, lesson #51). THIS IS THE MOST PRODUCTIVE RULE FOUND SINCE #48, AND IT CORRECTS HOW
#46/#48's LENGTH BAR WAS BEING APPLIED.** Lesson #48 says: land a container-call-form
conversion only on byte-exactness or a large diff cut WITH the length improving. That bar is
RIGHT and stays. What was wrong is that v116 applied it to each conversion measured **in
ISOLATION**, and two of the three it reverted were half of a PAIR:
- **`AddItemToInv` 0x428f50** — arm-swap alone 141 B at len 502; `Add(pNew)` alone 182 B at
  len 495 (i.e. WORSE than the 381 B baseline, with the length moving AWAY from the 506
  extent); **together 56 B at len 506 = the extent exactly.** 381 -> 6 after one more dial.
- **`ParsePuz2` 0x422fd0** — arm-swap alone 94 B at len 318 (overshoots); `Add` alone 164 B at
  len 309; **together 0 B at len 317 = the extent. BYTE-EXACT**, from 165 B. Its own note had
  NAMED both halves since G1 ("nDone++-arm layout family + SetAtGrow this-reg (lea vs add)")
  and still could not land it, because each half alone looks refuted.
⭐ **The MECHANISM, which is why this is a rule and not a coincidence:** the if/else ARM ORDER
(lesson #47) changes which arm is the fallthrough and which is parked out of line — i.e. it
changes the block LAYOUT, and therefore the WIDTH of every jcc whose target crossed the moved
block (a 2-byte short `jl` becomes a 6-byte near `jl`). On ParsePuz2 the whole 7-byte length
deficit was +4 from one such widening, +4 from a second, +2 for the arm's exit `jmp`, −2 for a
`jmp` we emit and it does not, and −1 for `lea ecx,[eax+0xd0]` (Add's fingerprint) replacing
our `add ecx,0xd0` plus a stray NOP. **A call-form change is worth ±1-2 bytes; an arm-order
change is worth ±4 per affected branch.** So a conversion that shortens by 1 CANNOT be judged
until the layout question is settled — the length rule must be applied to the COMBINATION.
⇒ **Procedure: when a call-form (or any small) conversion cuts diff but moves LENGTH the wrong
way, do NOT record it as refuted. Cross it with the arm order and re-measure the 4 cells.**
That is 4 compiles, and it landed two of the three functions v116 had written off.
⚠ **A stray NOP next to a container call is a SYMPTOM of the call form**, not a TRY-expansion
or alignment artifact — ParsePuz2's note blamed the TRY macro for a year.
⚠ Corollary for the OTHER direction: `Add` is **inert** where the container is a `CWordArray`
of values (`WorldgenFill*`, measured identical both ways). Lesson #48's seam is the CObArray
case; don't burn compiles on the value arrays.

⭐ **AND THE SAME "READ THE LENGTH, THEN FIND THE STATEMENT" METHOD CRACKED A THIRD FAMILY
(v119): A LOCAL'S ZERO-INIT PLACED BEFORE A CALL TAKING A LITERAL `0` GETS FOLDED INTO IT.**
`WorldgenFillQuestItemSpot` 0x41c580 and its clone `WorldgenFillSpawn` 0x41c730 wrote
`int j = 0; paSpots.SetSize(0, -1);`. cl reuses the zeroed register as the argument
(`push ebx`, 1 byte) instead of materialising the immediate (`push 0`, 2 bytes) — that single
byte WAS the entire length deficit, and the fold then cost the spots loop its EBX/EDI roles
and cascaded into a 200+ byte residual. Moving the decl AFTER the call
(`paSpots.SetSize(0, -1); int j = 0;`) gives **218 B -> 11 and 227 B -> 26, both landing on
their Ghidra extents.** Same family as lesson #39 (statement order around a materialized
constant), but in the opposite direction: there we needed to JOIN a constant's live register,
here we need to STOP joining it. ⇒ **If you are exactly 1-2 bytes SHORT and a nearby call takes
a small literal, look for a zero/constant init you have placed where cl can fold it.**

⭐ **THE CONTAINER CALL FORM IS A MATCHING LEVER — AND IT IS THE RICHEST SEAM FOUND IN
MANY SESSIONS (v116, lesson #48).** Lesson #35 said an MFC inline wrapper's CALL FORM is a
dial when the object expression is non-trivial. The container accessors are the same lever
with the OBJECT held constant and the ARGUMENT varying, and they are everywhere in this
codebase. Three measured pairs, all semantically identical, none codegen-identical:
- **`a.Add(v)` vs `a.SetAtGrow(a.GetSize(), v)`.** MFC 4.2's `Add` is the inline
  `{ int nIndex = m_nSize; SetAtGrow(nIndex, newElement); return nIndex; }` — it reads
  `m_nSize` straight off the object and takes the array's address with a **LEA**, loading
  `this` FIRST. The SetAtGrow spelling makes cl evaluate `GetSize()` as an ordinary
  argument: element loaded first, address computed with `add ecx,imm`, plus a NOP.
  Landed `ParseChar` 0x421e70 (**110 B → 0**), `RemoveEmptyZonesFromPlacedList` 0x403070
  (**24 B → 0**) and `DamageEntityAt` 0x405710 (**556 B → 51**, length 696 → 690 = MATCHES).
- **`a[i]` vs `a.GetAt(i)`.** `operator[]` is `{ return ElementAt(nIndex); }` /
  `{ return GetAt(nIndex); }` and folds identically at 271 of 277 sites — but not all.
  Landed `ParseChwp` 0x423300 (**47 B → 0**) and `ParseCaux` 0x423290 (41 B → 11).
- **`a[i] = v` vs `a.SetAt(i, v)`** — same family, measured mixed; nothing landed yet.
⇒ **Targeting rule:** grep a non-exact function for `SetAtGrow(X.GetSize()`, `.GetAt(` and
`.SetAt(`. This is a large, mostly unworked seam (~80 SetAtGrow + ~277 GetAt sites).
⚠ **THE SEAM IS MIXED IN BOTH DIRECTIONS, AND THAT IS WHAT MAKES IT EVIDENCE.** A TU-wide
conversion gives roughly as many losses as gains (Worldgen.cpp: 4 and 4). The 1997 author
used BOTH spellings, so a uniform rewrite is the ❌ v96 padding move. Convert PER FUNCTION.
A function that gets WORSE has its current spelling positively CONFIRMED — `CyclePalette`
0x415af0 goes 0 → 6 B, i.e. converting would BREAK an exact function.
⚠ **LAND ONLY ON STRONG EVIDENCE — require byte-exactness, or a large diff cut WITH the
LENGTH improving or already matching.** v116 first landed three conversions on small diff
gains and had to revert them: `AddItemToInv` bought 6 B of diff while moving its length 7
bytes FURTHER from Ghidra's extent (505/506 → 498/506), `ParsePuz2` 1 B for 1 byte of
length, `Generate` 6 B out of 5710. A few bytes of diff on a large residual at a worsening
length is a number, not a fact (lesson #46: LENGTH is the stronger signal).
⚠ **A PER-TU SWEEP'S PER-FUNCTION DELTA IS A CANDIDATE, NOT AN ATTRIBUTION.** The sweep
rewrites the whole TU, so each function's delta includes the phase rotation caused by
EVERY other function's sites. `OnSaveWorld` 0x424540 was reported 2022 B → 1956 B; its own
four sites in isolation move it **not at all** (2022 → 2022). ⇒ isolate before landing.
⚠ **AND THIS REFINES v106's "the TU joint phase is DOWNSTREAM-ONLY".** That rule was
measured on a SINGLE edit and still holds there, but it is not a safety proof when several
edits land together: `DrawRect` 0x424010 sits EARLIER in the file than `OnSaveWorld`, and
neither ParseChwp+ParseCaux nor ParseChwp+OnSaveWorld disturbs it — **only all three
together do**, costing it 60 B → 463 and its length match. The phase is a genuine
INTERACTION. Check upstream functions too after a multi-function landing.

⭐ **A CACHED LOCAL ALIAS FOR A POINTER MEMBER IS A DIAL — AND IT SUPPRESSES A RELOAD THE
ORIGINAL MAKES (v118, lesson #50) — `tools/aliasscan.py`.** cl 10.20 keeps a member load
(`this->pWorld`, i.e. `mov ecx,[edi+0x44]`) as a memory CSE, and **any store made through that
pointer invalidates it**, so the original RELOADS the member before the next use. A local alias
— `CDeskcppDoc *pW = pWorld;` — lives in a register and can never be invalidated, so our code
emits one load where the original emits two, and comes out SHORT. `AddHealth` 0x427690 was
exactly this: the entire 49-byte residual was ONE missing 3-byte `mov ecx,[edi+0x44]` between
the two `= 1` stores in its death tail, and its length read 517 against an extent of 520.
⇒ **Fingerprint: we are SHORT by a small multiple of 3-4 bytes, and the diff shows the original
re-loading a member you hold in a local.** Read it straight off `residuals.py --lenmis`.
⚠ **Dropping the alias is necessary but NOT sufficient, and alone it is usually a big LOSS.**
Removing the decl also perturbs the decl set, and on AddHealth every naive respelling measured
**421 B** (worse than the 49 it started at) because cl then hoisted `pWorld` into a callee-saved
EBX across the whole function. The fix only appears when the alias removal is swept JOINTLY with
the block's decl ORDER (lesson #45's interaction, one level down): `pTile,bFound,i` gives **0 B
at length 520 = the extent**, while the other five orders give 117-421. Land nothing on the
alias axis alone.
⚠ **It is NOT universal — the store is load-bearing.** `TextDialog::Position` 0x417570 holds
`pW = pParentView->pWorld` and is also 3 B short, but all 7 alias/decl variants are **dead flat
at 100 B**: its uses are pure READS, so there is no store to invalidate the CSE and the alias is
codegen-free. ⇒ require a store THROUGH the alias (or through `this`) before investing.
⭐ **`tools/aliasscan.py`** is the target list (READ-ONLY; takes a cached `--exact` set):
**61 hits** in non-exact functions, of which **6 also have a LENGTH mismatch** — that
intersection is the strong form. Best unworked: `UseWeapon` 0x427d20 (−7, six aliases),
`OnNewDocument` 0x41bb10 (−29).

⭐ **THE INNER-BLOCK DECL ORDER WAS UNREACHABLE BY EVERY TOOL IN THE PROJECT UNTIL v118 —
`declorder.py --inner`.** `hoisttest.py` asks about a decl's SCOPE; `declorder.py` permuted only
the LEADING function-scope run. Neither could permute the decls at the top of an `if`/loop body,
and that is precisely the axis that landed AddHealth (+2 with `DetonateAdjacentTiles` 0x428680
falling out for free — one of the three functions the v114 re-baseline cost). It paid twice the
same session: `BlitViewportDither` 0x428e30 went **125 B → 55** with its length moving 237 → 238
toward an extent of 242, purely by declaring the inner `prod` before `x`.
⚠ **The cheap band stays closed on this axis too** (0x41c200, 0x403aa0 flat; 0x423d20 / 0x405330
/ 0x423dc0 have no permutable inner block at all) — consistent with the standing v111–v117
verdict, now extended to inner blocks. Aim it at the MID and LARGE residuals: **45 non-exact
functions have an inner permutable block, 521 legal permutations** — a whole-seam sweep is
affordable, and it is the single most concrete unworked item for the next session.
⚠ **`--inner` SKIPS permutations that move a decl ahead of one its initializer needs** (v118) —
without that filter 16 of 23 orders on 0x41a1c0 came back `COMPILE FAILED`, burning most of the
compiles and burying the real (flat) result; the raw seam is 1435 permutations, the legal one
521. The dependency scan strips MEMBER names first (`mapGrid[i].id` otherwise makes the field
`id` look like a dependency of the local `idw`, a phantom CYCLE that skipped ALL 23 orders — the
v109/v111 "nothing to do" family again), and it SELF-CHECKS that the source's own order is legal
before filtering anything, disabling itself if not.
⚠ **Measured flat on this axis** (do not re-tread): 0x41a1c0, 0x408e70, 0x409c10, 0x40ec30,
0x40f060, 0x41d260. Two moved but stayed below the landing bar of lesson #48 — `BlitTile`
0x40a320 13 B → 12 (`sy` before `sx`) and `ZoneProvidesItem` 0x41c3b0 17 B → 15 (`j` before
`nObjs`, length already exact at 214) — both recorded here rather than landed.

⭐ **LENGTH FIRST: A RESIDUAL WHOSE EMITTED LENGTH IS WRONG IS A STRUCTURAL DEFECT, AND THE
REGISTER DIFFERENCE YOU SEE IS ITS CONSEQUENCE (v117, lesson #49) — `tools/residuals.py
--lenmis`.** Lessons #46/#48 already said LENGTH is the stronger signal; v117 makes it a
CENSUS and it immediately cracked a G1-era park. `ZoneProvidesItem` 0x41c3b0 carried the note
"original keeps `found` in EDI and spills the objects-loop index; ours allocates the reverse …
joint TU pass territory" — a textbook lesson-#37/#44 reading that had parked it for a year. But
its LENGTH was 239 against an extent of 214, and **no register permutation changes length**.
The +25 was two structural defects, and fixing them took it to **DIFF(17) at exactly 214**:
- ⭐ **An arm that RETURNS must not also ASSIGN.** The original's `itemId == -1` arm emits
  `test eax,eax; mov eax,1; jg <epilogue>` — literally `if (nCount > 0) return 1; return 0;`.
  We wrote `found = 1; if (nCount <= 0) return 0;`. That ONE extra assignment is what cost
  `found` its register: with it cl spills `found` to the frame and every later test becomes a
  memory form — i.e. **the "register allocation difference" in the diff was a SYMPTOM of a
  source-level control-flow difference.** 200 B -> 17 B.
- **`break;` vs `return x;` for an inner early-exit is a CODE-SHARING dial.** `return` makes cl
  emit a SECOND full epilogue; `break` falls through to the shared one. 239 -> 225 alone.
  Positively confirmed by the byte-EXACT sibling `ZoneHasIzxItemMaybe` 0x41bfa0.
⇒ **Run `residuals.py --lenmis` BEFORE reaching for any register dial**, and read a park note
that describes a register permutation with suspicion if the length is off.
⚠ **THE RAW LENGTH COMPARISON MANUFACTURES TARGETS — two confounds, both filtered in the tool,
both of which fooled the first run.** (1) 18 of the 410 extents are Ghidra STUBS reading `1`;
0x415a50 `OnKeyUp` reads extent 1 but plainly runs to a `ret 0xc` at 0x415ab5 = 104 B = exactly
our length. (2) The extents stop at the last RET, so a switch's trailing alignment NOP + JUMP
TABLE sits inside our COMDAT but outside the extent — `TriggerHotspotsMaybe` 0x40ec30 reads +36
purely from a 5-entry table at +316, and it is a function v116 had already correctly closed.
Unfiltered the census claimed 75 residuals / 3803 B and put Tick (+339), Run (+328) and
Generate (+237) on top — **all three artifacts**. Filtered: **44 residuals / 408 bytes of real
structural error, 48 length-EXACT (schedule/allocation work only), 31 not comparable.**
⚠ **A NEGATIVE delta means we are MISSING code and is the sharpest signal of all.**
`RefreshZone` 0x403ae0 is 6 B short, and that is exactly the two `movsx ebx,bx` / `movsx edi,di`
(3 B each) the original emits at its loop bottoms — see its source note for two refuted
hypotheses (the increment SPELLING is inert; it is NOT a call-argument promotion, so the
`short destX` blit signatures are positively confirmed by all 21 call sites).

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
⭐ **v116 built the scanner: `tools/armscan.py`** (the lesson was applied by EYE at v115).
It reports **10 hits, 6 of them two-armed** — a small list, so this lever is nearly mined out.
It landed `WorldgenPlaceItemOnLock` 0x41cdc0 (96 B → 93, then → 56 with lesson #39).
⚠ **A FLAT ARM-ORDER RESULT IS NOT A CLOSED FUNCTION.** armscan ranked `ParseChwp` 0x423300
its cleanest candidate (two-armed AND aligned — the flip IS the first differing byte); 15
control-flow spellings measured DEAD FLAT at 47 B and it was written off as source-closed. The
real lever was in the BODY — the container access form — and took it to BYTE-EXACT the same
session. The arm-order verdict was correct; the leap from "this axis is flat" to "this function
is closed" was not. Lesson #41, and easiest to forget when a scanner pointed you at the axis.

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
8. **v116 — a TENTH, the SAME unguarded-`main()` bug a THIRD time, now in `tools/bytediff.py`.**
   v111 fixed `residuals.py` calling `main()` at module level; `bytediff.py` had it too, so
   importing it (which `armscan.py`/`dtorscan.py` must, to share the anchor's pairing block)
   ran the whole census and parsed the IMPORTER's `sys.argv`. Now under `if __name__ ==
   "__main__"`. ⇒ **When you fix a bug of form X in one tool, grep every sibling for X the
   same session** — this project has now paid for that lesson three times.
9. **v116 — and a subtler one that is NOT a code bug: MIS-ATTRIBUTION.** A TU-wide call-form
   sweep reported `OnSaveWorld` 0x424540 improving 2022 B → 1956 B. The number was correct
   and the cause was not: its OWN sites move it not at all, and the gain came from other
   functions' conversions rotating the TU phase. Landing it cost an EARLIER function
   (`DrawRect`) 403 bytes and its length match. ⇒ **A sweep that rewrites many sites at once
   measures the COMBINATION. Isolate a hit before landing it** — the tool was not lying, the
   reading of it was.
10. **v117 — an ELEVENTH, and it had silenced an entire ORACLE.** `residuals.py`'s `lenmis`
    column was `len(orig) != L` where `orig = EXE[foff:foff+L]` is sliced to OUR OWN trimmed
    length — so it could never be True, every residual ever published read `lenmis=False`, and
    the `tie` classifier depended on it. `jointdecl.measure()` has the same vacuous `orig_len`,
    which makes its `L == OL` half of the exactness test dead weight. **A column that cannot
    disagree is not a measurement.** Fixed against Ghidra's extents, which is what created the
    `--lenmis` seam and lesson #49 — i.e. this bug had been HIDING the best remaining target
    list, not just reporting it wrong. ⇒ For every column a tool prints, ask "what input would
    make this say the other thing?" — if there isn't one, it is decoration.
11. **v117 — and the fix's OWN first output was wrong too, in the same session.** The raw
    length census confidently ranked Tick/Run/Generate as the top structural defects; all three
    are jump-table artifacts, and one of its targets (`TriggerHotspotsMaybe`) was a function the
    previous session had already correctly closed. Caught only by hand-disassembling two of the
    top hits before believing them. ⇒ The v103 positive-control rule applies to a NEW SEAM as
    much as to an empty result: disassemble two hits by hand before you trust the ranking.
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
barely moves. ⭐ v121 replaced its VACUOUS `origlen` column — which echoed our own length and so
could never disagree, the same bug v117 fixed in residuals.py — with `ext=<extent> <signed delta>`
read from `toolchain/test/app_funcs.txt`. A variant whose LENGTH moves the wrong way is the
cheapest refutation there is (lessons #46/#49) and that column was hiding it; `jointdecl.py` still
carries the same vacuous `orig_len` and is the next cheap chore) ·
**`chainscan.py [--max-diff N] [--lines]`** (⭐ v111 — the LESSON #43 target list: non-exact
functions whose source passes a POINTER CHAIN as an argument / call receiver / assignment RHS,
each hit tagged `arg`/`recv`/`other`. Reproduces the v110 hand-derived list exactly. ⚠ its own
first draft had TWO bugs, both caught by a known-answer positive control: the `cast()->` regex
missed the canonical PARENTHESIZED `((T *)p)->m` form, and restricting hits to argument lists
dropped both actual v110 wins, which were a receiver and an assignment RHS) ·
**`armscan.py [<tu.cpp>]`** (⭐ v116 — the IF/ELSE ARM ORDER target list, lesson #47: offsets where the ORIGINAL's jcc is the exact INVERSE of ours at the same instruction boundary. Ranks real if/else DIAMONDS above one-armed early-outs and flags sites past the first differing byte as `~unaligned`. 10 hits project-wide. ⚠ COMPILES — don't run it during a sweep) · **`dtorscan.py [<tu.cpp>]`** (⭐ v116 — the DESTRUCTOR-POSITION target list v110 opened by hand: an EH-state store `mov [ebp-4],imm` sitting on the other side of a loop's induction increments reads out how the author SCOPED an object. With v110's three filters applied the seam is **4 functions, not the ~20 estimated**. ⚠ a hit can be a SYMPTOM of a call-form difference, not a scoping error — `DamageEntityAt`'s cleared when lesson #48 was applied. ⚠ COMPILES) · **`unrotscan.py [--exact <file>|--all]`** (⭐ v114 — READ-ONLY target list for the loop ROTATION dial, sibling of loopform.py; currently 1 hit project-wide, i.e. MINED OUT) · **`loopform.py [--exact <file>|--all]`** (⭐ v113 — READ-ONLY target list for the LOOP-FORM dial: the original's countdown backedges vs our up-count COMPARE loops. Safe to run during a sweep; a hit is a candidate, not a defect. ⭐ **v129 GENERALISED IT**: it matched only `for (x = 0; x < n; x++)`, so the same defect written `do { ...; i++; } while (i < n);` was invisible — which is exactly what `LoadWorld` 0x421fd0's delete loop was (1047 B → 485 B at the exact extent). New `our-dowc` column; 14 → 22 candidates, five with no `for` loop at all. Its "0x403070 exact? expected False" control had also rotted since v116 and is replaced) · **`jointdecl.py <spec.py> --expect-exact N`** (⭐ v112 — the JOINT search: applies a combination of WHOLE-FUNCTION source variants, compiles the TU once, prints the byte-diff for EVERY marker in it. Cheap because TUs compile separately, so an edit here cannot move another TU. ⚠ its first run is a NEGATIVE result — 54 combinations over Iact.cpp moved no column but the edited function's own; see lesson #45's second half before reaching for it) · **`aliasscan.py --exact <file> | --all`** (⭐ v118 — READ-ONLY target list for the MEMBER-ALIAS
dial, lesson #50: non-exact functions holding a cached `T *p = <member>;` that suppresses the
reload the original makes after a store through it. 61 hits; cross with `residuals.py --lenmis`
for the 6 that are also length-mismatched. Safe to run during a sweep) ·
**`declorder.py <tu.cpp> <0xADDR> --expect N [--inner]`** (⭐ v108 — permutes the LEADING FUNCTION-SCOPE
decl block; ⭐ v118 `--inner` permutes EVERY decl run, the axis NO tool in the
project could reach (hoisttest asks about SCOPE, not order) — it landed AddHealth 0x427690 and
cut BlitViewportDither 125 B → 55; the axis `hoisttest.py` structurally cannot reach, and the one that landed
ParseTilesMaybe. ⚠ v111 fixed it AGAIN: it could not see a line holding SEVERAL declarations.
⭐ **v132 FIXED IT A THIRD TIME, and this one nearly DOUBLED the seam.** `--inner` only ever
STARTED a decl run at a line that is exactly `{`, i.e. it saw each block's LEADING run and
nothing else; a run opening mid-block — `if (...) { ... }` followed by
`int nObjs = ...; int j = 0;`, which `PlaceZone` 0x4260e0 carries THREE times — was invisible,
so v131's "24 permutations, flat" verdict there was computed over a strictly smaller space than
exists. Now scans every maximal run of consecutive declaration-only lines. Positive-controlled
as a strict SUPERSET over all 378 functions (no run may be lost): **685 → 1162 permutable decl
lines, 90 functions gain a run, 48 of them non-exact residuals.** Biggest new runs sit on
`BuildQuestPathMaybe` 0x403c80 (7 runs), `DrawHealthNeedle` 0x4278a0 (a 7-decl run),
`GetLocatorIconMaybe` 0x41a1c0 and `DrawHealthDial` 0x427490 (6 each)) · **`hoisttest.py <tu.cpp> <0xADDR> --expect N`** (decl SCOPE: hoist
inner-block locals. ⚠ v110: it only sees decls WITH an initializer, so a bare `Tile *pTile;`
is invisible to it — its "1 spelling" answer on DrawTextA was a tool limit, not a result;
sweep bare decls by hand) ·
**`residuals.py [--lenmis]`** (⭐ census of the non-exact functions RANKED by byte-diff, with a
commutative/tie-break classifier; the trustworthy replacement for sorting idiomscan's class D.
⭐ v117 `--lenmis` ranks instead by |our length − Ghidra's extent| = the STRUCTURAL-defect seam
(lesson #49), filtering the 18 stub extents and the jump-table functions whose COMDAT legitimately
exceeds the extent. 44 residuals / 408 B of real structural error; 48 residuals are length-EXACT; the census now reads 33 of 91 with a length mismatch = 321 B
of structural error, 58 length-EXACT, 31 not comparable. ⭐ v125 added the `operand-reassoc`
kind — a commutative exchange BETWEEN two neighbouring instructions, which `classify()`
structurally could not see because it compares one instruction against its counterpart) ·
**`formsweep.py <spec.py> --expect-exact N`** (⭐ v117 — applies each candidate edit ALONE against
the pristine TU and prints the WHOLE TU's marker vector, so lesson #48's collateral is visible in
the same row; k+1 compiles where `jointdecl.py` takes the cartesian product's 2**k. Reports LENGTH
against app_funcs.txt, never `jointdecl.measure()`'s vacuous `orig_len`. ⚠ COMPILES) ·
**`verify.py <src.cpp>`** /
**`match.py`** (per-TU marker compare, reloc-masked; best-fit can mis-pair clones — confirm name-keyed) ·
**`asmscore.py <src.cpp> 0xADDR [--dump]`** (graded disasm scorer; `--dump`: LEFT=original, RIGHT=ours;
recompiles the TU itself) · **`bugscan.py`** / **`vtcheck.py`** / **`msgcheck.py`** (correctness oracles —
wrong vtable slot / field disp / message-map entry; see anchor table) · **`link_exe.sh`** (full-image link
oracle) · `permute.py`, `survey.py`, `frontier.py`, `g2_link.sh`, `g2_diff.py`, `g2_order.py`,
`exactset.py`, `libfingerprint.py` (parked byte-match era; see PLAN_COMPLETED.md).
**`aritycheck.py`** (⭐ v120 — a TWO-SIDED ARITY ORACLE, and the fifth-and-a-half anchor check.
A `__thiscall`/`__stdcall` callee cleans its own args, so its terminal `ret N` states its ARITY
independently of anything we wrote; compare it with OUR compiled `ret N` per marker. A function
declared with the wrong number of parameters LINKS FINE (one caller, one callee, consistently
wrong) and passes bugscan/vtcheck/msgcheck — it shows up only as a diffuse byte residual you
will misread as a register problem, which is exactly what happened to `TextDialog::Layout`
0x4176f0 for the life of the project. Currently 0 mismatches / 263 comparable; re-run on newly
transcribed functions. ⚠ its first draft read the last 3 bytes of Ghidra's extent and reported
'0 mismatches' WITH the known Layout bug in the tree — the extent over-runs the real `ret` into
a trailing jump TABLE. Caught by the built-in control: every BYTE-EXACT function must agree, and
the tool FAILS LOUDLY if one does not) ·
**`jseqscan.py [--shape-only]`** (⭐ v134 — THE CONDITIONAL-JUMP SEQUENCE census,
lesson #63: the two ordered lists of jcc MNEMONICS, aligned by LCS so a schedule shift
cannot desync them. It answers the question `mixscan.py` can only pose — mixscan says
"five polarity flips", this says WHICH FIVE — and `armscan.py` structurally cannot,
because it needs an aligned instruction boundary. Three classes: POLARITY (arm order,
#47, STEERABLE), MIRROR (compare-operand exchange, #54, DIAL-BOUND — park), SHAPE (a
branch on one side only: real control-flow difference, or a MOVED block, which reads as
one delete + one insert). Currently 4 SHAPE + 25 substitution functions, of which 20 are
pure MIRROR = parks. Positive control CLEAN over 226 byte-exact functions. ⚠ its own
positional first draft claimed FOURTEEN flips on `LoadWorld` 0x421fd0 that do not exist —
see the lesson. ⚠ COMPILES) ·
**`mixscan.py [--all] [--min N]`** (⭐ v131 — THE INSTRUCTION-MIX CENSUS, lesson #60: decode
BOTH sides and diff `Counter(mnemonic)`, so a residual too noisy to read as bytes collapses to
a handful of counts. A NEGATIVE count is an instruction the ORIGINAL emits and we do not = the
structural half; an EMPTY delta with a nonzero byte diff is `PURE-REG` = the streams agree
instruction-for-instruction and only registers/encodings differ, i.e. lesson #44/#54 and
source-CLOSED. **27 length-mismatched hits; `--all` adds the length-exact ones and reports 45
PURE-REG functions in total** — that is half the residual census triaged as closed in one run.
Positive-controlled: every byte-exact function whose length equals its extent must report an
empty delta, and it exits 1 if one does not. ⚠ its own first two drafts BOTH lied and both were
caught by that control — reloc-MASKING the original corrupts a shifted stream (the v121 trap I
had just documented against), and padding a short extent out to our length with zeros
resurrected four byte-EXACT functions as phantom residuals. ⚠ functions carrying a jump TABLE
are EXCLUDED, not mis-reported (their reloc-zeroed entries decode as instructions on our side
only) — that bucket holds real targets like `Layout` 0x4176f0 at -35, so use `sbs.py` there.
⚠ COMPILES) ·
**`movsxscan.py`** (⭐ v130 — the SHORT->INT PROMOTION census, BOTH SIDES. Counts every
`movsx r32,<16-bit>` per function in the ORIGINAL and in our own COMDATs, split by whether
the source operand sits in a REGISTER or a FRAME SLOT — so it reads as a residency story as
well as a count. **132 CONFIRMED / 23 ORIG-MORE / 13 OURS-MORE**, a very strong positive
control. It independently re-derives v127's `ReadZa*` finding (orig reg-form, ours mem-form)
without being told about it. ⚠ its own first draft keyed on the SELF form (`movsx eax,ax`)
and ranked `WorldgenFillQuestItemSpot2Maybe` 0x41cf10 the sharpest target in the tree — that
site is a pure RENAME (orig `movsx eax,ax`, ours `movsx esi,ax`, same 3 bytes). Whether a
promotion is "self" is register allocation, not source. ⚠ COMPILES) ·
**`framescan.py [--exact <file>]`** (⭐ v128 — the LOCAL FRAME SIZE oracle, lesson #59: the
prologue's `sub esp,N` for every residual against the original's. The third prologue
instrument after `savescan.py` (the callee-save SET) and `thisscan.py` (`this` residency),
and the only one that can see an ordinary local. OURS LARGER = we homed a local the original
enregisters — usually a live range started too early, which is what 0x41d0c0 was; OURS
SMALLER = the original homed something we do not have. 15 hits, positive-controlled (no
byte-exact function may report one; it exits 1 if one does). It CONFIRMED lesson #55's last
open case, `ScrollZoneTransition` 0x411180, from a second side. ⚠ a hit is a CANDIDATE —
confirm with `sbs.py` before investing. ⚠ COMPILES) ·
**`sbs.py <src.cpp> <0xADDR> [lo] [hi]`** (⭐ v128 — SIDE-BY-SIDE disassembly of one
function, EVERY instruction on both sides with the mismatches marked, over
`residuals.paired()`. `asmscore --dump` prints only the DIFFERING lines, which is unreadable
once the schedule shifts; the v127 pickup recommended hand-rolling this view each session,
so it is now checked in. READ-ONLY apart from the compile) ·
**`widthscan.py [--exact <file>]`** (⭐ v127 — the PARAMETER-TYPE oracle, the gap
`aritycheck.py` structurally cannot see: a wrong parameter TYPE occupies the same stack slot
and cleans the same argument bytes, so every other oracle passes. Censuses the WIDENING stack
loads (`movsx`/`movzx` word) on BOTH sides per function; ORIG-wide = we declared a `short`
where the original has an `int`, ORIG-narrow = the reverse. 12 hits, positive-controlled. ⚠ a
hit is a CANDIDATE — read the CALLEE's own accesses to the slot before changing a signature;
v127's top hit was a false lead. ⚠ COMPILES) ·
**`pushscan.py [--exact <file>]`** (⭐ v126 — the PUSH-FORM census behind lesson #57: where
the ORIGINAL pushes an IMMEDIATE and we push a REGISTER at the same aligned argument slot, or
vice versa. Two-sided: ORIG-reg = a statement-order fold we destroyed (#39), ORIG-imm = a call
the original DUPLICATED and we merged (#52). BOTH v126 wins came out of this one scan, in
opposite directions. 8 hits; positive-controlled against the byte-exact set — a hit there would
mean the instruction alignment is lying, and it FAILS LOUDLY. ⚠ COMPILES) ·
**`xjumpscan.py [--exact <file>]`** (⭐ v126 — the GENERALISED lesson-#52 cross-jump census,
measured on BOTH sides. It exists because v121's version hard-coded a ONE-push arm and so could
not see OnDraw 0x409110's two-push diamond, the find that closed that function; generalising
the arm takes the seam from 6 functions to 12. Reports ours as well as the original, so a
diamond in both images reads as CONFIRMED rather than as a target — only 2 are open. Asserts it
re-finds all 6 of v121's diamonds. ⚠ COMPILES) ·
**`impcse.py [--orig]`** (⭐ v125 — the BOTH-SIDES census behind lesson #56: which
functions cache an IMPORT ADDRESS in a callee-saved register (`mov <cs-reg>,[__imp__X]` +
`call <reg>`) instead of repeating the 6-byte memory call, in the ORIGINAL *and* in our own
compiled COMDATs. It exists because the v124 note claimed we lacked a construct we in fact
emit; it asserts a positive control (three byte-exact functions must show it on both sides)
and distinguishes "same construct, different register" from a genuinely differing set — 5 of
the latter, a real target list. `--orig` is READ-ONLY and safe during a sweep; the default
COMPILES) ·
**`thisscan.py --exact <file> | --all`** (⭐ v124 — READ-ONLY census of how the ORIGINAL
treats `this` in every `__thiscall` function: ENREG / SPILL / ECX, with the EH-frame flag, the
callee-save set and the saturation test. This is the instrument behind lesson #55; run it
before reading a "-N length" residual as a register-allocation mystery. Safe to run during a
sweep) ·
**`epiloguescan.py`** (⭐ v120 — the DUPLICATED-EPILOGUE target list: length SHORT of the extent
AND the original decoding MORE `ret`s than us = the *then*-arm `return;` that cl gave a local
epilogue. Landed PlaySound 0x409060; now 0 hits, i.e. MINED OUT) ·
Resources: **`make_res.py`** (+`reslib.py`), `extract_res.py`.

## 📋 Session protocol

1. **Orient:** read the ⏭ pickup block below; run `python3 tools/progress.py` to confirm the anchor (257)
   reproduces BEFORE changing anything (if not, a header drifted — bisect first). Then run
   `python3 tools/residuals.py --lenmis` — since v117 that is the sharpest target list (lesson #49).
2. **Work** the pickup goals. Ghidra writes: always `program=`. Anchor rule for every shared-TU edit
   (ifdef fall-through = original tokens); re-run the anchor oracles after shared-code changes.
3. **Agents** for read-only RE sweeps (naming/xref surveys); keep build-and-test iterations in the main thread.
   Escalation: spawn a `fable`-model agent with the disasm + relevant lesson numbers for novel mechanisms.
4. **Session end:** update the ⏭ pickup block (findings → instincts, done items removed, next steps concrete);
   demote the old pickup to a condensed ⏮ block APPENDED to PLAN_COMPLETED.md; distill new mechanisms into
   the lessons lists (PLAN_COMPLETED.md) or the standing-lesson bullets here; sync new struct fields/renames
   to Ghidra (or list as PENDING); `save_program`; commit with a descriptive message.

### ⏭ NEXT SESSION PICKUP (2026-09-07 v138 — **held at 257 exact, +0/−0, but with the
biggest structural landing since v135: `Layout` 0x4176f0 went 1396 B @ ext−23 → 1412 B @
ext−7 on TWO COMPOSING fixes.** All oracles green: **257 exact** / 99.17 % / link 0 unresolved
0 dup / bugscan 1 HIGH (the documented benign `StartGame` 0x4037a0 `@+0x14a` finding) 0 SHIFT /
vt 10 CLEAN / msg 11 CLEAN / arity 0 mismatches. `--lenmis` fell **172 B → 156 B** and Layout
dropped from #2 to #6. Every landing verified with `exactset.py` + `comm` — IDENTICAL exact set,
note edits included. v137 log condensed into PLAN_COMPLETED.md.)

**▶ READ FIRST — SIXTEEN triage rules.** (1)–(15) unchanged from v137.
(16) ⭐ **NEW (v138): WHEN THE ORIGINAL RELOADS A MEMBER PER BLOCK AND NO STORE EXPLAINS IT,
CHECK WHERE THE VARIABLE'S LIVE RANGE STARTS BEFORE COUNTING REGISTERS.** A `T v = expr;
if (c) v op= ...;` keeps `expr`'s VALUE CSE alive; a `T v; if (c) v = expr op ...; else
v = expr;` kills it and forces the ADDRESS CSE. Full rule = lesson #68, the converse of #59.
Corollary: v137's "find one more long-lived value" was the wrong question — the register COUNT
was the same on both sides.

**▶ WHAT LANDED.**
1. **`Layout` 0x4176f0 ext−23 → ext−19** — `bx` assigned in BOTH ARMS (lesson #68). The emitted
   ladder now reproduces the original INSTRUCTION FOR INSTRUCTION, registers included
   (`lea edx,[esi+0x18]`, `mov ecx,[edx]`, per-block `mov eax,[edx]`).
2. **…then ext−19 → ext−7** — that un-refuted the **case-2 inner nTailDir ladder**, a standing
   ⛔ MEASURED NEGATIVE (it cost +21 at ext−23). At the new baseline it GAINS ON BOTH MEASURES
   (len → 1412, diff 1036 → 1031). This is the v134 rule paying for the second session running:
   **re-measure a function's own ⛔ list after every structural fix.**
3. **`CyclePalette` 0x415af0 recovered for the FIFTH session, and TWICE within this one** (each
   landing re-rolled the phase). Minimal move: a2 → the MEMBER form, one token. ⚠ Layout is
   DOWNSTREAM of CyclePalette, so v106's downstream-only rule did NOT protect it — v116's
   "several edits make the phase an INTERACTION" governs. Always re-run `exactset.py`.

**▶ NEXT — concrete, in priority order.**
1. ⭐ **`Layout` 0x4176f0 (−7) — the whole remainder is now ONE construct.** The case-2 arm is
   literally ONE INSTRUCTION from the original: the store-killed reload `mov eax,[esi+0x60]` at
   +0x202 (3 B), and the same in case 1, plus the 11-byte case-1 ladder that reload blocks.
   ⛔ Refuted and not to be re-tread: the pointer-store lever (v137, over-kills, ext+5); v135's
   member-pointer probe A; the case-1 ladder as a spelling problem (+32 on top of case 2);
   ⭐ NEW at v138 — **the "when does the address escape" hypothesis is CLOSED**: the point[]
   array's address is taken at the SAME place on both sides (the ctor loop's
   `lea ebp,[esp+0x10]`), now byte-identical between the images. ⇒ the open question is a
   source construct that kills a member CSE ONCE PER BLOCK. Note cl reaches the original's
   2-loads-for-3-uses by SCHEDULING the third use's arithmetic ABOVE the second store
   (`dec eax` at +0x1e9 precedes `mov [esp+0x1c],ecx`), so "every store kills" IS the model.
2. ⭐ **`IactProbeMove` 0x406550 (+26) — now the #1 structural residual.** Unchanged from v137:
   the whole +26 is one contest (original puts `found` in EBP and homes `r` at S0+8; we do the
   reverse). ⛔ CLOSED: decl SET+ORDER, statement order around `r`, `n`'s SCOPE. Wants a NEW
   mechanism that makes cl rank a constants-only, test-only variable above an arithmetic one.
   ⚠ **lesson #68 is a candidate to try here first** — it is exactly a "where does the live
   range start" lever and it is one compile.
3. ⭐ **`DrawHealthNeedle` 0x4278a0 (−17) / `DrawHealthDial` 0x427490 (−16)** — now #2/#3, one
   `this`-residency question. ⛔ 0x427490's decl axis fully closed; EBX idle for its first 0xb6
   bytes so scarcity is REFUTED. ⚠ read the leaf vtable stores (lesson #61) before any
   decl-order win on 0x4278a0. ⛔ **NEW: the v138 address-CSE probe's "orig 4 / ours 26" on
   0x4278a0 is an ARTIFACT — ignore it** (see lesson #68's ⛔ bullet; the two sides' `lea` lists
   are near-identical CPen/CBrush object addresses).
4. ⭐ **`ShowWinMessage` 0x40f4b0 (−9)** — unchanged from v137: three surplus `movsx` in the
   arm-C tail (ORIG {mem 3, self 2} vs OURS {mem 4, self 1, reg 3}); find the one missing
   long-lived value there and all three go with it.
5. ⭐ **`ScrollZoneTransition` 0x411180's last −1** — a pure residency permutation; diff against
   `DrawEntities` 0x40b160, `SaveZoneRecursive` 0x4033b0, `LoadZoneRecursive` 0x403450.
6. ⛔ **The v132 decl-run seam stays mostly closed** (v137 swept five named targets to a floor).
   What is left is the ~40 residuals nobody has named; regenerate by crossing
   `declorder.inner_blocks` against `residuals.scan()` and SKIP the `PURE-REG` ones. Low
   expected value — prefer items 1–4.
7. **Unchanged from v130–v137:** the `jl/jg + mov -1 + test/cmp` cluster (pure MIRROR = #54
   parks); `movsxscan.py`'s ORIG-MORE list; the remaining `framescan.py`, `pushscan.py` and
   `widthscan.py` hits; the 5 generalised `loopform.py` candidates; the `movsx` self-extension
   family (0x403ae0 −6, 0x423df0 −6, 0x409650 +3) with its two dictionary entries.
8. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19,
   DeskcppDoc's `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp
   `sizeof` dial note). **Phase-H goals 2–5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY (v104–v137 rules all stand; v138 re-used them all).**
Every sweep MUTATES a source file — always `git status --porcelain src/` AFTER each one; run long
sweeps with `run_in_background` writing to a LOG FILE; restore a single function from
`git show HEAD:<file>`, never `git checkout <file>` mid-sweep; never run two sweeps concurrently,
or one while any COMPILING tool is in flight (they share `build/*.obj`). `thisscan.py`,
`loopform.py`, `unrotscan.py`, `aliasscan.py` and `impcse.py --orig` are READ-ONLY.
⭐ **v138 method note — ALWAYS PASS `--expect`, AND LET IT BE WRONG.** The CyclePalette sweep was
launched with a stale `--expect 12` copied from the v136 note; vartest HARD-FAILED at the baseline
and printed the true residual (6) in the same breath, costing one cheap run and saving a whole
sweep's worth of untrustworthy deltas. A guessed `--expect` is not a mistake to avoid — it is the
cheapest way to *learn* the baseline, because the guard cannot be talked past.
⭐ **v138 method note — A NEW CENSUS NEEDS ITS CONTROL PRINTED *AND READ*.** The address-CSE probe
failed its positive control TWICE (a reloc-zeroed displacement decoding as `[eax+0]`; then a
jump-table function) and, on the third run, produced a clean-looking 10-row table that was still
UNSOUND for a reason no control could catch — its key was not register-blind. ⚠ And the first
reading of its output was `tail -30`, which silently CUT the `!! CONTROL FAIL` lines above the
table. **Print the control verdict LAST as well as first, and never read a census through `tail`.**
⭐ **v138 method note — VERIFY THE SHAPE, THEN THE SCORE, THEN THE SET.** Both v138 landings were
accepted only after `sbs.py` showed the emitted construct matching the original (triage rule 13),
and each was followed by a full `exactset.py` + `comm` diff. That is 3 runs per landing and it is
the right price: the first landing looked free, and was not — it cost CyclePalette until the sweep
recovered it.
⭐ **v137 method notes — A ONE-COMPILE MECHANISM TEST BEATS A SWEEP WHEN THE PARK NOTE NAMES A
CAUSE; READ THE ORIGINAL END TO END BEFORE SWEEPING** (~40 lines of read-only capstone over
`EXE[va - match.TEXT_VA + match.TEXT_RAW]` sliced to the Ghidra extent; needs no build, so it is
safe while a sweep is in flight).
⚠ **v137 SHELL TRAP — THIS IS zsh, WHICH DOES NOT WORD-SPLIT UNQUOTED VARIABLES.** A sweep loop
written `for spec in "0xADDR 123"; do set -- $spec; ...` silently gives an EMPTY `$2`, so
`--expect` gets nothing and the loop no-ops while still printing its headers. Use `${spec%%:*}` /
`${spec##*:}` and check the echoed `--expect` value in the log.
⭐ **v136 method notes — a scratch `apply.py` with ONE FLAG PER AXIS beats a variants file when the
edit spans non-contiguous sites** (`vartest.py` needs ONE contiguous BASE block; rebuild the body
from a PRISTINE copy per cell and `assert` the line count); **DISASSEMBLE OUR SIDE, NOT JUST THE
ORIGINAL** (`sbs.py` cannot pair once the schedule shifts); **a mnemonic census is a PROGRESS BAR.**
⭐ **v135 method notes — READ THE ORIGINAL'S ARGUMENT SETUP BEFORE ANY REGISTER STORY; A FREE
ORACLE BEATS A COMPILE; A THREE-CELL PROBE WITH AN INERT CELL IS THE HONEST SHAPE.**
⭐ **v134 method notes — THE THREE-CENSUS OPENING IS STANDARD**: `residuals.py --lenmis` (where),
`mixscan.py` (what kind), `jseqscan.py` (which sites). **A DIFF COUNT THAT DOES NOT MOVE IS NOT A
FLAT RESULT** — verify the structure, not the score.
⭐ **v133 method notes — WHEN vartest's BASE CANNOT SPAN THE SITES, HAND-APPLY + `bytediff.py`**;
do NOT read `src/` while a sweep is in flight.
⭐ **v131 method note — MEASURE COLLATERAL WITH `verify.py <tu.cpp> | tail -3` FIRST.** ⚠ neither
`verify.py` nor `progress.py` names WHICH function moved — only `exactset.py` + `diff`/`comm` does.
⭐ **v129 method note — `vartest.py` output is \r-heavy; pipe through `tr '\r' '\n'`**, and do NOT
launch it as `nohup ... &` — use `run_in_background: true`. ⚠ a 2-minute FOREGROUND `vartest.py`
WILL time out and leave the TU MUTATED.
⭐ **`vartest.py` and `declorder.py` print the REAL extent** (`ext=<extent> <signed delta>`); read
the delta on every row. ⚠ **`jointdecl.py` still carries the vacuous `orig_len`** — a cheap chore.
⚠ **`asmscore.py` CANNOT PAIR a function whose doc comment contains a `Class::Method (` string.**
⭐ **A THROWAWAY PROBE beats a general tool for a one-off question — but give it a POSITIVE
CONTROL, and promote it only once it overturns something.** (v138's did not, and was not promoted.)
⭐ **v130 method note — `bugscan.py --all | tail -3` shows the tail of the LOW list**, which looks
alarmingly like a changed HIGH finding; grep for the `=== HIGH` section header instead.

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
