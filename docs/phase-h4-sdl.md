# Phase H4 — the portable SDL target via "microfx" (a source-compatible MFC subset)

*Status: M4 CORE COMPLETE, ⭐ USER-CONFIRMED (2026-07-10, v79a-g). RESOURCES + UI CHROME WORK
NATIVELY: embedded .res loader (LoadString/LoadIcon/LoadCursor/named RT_BITMAPs), real GDI
chrome (pens/brushes/FillRect/PatBlt/Pie/RoundRect/Polygon/lines/pixels + PC_RESERVED-aware
color mapping), genuine MS Sans Serif bitmap-font text (13px/16px FNT strikes), a REAL modal
GetMessage pump + word-wrapping CEdit + CBitmapButtons (TextDialog speech bubbles fully
modal — text renders, close button shows, Enter/Esc/click dismiss, zone repaints beneath),
software cursor composited at present time, Win95-style SB_CTL scrollbar, and real window
teardown (view dtor → WaveMixCloseSession — "[snd] session closed"). USER-CONFIRMED: item
click-drag (incl. heal via player AND health-circle drop targets), locator textboxes
(the v78 test case), close-on-PRESS button fidelity matching the original. Remaining M4/M5
tails: CDialog::DoModal (F8 stats, About, option sliders, save/load), menus, INDY×SDL
in-game playtest. Details per subsystem below; v79 lessons at the end of the lessons list.*

*M3-era status (v78): SOUND + MUSIC WORK NATIVELY:
`snd/mfxsnd.cpp` implements the full WaveMix* set + mciSendString over SDL2_mixer — the Yoda
startup theme, the intro X-Wing STUP flight and SFX all play and MIX correctly (user-confirmed
live), and the previously-untested INDY×SDL config now BUILDS AND RUNS natively (DESKTOP.DAW
load, all 15 WAVs open, MCI sequencers open, startup THEME.MID plays through the fluidsynth
SoundFont path). Mechanisms + the ⭐ WaveMix dwFlags=2 lesson below. Headless oracles re-green
(worldgen_smoke exit 0, game_walk WALKED), anchor table FULL GREEN after the one shared-TU edit
(Worldgen.cpp GAME_INDY-region old-for-scope decls — see the footprint list).*

*M2-era status (v76): THE GAME RUNS NATIVELY ON macOS:
`build-sdl/yoda` boots to the Yoda Stories title screen, auto-plays the intro into the starting
Dagobah zone, ticks the real game loop (SetTimer 0x1d1d → WM_TIMER → CDeskcppView::OnTimer →
Tick), takes keyboard/mouse input through the game's OWN message maps, and the hero WALKS
(camera scroll included). Verified two ways: (1) `game_walk` — the deterministic headless
walk oracle (bootstrap → first WM_PAINT → Load @ pinned seed → pump timers to play mode →
synthesize WM_KEYDOWN arrows → assert cameraX/Y moved; exit 0 = GREEN, walk.bmp dumped);
(2) live SDL window screenshots (`YODA_SHOT=<prefix>` env dumps the screen DIB every 2s):
title screen w/ correct palette + starfield → intro auto-advance → zone with Luke/X-wing/
R2-D2 correctly rendered → hero displaced + viewport scrolled after a YODA_AUTOKEY hold.
Delivered in v76: mfxwnd.cpp (HWND object model, the AFX_MSGMAP dispatch engine decoding all
17 AfxSig shapes, real SetTimer/KillTimer table, posted-msg queue, real SDI bootstrap
LoadFrame→WM_CREATE→OnCreateClient→view-with-real-HWND), mfxpump.cpp (SDL events → WM_* →
maps; screen-DIB present per frame w/ integer YODA_SCALE), gdi palettes (CreatePalette/
Select/Realize→DIB-color-table, AnimatePalette write-through for cycling), yoda_main.cpp
(AfxWinMain equivalent) + `yoda` target. HUD chrome (pens/brushes/Pie/FillRect bevels),
child controls, dialogs and sound remain stubbed → right panel black, bubbles auto-dismiss
(GetMessage stub) — M3/M4. worldgen_smoke digest re-verified byte-identical to the pre-M2
reference (the real-window bootstrap is worldgen-neutral); zone_view GREEN; anchor 211.*

*M2 repro: `cmake --build build-sdl && cd build-sdl && cp <ini> game_walk.INI && ./game_walk
0x2a` (headless, exit 0 = GREEN) · `cp <ini> yoda.INI && YODA_SHOT=shot YODA_AUTOKEY=11000:39:2500
./yoda` (SDL window; VK 0x27=RIGHT held 11s→13.5s; shots land in shotNN.bmp). Ensure
Terrain∈{1,2,3} in the INI (the ⚠ below). ⚠ Walk-oracle trap: `playerX/playerY` are the 10x10
WORLD-MAP cell — the in-zone hero anchor is `cameraX/cameraY` (pixels, cell*32; the F8 debug
dialog shows /32). ⚠ SDL turns SIGTERM into SDL_QUIT: kill → SC_CLOSE → ConfirmExit →
auto-IDYES (AfxMessageBox stub) → PostQuitMessage — the polite exit path, exercised on every
run.*

*M1 status (v75): COMPLETE. The gdi/ layer is REAL (memory DCs +
8bpp DIB sections + BitBlt + SetDIBColorTable, pure C++ — `microfx/src/gdi/mfxgdi.cpp`), and the
`zone_view` harness renders zones natively through the game's OWN path
(CDeskcppDoc::RefreshZone → Canvas::BlitFast/BlitMasked → DIB) with the game palette: verified by
eyeball on the intro/title zone, a Tatooine 18×18 desert zone, a 9×9 interior, and a Hoth snow
zone, both as 8-bit .bmp dumps (headless) and in a live SDL window (`--show`,
SDL_CreateRGBSurfaceWithFormatFrom wrapping the DIB bits as INDEX8). Repro (build-sdl as below):
`./zone_view 0x2a --dump zone.bmp` (intro zone) or `--zone <id>` / `--show`. ⚠ zone-id slots for
zones NOT on the current planet hold a **-1 sentinel, not NULL** (GetZoneById returns it
unchecked; the game only ever asks for on-planet ids) — any harness must filter `(Zone*)-1`.
Anchor untouched by construction (zero src/ edits this milestone) and re-verified: 211 exact.*

*M0 status (v74): ALL 13 game TUs compile AND
whole-archive-link natively on arm64 macOS, and the FULL GAME BOOTSTRAP runs headless: theApp →
InitInstance → doc template → CWinApp::OnFileNew (real SDI doc/frame/view creation) →
CDeskcppDoc::Load() parses YODESK.DTA (658 zones) → fixed-seed Generate()+Populate() succeed →
WORLD/CELL digest logged. **The native digest is byte-identical (modulo CRLF) to a same-seed
same-INI wine/Win32 run of build-full-dbg** — all 100 map cells, zone ids/types, quest items.
Repro: `cmake -B build-sdl -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL -DYODA_DEBUG=ON && cmake
--build build-sdl && (cd build-sdl && ln -sf ../YodaFull/YODESK.DTA . && cp <bottle>/windows/
yoda.INI worldgen_smoke.INI && ./worldgen_smoke 0x2a)`; wine side: build-full-dbg (YODA_DEBUG=ON)
exe in YodaFull/, `YODA_SEED=0x2a` env, digest lands in YodaFull/yoda_debug.log on first paint.
Ensure Terrain∈{1,2,3} in BOTH INIs first (see the ⚠ below), and reset both INIs before each run
(the doc ctor writes the re-picked planet back). `worldgen_smoke -` = unpinned random seeds.
Anchor re-verified after: 211 exact, link 0/0/exit0, bugscan 0/0/0, vtcheck 10 CLEAN, msgcheck
11 CLEAN.*

*Owner decision (user): implement a subset of MFC and reuse the existing macros/message-map
conventions, rather than ifdef'ing every MFC touch — "otherwise it'll be a tangle of ifdefs and
whack-a-mole-ing. OpenJKDF2 ultimately had to operate similarly with the menus and stuff like WM_PAINT."*

## Shared-source footprint (v74 — keep this list complete)

The ONLY changes ever made to byte-match-era sources for H4 (all anchor-token-neutral, oracles re-run):
- `Canvas.cpp` — 2 regions: `BlitFast`/`BlitMasked` hand-asm tails behind `#ifndef YODA_PORTABLE`
  with C equivalents in the portable branch (32-byte row copy / color-key blit).
- `Deskcpp.cpp` — 1 region: the CPUID `__asm` probe guarded out (flag already 0 → C blit paths).
- `DeskcppView.cpp` + `Worldgen.cpp` + `WorldgenHelpers.cpp` — `PTRINT` macro (anchor:
  `#define PTRINT int` → original tokens; portable: `intptr_t`) at 12 pointer-through-int sites
  (IactProbeMove's dead a5 arg ×10, the `// sic` equipped-item degrade ×1, StartGame's
  paZonePtrGrid clear walk ×1).
- `GameTypes.cpp` — `AppWnd` message map defined under `YODA_PORTABLE` (original data @0x44b000:
  WM_TIMER+WM_PAINT; anchor never emits the vtable, clang's key-function rule does).
- `Worldgen.h` — portable-only `virtual ~CDeskcppDoc();` declared FIRST in the facade view of the
  class (see ODR lesson below).
- `DeskcppDoc.cpp` — guarded `#include "Deskcpp.h"` + named `m_nFrameDelay` access replacing the
  `*(int*)((char*)pApp + 0xc4)` MFC-4.2-offset read (ctor path; microfx CWinApp is smaller).
- `DeskcppStub.h` + `GameObjects.h` — full parallel `class CDeskcppDoc` under `YODA_PORTABLE`
  (lesson 5 below); anchor branch untouched in the `#else`.
- `Worldgen.cpp` — v78: three `#ifdef YODA_PORTABLE` loop-variable declarations (`int i;` in
  IndyPopulateUsefulObjectZone, `int row, base;` in IndyAssignQuestStepCells, `int y;` in
  IndyGenerate) for VC4.2 old-for-scope leaks that clang hard-errors on. All three sit INSIDE
  the `#ifdef GAME_INDY` region that runs from line ~7917 to EOF, so the anchor sees neither
  the tokens nor any line-number shift (no anchor function exists below the region start);
  the VC4.2 Indy build skips them via the YODA_PORTABLE guard (adding `int` to the `for`
  would be C2374 there — old scope already leaked the variable). Anchor table re-run FULL
  GREEN. Pattern for future INDY×SDL old-for-scope errors: guarded decl at function top,
  NEVER `int` in the later `for`.
- `Worldgen.cpp` — v74 debug-oracle instrumentation, ALL under `YODA_DEBUG`: guarded
  `#include "DebugLog.h"` (⚠ must stay guarded — lesson 6), `YODA_SEED` env override in
  `Randomize()`, WORLD/CELL digest at `Load()`'s success tail. Added top-of-file lines were
  reclaimed from comments so every function keeps its original line number.
- `MainFrm.h` — v76: the 32-bit stub views (`CDeskcppView` pads / `FrameWorld` / `MusicThread`)
  wrapped in `#ifdef YODA_PORTABLE` parallel bodies (lesson-5 pattern: real types in the full
  declarations' order; MusicThread mirrors microfx CWinThread {CCmdTarget, m_pMainWnd,
  m_bAutoDelete, m_hThread}). Verified by a 12-field offsetof probe diffed stub-vs-real
  (IDENTICAL) AND the anchor oracle table re-run FULL GREEN after recompiling the Frame TU
  (the anchor branch moved down ~170 header lines — dial did not rotate).

## ⭐ Lessons (portable-build class, distinct from byte-match lessons)

1. **Key-function/ODR hazard:** the byte-match era keeps SEVERAL declarations of `CDeskcppDoc`
   (DeskcppDoc.h full / Worldgen.h facade / DeskcppStub.h stub) with identical layout but different
   override SUBSETS. All their virtuals are base-slot overrides, so MSVC-era layout agreed — but
   under the Itanium ABI each declaration nominates its own key function, and a TU seeing a PARTIAL
   declaration emits an INCOMPLETE vtable (base methods in un-declared override slots). Duplicate-
   symbol suppression would be UNSOUND. Fix: pin the key function of every partial view to the TU
   that sees the full declaration (declare a full-view virtual, e.g. the dtor, first under
   `YODA_PORTABLE`). Audit this for any OTHER multiply-declared class as more of the app layer wakes.
2. `TRY{...}END_TRY` (no CATCH) in real MFC catches-and-swallows `CException*` into the local
   `_afxExceptionLink` — the game hand-expands this pattern, so microfx must match it exactly.
3. Missing-shim = compile error is the completeness oracle, and it converges FAST: the entire 25k-line
   game needed only ~6 error batches of afx/Win32 additions.
4. Warnings from clang on faithful `// sic` code are expected (tautological WORD==-1, `&aPlan[109]`
   bound) — do not "fix" them; they are the original engine.
5. **Multi-declared CDeskcppDoc, part 2 — DATA layout (v74).** The byte-match era's stub views
   (DeskcppStub.h for Iact/WorldgenHelpers, GameObjects.h for GameObjects) model the class as
   32-bit pads + raw MFC-4.2 internals (CObArray guts as `tileArray`/`tileCount`, pointer grids
   as `int[]`). On LP64 those offsets diverge from the full declaration (probe: sizeof 14136 vs
   14888, `placedZoneIds` 560 vs 792) → the doc constructed by DeskcppDoc.cpp is read at wrong
   offsets → wild crashes deep in worldgen. Fix (no TU edits): a parallel `#ifdef YODA_PORTABLE`
   class body per stub header using REAL types in the full declaration's order (layout equal by
   construction), with the raw-guts names kept alive as anonymous-union overlays of the microfx
   CObArray layout `{vptr, m_pData, m_nSize, m_nMaxSize}`, `zones[200]` = mapGrid+mapGridBackup,
   and `paZonePtrGrid` as `intptr_t[120]` (+1 PTRINT site). Verified with a per-view offsetof
   probe diffed against the full view (kept conceptually in `tools/`-style scratch; re-derive
   with clang -DYODA_PORTABLE offsetof dumps if views change). MainFrm.h's
   `CDeskcppView`/`FrameWorld`/`MusicThread` stub views were the same trap class — FIXED v76
   (see the footprint list above) before the pump went live.
6. **Header presence is an anchor dial input even at ZERO tokens (v74).** Adding an unguarded
   `#include "DebugLog.h"` to Worldgen.cpp flipped `IsZoneUsed` (DIFF(2)) off byte-exact even
   after re-aligning all line numbers — with YODA_DEBUG off the header contributes only a macro
   definition, yet VC4.2's dial still moved (same family as the afxcmn-header lesson,
   PLAN_COMPLETED). Keep debug includes inside `#ifdef YODA_DEBUG`, and reclaim any added
   top-of-file lines from comments so function line numbers stay identical (lesson #23).
7. **⭐ WaveMix MIXPLAYPARAMS dwFlags=2 is WMIX_USELRUCHANNEL, not CLEARQUEUE (v78).** The MS
   WaveMix sample's flag values (WAVMIX32.DLL is its 32-bit port) are QUEUEWAVE=0 CLEARQUEUE=1
   USELRUCHANNEL=2 HIPRIORITY=4 WAIT=8. The game's only play shape — iChannel=0, dwFlags=2 —
   therefore means "IGNORE iChannel, pick the least-recently-used free channel": concurrent
   sounds MIX (the startup theme keeps playing under SFX). First coded as CLEARQUEUE
   (halt-then-play on channel 0) every new sound cut the previous one — user-detected as "the
   intro STUP sound gets cut off prematurely". Symptom key: sounds cutting each other = flag
   misread, not an SDL_mixer channel shortage.
8. **Old-for-scope is an INDY×SDL class of clang hard-error (v78).** The GAME_INDY worldgen
   transcriptions rely on VC4.2 leaking `for (int i...)` variables to function scope (later
   loops are deliberately written `for (i = ...` because a second `for (int i` is C2374 under
   old scope). clang errors "undeclared identifier". Fix pattern: `#ifdef YODA_PORTABLE` real
   declaration at function top (behavior-identical — every leaked use re-initializes in its
   for-init; verify that before applying). Worldgen.cpp was the only affected TU (3 sites).
9. **⭐ The GetPixel probe demands WM_ERASEBKGND (v79).** DrawGameArea probes pixel
   (0x138,0x11c) and triggers a FULL RedrawWindow whenever it isn't COLOR_3DFACE gray — the
   gray comes from OnEraseBkgnd, which Win32's BeginPaint delivers before WM_PAINT. Making
   GDI real without delivering the erase turned every game blit into a full-repaint storm
   (~15s CPU); MfxPaintIfDirty now sends WM_ERASEBKGND ahead of WM_PAINT.
10. **⭐ GDI color matching must skip PC_RESERVED palette entries (v79, user-detected).** The
   game flags its palette-cycling ring entries peFlags=1; Win32 never matches solid colors
   into them. A naive nearest-across-256 mapped the health dial's green onto a cycling index
   — the disc visibly "flashed" as CyclePalette animated. MfxMapColor now matches against
   the DC's selected palette, skipping reserved entries. Sibling fidelity bug, same class:
   Win32 LineTo EXCLUDES the endpoint — an inclusive Bresenham overshot the DrawRect bevel
   mitres by one pixel (user-visible on the panel's right edge).
11. **Shared-surface children need re-compositing + boot-dirty care (v79).** (a) Win32 child
   controls are separate surfaces a parent blit can't erase; here everything shares one
   screen DIB, so DrawGameArea would wipe the bubble edit/buttons — gdi fires an OVERLAY
   hook (MfxPaintChildren) before the present hook on every screen write, reentry-guarded,
   with MfxTouchHold/Release batching a control's many primitives into one present. (b) The
   game leaves the bubble BUTTONS at WM_SETREDRAW(0) forever yet Win32 still shows them —
   treat the redraw flag as gating only state-change self-repaints, never show/full-repaint
   passes. (c) The frame is created WS_VISIBLE, so "mark dirty only on hidden→visible
   transition" never fires — a redundant ShowWindow(SW_SHOW) must still mark the first
   paint dirty (black-screen regression until fixed).
12. **Modal loops: one MSG queue + a headless bail (v79).** TextDialog::Run and the F8/stats
   loops call raw GetMessage/DispatchMessage — the pump was restructured so SDL events and
   due timers become MSGs in the SAME posted queue that GetMessageA drains (msg.pt carries
   the cursor pos the game's PtInRect tests read; mouse messages hit-test to child windows,
   capture-aware). CWinThread::Run is now itself a Win32-shaped GetMessage-less pump over
   that queue. GetMessageA returns 0 immediately when no SDL window exists — a headless
   harness (game_walk) that trips a bubble would otherwise hang forever in a modal wait
   (observed: game_walk 100%-CPU hang before the guard).
13. **Cursors are SOFTWARE by default (v79, user-detected regression → redesign).** SDL
   hardware color cursors misbehaved (cursor vanished everywhere); now the decoded .res
   cursor is composited over the window surface at present time — chunky at the window
   scale (matches the game pixels), screenshot-visible, and the path a DS port needs.
   YODA_HWCURSOR=1 re-enables the SDL hardware path. Fidelity rules: before the game's
   FIRST SetCursor Win32 shows the class arrow (leave the OS cursor); SetCursor(NULL)
   hides (keyboard-move/drag modes); IDC_ARROW keeps the OS arrow.

## What runs vs what's stubbed (v74)

REAL in microfx: CString/CFile/CArchive/collections/CObject/CRuntimeClass/exceptions (core/),
message-map data structures + macros, GetTickCount/Sleep, rect helpers, _splitpath/_makepath,
**CWinApp::OnFileNew** (SDI doc/frame/view creation from the template's CRuntimeClasses),
**GetModuleFileName** (real exe path; the game derives the data dir from it — CFile::Open
normalizes the game's '\\'-shaped paths to '/'), **CreateDIBSection** (callocs the pixel buffer —
Canvas::Clear writes it unguarded) + nonzero CreateCompatibleDC, **INI-backed
GetProfileInt/String + Write*** ("<exebase>.INI" next to the exe, same [OPTIONS]/[GameData]
format as the Win32 build's <exe>.INI — copy a bottle INI over verbatim to align runs), and
**MSVC-4.2 rand()/srand()** (afxwin.h redirects the game TUs to the exact CRT LCG
`x*214013+2531011 >>16 &0x7fff`, holdrand init 1 — worldgen parity depends on it).
REAL as of M1 (gdi/mfxgdi.cpp — pure C++, no SDL dependency, so worldgen_smoke stays headless):
CreateCompatibleDC/DeleteDC (memory DC = one DIB selection slot), CreateDIBSection (8bpp only:
tagged HBITMAP__ object, pixels top-down pitch==width, color table initialized from bmiColors —
Canvas's `(BITMAPINFO*)&biHeader` cast is load-bearing: palette[256] sits right after the header),
SelectObject (bitmap→DC; non-bitmap handles pass through), DeleteObject, BitBlt (clipped 8bpp
row copy, rop ignored — the game is all-SRCCOPY), Set/GetDIBColorTable. Presentation reads the
DIB back out via the extension API in `microfx/include/microfx.h` (MfxGetDCDib / MfxWriteDibBMP);
game TUs never include it.
REAL as of M2 (app/mfxwnd.cpp pure C++ + app/mfxpump.cpp SDL-only + gdi palettes):
**HWND objects** (tagged HWND__ {CWnd*, parent, id, rect, visible}; first parentless Create =
the root, which sizes the shared screen DC's 8bpp DIB — GetDC(anything incl. NULL) returns that
one DC; view at (0,0) so view coords == DIB coords), **the message-map dispatch engine**
(GetMessageMap chain walk + all 17 AfxSig decodings via MFC-style member-pointer union;
WM_COMMAND routes view→frame→app), **SendMessage/PostMessage** (+queue), **SetTimer/KillTimer**
(real table; MfxPumpTimers fires WM_TIMER — 0x1d1d is the game loop), **the SDI bootstrap**
(LoadFrame → virtual PreCreateWindow (CMainFrame pins 525x310) → WM_CREATE → CFrameWnd::OnCreate
→ OnCreateClient → view CreateObject+Create+AddView+SetActiveView; OnFileNew never paints —
first WM_PAINT comes from the pump, so headless harnesses keep the M0 flow), **CView::OnPaint**
(→ CPaintDC + OnDraw, map entry on CView's map), **capture/focus + GetAsyncKeyState/GetCursorPos**
(pump-fed state arrays), **palettes** (gdi: CreatePalette/CreateHalftonePalette real objects;
SelectPalette per-DC slot; RealizePalette → entries into the selected DIB's color table;
AnimatePalette writes through to the last-realized DC — palette cycling works), **CWinThread::Run**
(mfxpump.cpp: SDL_PollEvent → VK translate → WM_KEYDOWN/UP/CHAR/MOUSE* to focus/capture target;
SDL_QUIT → SC_CLOSE (2nd = hard quit); FOCUS_GAINED/LOST → WM_ACTIVATE (the game's own
pause-on-deactivate runs); per-frame present = screen DIB wrapped as INDEX8 SDL_Surface +
SDL_SetPaletteColors + BlitScaled at integer YODA_SCALE (default 2); YODA_SHOT / YODA_AUTOKEY
debug oracles), and the **`yoda` executable** (harness/yoda_main.cpp = AfxWinMain equivalent).
REAL as of M3 (snd/mfxsnd.cpp — SDL2_mixer; silent-stub fallback when built without it):
**the full WaveMix* set** (Init opens SDL audio 44.1k/stereo → nonzero session or 0 on
failure/YODA_NOSOUND; OpenWave = Mix_LoadWAV with '\\'→'/' + lowercase retry, handles =
1-based indices into an internal Mix_Chunk table since `g_waveHandles` is int[64];
OpenChannel/Activate return 0=SUCCESS; Play parses the packed MIXPLAYPARAMS by memcpy and
honors dwFlags=2 = USELRUCHANNEL — lesson 7 — with LRU steal when all 16 channels busy;
Flush/Close/Free halt-safe; Pump = no-op, SDL_mixer self-mixes, so AfxBeginThread stays a
deliberate no-thread object), and **mciSendString** covering exactly the game's four
sequencer shapes ("open sequencer!<file> alias <NAME>" / "play <NAME> from 1" / "stop" /
"close") over Mix_Music, with SoundFont resolution for fluidsynth MIDI (YODA_SOUNDFONT env >
Mix defaults > probe list > brew fluid-synth demo font). ⚠ A non-GM bank (the demo font)
renders the percussion channel as melodic patches — user-reported as an intermittent
"wip-wip" laser sound absent under wine; install a GM bank at
/opt/homebrew/share/soundfonts/default.sf2 (first probe path; GeneralUser GS works) or set
YODA_SOUNDFONT. YODA_SNDLOG=1 traces to stderr.
Yoda: theme + intro + SFX user-confirmed audible and mixing. Indy: MCI sequencers open +
THEME.MID plays natively (build-sdl-indy — config now builds and runs; in-game playtest
pending). Known M4 tail: the polite-exit path never runs the view dtor (pump exit doesn't
destroy the frame), so WaveMixCloseSession isn't reached — the OS reclaims audio at process
death; harmless but wire it when real window teardown lands.
REAL as of M4 (v79): the whole HUD/bubble chrome — see the M4 milestone entry below for the
full inventory (res loader, GDI drawing, MS Sans Serif text, modal GetMessage + CEdit +
CBitmapButton + CScrollBar, software cursors, teardown).
STILL STUBBED (M5-ish): CDialog::DoModal → IDCANCEL (F8 stats, About, the three option
sliders, save/load CFileDialog), menus (ON_COMMAND ids reachable by keyboard only).
⚠ Worldgen requires a REAL planet: `Generate` filters zones by `pZone->planet == currentPlanet`,
so Terrain=-1 in the INI (Indy writes -1 into a shared bottle INI — Indy zones carry planet=-1)
makes Yoda worldgen retry FOREVER; with a YODA_SEED-pinned Randomize that's a 100%-CPU hang on
BOTH wine and native. Set Terrain to 1/2/3 before oracle runs. The doc ctor re-picks the planet
with one `rand()%2` — deterministic and equal on both sides (first rand() call of the process).
Next milestone M4: resources + UI chrome (see the milestone list).

## The core idea

Every game TU acquires MFC exclusively through four headers — `<afxwin.h>`, `<afxext.h>`, `<afxcmn.h>`,
`<afxcoll.h>` (plus `<mmsystem.h>` in DeskcppView.cpp). For `YODA_PLATFORM=SDL` we put
**`microfx/include/` first on the include path**, so those *same include directives* resolve to **our**
drop-in headers, which declare a minimal source-compatible subset of MFC + Win32 implemented over SDL2.

**Why this preserves the anchor by construction:** the 13 byte-matched TUs are compiled *unmodified* —
zero token changes, zero line-number shifts (the lesson-#23 codegen-rotation hazard never arises), because
the platform swap happens in the header search path, not in the source. The WIN32/MFC anchor build never
sees `microfx/`.

This is the same shape OpenJKDF2 landed on (its `Win95/` modules keep their Win32-era API surface;
`Platform/SDL2` reimplements the internals) — one level up the stack: we keep the *MFC*-shaped code and
reimplement MFC.

## Layout

```
microfx/
  include/          # drop-in headers, named so <afxwin.h> etc. resolve here
    windows.h       # Win32 types + the GDI/USER/kernel subset the game calls
    afxwin.h        # CObject/CCmdTarget/CWnd/CWinApp/CDocument/CView/CDialog/CDC/GDI objects/
                    #   CString/CFile/message-map macros/DYNCREATE/afx globals
    afxext.h  afxcmn.h  afxcoll.h  afxres.h   # thin — mostly satisfied by afxwin.h
    mmsystem.h      # PlaySound / mciSendString / MIDI decls
    wavmix.h        # WaveMix* decls (game links WAVMIX32)
  src/              # implementations, compiled ONLY in the SDL config, host compiler + SDL2
    core/           # no-SDL pure C++: CString, CFile, CObArray/CWordArray, CObject+CRuntimeClass,
                    #   CException/CFileException, CArchive (if needed), CPoint/CRect/CSize
    msg/            # CCmdTarget dispatch engine: our BEGIN_MESSAGE_MAP tables, WM_*→afx_msg routing,
                    #   ON_COMMAND/ON_UPDATE_COMMAND_UI, timer delivery
    gdi/            # HDC/HBITMAP/HPALETTE over SDL_Surface: CreateDIBSection≈8-bit surface,
                    #   BitBlt≈SDL_BlitSurface, palettes (incl. AnimatePalette for the Indy cycler)
    app/            # CWinApp::Run = SDL event pump → WM_* synthesis → message maps; SetTimer;
                    #   window/view/frame as ONE SDL window; cursors via SDL_CreateColorCursor
    snd/            # WaveMix*/PlaySound/mciSendString over SDL2_mixer (WAV channels + MID music)
    res/            # loader for icons/cursors/bitmaps/strings/dialog templates from an embedded
                    #   .res blob (reslib.py knowledge, in C++); AfxGetResourceHandle-compatible
    ui/             # dialogs (TextDialog, About, save/load) — LAST; see open questions
```

## What the shim must cover (measured, v72 — full inventory in progress)

- **Classes** (by usage count): CString 113, CFile 106, CDC 87, CWordArray 62, CObject 55,
  CFileException 48, CWnd 42, CDialog 39, CPalette 37, CObArray 37, CCmdUI 36, CPoint 33, CWinApp 30,
  CView 19, CScrollBar 19, CFrameWnd 16, CDocument 13, CBrush 13, CPen 12, CDataExchange 12, CBitmap 10,
  CBitmapButton 9, CRect 8, CFont 7, CEdit 7, CArchive 6.
- **Message maps**: 24 ON_COMMAND, 18 ON_UPDATE_COMMAND_UI, ~30 ON_WM_* (timer, keys, mouse, palette,
  scroll, ctlcolor, create/destroy/size/activate…), 3 ON_BN_CLICKED.
- **Afx globals**: AfxMessageBox, AfxGetApp, AfxAbort, AfxGetResourceHandle, AfxGetModuleState (PLAN lesson —
  resource vs instance handle matters), AfxBeginThread (pMusicThread), AfxGetMainWnd, AfxGetInstanceHandle.
- **Direct Win32**: GDI (CreateDIBSection/CreateCompatibleDC/SelectObject/BitBlt/pens/brushes/Pie/DrawIcon),
  USER (scrollbar get/set, SendMessage, SetCursor, SetTimer, GetSysColor), mmsystem (PlaySound,
  mciSendString — Indy MIDI), WAVMIX32 (full WaveMix* set).

## Non-goals / simplifications

- **Struct-layout fidelity is NOT required.** File I/O is field-by-field (e.g. `pFile->Read(&pTile->flags,4)`)
  — no whole-struct dumps — so microfx base classes may have any size. Save files are self-consistent per
  build; cross-compat with Windows saves is a non-goal.
  - ⚠ ONE known layout-pinned line: `DeskcppDoc.cpp:463` reads `*(int*)((char*)pApp + 0xc4)` (an
    MFC-4.2-offset CDeskcppApp field, still unnamed). Must become a named member on the portable path —
    first RE the field, then a `YODA_PORTABLE` branch (or name it everywhere if token-neutral is provable).
- **No general MFC**: only the members this codebase calls. Unused = undeclared (missing-shim = compile
  error, which is the completeness oracle).
- Menus/toolbars/dialog chrome come last; core gameplay (view + timer + input + blit + sound) first.

## Incremental milestones (each independently verifiable)

- **M0 — core + logic TUs on macOS.** microfx `core/` (no SDL). Compile the pure-logic TUs
  (GameTypes, Score, WorldgenHelpers, GameObjects, Worldgen, Iact, IactScript…) with host clang.
  **Oracle:** a tiny native `worldgen_main` harness loads YODESK.DTA, runs worldgen with a fixed seed, and
  its YDBG log diffs clean against the same-seed wine/Win32 log. (Headless, fast, no window needed.)
- **M1 — Canvas → SDL.** ✅ v75. `gdi/` real DIB/DC objects (pure C++); `zone_view` harness renders
  zones via the game's RefreshZone into the Canvas DIB — .bmp dump + SDL window both verified
  (intro/desert/interior/snow zones, correct palette + masked blits).
- **M2 — app shell + pump.** ✅ v76. CWinThread::Run SDL pump; events → WM_* → the EXISTING
  message-map handlers; real SetTimer → WM_TIMER 0x1d1d drives OnTimer (the game loop); screen
  DIB presented per frame. **Oracle GREEN both ways:** `game_walk` (headless, deterministic —
  hero cameraX/Y moves under synthesized arrow keys) + live-window YODA_SHOT screenshots
  (title → intro → zone → hero walked/camera scrolled).
- **M2 tail — transitions + drag (v77).** ✅ USER-CONFIRMED in-game: zone-edge scroll pans (all 4
  directions), the X-Wing STUP flight, the X-Wing fly-in IACT, drag save-under; doors/buildings
  no regressions. Three mechanisms, all microfx-only:
  - **Overlap-aware BitBlt** (gdi/mfxgdi.cpp): ScrollZoneTransition (0x411180) scrolls by blitting
    the screen OVER ITSELF (same DIB as src+dst). Per-row memmove covers horizontal overlap; a
    DOWNWARD self-blit (dst y > src y, same pBits) must iterate rows bottom-up or later source
    rows are clobbered before they're read.
  - **Present-on-screen-write hook** (`MfxSetScreenWriteHook`, microfx.h): Win32 makes a BitBlt to
    the screen DC visible IMMEDIATELY; our pump presents only between handler returns. Game code
    animates INSIDE one handler with clock() busy-waits (ScrollZoneTransition; StartGame's 5-frame
    X-Wing STUP flight, WorldgenHelpers.cpp:795) — those frames were drawn then overwritten
    unseen ("transition looks instant" = frames drawn but never presented). gdi BitBlt now fires
    a registered callback on every screen-DC write; the pump registers its SDL presenter. gdi
    stays SDL-free (raw fn ptr; headless harnesses never register one → worldgen_smoke/game_walk
    untouched). Diagnostic key: mode-6 door transitions always worked because they step once per
    TIMER TICK (pump presents between ticks) — per-tick animations work, in-handler ones don't.
  - **Win32-CRT clock() shim** (afxwin.h tail, same pattern as the rand/srand LCG remap): MSVC
    clock() is WALL ms (CLOCKS_PER_SEC=1000); host clock() is CPU-time µs → every busy-wait ran
    ~1000x fast (transitions instant even when painted, IACT waits skipped). `#define clock
    mfx_clock` → monotonic ms (mfxcore.cpp). <time.h> is included before the define so the host
    declaration can't be rewritten into a conflicting one.
  - Also real now: CreateBitmap/Set/GetBitmapBits (an 8bpp DDB = a DIB in our device; the 32x32
    drag save-under in UpdateDragCursor). YODA_SHOT extended: `YODA_SHOT=<prefix>[:count]`
    (default 8 shots).
  - Known rough edge: item drag redraws at game-tick rate (low-refresh feel vs Win32's hardware
    cursor). A hardware-cursor path (SDL_Cursor) is a possible M4+ option — but KEEP the software
    path as a build option (user plans to try a DS port; no hardware cursors there).
- **M3 — audio.** ✅ v78, USER-CONFIRMED. snd/mfxsnd.cpp over SDL2_mixer (WAV channels ≈
  WaveMix with the dwFlags=2=USELRUCHANNEL mixing semantics — lesson 7; Mix_Music ≈ MCI MIDI).
  Oracle: Yoda startup theme + intro STUP + SFX audible and mixing (user); Indy THEME.MID
  plays natively; worldgen_smoke/game_walk still green; anchor table FULL GREEN.
  CMake: find_package(SDL2_mixer) → MICROFX_HAS_MIXER; absent = silent-stub fallback.
  Bonus: the INDY×SDL config (`cmake -B build-sdl-indy -DYODA_PLATFORM=SDL -DYODA_GAME=INDY`)
  builds and runs natively after 3 old-for-scope fixes (lesson 8).
- **M4 — resources + UI chrome.** ✅ CORE v79 (a-g), USER-CONFIRMED. Delivered:
  - **res/** (mfxres.cpp): the SAME yoda.res the WIN32 link consumes, embedded via
    tools/bin2c.py (PortableSDL.cmake mirrors the demo/full/indy make_res variants) and
    parsed at runtime (.res container walk). LoadString (all bubble/HUD text), LoadIcon +
    DrawIcon (HUD direction arrows 0xc4-0xcb), LoadCursor (the 11 game cursors + IDC_*),
    named RT_BITMAPs (bubble button faces CLOSEU/DNAU/UPAU…). Images decode to a uniform
    MfxImg (indices + own color table + AND mask); gdi's MfxDrawImage maps colors per-pixel
    into the DC palette (microfx.h MFXIMG API).
  - **gdi chrome** (mfxgdi.cpp): real pen/brush/font objects + per-DC draw state; FillRect/
    PatBlt/Rectangle/RoundRect/Polygon/Pie/MoveTo/LineTo(endpoint-exclusive)/SetPixel/
    GetPixel/GetClipBox/GetSysColor(Win95 scheme); MfxMapColor skips PC_RESERVED (lesson 10);
    all writes fire the overlay+present hooks (MfxTouch, batched via MfxTouchHold/Release).
  - **text** (mfxgdi.cpp + mfxfont_data.c): genuine MS Sans Serif 13px/16px FNT strikes
    (tools/fon2c.py from a Windows SSERIFE.FON, committed), GDI-style strike mapping for
    CreateFont(-8/-14), synthesized bold, TextOut/GetTextMetrics/GetTextExtentPoint32.
  - **modal UI** (mfxpump.cpp/mfxwnd.cpp/mfxctl.cpp): real GetMessageA over the shared MSG
    queue (lesson 12), word-wrapping CEdit (EM_GETLINECOUNT/EM_LINESCROLL/WM_SETFONT/
    WM_SETREDRAW/WM_SETTEXT), CBitmapButton (BM_GETSTATE auto-repeat, BN_CLICKED via queue),
    child registry + mouse hit-testing + overlay re-compositing (lesson 11). Bubbles fully
    modal; the v78 locator-click test case passes (user-confirmed).
  - **cursors**: software composite at present (lesson 13).
  - **scrollbar** (M4e): Win95-chrome SB_CTL (bevel arrows/checker track/thumb drag), scroll
    state on HWND, WM_VSCROLL(SB_*) to the parent → InvScrollBar's reflected handler.
  - **teardown**: pump exit destroys view+frame; ~CDeskcppView reached → WaveMixCloseSession
    ("[snd] session closed" in YODA_SNDLOG).
  - REMAINING (M5-ish): CDialog::DoModal (F8 stats box, About, Difficulty/GameSpeed/
    WorldSize sliders, save/load CFileDialog), menus/command UI, INDY×SDL in-game playtest.
- **Done when:** native SDL Yoda Stories runs on macOS AND `tools/progress.py` still reports 211 exact
  (trivially true if TU edits stay at zero; verify anyway after any shared edit).

## Compiler reality (host clang vs 1997 C++)

The TUs must ALSO compile under modern clang. Expect: writable string literals, implicit int, old-for-scope,
`char*` conversions. Strategy: permissive flags first (`-Wno-writable-strings -fms-extensions`, C++98/03 mode
if needed); only where clang hard-errors do we touch a TU, and then the WIN32 path must remain the
token-identical fall-through (`#ifdef YODA_PORTABLE` alternate + original in `#else`), anchor oracles re-run.
Track every such edit in this doc — the count should stay near zero.

| risk | mitigation |
|---|---|
| clang hard-errors in shared TUs | permissive flags; last resort guarded edit + anchor re-run |
| dialogs (TextDialog text bubbles use real EDIT controls) | M4; consider rendering via game Canvas + our own tiny edit-control |
| `+0xc4` app-object offset read | RE + name the field; guarded named access |
| menus/command routing | ON_COMMAND ids are just ints — a keyboard/menu-bar shim can post them |

## References

- `~/workspace/DesktopAdventures` — portable recreation; platform-abstraction map (SDL usage patterns),
  NOT behavior truth.
- `~/workspace/OpenJKDF2` — precedent for the API-shim-not-ifdef strategy, CMake layout.
- CLAUDE.md "H4 spec" section (now points here).
