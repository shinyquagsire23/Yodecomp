# Phase G2 — whole-image layout reconstruction

Goal: a **byte-identical whole image** (the user's ~100% goal), not just per-function content matches.
The 212 exact-func / reg-coloring story is about CONTENT (bytes of each function). G2 is about LAYOUT
(where each function/datum lands) + the residual plumbing. This doc holds the model + the worklist.
Started v41 (2026-07-08).

## Tooling
- **`tools/g2_link.sh`** — links the 13 app `.obj`s in ADDRESS order (the v40 link order) + NAFXCW/
  LIBCMT/Win32 imports, with `/OPT:NOREF` + `/MAP:yoda.map`. Output in `$CLAUDE_JOB_DIR/tmp/g2/`.
  Uses `/OPT:NOREF` deliberately (see mechanism #1). This is the layout-ANALYSIS build (yoda.exe ~522 KB,
  bloated by kept-but-unused lib code) — NOT the final image; link_exe.sh remains the runnable/oracle build.
- **`tools/g2_diff.py`** — for every `// FUNCTION: YODA 0xADDR` marker, reads the map and reports:
  - **LAYOUT** = linked Rva+Base == original marker address (the G2 goal)
  - **CONTENT** = reloc-masked bytes identical (the per-TU exact story; ~226 here vs progress.py's 212
    because NOREF keeps ~14 tiny lib-default COMDATs — CObject::Serialize/AssertValid/Dump etc. — that
    trivially match; those are folded lib defaults, not hand-written matches).
  - `--show-misplaced` lists orig→linked deltas.

## Baseline (v41, address-order + NOREF)
`total paired 378/378 · LAYOUT 2/378 · CONTENT 226/378 · BOTH 1/378`. Only the very first functions land
correctly; everything drifts by small (0x10–0x50) accumulating deltas.

## v42 progress (AppData reconciled → LAYOUT 39/378, BOTH 32/378)
Fixed AppData's two head divergences → the ENTIRE World scorer region + most of GameData snapped into place
(the cascade cancelled). Two changes in src/AppData/AppData.cpp, both CONTENT-neutral (still 14/14 exact):
1. **Removed `BEGIN_MESSAGE_MAP(AppWnd,...)` entirely.** It emitted a `GetMessageMap` COMDAT that the
   original AppData.obj does NOT have (all 9 real GetMessageMaps live at >0x408000; AppWnd's belongs to the
   class's primary TU elsewhere / was REF-dropped). Our copy emitted FIRST → shoved OnTimer off 0x401000,
   and (once moved to the tail) still added +0x10 that cascaded to every downstream TU. Deleting it makes
   OnTimer emit first at 0x401000 AND makes AppData the correct total length → World UpdateScore lands at
   0x401450 and the whole World scorer run + LoadStory*/SaveStory* head are LAYOUT-correct. The whole-image
   link (link_exe.sh) still resolves 0/0 — nothing referenced AppWnd::messageMap symbolically (the msgmap
   DATA @0x44b000 comes from the verbatim-copied .rsrc/.rdata, not our TUs). The `DECLARE_MESSAGE_MAP()` in
   the class decl stays (harmless — declares members, emits nothing).
2. (attempted, reverted) WorldgenZoneEntry ctor/dtor SOURCE swap to move ??_G before the ctor — did NOT
   work (put the dtor BODY at 0x401370 instead). See the two PARKED residuals below.

## ⭐ v42 KEY FINDING — G2 LAYOUT is GATED by per-function LENGTH, and the length divergences ARE the intrinsic reg-coloring wall (couples G2 to the 212-content ceiling)
Layout progresses in STRICT ADDRESS ORDER: a function that is N bytes longer/shorter than the original
shifts EVERY downstream function by N, so nothing downstream can LAYOUT-match until the upstream length is
reconciled. There are exactly TWO kinds of divergence, and only one is fixable:
1. **Emission-ORDER scrambles** — a function/COMDAT emitted in the wrong source position (AppData's
   GetMessageMap-at-head; Character::Read defined after GetFrameTile instead of after Init). FIXABLE by
   reordering the source, CONTENT-NEUTRAL (exact count unchanged). These are the clean G2 wins. When fixed,
   the affected region collapses from a local +/- scramble to a UNIFORM delta == the upstream shift, i.e.
   the TU becomes internally in-order and "prepared" to snap into place once upstream is resolved.
2. **LENGTH divergences from INTRINSIC register allocation** — a non-exact function whose bytes differ in
   COUNT, not just register names. Proven root cause on the SaveStoryHistory clone family (0x402670/9c0/d10,
   DIFF 611/858, +10 bytes each): OUR cl allocates one MORE callee-saved register — ours pushes ebx/esi/edi
   (3), the original pushes only esi/edi (2). Ours grabs EBX for the 2nd sprintf arm, which then FAILS to
   cross-jump the shared `[lea buf; inc; push; call]` tail the original shares (lesson #18) → a LONGER
   instruction stream on identical IR (the original is the more compact one). This is the lesson-#29
   ABI-pinned reg-coloring class, but here it changes LENGTH (not just same-length renames), so it SHIFTS
   layout. Prior sessions EXHAUSTED the source knobs (decl order/position ×4, ternary-of-calls, arms
   swapped, k++ placement, v-temp homing, loader-form phase sweep — see the in-source autopsy at
   src/GameData/GameData.cpp:207); NOT per-TU/source/flag steerable (#29/#30). These are hard walls.

**Consequence / corrected plan:** the byte-identical whole image (the ~100% goal) is bounded by the SAME
cl register-allocation wall as the 212 content ceiling — not only in bytes but in LAYOUT, because reg-count
differences change function lengths. The FIRST hard wall after AppData is GameData's SaveStoryHistory family
(+0x50 accumulated by GameData's end: SaveStory ×3 + StartGame/others). Everything downstream (Records —
now internally in-order at a uniform +0x50; Zone; Character helpers, all mostly exact/same-length) is
BLOCKED from absolute-address match by that upstream +0x50, even though those TUs are themselves clean.

**Therefore the productive G2 layout work is:** (a) reconcile every emission-ORDER scramble so each TU is
internally in-order ("prepared"); (b) accept that ABSOLUTE-address LAYOUT match caps at the first intrinsic
length divergence; (c) verify RELATIVE layout within same-length runs. Absolute whole-image byte-identity
requires cracking the central open problem (a different/period-correct cl.exe build, or the exact original
per-function source) — the standing wall. Do NOT grind the SaveStory/intrinsic-length funcs for layout;
they are the same park as content. DO keep fixing order-scrambles (cheap, correct, content-neutral).

### v43 — the length-wall ceiling, quantified (tools/g2_order.py)
`tools/g2_order.py` (needs build/*.obj + the Ghidra addr cache toolchain/test/orig_func_addrs.txt):
- `--walls` maps every function whose padded COMDAT length != the ORIGINAL's true slot (next Ghidra function
  addr − addr, so non-marker lib/EH gaps don't confound; non-16-aligned orig slots = EH-funclet/clone
  confound, auto-skipped). **~43 clean length walls**, ALL in non-exact functions (the reg-coloring class).
  First wall = SaveStoryHistoryNevada 0x402670 (+0x10) ⇒ absolute LAYOUT is achievable only for the **26
  markers before it**.
- `--scramble` lists per-TU emission-ORDER inversions (our obj COMDAT order vs orig address order) — the
  RELATIVE-layout metric. Reliable (unlike --walls, which has ±clone/funclet noise at TU boundaries).
  Currently in-order: World, GameData, Records, Iact, Canvas, IactScript, Dlg, WorldDoc. Effectively
  in-order: AppData (1 = the parked WorldgenZoneEntry ??_G quirk), GameView (1 = the deliberately
  end-placed InvScrollBar dtor, lesson #23). Real scrambles left: **Frame (3 handlers), App (3), Worldgen
  (3, the GameView-tail block)**.

**⭐ Why LAYOUT is 39 not 26 — the cumulative drift OSCILLATES.** The walls have MIXED signs (some funcs +N,
some −N; e.g. SaveStory +0x10 each but ReadZax2/3 −0x10, OnTimer −0x40, Tick +0x30…). The running cumulative
drift wanders around 0 and passes THROUGH 0 at several points (e.g. it returns to +0x0 at OnHScroll 0x418560),
so downstream functions COINCIDENTALLY re-align to their original address wherever cum==0. So absolute
LAYOUT == (# markers where cumulative length-drift == 0), NOT a clean prefix. This is why fixing the FIRST
wall would help but the true fix is eliminating ALL walls (cum stays 0 everywhere) — which needs the cl
reg-allocation wall cracked (lessons #29/#30). Fixing an emission-ORDER scramble does NOT change any wall
(order ≠ length); it only makes a TU internally correct so it snaps cleanly if/when the walls vanish.

**Realistic G2 completion bar (this is the achievable deliverable):** every app TU internally emission-order
correct (the --scramble metric → 0 real inversions) + a linkable/runnable image. ABSOLUTE whole-image
byte-identity is gated by the ~43 intrinsic length walls == the same cl reg-allocation wall as the 212
content ceiling; unobtainable without a period-correct/different cl.exe or exact original per-function source.

### AppData residuals (both self-correcting — do NOT cascade past 0x401450; ~5 local funcs)
- **CObject trio position (-0x30):** Disable/Enable/MapZone-ctor land 0x30 low because the original emits
  `CObject::Serialize/AssertValid/Dump` (0x401060/70/80) right after OnPaint (before Disable), whereas our
  obj emits them later with MapZone's vtable (after MapZone ctor + ??_GCObject/??1CObject). The trio is a
  CObject-vtable-default cluster; the original pulls it in early — almost certainly because the REAL AppWnd
  class had its vtable emitted here (full class w/ msgmap+DYNCREATE), which we deliberately don't model.
  Re-adding a msgmap brings back the GetMessageMap +0x10 problem (tension). The shift RE-CONVERGES at
  ??_GMapZone (0x401160). PARKED — costs 3 local funcs, self-corrects.
- **WorldgenZoneEntry ??_G-before-ctor:** original = ??_G@0x401370, ctor@0x401390, ~body@0x401400 (the
  scalar-deleting dtor COMDAT is detached from the dtor BODY and pulled early). Simple ctor/dtor source
  reorder does NOT reproduce it (moves the body, not ??_G). Compiler-internal emission quirk. PARKED.

## The layout model (two mechanisms, both proven v41)

**1. `/OPT:REF` eliminates unreferenced COMDATs.** link.exe's default (`/OPT:REF`) drops any COMDAT not
reachable from the entry graph. Our partial/loosely-wired image doesn't reference every function the way
the COMPLETE original does, so REF silently deletes ~19 markers (e.g. `AppWnd::Disable`/`Enable`/
`GetMessageMap`) → they vanish from the map and everything after shifts. `/OPT:NOREF` keeps all 378.
⇒ For layout work use NOREF. For the *final* image we must eventually match the original's exact kept-set
(either reproduce its reference graph so REF keeps the same functions, or confirm the original itself was
linked /OPT:NOREF — TBD; the demo's larger .text suggests it kept more).

**2. The linker lays out `.text` COMDATs in OBJ EMISSION ORDER (= source order), per obj in link order.**
PROVEN: our NOREF linked order for AppData and World is byte-for-byte the obj's COMDAT emission sequence
(`match.coff_functions` order). This is a REFINEMENT of lesson #28 — #28 said "don't reorder source to
chase addresses" because per-function CONTENT (compared at fixed marker addresses) is order-invariant;
but LINKED-image LAYOUT is fully determined by source/emission order. So:
> **linked address order == per-TU source order, concatenated in obj link order.**

⇒ Layout reproduction reduces to three matchable things:
  (a) the kept-COMDAT SET (mechanism #1),
  (b) each TU's source/emission ORDER == the original's source order,
  (c) each COMDAT's SIZE == the original's (mostly already true — most funcs are exact or same-length;
      the reg-coloring residuals are same-length, so they don't shift layout).
Fix an upstream divergence and everything downstream re-aligns (World is already in perfect order, merely
+0x10 shifted by AppData coming out 0x10 long).

## First divergence (AppData, 0x401000)
- ORIGINAL: `OnTimer@401000, OnPaint@401010, …gap…, Disable@401090, Enable@4010a0, MapZone ctor@4010b0`.
- OURS (NOREF): `GetMessageMap@401000, OnTimer@401010, OnPaint@401020, Disable@401070, Enable@401080,
  MapZone ctor@401090`.
Two source-order deltas to reconcile in AppData.cpp:
  1. We emit `GetMessageMap` FIRST (our BEGIN_MESSAGE_MAP / message-map macro sits before the handlers);
     the original has `OnTimer` at 401000, so its message-map COMDAT emits LATER. Move the message-map
     definition after the handlers (or wherever reproduces OnTimer-first).
  2. The original has a ~0x30 gap between OnPaint (ends ~401060) and Disable (401090) — i.e. the original
     emits something between OnPaint and Disable that we don't (or GetMessageMap lands there). Determine
     from Ghidra what occupies 0x401060–0x401090 in the original and match it.
Then AppData's tail (MapZone/InvItem/WorldgenZoneEntry + the folded CObject defaults) must match the
original's fold/emit set. Once AppData's total size == original, World (already in-order) layout-matches
en masse, and the diff cascades forward TU by TU.

## Worklist (do in address order — fixing TU N re-aligns N+1…)
1. **AppData** — reconcile the two source-order deltas above + the CObject-default fold set. Target:
   AppData region 0x401000–0x401450 all LAYOUT-match.
2. **World, GameData, …** — after each upstream fix, re-run g2_link.sh + g2_diff.py; the delta column
   shows the next divergence. Expect long runs to snap into place at once (they're already in-order).
3. **COMDAT fold-vs-survive geography** (the documented open question): CObject::Serialize/AssertValid/
   Dump fold to one copy (0x401060/70/80 in the original); the CException/CFileException dtor family
   survives per-TU at each TU head; ??_GCPalette folds from WorldDoc. Our TUs over-emit some GDI dtors
   + under-emit others — reconcile against the binary's actual function list.
4. **Non-.text**: .rdata (vtables/msgmaps/string pools), .data, .rsrc (already copied verbatim by
   extract_res.py). Then the linker section-group order (.text$AFX_* subsections — see the map's section
   table) for the lib region.
5. **Plumbing**: EH funclets (0x405320, 0x408c2a CxxFrameHandler thunk, 0x4161bd/…/424f69), the 0x424fb0
   bare-jmp thunk, static-init/atexit, PE timestamp + checksum mask.
6. **Final**: reccmp-style whole-image diff → progress.py toward image-level ~100%.

⚠ G2 does NOT change the per-function CONTENT deltas (the reg-coloring residuals are compile-time and
frozen — lesson #29). G2's product is the byte-identical IMAGE + a runnable/verifiable artifact, carrying
the known ~48 reg-coloring .text deltas until/unless the central open problem (a different cl.exe) is cracked.

## v44 progress — emission-order scrambles closed (App + Worldgen tail) + the /OPT question SETTLED
1. **App TU reordered to original emission order** (src/App/App.cpp, CONTENT-neutral, 212 stands):
   msgmap(CTheApp) → ctor → `theApp` global → InitInstance → OnIdle → LogWrite → CAboutDlg ctor →
   DoDataExchange → msgmap(CAboutDlg) → OnAppAbout → OnInitDialog. `g2_order.py --scramble` → **App: in-order**.
2. **Worldgen GameView-tail reordered to ascending address** (src/Worldgen/Worldgen.cpp, CONTENT-neutral):
   the tail block is now UseWeapon(0x427d20) → DetonateAdjacentTiles(0x428680) → OnCmdMinimize →
   DrawWeaponBox → DrawWeaponIcon → BlitViewportDither → PreCreateWindow → AddItemToInv →
   RemoveItem(0x429150). Was 4 inversions → **1** (only the compiler-placed `??_GCProgressCtrl` left).
3. **The remaining 3 scrambles are ALL compiler-placed library COMDATs — BENIGN / not source-steerable:**
   - AppData: `??_GWorldgenZoneEntry` before its ctor (v42 PARKED — self-corrects at ??_GMapZone).
   - GameView: 4 lib-COMDAT interleavings (CObject::operator delete / CScrollBar dtor / CBitmapButton
     dtor / TriPoint ctor) + the end-placed `InvScrollBar` dtor (lesson #23 free-lunch placement).
   - Worldgen: `??_GCProgressCtrl` (0x425e10) emits at FIRST odr-use of CProgressCtrl's deleting dtor
     = LoadWorld's local `CProgressCtrl progress;` (Worldgen.cpp:3999). The original emits it ~0x425e10
     (after OnLoadWorld), i.e. its first odr-use was a LATER function — moving it is a CONTENT change
     (which function instantiates the progress bar), not a reorder. Left as-is.
   ⇒ **All STEERABLE emission-order scrambles are now closed** (the "every app TU internally in-order"
   milestone, modulo compiler-placed lib COMDATs). LAYOUT stays 39/378 because these functions are all
   DOWNSTREAM of the first length wall (GameData SaveStoryHistoryNevada 0x402670) — cumulative drift
   already displaced them, so fixing relative order is a health/correctness win, not an absolute-address
   one until the walls crack (= the reg-alloc ceiling, same as 212 content).

## v47 — .rdata vtable-content oracle (tools/vtcheck.py); World + GameView vtables VALIDATED
Non-.text CONTENT verification (absolute .rdata LAYOUT stays wall-blocked — everything shifts after the first
.text length wall — but content correctness is layout-independent and catches real bugs). Built `tools/vtcheck.py`:
reads the ORIGINAL vtable from the exe (VA→file offset) + OUR vtable from `build/<TU>.obj` (the `??_7<Class>@@6B@`
data COMDAT + its relocations → per-slot target symbol), and checks the two override the SAME slots with the SAME
class methods. The data-side complement to bugscan.py — a MISSING override = a virtual we forgot to declare (the
base runs — a runtime bug), exactly the class of `World::IsModified` before v46. Result: **World 8/8 and GameView
8/8 override slots match — CLEAN.** (v45/v46's .rdata work confirmed correct.)
- **World dtor slot (+0x04) `??_E` vs orig `??_G` — BENIGN (ICF).** Our vtable references `??_EWorld` (vector
  deleting dtor) where the original slot → 0x41b2d0 (`??_G` scalar, disasm-confirmed `call ~World; test flag;
  delete`). But `??_EWorld` and `??_GWorld` link to the SAME address (0x41b350) under BOTH /OPT:REF and NOREF —
  identical bodies for a never-array-allocated class, folded — so the slot resolves to the same scalar dtor. No fix.
- **Two tool gotchas baked in:** (a) ICF folds trivial BASE methods to app-region addresses that masquerade as
  overrides — the FOLDED_BASE set (0x401060/70/80 CObject defaults, 0x401090/a0 DisableSelfWindow/Enable,
  0x40e3f0 CView no-op) is whitelisted as base. (b) BOUND the compare to OUR vtable's real slot extent (max reloc
  offset) — reading past the vtable end walks into adjacent .rdata (World +0xbc→??_GCPalette 0x41e8b0, GameView
  +0x114/+0x12c→the embedded Balloon sub-vtables) and mis-reports MISSING overrides.

## v45 — the REF-DROP oracle + World document message map reconstructed (closes 15 of 22 gaps)
The concrete way to find "which functions the original references that our partial call-graph doesn't":
link the SAME obj set twice (`/OPT:REF` and `/OPT:NOREF`, each with `/MAP`), diff the app-obj symbol sets
(Lib:Object ∈ our 13 objs); `NOREF − REF` = the functions REF drops as unreferenced. In the complete
original these ARE referenced, so each is a real gap in our reproduced reference graph. v45 baseline: **22
dropped**, of which **19 were the World document's command/update-UI handlers** — because our
`BEGIN_MESSAGE_MAP(World, CDocument)` was an EMPTY TODO stub, so nothing referenced the handlers.

**Reconstructed the map byte-for-byte from the binary.** GetMessageMap (0x419f50) returns `&messageMap`
@0x44c2c8 = `AFX_MSGMAP{ pBaseMap=CDocument::messageMap@0x44d108, lpEntries=0x44c2d0 }`; the entries are a
24-byte `AFX_MSGMAP_ENTRY{UINT nMessage,nCode,nID,nLastID,nSig; AFX_PMSG pfn}` array (14 entries + a zeroed
terminator). Read the fixed fields, matched each `pfn` to a handler by address (7 in Worldgen @0x424xxx, 7 in
GameData @0x403xxx — both are World-TU source files), recovered the macro list (ON_COMMAND nCode=0 nSig=0x0c;
ON_UPDATE_COMMAND_UI nCode=-1 nSig=0x2c). Wrote the 14 macros in array order into WorldDoc.cpp + declared the
handlers `afx_msg` in WorldDoc.h's World class (address-only refs, no call sites ⇒ codegen-inert for the WorldDoc
functions). Result: **all 14 entries' fixed fields byte-match** the binary's `@0x44c2d0`; REF-dropped 22→7.
- **COST (accepted): `~World` 0x41b2f0 PHASE-DISPLACED, DIFF(6) align=0 (212→211 exact).** The original
  WorldDoc.obj was compiled WITH this map, so the map is the TRUE TU context; adding it re-rotates ~World's
  intrinsic esi/edi symmetric register 2-cycle (a KNOWN oscillator on this function). Source proven correct
  (align=0, 405/405 insns) — the lesson-#29 reg-coloring ceiling, not a miss. Net for the IMAGE is positive:
  a byte-exact `.rdata` msgmap array + 15 `.text` functions the original keeps (were REF-dropped) vs one
  6-byte intrinsic .text displacement. The msgmap is FUNCTIONALLY ESSENTIAL (dispatches File>New/Save/Load/
  Replay + sound/music toggles + their menu enable states) — a faithful decomp must have it.
### v46 — World vtable overrides close 2 more (REF-dropped 7→5, ZERO content regression)
`World::IsModified`/`SetModifiedFlag` were dropped because WorldDoc.h's World class (the `IMPLEMENT_DYNCREATE`
TU that emits the vtable) didn't declare them `virtual` — so the emitted World vtable pointed at CDocument's
base versions and our overrides went unreferenced. Added `virtual BOOL IsModified();` +
`virtual void SetModifiedFlag(BOOL=TRUE);` to WorldDoc.h → the vtable slots now target OUR overrides →
kept under REF. UNLIKE the v45 message map this was **codegen-neutral (211 held)**: overriding EXISTING base
slots changes only the vtable DATA's slot targets (relocations), not slot ORDER / sizeof / any function body.

**5 REF-drops REMAIN — the genuinely-hard tail (each risks a regression for ≤2 functions; left, documented):**
- `AppWnd::OnPaint`(0x401010)/`OnTimer`(0x401000) — AppWnd HAS a real message map (entries @0x44b008:
  `ON_WM_TIMER`→OnTimer sig 0x0d, `ON_WM_PAINT`→OnPaint sig 0x0c) but v42 PROVED the original AppData.obj
  emits NO GetMessageMap (all 9 real ones are >0x408000) ⇒ AppWnd's map lives in a DIFFERENT TU in the
  original. Putting `BEGIN_MESSAGE_MAP(AppWnd,CWnd)` in AppData.obj (anywhere) adds a COMDAT the original
  lacks → re-breaks the v42 layout cascade. Correct fix = emit AppWnd's map in its true (unidentified) TU.
- `AppWnd::Disable`(0x401090=`DisableSelfWindow`, `EnableWindow(m_hWnd,FALSE)`)/`Enable`(0x4010a0) — the 12-byte
  bodies are ICF-FOLDED with trivial MFC vtable methods (0x401090 has 19 vtable DATA xrefs in the original);
  they survive there via the fold + a game call site we don't reproduce. Needs the fold set + the caller.
- `??_H@YGXPAXIHP6EX0@Z@Z` = `__vector_constructor_iterator` CRT helper — emitted at an array-of-objects-with
  -ctor site (`new T[n]`); find the original odr-use (a needle-hunt).

### ⭐ v44 — /OPT:REF vs /OPT:NOREF SETTLED: the original is a `/OPT:REF` build.
Measured `.text` vsize of three links of the SAME build/*.obj set (from repo root, +wavmix32.lib):

| build | .text vsize | vs original |
|---|---|---|
| **YodaDemo.exe (original)** | 0x49e7c (302,716 B) | — |
| our **/OPT:REF** | 0x48ac7 (297,671 B) | **−5,045 B (−1.7 %)** |
| our /OPT:NOREF | 0x57df7 (359,927 B) | +57,211 B (+19 %) |

NOREF keeps ~57 KB the original DROPPED ⇒ the original is NOT NOREF. REF lands within 1.7 % of the
original (the release/non-`/DEBUG` default for link 3.10 is transitive COMDAT elimination = `/OPT:REF`).
This OVERTURNS the v43-pickup guess ("demo .text 454K vs our 446K hints NOREF-like") — those were
full-FILE sizes distorted by the verbatim-copied `.rsrc`; the `.text` comparison points the opposite way.
- **Consequence for the FINAL image:** target `/OPT:REF`. `g2_link.sh`'s NOREF is only a layout-ANALYSIS
  scaffold (keeps our transcribed-but-not-yet-fully-cross-referenced functions visible in the map). The
  byte-identical image needs REF **+** a complete reference graph.
- **The −5 KB REF gap = we slightly UNDER-reference:** a few real MFC/GDI COMDATs the complete original
  odr-used that our stubbed/partial source doesn't (the documented ??_GCPalette-style under-emit, plus
  small reg-coloring length deltas net-negative). Closing it is the COMDAT fold-vs-survive geography work
  (worklist #3), not a linker-flag change.
