# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#35 + MFC-matching lessons** (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-11, v87; re-baselined 217→234 at v100 (MEASUREMENT FIX); 234→237 at v102, 237→240 at v103 (REAL MATCHES))

Phases A–G (byte-matching YodaDemo.exe's app region): **240 functions byte-exact / 99.17 % coverage** (v100's
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
| exact count | `python3 tools/progress.py` | **240 exact / 99.17 %** |
| full link | `tools/link_exe.sh` | 0 unresolved / 0 duplicates / exit 0 |
| field/slot bugs | `python3 tools/bugscan.py --all` | 0 HIGH / 0 SHIFT |
| vtables | `python3 tools/vtcheck.py` | 10 classes CLEAN (+13 skipped, unanchorable) |
| message maps | `python3 tools/msgcheck.py` | 11 maps CLEAN |

⚠ **240 is the CURRENT baseline (234 at v100, +3 REAL at v102, +3 REAL at v103; all five oracles re-run in the
same pass).** ⭐ **v100's +17 was a MEASUREMENT CORRECTION, not 17 new byte-matches** — those functions
were ALREADY byte-exact and were being scored against the WRONG addresses; do not read it as progress on
matching. **v102's +3 and v103's +3 ARE matching** (v102: the TextDialog scroll family, via `CWnd::SendMessage`;
v103: `ParseSnds` via a buffer SIZE, `OnEraseBkgnd` + `CyclePalette` via the member-call form). The project-wide per-TU count is the thing that must never drop; re-baseline deliberately,
never let it drift.

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

Byte-match harness (anchor checks): **`progress.py`** (headline dashboard) · **`bytediff.py <src.cpp>
[0xADDR...]`** (⭐ the ANCHOR's own definition of exact for ONE function — reloc-masked BYTE diff +
hexdump of each differing run; run this BEFORE investing in any residual, per v100) ·
**`vartest.py <tu.cpp> <0xADDR> <variants.py> --expect N`** (⭐ batch-A/B a set of source SPELLINGS for
ONE function against the anchor's byte oracle — the instrument that landed both v102 wins; ALWAYS pass
`--expect`, it hard-fails when its own baseline disagrees with the anchor) ·
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

1. **Orient:** read the ⏭ pickup block below; run `python3 tools/progress.py` to confirm the anchor (240)
   reproduces BEFORE changing anything (if not, a header drifted — bisect first).
2. **Work** the pickup goals. Ghidra writes: always `program=`. Anchor rule for every shared-TU edit
   (ifdef fall-through = original tokens); re-run the anchor oracles after shared-code changes.
3. **Agents** for read-only RE sweeps (naming/xref surveys); keep build-and-test iterations in the main thread.
   Escalation: spawn a `fable`-model agent with the disasm + relevant lesson numbers for novel mechanisms.
4. **Session end:** update the ⏭ pickup block (findings → instincts, done items removed, next steps concrete);
   demote the old pickup to a condensed ⏮ block APPENDED to PLAN_COMPLETED.md; distill new mechanisms into
   the lessons lists (PLAN_COMPLETED.md) or the standing-lesson bullets here; sync new struct fields/renames
   to Ghidra (or list as PENDING); `save_program`; commit with a descriptive message.

### ⏭ NEXT SESSION PICKUP (2026-09-02 v103 — **237 → 240**, +3 gained / 0 lost, all real
matching, plus a 28 B→21 B improvement. All 5 oracles green (240 exact / link 0-0-exit0 /
bugscan 0 HIGH 0 SHIFT / vt 10 CLEAN / msg 11 CLEAN); build-sdl + build-sdl-indy relinked.
v102 log demoted to PLAN_COMPLETED.md ⏮.)

**▶ WHAT LANDED.**
1. **`ParseSnds` 0x4233f0 (5 B → EXACT) — a buffer's DECLARED SIZE is a dial.** `char fname[9]`
   (DOS 8.3 basename + NUL), not `[12]`. v36 had exhaustively permuted all 24 decl ORDERS and
   parked it as irreducible; it never varied sizes. New standing lesson #36 in CLAUDE.md (incl.
   the method: solve the original's frame by normalising `[esp+N]` through the prologue).
2. **`OnEraseBkgnd` 0x413b20 (6 B → EXACT) — `pDC->PatBlt(...)`, lesson #35.** The residual was
   the TAIL FUNCLET ORDER, an axis the old note declared "not source-steerable". It is.
3. **`CyclePalette` 0x415af0 (6 B → EXACT) — `pWorld->pPalette->AnimatePalette(...)`.** ⚠ the
   conversion is NON-MONOTONIC: both calls = 6 B, first only = 0 B, both + DC members = 0 B.
4. **`DrawDirectionArrows` 0x4270f0 28 B → 21 B** via `pDC->FillRect(&rc, &br)`. Its last block's
   x/y decl order re-probed and CONFIRMED correct (swapping = 27 B); pOldPal-first head is inert.
5. **microfx gained `CDC::PatBlt`** (afxwin.h) for the portable build.

**▶ NEXT — concrete, in priority order.**
1. **⭐ Keep mining the member-call seam — it is NOT exhausted.** Regenerate the census
   (`tools/residuals.py --csv out.csv`) then scan for global-form `::Call(` sites inside non-exact
   functions. ⚠ **`va` in that CSV is DECIMAL — parse with `int(va)`, not base 16** (that exact bug
   made a v103 scan report zero hits and nearly closed this seam; see harness-lies item 4).
   At v103 the scan found 28 such functions; still unconverted and ranked by ndiff:
   `OnInitialUpdate` 0x426c40 (14, `::SetTimer(m_hWnd,…)` — trivial object, low odds),
   `UpdateDragCursor` 0x412cc0 (379, `::SetBitmapBits((HBITMAP)pBitmap->m_hObject,…)` →
   `pBitmap->SetBitmapBits(…)`, and `::BitBlt(pDC->m_hDC, …, dcMem.m_hDC, …)` → `pDC->BitBlt(…, &dcMem, …)`),
   `DrawHealthNeedle` 0x4278a0 (803, `penA.Attach(::CreatePen(…))` → `penA.CreatePen(…)`, same for
   `CreateSolidBrush`), `OnNewDocument` 0x41bb10 (537, `pPal->Attach(::CreatePalette(…))`),
   `AddItemToInv` 0x428f50 (381) + `OnTimer` 0x40d470 (2870) (`::SetScrollRange(pInvScrollBar->m_hWnd,…)`).
   Prefer PONTER-CHAIN objects; `m_hWnd`/`pDC->m_hDC` roots are usually inert.
2. **Proven INERT at v103 — do not re-tread:** `GetZoneIndex` 0x423dc0 (9 loop spellings; the
   do-while shape IS right — pre-test loops are 42 B vs the original's 44 B), `ParseTilesMaybe`
   0x41a030 (6 spellings; the old note's "operand flip proven inert" was CORRECT), both MainFrm
   palette handlers 0x4193f0/0x419460 (8 member+boolean variants, all 54 B), `DrawIcon` member form.
   ⇒ The **cmp-swap / jcc-mirror class is now source-inert 3-for-3** — residuals.py's `tie=True`
   flag (top 6 rows) is trustworthy; skip those rows.
3. **Remaining cheap residuals, unexamined:** `~CDeskcppDoc` 0x41b2f0 (6 B — pure esi↔edi 2-cycle
   on a count/index loop), `SetCurrentToIntroZone` 0x423d20 (7), `~IactScript` 0x4187e0 (7),
   `ReadIzon` 0x405ae0 (7), `GetFrameTile` 0x404850 (2, tie=True). `SaveZoneRecursive` 0x4033b0 is
   the documented ebx↔ebp 2-cycle — skip.
4. **Still open from v98:** de-hex leftovers (`0x68`→PLAN_WALL, TileFlags bits 16-19, DeskcppDoc's
   `0xffffffff`/`0x11/0x10/0xe` codes, `WORLD_GRID_SIZE 10`, the Canvas.cpp `sizeof` dial note).
5. **Phase-H goals 2-5 untouched** this session.

**▶ HOW TO WORK THE DIAL SAFELY:** every sweep MUTATES a header — always restore (the tools do, via
atexit+finally, and leave a `.bak` if restore fails). ⚠ never run two sweeps concurrently or start one
while a `progress.py` is in flight: they fight over the header AND `build/*.obj`. Verify a clean tree
with `git diff --stat src/` + `grep -rn "DIALSWEEP GENERATED" src/` before trusting any number.

---

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
