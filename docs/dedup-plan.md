# Struct de-duplication plan (integration / Phase G2)

Goal: collapse the per-TU stub copies of each struct into ONE canonical declaration per
struct (mirroring the original project's shared headers), so every TU `#include`s the same
definition. This is genuinely **G2 integration work** — it rotates the codegen dial of every
affected TU (the member-decl set IS the dial, CLAUDE.md lesson #8), so currently-exact
functions will flip to PHASE-DISPLACED until *all* headers are simultaneously canonical (the
true fixed point). The user has accepted this churn **provided each flipped function's
in-source annotation is updated** to reflect its new state.

## Current duplication map (surveyed 2026-07-07 v27)

| struct | canonical target | duplicate copies (header : TUs that include it) |
|---|---|---|
| **World** (0x33c0) | `Worldgen/Worldgen.h` (the "emerging real CDeskcppDoc") | `WorldDoc.h`:WorldDoc · `GameData/WorldStub.h`:GameData,Iact · `Worldgen.h`:GameView,World,Worldgen |
| **MapZone** (0x34) | `Worldgen.h` (vptr-true) | `WorldStub.h` · `WorldDoc.h` · `Worldgen.h` |
| **Canvas** (0x43c) | `Canvas/Canvas.h` (owns the methods) | `Canvas.h`:Canvas · `GameView.h`:GameView,World,Worldgen · `WorldStub.h`:GameData,Iact · `WorldDoc.h`:WorldDoc |
| **GameView** (0x310) | `GameView/GameView.h` (real, full) | `GameView.h` (real) · `WorldStub.h` (stub):GameData,Iact · fwd-decl in `Records.h` |
| **InvItem** (0xc) | `GameView.h` | `GameView.h` · `WorldStub.h` |
| Records (Tile/ZoneObj/Zone/Character/Puzzle/MapEntity) | `Records/RecordClasses.h` | `RecordClasses.h`:Iact · `Records.h` (own stubs):Records |
| **CDC** (stub) | — see obstacle #2 | `Canvas.h` (`struct CDC{void*;HDC;}`) vs real MFC `CDC` everywhere else |

Include graph (2026-07-07): WorldStub.h←{GameData,Iact}; Worldgen.h(→GameView.h)←{GameView,
World,Worldgen}; WorldDoc.h←WorldDoc; RecordClasses.h←Iact; Records.h←Records; Dlg.h←{Dlg,
GameView}; Canvas.h←Canvas; App.h←App (also pulled via Worldgen.h); Frame.h←Frame.

## The two real obstacles (why this is not a mechanical rename)

1. **Field-name + granularity divergence.** Each TU named the same struct's fields
   independently. Proven for World (WorldDoc→Worldgen attempt, v27): 102 compile errors, e.g.
   WorldDoc.cpp uses `rectViewport`/`rectInventory`/`rectRightPane` where Worldgen.h has
   `rectUnk3274`/`rectUnk3284`/`rectInvScrollMaybe`; and WorldDoc.cpp uses **four ints**
   `nWeaponBoxLeft/Top/Right/Bottom`@0x32a4 where Worldgen.h has **one RECT** `rectWeaponBox`.
   The latter is a *granularity* mismatch — `nWeaponBoxLeft` must become `rectWeaponBox.left`,
   not a simple rename. Also `bWorldReady`↔`bWorldReadyMaybe`, `bHidePlayer`↔`bHidePlayerMaybe`,
   `unk2e44`↔`bWeaponHitPendingMaybe`, etc. Reconciling needs per-field human judgment by
   OFFSET (both headers carry `// +0xNNNN` comments — build the map from those), keeping the
   most-informative name and updating the *other* TU's .cpp to it.
2. **Mixed MFC / stub environments.** `Canvas.cpp` deliberately compiles against a minimal
   `windows.h` + a 2-field stub `struct CDC { void* _vfptr; HDC m_hDC; }` (NO afxwin). Every
   other TU uses full MFC's real `CDC`. Unifying `Canvas` forces `Canvas.cpp` into the MFC
   environment (rotates its dial, and MFC's `CDC` is larger/virtual) OR forces the stub `CDC`
   into an MFC TU (redefinition conflict). Resolve the CDC representation FIRST, then Canvas.

## Progress (2026-07-07 session)

- **Step 1 Records: was already done** (Records.h includes RecordClasses.h; its GameView/World
  stubs are step-5/6 scope — the World one remains).
- **Step 2 MapZone: DONE** (canonical src/Worldgen/MapZone.h). WorldStub's shifted variant was
  compensated by zones@0x4b4 + a 121-int pointer array swallowing cell 0's vptr; the real grid
  is 0x4b0. ⭐ Fixed StartGame 0x4037a0 DIFF(254)→DIFF(79) — the shifted stub's off-by-4
  displacements were silently poisoning every cell store. Commit 60ac1c8^..
- **Steps 3+4 CDC+Canvas: DONE** (canonical src/Canvas/Canvas.h: full fields, .text decl
  order, 0x407df0=ctor / 0x407eb0=dtor; `class CDC;` fwd only — the 2-field CDC stub is
  PRIVATE to Canvas.cpp, MFC TUs see the real CDC). Canvas 8/11→9/11 (Clear EXACT);
  Worldgen +4. Stale dtor addresses 0x408010/0x408400 in the old copies were comment errors.
- **Step 5 GameView+InvItem: DONE** (WorldStub stubs retired; GameData/Iact include the real
  GameView.h + afxext.h). Stub lies fixed: OnWalk=ZoneTransitionStep(short,short),
  PlayerMove=PlaySound, bSuppressWalkSound=bWeaponIactActiveMaybe. Dial cost GameData −3 /
  Iact −1 (small same-length tie-breaks, annotated, G1).
- **Step 6 World: REMAINING** — the gating field-reconciliation job (~102 errors' worth,
  granularity mismatches like nWeaponBoxLeft→rectWeaponBox.left). Also retires Records.h's
  World stub. Do as its own session; net-positive on the dial per the v27 dry-run.

## Recommended order (least-entangled first)

1. **Records** — `Records.h`'s local stubs → include `RecordClasses.h`. Records TU only;
   verify Records + Iact hold. (Records.h has `GameView`/`World`/`Character` stubs.)
2. **MapZone** — pick `Worldgen.h`'s vptr-true copy canonical; reconcile WorldDoc.h/WorldStub.h
   names by offset. Affects WorldDoc + GameData/Iact.
3. **CDC** — decide the canonical CDC (keep the 2-field stub as a shared `mfc_stubs.h`, or move
   Canvas.cpp to MFC). Prereq for Canvas.
4. **Canvas** — `Canvas.h` canonical (union of methods; reconcile the `Init` method vs
   `Canvas(int,int)` ctor modeling of 0x407df0 — it is really the ctor). Update GameView.h/
   WorldStub.h/WorldDoc.h to include it.
5. **GameView** + **InvItem** — `WorldStub.h`'s stubs → real `GameView.h`. Pulls GameView.h's
   dep tree into GameData/Iact.
6. **World** + doc surface — make `Worldgen.h` the single canonical World. Merge the 7 doc-only
   decls from WorldDoc.h (`World()`, `~World()`, `FindAdjacentGateDirMaybe`, `OnNewDocument`,
   `OnOpenDocument`, `DECLARE_DYNCREATE`, `DECLARE_MESSAGE_MAP` — all vtable-safe: overrides at
   inherited CDocument slots + non-virtuals). Reconcile field names/granularity (obstacle #1).
   Point WorldDoc.h / WorldStub.h at Worldgen.h.

### v27 dry-run result (reverted, informative)
Adding the 7 doc decls to Worldgen.h's World compiled clean across all its TUs and moved the
dial: **GameView 55→57 exact (+2)**, **Worldgen 33→32 (−1)**, World unchanged. So the World
merge is *net-positive* on the dial even before field reconciliation — but WorldDoc.cpp needs
the 102 field fixes before it can switch to Worldgen.h. That reconciliation is the gating work.

## Per-step protocol (MANDATORY — user directive)
1. Record the exact-count baseline of EVERY TU that includes the touched header (v27 baseline
   below).
2. Make the canonical header change; reconcile field names/granularity by offset until all
   affected .cpp files compile.
3. Re-verify every affected TU; `comm`-diff the MATCH lists before/after to get the exact list
   of functions that flipped OUT (exact→PHASE-DISPLACED) and IN.
4. **Update each flipped-OUT function's in-source annotation** to say it is PHASE-DISPLACED by
   the de-dup (name the struct), so comments stay accurate. Flipped-IN functions: drop stale
   EFFECTIVE notes.
5. Commit per struct with the flip list in the message.

## Baseline exact counts (2026-07-07 v27, for flip detection)
App 11/12 · Canvas 8/11 · Dlg 5/5 · Frame 14/18 · GameData 13/27 · GameView 55/100 ·
Iact 2/10 · IactScript 11/12 · Records 24/33 · World 6/8 · WorldDoc 7/13 · Worldgen 33/90.
Total = 189 exact functions.
