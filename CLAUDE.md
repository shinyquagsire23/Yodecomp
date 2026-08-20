# Yodecomp — Desktop Adventures decompilation + engine

Decompilation of LucasArts' *Desktop Adventures* engine (Yoda Stories / Indiana Jones' Desktop Adventures)
into real, buildable C++/MFC source, plus an extended multi-game engine built on it. Patterns follow
`~/workspace/OpenJKDF2` (CMake, macOS/Linux hosts, `wine` for Windows toolchains). Claude is permitted to
modify this file with any useful notes that will aid other/later Claudes.

**Deep history lives in `PLAN_COMPLETED.md`** — the full phased plan (A–G), TU/struct status tables, the
v1–v71 milestone chain, and the ⭐ **KEY codegen lessons #1–#33 + MFC-matching lessons** (cite as
"PLAN_COMPLETED.md lesson #N"). This file carries only what's needed to work NOW.

## Where the project stands (2026-07-11, v87)

Phases A–G (byte-matching YodaDemo.exe's app region): **211 functions byte-exact / 99.17 % coverage**,
every function transcribed (exact or annotated-EFFECTIVE), a runnable `/OPT:REF`-linked image, all
oracles green.

⭐ **RE-OPENED v96 (2026-07-26) — the "compiler wall" was WRONG; 211 is a plateau, not a ceiling.**
The prior text here said the gap was "a compiler-intrinsic register-coloring wall — do NOT re-chase
it, every lever proven dead". That is **retracted**. `tools/dialsweep.py` reaches **215 exact
project-wide with FOUR GAINED AND ZERO LOST**, from nothing but 7 extra file-scope symbols visible
through `Worldgen.h` — validated 4 ways (struct/typedef/extern/6-field-enum all land on exactly 215)
plus a determinism repeat. Both surviving "only VC 4.0 can make these" functions (`ParseZaux`
0x423110, `ZoneHasIzxItemMaybe` 0x41bfa0) go byte-exact under **our own VC 4.2**, so the
interim-compiler hypothesis is dead (docs/compiler-hunt.md v96). ⚠ the +7 is currently PLACEHOLDER
declarations and is deliberately NOT committed as source — it is a **measurement** saying the
original's headers carried ~7 more symbols than ours. The work is finding the real seven; **never
pad to hit a number** (see "the dial is an instrument" below). **v97 (same day): the search is NARROWED — members are INERT (only file-scope symbols / enum ENUMERATORS dial; `tools/membertest.py`), and the Ghidra globals inventory found the one real unmodelled worldgen global = the DTA record-tag table (0x00456890, SHIPPED at EOF of Worldgen.cpp) + TileFlags TILE_PLAYER/ENEMY/FRIENDLY enumerators (SHIPPED). Real additions did NOT reproduce the +7 extern sweep → 211 is the honest plateau, 0x41f830 still gated on the exact ~7 (see ⏭ v97).**

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

**Anchor oracles — run after ANY shared-code edit, all must hold:**
| oracle | command | green state |
|---|---|---|
| exact count | `python3 tools/progress.py` | **211 exact / 99.17 %** |
| full link | `tools/link_exe.sh` | 0 unresolved / 0 duplicates / exit 0 |
| field/slot bugs | `python3 tools/bugscan.py --all` | 0 HIGH / 0 SHIFT |
| vtables | `python3 tools/vtcheck.py` | 10 classes CLEAN |
| message maps | `python3 tools/msgcheck.py` | 11 maps CLEAN |

⚠ **211 is the CURRENT baseline, not the ceiling (v96).** It is the number to hold while the header
set is what it is — a drop still means you broke something. But **215 is demonstrably reachable**
with the same toolchain (see above), so when the real missing declarations are found this table
re-baselines UPWARD. Re-baseline deliberately, with all five oracles re-run in the same pass —
never let it drift.

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
Use `tools/dialsweep.py` to MEASURE how many symbols are missing and which TUs see them, then go
find them. See docs/compiler-hunt.md v96 for the mechanism (pure symbol count; identifier length
irrelevant; enum = tag + field count; macros are free because they never enter the symbol table).

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

### ⏭ NEXT SESSION PICKUP (2026-07-26 v97 — pickups 1&2 EXECUTED: member-vs-file-scope RESOLVED (members inert), Ghidra globals inventory DONE, real record-tag table + TileFlags enumerators SHIPPED; anchor held 211/99.17% + all 5 oracles; dial model refined — "~7" is file-scope/ENUM only and NOT any 7 decls. Tree GREEN + COMMITTED.)

**▶ WHAT HAPPENED.** Two halves. (1) finished the user's de-hex readability sweep; (2) that sweep's
dial lessons made us re-open the parked compiler hunt — and **211 turned out to be a plateau, not a
ceiling**. Commits: `b0d430a` de-hex · `fff083b` dial tools + 215 · `f747e02` prior retractions ·
`2c9067f` headersweep localization. All 5 oracles green at commit time (211 exact / 99.17 %, link 0/0,
bugscan 0 HIGH/0 SHIFT, vt 10 CLEAN, msg 11 CLEAN); `build-sdl` + `build-sdl-indy` build.

**▶ ⭐ THE HEADLINE — 215 > 211, with ZERO regressions.** `tools/dialsweep.py` reaches **215 exact
project-wide, +4 gained / −0 lost**, from nothing but **7 extra file-scope symbols** through
`Worldgen.h`. Plateau at 6/7/8 (214/215/214), not a knife-edge. Validated FOUR ways —
`struct`/`typedef`/`extern`/one-6-field-`enum` all land on exactly 215 — plus a determinism repeat.
⇒ **the interim-compiler hypothesis is DEAD.** Both surviving pillars of it go byte-exact under OUR
VC 4.2 (`ParseZaux` 0x423110, `ZoneHasIzxItemMaybe` 0x41bfa0), and the famous "3 VC 4.0 wins" was
already only 2 — `DetonateAdjacentTiles` no longer reproduces under 4.0 at all (our source drifted
since v52), exactly the fragility expected if those were coincidental allocator landings.

**▶ THE MECHANISM (all measured, `tools/enumfieldtest.py`).** The dial is a **pure file-scope SYMBOL
COUNT**:
- an `enum` costs **tag + field count** (11/11 positions match the plain-symbol curve); an EMPTY enum
  body is free ⇒ **unused enumerators ARE dial-active**, and a mis-transcribed enum is a *quantified*
  dial error;
- **identifier LENGTH is irrelevant** (40-char vs 1-char names, identical) ⇒ it is a clean scalar, not
  symbol-table bytes / hash occupancy — which is what makes it usable as an instrument;
- **macros are FREE** (never enter the symbol table) — why our huge `#define` blocks cost nothing while
  one 23-name `enum ArtooHint` cost 6 functions;
- declaration KIND is irrelevant (struct == typedef == extern at every n).
⚠ NOT everything is symbol count: an **empty include FILE** still costs a function (v95) and
`sizeof(T)` for the literal costs one (v96) — at least three distinct mechanisms; don't over-unify.

**▶ ⭐ LOCALIZED (`tools/headersweep.py`) — the gap is TWO TUs, and it points at `Worldgen.h`.**
- `DeskcppView.cpp` ~6-8 symbols short → gains 0x40ebe0, 0x40fca0
- `Worldgen.cpp` exactly **7** short → gains 0x423110 (n≥3), 0x41f830 (only n=7)
- **every other TU is ALREADY correct** — Iact/WorldgenHelpers/GameObjects/IactScript/DeskcppDoc only
  ever LOSE when perturbed. Free gains appear ONLY in headers avoiding those TUs (`Worldgen.h`,
  `Deskcpp.h` — identical curves, its 2 extra TUs inert — and `TextDialog.h`, the clean control that
  delivers DeskcppView's two and neither of Worldgen's). Full table: docs/compiler-hunt.md v96b.

**▶ ⛔ THE RULE THAT KEEPS THIS HONEST — the dial is an INSTRUMENT, not a knob.** Full statement in
the 🛡 ANCHOR section. Short form: a **free gain (zero regressions)** is the fingerprint of a REAL
missing fact (the `afxcmn.h` pattern); a **trade (+3/−3)** is the fingerprint of padding. **NEVER pad
to a number** — the 215 is currently placeholder decls and is deliberately NOT committed as source.

**▶ v97 (2026-07-26, THIS session) — pickup steps 1 & 2 EXECUTED; real RE artifacts SHIPPED (all
5 oracles green, anchor held 211/99.17 %, never dropped, build-sdl green).**
- **⭐ Member-vs-file-scope A/B — RESOLVED** (`tools/membertest.py`, was UNTESTED): members are
  **INERT** — a struct's N members don't dial (exact flat across n=1..12), only the single struct
  TAG does; externs move it and **n=7 uniquely unlocks 0x41f830**. ⇒ the missing symbols MUST be
  **file-scope or enum enumerators** (enumerators leak to enclosing scope; struct members never
  escape the class scope). Reconciles v36 #8 with v96. **Tool kept: `tools/membertest.py`.**
- **⭐ Ghidra globals inventory — DONE.** Only unmodelled REAL worldgen-TU global is the
  **DTA/.wld record-tag table (0x00456890)**: 16×8 bytes `ENDF ACTN HTSP ZAX3 ZAX2 ZAUX VERS ZONE
  PUZ2 SNDS CAUX CHWP CHAR TNAM TILE STUP` (+`YODASAV44` magic at 0x456910) — we'd inlined them.
  **SHIPPED `char g_aDtaRecordTags[16][8]` at EOF of Worldgen.cpp** (unreferenced-but-faithful;
  wiring the strcmp sites deferred).
- **Placement/#line refinement (MEASURED):** the table at TOP of Worldgen.cpp flips 0x41d8d0 OFF
  (34→33); at **EOF (line-neutral) it is 34/34 zero-delta**. The 33-dip was #line rotation, NOT the
  +1 symbol. ⇒ **EOF is the dial-safe home for any new real global** in a byte-matched TU.
- **`TileFlags` +3 enumerators SHIPPED** (`TILE_PLAYER/TILE_ENEMY/TILE_FRIENDLY` =1<<16/17/18,
  comments→enumerators, readability, pickup-sanctioned).
- ⚠ **MODEL REFINEMENT: the "~7" is not any 7 file-scope decls.** Real additions (cpp-EOF array +
  shared-enum aliases) did NOT reproduce the +7 extern sweep — **211 held, 0x41f830 STILL gated** on
  exactly matching the original's ~7 symbols. Don't chase it with filler; 211 is the honest plateau.

**▶ NEXT — pick up here (real RE, not sweeping):**
1. **(DONE v97 — see above.)** Remaining honest open thread: wiring `g_aDtaRecordTags` into the ~50
   `strcmp(tag, "…")` Load/Save sites (line/byte-neutral by construction — relocs masked; keep the
   callsites' line counts stable). If the original's dispatcher was a table LOOP, reproduce that
   shape from DESKADV/YodaDemo disasm rather than per-index guessing.
2. **(DONE v97 — membertest settled it.)**
3. **Re-baseline is a NO-OP for v97** (211 held; real change was dial-net-neutral). Do NOT re-baseline
   upward unless a REAL missing decl lands with zero regressions AND raises the count — never pad.
4. **Reopen the residual hunt with the right partition** (`tools/idiomscan.py`): **41** functions differ
   by regalloc/scheduling ONLY (16 perfectly aligned) — that is the dial's population, ~10 already
   proven dial-reachable. **~134** are unfaithful SOURCE (ordinary decomp work; the small-`align` ones
   are the cheap wins). **A hard core is dial-invariant** — `DetonateAdjacentTiles` never moved once
   across ~70 positions, corroborating PLAN_COMPLETED #29 *for that function*.
5. **De-hex leftovers** (all still valid): `0x68`→PLAN_WALL in WorldgenHelpers/DeskcppDoc (blocked — a
   shared `#define PLAN_WALL` would rewrite Worldgen.h's enum declaration into `104 = 104`; needs the
   enum relocated, itself a dial risk now measurable); ambiguous `TileFlags` bits 16-19 (need real RE);
   DeskcppDoc's `0xffffffff` sentinels + `0x11/0x10/0xe` zone-state codes; `WORLD_GRID_SIZE 10` (user's
   call); the `Canvas::Canvas` `sizeof` dial note at Canvas.cpp EOF.

**▶ TOOLS ADDED (all committed, all restore headers via atexit+finally):** `tools/idiomscan.py`
(classify residuals; ⚠ slice the original at OUR trimmed COMDAT length — `toolchain/test/app_funcs.txt`
extents are for coverage accounting and have bogus entries, e.g. 0x416620 listed as 1 byte, which
fabricated a whole function of phantom delta before the built-in assert caught it) · `tools/dialsweep.py`
(`--all-tus` for project-wide) · `tools/enumfieldtest.py` (mechanism + determinism/name-length controls)
· `tools/headersweep.py` (reach fingerprints) · `toolchain/vc40mix/` (4.0 BIN + 4.2 headers, the
documented A/B config).

**▶ HOW TO WORK THE DIAL SAFELY:** every sweep MUTATES a header — always restore (the tools do, via
atexit+finally, and leave a `.bak` if restore fails). ⚠ never run two sweeps concurrently or start one
while a `progress.py` is in flight: they fight over the header AND `build/*.obj` (this confounded the
first ArtooHint measurement and cost a full re-run). Verify a clean tree with
`git diff --stat src/` + `grep -rn "DIALSWEEP GENERATED" src/` before trusting any number.

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
