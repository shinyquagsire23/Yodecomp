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
