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

> **Naming (2026-07-05):** all `Worldgen_*` are now **`World::`** methods (`World::Generate/Randomize/
> Populate/SetupGrid/PlaceZone/PlacePuzzle/Serialize`) — the whole doc TU is the `World` (`CDeskcppDoc`)
> namespace so `this` types as `World*`. The `Worldgen_` names below are the old flat form.

**Backup / restore of the placed records (2026-07-05).** `World::BackupRecords` (0x426690) and
`World::RestoreRecords` (0x426380) are an inverse copy pair moving a 3-sub-record block between the
**active** copy at `World+0xda4` and a **backup** at `World+0x2d54`, re-writing the zone-content type
markers (`0x5d/0x5e/0x5f`, same tags `World::PlaceZone` writes). `World::Populate` initializes the active
copy. This mirrors `World::RestoreGridFromBackup` (0x421520, whole 10×10 grid zones[100..199]→[0..99]) —
the engine keeps saved copies so an adventure can be replayed/reset. Exact record identity still TBD
(the 3 sub-records sit at grid-stride offsets; `0x2d54` = a region just past the 200-entry grid span).

## Save / load
- **`Wld_Serialize` (0x424fc0)** — `.wld` serialization. Reads/writes the **`ASAV44`** signature via
  `CFile`, `SetSize`s + `operator_new`s the world structures on load. Format: `*.wld` (`World Files
  (*.wld)`), sig `ASAV44`. (MFC `CDocument::Serialize`-style; the load branch rebuilds the doc.)
- **Sparse save (confirmed by the author, 2026-07-05):** a `.wld` only stores the maps/zones that have
  been **loaded AND modified** — not the whole world — to keep saves small. This is *why* the engine
  keeps the dual copies: the **backup** grid (`zones[100..199]`) + the backup record block at
  `World+0x2d54` are the **as-loaded reference**, and the **active** copies (`zones[0..99]`, `World+0xda4`)
  are the live/mutated state. On save, `Wld_Serialize` walks the active zones and serializes only those
  that (a) were actually loaded (`exists`/loaded flag set) and (b) differ from their backup — i.e. the
  player changed them. So `World::BackupRecords`/`RestoreRecords` + `RestoreGridFromBackup` aren't just
  replay/reset: the backup is the baseline the save-diff is taken against. **Confirmed 2026-07-05:** `GameView::TransitionZone` (0x40e7c0) drives it — entering a zone via a door calls `World::RestoreRecords` (restore that zone's saved state), leaving calls `World::BackupRecords` (snapshot the current zone), so per-zone state is captured on every transition. (**TODO:** find the exact
  per-zone loaded/dirty flag `Wld_Serialize` tests, and confirm the diff vs. a plain dirty-bit.)

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

## World-namespace functions mapped (2026-07-05, autonomous grind)
Named ~35 previously-`FUN_*` functions in the `World` (`CDeskcppDoc`) namespace (server-side callee
analysis; HTTP decompile was too flaky). All names carry `Maybe` where the role is clear but the exact
content/direction isn't yet pinned:
- **`World::LoadWorldMaybe`** (0x421fd0) — world load/build: reads the `Terrain` option, opens the data
  via `CFile`, and hard-calls the DTA parsers (`ParseZone/Zaux/Zax2/Zax3/Actn/Htsp`), sets `nFrameMode`.
- **Worldgen placement family** — `WorldgenPlaceRandInZone{,2..7}Maybe` (0x41c580/730/cf10/d260/d480/
  e920/f120): `GetZoneById` → gather candidate spots from the zone's worldgen scratch-lists (CDWordArrays
  at `zone+0x810/+0x824`) → `_rand`-pick one to place generated content. **Orchestrators** call these:
  `WorldgenPopulateZoneMaybe` (0x41c8f0), `WorldgenPlaceChain{,2}Maybe`, `WorldgenPlaceItemMaybe`,
  `WorldgenPlacePuzzlesMaybe` (0x421930, calls `PlacePuzzle`), `WorldgenBuildQuestMaybe` (0x41d940, 2 KB).
  Array helpers: `Worldgen{Insert,Add}ZoneEntryMaybe`, `Remove{,2}ZoneEntryMaybe`, `BuildZoneListsMaybe`.
- **`World::BackupZoneGrid`** (0x421460) — active grid (`+0x4b4`) → backup (`+0x1904`); counterpart to
  `RestoreGridFromBackup`; snapshots a zone on transition (sparse-save mechanism).
- **Save/load + doc**: `LoadOrSaveWorldMaybe`/`SaveWorldMaybe` (0x423850/b30, `CFile`+`UpdateAllViews`+
  error box), `SerializeZone{,Objects}Maybe`, `DrawTileToCanvasMaybe` (0x423df0), `GetCanvasDataMaybe`.
- **Queries**: `FindZoneCellById`, `ZoneHasItemOrDoorMaybe`, `FindObjectByIdMaybe`, `FindValueInListMaybe`,
  `FindSpecialZoneMaybe`, `CheckZoneObjectsMaybe`; plus `ResetGameStateMaybe` (0x4037a0, new-game health/
  weapon reset). Remaining `World::FUN_*` are all <0x40 funclets/stubs.


## Planet tables, seed & gen scratch fields (2026-07-05, World-struct grind)
`World::Generate` (0x41f960) persists a per-planet obfuscated word list to the registry and uses several
newly-named World fields:
- **`World.worldSeed@0x2cc`** — the RNG seed. `Generate(seed)` stores it here and calls `_srand(seed)`;
  `SavePlanetTable*` writes it as the decimal prefix of each saved registry line.
- **`World.bHasGeneratedWorldMaybe@0x6c`** — gate flag. When `<0`, `Generate` first *loads* the current
  planet's persisted table; it's set to `-1` at the end of a successful generate (so regenerations reload).
- **Per-planet persisted tables** (embedded `CWordArray`), selected by `World.currentPlanet@0x2e3c` (1/2/3):
  `planetTable1Maybe@0x284` (registry section `GameData`, keys `Nevada0..N`), `planetTable2Maybe@0x298`
  (`Alaska0..N`), `planetTable3Maybe@0x2ac` (`Oregon0..N`). Load = `GameData::LoadPlanetTable{1,2,3}Maybe`
  (0x401ac0/401ea0/402280); Save = `GameData::SavePlanetTable{1,2,3}Maybe` (0x402670/4029c0/402d10).
  Values are obfuscated with a per-save additive key (`rand()%255+1`) added on save, subtracted on load.
  (Nevada/Alaska/Oregon are just the dev codenames = registry keys for the 3 demo planets.)
- **`World.paZonePtrGrid@0x2d0`** — a 10×10 (`int[100]`) grid of `Zone*` pointers, parallel to the
  `MapZone[100]` grid at `+0x4b4`; `Generate` fills it with `zoneObjects[id]` as it places zones.
- **`World.placedZoneIds@0x220`** (`CWordArray`) — list of zone ids placed this generation.
  `GameData::FilterEnemyZonesFromListMaybe` (0x403070) rebuilds it in place, dropping any zone whose
  map-flag == `ENEMY_TERRITORY(1)`.
- **`questItemsAMaybe@0x1e4` / `questItemsBMaybe@0x1f8`** (`CWordArray`) — the two halves of the main
  quest item chain (each sized ~half the total quest count; indexed into the item table at `+0xd4`).
- **`goalTileListMaybe@0x20c`** (`CWordArray`) and **`goalItemTileIdMaybe@0x2e50`** — hold the demo goal
  item tile id (`0x6c`).
- **Gen output scratch block `+0x3380..+0x339c`** (8 ints written by `WorldgenPlaceRandInZone7Maybe`,
  then copied into the placed `MapZone`): `genCellItemCScratch@0x3380`, `genCellQuestSlot5Scratch@0x3384`,
  `genCellItemAScratch@0x3388`, `genCellItemBScratch@0x338c`, `genCellQuestSlot6Scratch@0x3390`,
  `genCellQuestSlot0Scratch@0x3394`, `genCellQuestSlot1Scratch@0x3398`, `genZoneTypeScratch@0x339c`
  (the placed zone's content type → `MapZone.field_0x4`).

## End-screen zone accessors (2026-07-05)
`World::GetVictoryZoneIndexMaybe` (0x401a40) returns fixed zone index 76 iff `zoneObjects[76]` has
map-flag `VICTORY_SCREEN(0xd)` (else -1); `World::GetLossZoneMaybe` (0x401a60) returns `zoneObjects[77]`
iff it has map-flag `LOSS_SCREEN(0xe)` (else NULL). Both demo-hardcoded, called by `UpdateFrameMaybe`
(0x40d470) — the win/lose end-screen check. (Map-flag enum from DesktopAdventures `map.h`: 13=VICTORY,
14=LOSS, 1=ENEMY_TERRITORY.)

## Quest-path layout helpers (2026-07-05)
- **`World::GetGridOrderMaybe`** (0x421e50) — `return gWorldgenGridOrderTable[col + row*10]` (static 10×10
  dword table @0x456630). Gives the worldgen placement/traversal order weight for a grid cell; called by
  `Generate`, `PlacePuzzle`, `WorldgenPlacePuzzlesMaybe`, `WorldgenBuildQuestMaybe`, `BuildQuestPathMaybe`.
- **`GameData::BuildQuestPathMaybe`** (0x403c80) — grid-layout pass over the 10×10 quest grid
  (`param_1`=cell-type shorts, `param_2`=parallel order-id shorts): counts special cells
  (0x65/0x68/0x12d..0x130), then walks and assigns sequential order ids (writing 0x132 markers),
  building the quest path chain. Returns the number of placed quest steps.
