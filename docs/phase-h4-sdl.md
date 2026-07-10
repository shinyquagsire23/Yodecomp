# Phase H4 — the portable SDL target via "microfx" (a source-compatible MFC subset)

*Status: M0 ACHIEVED (2026-07-10, v73). ALL 13 game TUs compile AND whole-archive-link natively on
arm64 macOS (`cmake -B build-sdl -DYODA_PLATFORM=SDL && cmake --build build-sdl` →
`build-sdl/worldgen_smoke` passes core-class self-tests). Anchor re-verified after: 211 exact, link
0/0/exit0, bugscan 0/0/0, vtcheck 10 CLEAN, msgcheck 11 CLEAN.*

*Owner decision (user): implement a subset of MFC and reuse the existing macros/message-map
conventions, rather than ifdef'ing every MFC touch — "otherwise it'll be a tangle of ifdefs and
whack-a-mole-ing. OpenJKDF2 ultimately had to operate similarly with the menus and stuff like WM_PAINT."*

## Shared-source footprint (v73 — keep this list complete)

The ONLY changes ever made to byte-match-era sources for H4 (all anchor-token-neutral, oracles re-run):
- `Canvas.cpp` — 2 regions: `BlitFast`/`BlitMasked` hand-asm tails behind `#ifndef YODA_PORTABLE`
  with C equivalents in the portable branch (32-byte row copy / color-key blit).
- `Deskcpp.cpp` — 1 region: the CPUID `__asm` probe guarded out (flag already 0 → C blit paths).
- `DeskcppView.cpp` + `Worldgen.cpp` — `PTRINT` macro (anchor: `#define PTRINT int` → original
  tokens; portable: `intptr_t`) at 11 pointer-through-int cast sites (IactProbeMove's dead a5 arg
  ×10, the `// sic` equipped-item degrade ×1).
- `GameTypes.cpp` — `AppWnd` message map defined under `YODA_PORTABLE` (original data @0x44b000:
  WM_TIMER+WM_PAINT; anchor never emits the vtable, clang's key-function rule does).
- `Worldgen.h` — portable-only `virtual ~CDeskcppDoc();` declared FIRST in the facade view of the
  class (see ODR lesson below).

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

## What runs vs what's stubbed (M0)

REAL in microfx: CString/CFile/CArchive/collections/CObject/CRuntimeClass/exceptions (core/),
message-map data structures + macros, GetTickCount/Sleep, rect helpers, _splitpath/_makepath.
STUBBED (M1/M2/M3/M4): all GDI (returns null handles), all USER (no-ops), WaveMix/MCI, registry,
dialogs (DoModal→IDCANCEL), CWinApp::OnFileNew (doc creation), profile settings, LoadString.
Next milestone M0-finish/M1: real doc-creation path + YODESK.DTA load + fixed-seed worldgen log diff
vs wine, then Canvas→SDL_Surface.

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
- **M1 — Canvas → SDL.** `gdi/` shim; Canvas's 8-bit DIBSection framebuffer becomes an 8-bit paletted
  SDL_Surface (pData pixel writes work unchanged). **Oracle:** render one zone to an SDL window / dump PNG.
- **M2 — app shell + pump.** CWinApp::Run event pump; SDL events → WM_KEYDOWN/WM_MOUSEMOVE/… → the
  EXISTING message-map handlers; SetTimer → WM_TIMER drives OnTimer (the game loop). **Oracle:** walk
  around a zone natively.
- **M3 — audio.** snd/ over SDL2_mixer (WAV channels ≈ WaveMix, Mix_Music ≈ MCI MIDI for Indy).
- **M4 — resources + UI chrome.** res/ embedded-blob loader (cursors, icons, strings); TextDialog +
  save/load dialogs; menus (SDL UI or keyboard shortcuts first).
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
