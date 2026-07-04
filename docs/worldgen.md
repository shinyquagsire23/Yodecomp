# YodaDemo.exe — world generation & `.wld` save/load

Lives in the big `0x41c340–0x429000` `.obj` (one monolithic TU that also holds the `.DTA` loader
`Dta_*`). Kicked off from `Dta_Load` when it reads the **`ENDF`** tag: asset load first, then generate.

## Pipeline
The **generator megafunction is `Worldgen_Generate` (0x41f960, ~6.6 KB)** — 16× `_rand`, writes the
10×10 grid (`World+0x4b4`), and drives a large helper subtree (`0x41c3b0–0x421930`, the bulk of the
worldgen code, which sits *before* the `.DTA` loader in the TU). `Worldgen_Randomize`/`Populate` below
are the RNG seed + a placement stage in that flow.
```
ENDF (in Dta_Load)  /  new-game trigger (GameData_FUN_004037a0)
  → Worldgen_Generate (0x41f960)    THE generator: build the quest into the 10×10 grid (rand-driven)
  → Worldgen_Randomize (0x424380)   seed RNG: GetCursorPos + time() + clock() → srand (0x42a640),
                                     then packs rand() bytes into the world seed
  → Worldgen_Populate (0x425e30)    lay the quest into the 10×10 world grid (World+0x4b4)
       ├ Worldgen_SetupGrid (0x4269a0)   initialize the grid (World+0x4b4, MapZone[100])
       ├ Worldgen_PlaceZone (0x4260e0)   place a zone's content by id (e.g. 0x5d/0x5e/0x60) + tile;
       │                                 the demo hardcodes its shipped zones (branch on doc+0x2e50)
       └ Worldgen_PlacePuzzle (0x421620) place puzzle zones (the item-for-item / "Find Puzzle" chain;
                                         allocates puzzle records; logs "!!!!No Place to put Find Puzzle!!!"
                                         when the grid can't fit one)
```
The **demo** ships a fixed subset of zones, so `Worldgen_Populate` largely does hardcoded placement
(explicit zone ids written into the grid); the full game's generator randomizes the quest graph.

## Save / load
- **`Wld_Serialize` (0x424fc0)** — `.wld` serialization. Reads/writes the **`ASAV44`** signature via
  `CFile`, `SetSize`s + `operator_new`s the world structures on load. Format: `*.wld` (`World Files
  (*.wld)`), sig `ASAV44`. (MFC `CDocument::Serialize`-style; the load branch rebuilds the doc.)

## Untangling Dta / Worldgen / Wld (call-graph clustering, 2026-07-04)
The big `0x41c340–0x429000` TU (+ the `0x41bee0` helper class) mixes three concerns that share the
`World` doc. `Dta_Load` calls `Worldgen` on `ENDF`, so naive reachability from the loader swallows
everything — the fix is to anchor **DTA on the chunk parsers** (not `Dta_Load`), Worldgen on
`Worldgen_*`, WLD on `Wld_Serialize`, and take **exclusive** reachability:
- **`Worldgen_`** — generation-exclusive: the `Worldgen_*` entry points + helpers `0x421460`, `0x421e50`,
  `0x426690` (were mis-tagged `Dta_`).
- **`Wld_`** — save/load-exclusive: `Wld_Serialize` (0x424fc0) + the `0x41bee0` helper class
  (`0x41bee0–0x41c340`, serializes world objects) + `0x41c340` (were mis-tagged `Worldgen_`/`Dta_`).
- **`Dta_`** — the loader (`Dta_Load`) + chunk parsers + shared record/zone readers (the common data
  layer used by *both* load and gen — genuinely shared, kept `Dta_`).

## Notes
- The generator + `Dta_*` loader share the `.obj`, so both operate on the same `World` doc; the
  worldgen functions above are now typed `World*`. Many remaining `Dta_FUN_*` in this range are
  generation helpers (grid math, zone/item placement) — named incrementally as their role clarifies.
- RNG: `srand`=0x42a640, `rand`=0x42a650 (static CRT). The seed mixes cursor pos + wall clock, so each
  play is different (matching Yoda Stories' "random galaxy each game").
