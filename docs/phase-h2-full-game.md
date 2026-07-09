# Phase H2 — demo → full Yoda Stories (`YODA_VARIANT=FULL`)

Turn the byte-matched **demo** decompilation into a build that plays the full retail Yoda Stories,
gated entirely behind `#ifdef YODA_FULL` so the default (demo) config stays the byte-exact 211 anchor.

Build & run:
```sh
cmake -B build-full -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_VARIANT=FULL
cmake --build build-full
./run_full.sh                 # runs build-full/yoda.exe against YodaFull/YODESK.DTA under wine
```

## The demo is the full engine + a few forced overrides
The demo `YodaDemo.dta` (4.59 MB) is essentially the full `YODESK.DTA` (4.60 MB) with a demo STUP
(title) graphic and non-Hoth planet tiles blacked out — so the restriction lives in the **exe**, as a
handful of hardcodes that clobber otherwise-full logic. H2 loads the full data and reverts each
override under `#ifdef YODA_FULL`.

References used: retail `Yoda Stories/Yodesk.exe` (loaded as a 2nd Ghidra program) + full
`~/workspace/DesktopAdventures/YODESK.DTA`. The retail engine is a near-identical version, so its
functions diff cleanly against our demo source. (DesktopAdventures' own `world_generate()` is an empty
stub — not a worldgen reference.)

## The 4 worldgen-blocking demo restrictions (fixed; verified vs retail)
Each guard's fall-through (no macro) is the exact demo/anchor code.

| # | Where | Demo | Full (`YODA_FULL`) | Retail proof |
|---|---|---|---|---|
| 1 | `Deskcpp.cpp` InitInstance | data file `YODADEMO.DTA` | `YODESK.DTA` | — |
| 2 | `DeskcppDoc.cpp` ctor | `currentPlanet=2; worldSize=1;` | keep registry/rotation values | ctor reads Terrain/WorldSize then demo clobbers |
| 3 | `Worldgen.cpp` `LoadWorld` (~4013) | `currentPlanet=2; WriteProfileInt("Terrain",2)` | keep rotated planet; persist it | retail `FUN_004248a0` runs the rotation switch and writes the **computed** Terrain, no `=2` |
| 4 | `Worldgen.cpp` `Generate` goal region (~2766) | goal = const `0x6c` | `goal = nRequestedGoalItem>=0 ? nRequestedGoalItem : WorldgenSelectPuzzle(-1,-1,9999)`; `if (goal<0) return 0` (retry) | retail `Generate` `FUN_00422210` does exactly this; the demo replaced the whole selection with `0x6c` |

**#3 is the operative Hoth-forcer** — `LoadWorld` re-forces the planet right before worldgen, so fixing
only the ctor (#2) still yields Hoth. Both must be guarded.

Effect: `Generate` (retried by `LoadWorld`'s `do…while(nGenerated==0)`) now succeeds for any planet, so
the world generates instead of the demo hanging at the end of the loading screen when a non-Hoth planet
was selected against the `0x6c` (Hoth) goal.

## Verified NOT demo restrictions (source comments were misreads — retail is identical)
Confirmed by decompiling the retail twins; left as-is (and one stale comment corrected in-source):
- **Per-planet goal-id whitelist** in `WorldgenSelectPuzzle` (Nevada `0x55/0x73/0xb9/0xc7/0xc9`, Hoth
  `0x67/0x6c/0x87/0xbd/0xc5`, Endor `0x83-0x86/0xc6`) — retail `FUN_00421360` (9999 arm) has the exact
  switch. These are the real full-game mission-puzzle ids per planet.
- **`ReadZone`** per-planet filter `if (currentPlanet==nPlanet || bForce)` — `bForce` is the shared
  zone set (intro/victory/loss/goal: `0x4c,0x4d,0x5d-0x60,0x10a-0x10f,0x1d7,0x217,0x282`). Standard
  per-planet loading; works for any planet once `currentPlanet` is right.
- **`Populate`** goal-zone selection (`rand()%4`, `goalItemTileId==0x84→3` Endor, `==0xbd→4` Hoth) —
  multi-planet logic.
- **Victory/loss zones 76/77** (`GetVictoryZoneIndexMaybe`/`GetLossZoneMaybe`) = `0x4c`/`0x4d`, both in
  `ReadZone`'s force-load set ⇒ shared zones at fixed indices for every planet.

## Cosmetic demo gates (fixed; non-blocking)
`Save/Load World` + `Replay Story` (`DemoDisable` in `WorldgenHelpers.cpp`) and `World Size` + `Stats`
(`DeskcppView.cpp`) are force-grayed in the demo; enabled under `YODA_FULL`.

## Status / open
- ✅ Loads `YODESK.DTA`, worldgen succeeds, no stall; Hoth verified playable (user).
- ⏳ Multi-planet (Nevada/Endor) rotation is wired (fix #3) — pending visual confirmation that non-Hoth
  worlds generate + play.
- Not yet audited for full-game correctness (endgame/replay/save): the full save/load path and any
  further planet-specific behavior. These are follow-ups; none block generation/initial play.
