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
