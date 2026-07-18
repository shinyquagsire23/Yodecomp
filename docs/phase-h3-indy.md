# Phase H3 — 32-bit Indiana Jones' Desktop Adventures (`GAME_INDY`)

Port the shared CDeskcpp engine (our decompiled 32-bit Yoda source) to load and play **Indiana Jones'
Desktop Adventures**, gated behind `#ifdef GAME_INDY`, keeping the Yoda demo byte-match anchor intact.

Build: `cmake -B build-indy -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_GAME=INDY`

## The premise
Indy shipped only as **16-bit** `INDYDESK/DESKADV.EXE` — there is no 32-bit Indy binary to decompile.
But Indy and Yoda are the **same CDeskcpp engine** built from one source with a game flag (confirmed:
shared class names, WaveMix, WinG; DESKADV.EXE has "Wrong version DAW File!" — the same versioned
chunk-based asset format). So the target is: our Yoda engine + `#ifdef GAME_INDY` branches reproducing
Indy's format/logic deltas ⇒ a **new 32-bit build** of Indy on the shared engine. Byte-match N/A.

## References (in priority order)
1. **`INDYDESK/DESKADV.EXE`** — the original 16-bit Indy engine, loaded in Ghidra (`program=DESKADV.EXE`,
   x86:LE:16). **Ground truth** for every delta. ⚠ 16-bit codegen + segmented addrs (`11b0:000f`) make
   function-diffing harder than H2's 32-bit-vs-32-bit; recover the *logic* (field orders, record sizes),
   not the codegen.
2. **`~/workspace/DesktopAdventures`** — the user's portable reimplementation. Its `is_yoda` flags are a
   precise **map of WHERE** Indy and Yoda diverge (~15 in `src/assets.c`, a few in map/ui/player/palette).
   ⚠ It parses its own way — NOT a byte-accurate spec (e.g. its Yoda ZONE header `NUM_MAPS/unk/LEN` does
   not match our engine's `ParseZone`, which reads just `nZones`). Use it to find deltas, confirm in DESKADV.
3. **Data:** `INDYDESK/DESKTOP.DAW` (2.36 MB; Yoda's `YODESK.DTA` is 4.6 MB). Support assets (WAV/MID/BMP)
   also in `INDYDESK/`.

## Delta surface (DesktopAdventures `is_yoda` map → our engine → verify in DESKADV.EXE)
Our `.dta` loader is `CDeskcppDoc::Load()` (0x4158) dispatching FourCC chunks to `Parse*`/`Read*`; the
Indy deltas live in those handlers.

| Delta | DesktopAdventures site | Our engine site | Notes / status |
|---|---|---|---|
| Data filename | assets.c:79 | `Deskcpp.cpp` InitInstance | ✅ `DESKTOP.DAW` under `GAME_INDY` |
| VERS value | "Wrong version DAW File!" | `Load()` VERS branch (`nLen!=0x200`) | Indy version differs — confirm the DAW VERS value in DESKADV |
| ZONE chunk header | assets.c:142 (Indy reads chunk LEN then NUM_MAPS) | `Load()` `if(tag!="ZONE") Read(&nLen)` + `ParseZone` | Yoda skips ZONE's chunk len; Indy likely reads it. Confirm exact header in DESKADV |
| IZAX | assets.c:186 | `ReadZone`/`ReadIzax` | Indy counts zones differently across IZAX |
| **ACTN / IACT** | assets.c:260 ("Indy lumps all IACTs into one giant section") | `ParseActn` + `IactScript::Read` | **Biggest delta.** Yoda: per-zone IACT list w/ length ids. Indy: one global ACTN block, sifted+linked to zones |
| CHAR record size | assets.c:417 (`0x54`→`0x4E`) | `ParseChar` | Indy character record is 0x4E, not 0x54 |
| record/name sizes | assets.c:522/529 (`26/24`→`18/16`) | likely `ParsePuz2`/`ParseTnam`/name reads | Indy shorter records + 16-char names (vs 24). Identify which chunk |
| HTSP / object qty | map.c:143/146 | `ParseHtsp` / object placement | Indy: `htsp_offset==0 ⇒ 0 objects` special-case |
| Palette | palette.c (`indy_palette`, no cycling) | palette load / `CyclePalette` | Indy has its own palette; no palette cycling |
| Worldgen | (n/a) | `Generate`/`LoadWorld`/`WorldgenSelectPuzzle` | Indy has NO 3-planet system (Nevada/Alaska/Oregon) — the planet rotation + per-planet goal whitelists are Yoda-specific. Indy's world assembly must be diffed from DESKADV |
| Resources (icon/menu) | (n/a) | CMake `.res` source | Use Indy's icon/resources, NOT Yoda's (USER note). DESKADV.EXE is 16-bit NE — its resources may need a different extractor, or supply an Indy `.res`/`.ico` |

## Milestones (each a runnable checkpoint; verify the anchor stays 211 after any shared-code edit)
1. **Scaffolding** ✅ — `GAME_INDY` config + `DESKTOP.DAW` data path; a `build-indy` that compiles.
2. **DESKTOP.DAW parses** — implement the load-time format deltas (VERS, ZONE header, IZAX, CHAR size,
   record/name sizes, ACTN lump) so the full asset load completes without crash/misread. Verify each
   against DESKADV.EXE. Biggest sub-task: the ACTN IACT lump.
3. **Renders** — Indy palette; first zone/tiles draw correctly.
4. **Worldgen** — Indy world assembly (no planets); a generated Indy adventure is playable.
5. **Polish** — Indy resources/icon, sound (Indy WAV/MID set), menus.

## Milestone 2 COMPLETE — DESKTOP.DAW fully parses (engine-confirmed 2026-07-08)
The debug build (`-DYODA_DEBUG=ON`) logged the real `Load()` dispatch: every chunk parses
`VERS→STUP→SNDS→TILE→ZONE→[globals]→PUZ2→CHAR→CHWP→CAUX→TNAM→ZNAM→PNAM→ANAM→ENDF`, then
`LOAD COMPLETE, entering worldgen (zones=366 puzzles=157 chars=27 planet=2)`. So the full Indy asset
set loads correctly. The remaining hang is `Generate` failing every retry (`Generate try #1..N -> fail`)
= **milestone 4** (Indy worldgen), NOT a load bug.

## Milestone 2 progress (DESKTOP.DAW parse) — ground-truth findings
Verified by examining the **raw DAW/DTA bytes** (the data itself — the most reliable ground truth, and
it sidesteps 16-bit RE). The Yoda `.dta` and Indy `.daw` share the chunk vocabulary but differ in
**zone layout**:
- **Yoda = self-contained zone records:** `"ZONE" + nZones(2)`, then each zone = `planet(2)+len(4)+
  pad(2)` + `IZON`(tiles) + objects(inline) + `ZAUX/ZAX2/ZAX3/ZAX4` + `IACT` scripts(inline).
- **Indy = parallel arrays:** `"ZONE" + chunkLen(4) + nZones(2)`, then back-to-back `IZON` tile records
  (NO per-zone planet prefix, NO planet filter — Indy has no planets). Each zone's aux (`IZAX/ZAX2/ZAX4/
  ZAX3`), objects (`HTSP`) and scripts (`ACTN` — one lump, its length spans all IACTs) are SEPARATE
  GLOBAL chunks after the zones, plus Indy-only `PNAM`/`ANAM` (puzzle/actor names).
- **`ReadIzon` is shared** — Indy's IZON consumes the same 8 header bytes (width/height/type/globalVar/
  planet) + tiles; only the field *semantics* differ, not the byte count.

**Implemented (v56, `#ifdef GAME_INDY`, anchor 211 held):**
- `ParseZone`: read Indy's `chunkLen(4)` before `nZones`.
- `ReadZone`: Indy branch = `new Zone; ReadIzon;` (tiles only — no prefix/filter/inline objects/scripts).
- `Load()` dispatcher: skip the Indy global chunks (`IZAX/ZAX2/ZAX4/ZAX3/HTSP/ACTN/PNAM/ANAM`) by length
  so the load walks past the zones — their per-zone distribution is the next sub-step (2b+).

**⭐ FULL DAW PARSE COMPLETE (v57, verified by byte-simulation + our real parsers):** every load-time
delta found + fixed, so `Load()` walks `DESKTOP.DAW` cleanly to `ENDF`:
- `IZON` header 8 bytes not 12 (drop globalVar+planet) — `Zone::ReadIzon` (the zone-alignment fix).
- `ParseZone` reads Indy's `chunkLen(4)`; `ReadZone` Indy branch = tiles-only.
- `PUZ2`/`Puzzle::Read`: Indy IPUZ drops `unk3(4)` + `itemB(2)` — verified 157 puzzles align to CHAR.
- `CHAR`/`Character::Read`: Indy ICHR is `0x4E` not `0x54` — 6-byte-shorter frame block (`0x2a` not
  `0x30`) — verified 27 chars align to CHWP.
- `TNAM`/`ParseTnam`: Indy tile names are `0x10` not `0x18` — verified 143 names align the tail to ENDF.
- SHARED (no delta): `VERS`/`STUP`/`SNDS`/`TILE`, `CHWP`, `CAUX`. SKIPPED: the global aux/object/script
  chunks + Indy-only `ZNAM`/`PNAM`/`ANAM` (dispatcher length-skip).
Method note: the raw-byte simulation (walk the DAW with each candidate delta, confirm the next tag lands
exactly) proved every delta WITHOUT a run — faster + anchor-safe vs C++ instrumentation.

## Milestone 4 IN PROGRESS — worldgen root-cause fix + aux distribution (2026-07-09)

**Root cause of the `Generate try #N -> fail` infinite retry (found + fixed):** `PlaceQuestNode` filters
candidate zones by `pZone->planet == currentPlanet`, but **Indy has no planets** — the Indy IZON header
carries no planet field, so every Indy zone keeps the `Zone` ctor default `planet == -1`, while
`currentPlanet` was forced to `2` (demo/ctor hardcode). Result: the candidate list was ALWAYS empty →
`PlaceQuestNode` returned 0xffff → `goto fail_a` → Generate returned 0 → retried forever.

**The Indy planet model (fix, all `#ifdef GAME_INDY`, anchor 211 held):** set `currentPlanet = -1` for Indy
(in both the `CDeskcppDoc` ctor and `LoadWorld`'s forcer). `-1` == every zone's `planet == -1`, so the
zone filter accepts ALL zones, AND every `switch (currentPlanet)` (the Nevada/Alaska/Oregon story-history
branches) naturally falls through to no-op — exactly right for a planet-less game. Verified from raw bytes:
all 366 Indy zones carry a full spread of quest-node `type`s (type 10 FINAL_ITEM ×15, 15/16/17 present),
so the generator has zones of every type to place.

**Goal selection (fix):** enabled the FULL-game dynamic goal path for Indy (`#if defined(YODA_FULL) ||
defined(GAME_INDY)`), and gave `WorldgenSelectPuzzle`'s `9999` (WORLD_MISSION) arm a `GAME_INDY` branch that
accepts ANY WORLD_MISSION puzzle (no per-planet whitelist / no story-history screen — both Yoda-specific).
Verified from raw bytes: DESKTOP.DAW has **15 WORLD_MISSION (nType==3) puzzles**; the Yoda planet-2
whitelist would have matched only 1.

**Milestone 2b (aux distribution) — DONE for aux (ZAUX/ZAX2/ZAX3):** the worldgen quest builder needs each
zone's item pools (`WorldgenPickItemFromZone` reads `providedItemsA`/`providedItemsB`; renamed from
`cobArray4`/`cobArray5`), which were empty because the Indy global aux chunks were length-skipped. Now
distributed to zones via `Parse{Zaux,Zax2,Zax3}Indy` (Indy-only). ⭐ **Indy aux format cracked from raw
bytes (validated to consume ALL 366 records exactly):**
- **IZAX** (ZAUX): `mission_spec(2) + num_entries(2) + num_entries×{charId,x,y}(6B each) + count(2) +
  items(2 each)` — 6-byte entities (vs Yoda's 44) and only ONE item pool (Yoda has two). `ReadIzaxIndy`
  populates entities + `providedItemsA`; ⚠ it also MIRRORS the single pool into `providedItemsB`
  (HYPOTHESIS: the goal-zone placement needs both branches; Indy's real two-branch model is unconfirmed —
  needs DESKADV.EXE worldgen RE).
- **IZX2** (ZAX2) → `genCandidateA`, **IZX3** (ZAX3) → `genCandidateB`: byte-IDENTICAL to Yoda, so
  `ReadZax2`/`ReadZax3` are reused verbatim.
- **IZX4** (ZAX4): still length-skipped (static-map flag; Yoda `ReadZax4` discards it anyway).

### ⭐ Worldgen trace RESULT (2026-07-09, user ran instrumented build-indy) — the Yoda quest machinery is the WRONG MODEL for Indy; DECISION: decompile DESKADV.EXE worldgen separately
A `-DYODA_DEBUG` build (bounded-retry + per-gate logging, since reverted — anchor 211) traced Generate's
failure through four layers. Each fix revealed the next Yoda-specific assumption Indy's data can't satisfy:
1. **Planet filter** (`PlaceQuestNode`: `planet==currentPlanet`) → fixed with `currentPlanet=-1`. Zones
   become candidates. ✅ (kept)
2. **Goal selection** picks a valid WORLD_MISSION puzzle every try (109/136/134/…). ✅ (kept)
3. **`PQN type=10` (FINAL_ITEM goal zone) rejects all 15 candidates in phase 2.** The gate
   `ZoneRequiresItemMaybe(a4)==1 && ZoneRequiresItemMaybe(a5)==1` reads `genCandidateA` (IZX2), but **Indy
   goal zones have EMPTY IZX2/IZX3** (`genA=0 genB=0` on every candidate), and **Indy puzzles have no
   itemB** so `a5` is always `-1` (→ `req5` can never be 1). Structurally unsatisfiable.
4. Relaxing that gate + making the goal single-branch (Indy's single-item puzzles) got the zone to pick a
   valid item + GOAL_PRIZE puzzle (`itemA=663 puzA=104`), but **`WorldgenPopulateGoalZone` returns 0**: its
   success paths need `PickUnplacedItemMaybe` (reads `genCandidateB`/IZX3 — empty) or `WorldgenFillQuestItemSpot`
   (reads `genCandidateA`/IZX2 — empty). Seeding the goal zone's genCandidate lists from its IZAX pool still
   failed downstream (`populate=0`).
**Conclusion (user-agreed):** every layer of Yoda's quest builder assumes **two-item puzzles + per-zone
IZX2/IZX3 required-item lists**, and Indy goal zones have NEITHER — only the single IZAX item pool. Reverse-
engineering Indy's rules by trial-and-error against Yoda's code doesn't converge and produces semantically
wrong quests. The RIGHT path (the plan's original directive) is to **decompile DESKADV.EXE's actual Indy
worldgen** and reimplement it under `GAME_INDY`, rather than bend the Yoda logic. All heuristic patches were
REVERTED; kept only the correct base: planet fix + goal-selection + aux data loading.

### What Indy's DATA has (confirmed from raw bytes; use as constraints for the DESKADV RE)
- Puzzles: single-item (no `itemB`); types 0/1/2/3 = TRANSACTION/TRADE/GOAL_PRIZE/WORLD_MISSION (49/49/43/15).
- Zones: full quest-type spread; **type-10 goal zones (×15) have `providedItemsA` (IZAX pool) but NO
  IZX2/IZX3**. Regular zones DO have IZX2 (141 zones)/IZX3 (32 zones). So Indy uses IZX2/IZX3 for regular
  quest zones but NOT goal zones — goal handling is the biggest logic delta.
- IZAX = one item pool per zone (Yoda has two: providedItemsA + providedItemsB). The `providedItemsB` mirror
  in `ReadIzaxIndy` is a KNOWN wart to revisit in the rework (Indy is single-pool).

### ⏭ NEXT (milestone 4 = the DESKADV.EXE worldgen RE): decompile + NAME Indy's worldgen in `program=DESKADV.EXE`
Find Indy's equivalents of `Generate`/`PlaceQuestNode`/`WorldgenSelectPuzzle`/`WorldgenPopulateGoalZone`,
recover the LOGIC (not codegen — 16-bit), and reimplement under `GAME_INDY`. Anchors to start from: the DAW
loader (`"DESKTOP.DAW"`@1010:dd0e caller), and `srand`/`rand` call sites (worldgen is rand-heavy). Then the
remaining sub-steps: **HTSP → zone objects** (worldgen also reads `pZone->objects` for vehicles/DOOR_IN),
**ACTN** (one IACT lump → per-zone scripts) for gameplay, and **3** Indy palette. USER visual is the run oracle.

## DESKADV.EXE (Ghidra) — naming practice + RE friction
**USER directive:** as Indy functions are identified in DESKADV.EXE, **rename them in its Ghidra program**
(`program=DESKADV.EXE`) so future sessions don't re-discover them. Track named functions below.
- ⚠ **16-bit RE friction:** DESKADV.EXE is `x86:LE:16 Protected Mode` (segment:offset addrs like
  `1010:dd0e`). The Ghidra HTTP/MCP `get_xrefs_to` returns *no* references for data-string addresses
  (16-bit auto-analysis didn't build string xrefs), so the usual "string → referencing function" anchor
  fails. Tag compares are also **integer** (packed FourCC), not string literals — no "ZONE"/"TILE"
  strings to anchor on. Workarounds for future sessions: byte-pattern search for the seg:off of a known
  string, the Ghidra GUI's xref view, or `decompile_function` on a known code address. Anchor strings:
  `"DESKTOP.DAW"`@1010:dd0e, version-error@1200:0068, file-open-error@11f8:011a.
- **Named DESKADV.EXE functions (worldgen RE sweep, 2026-07-09):** the FULL Indy worldgen is decompiled +
  named. Key addresses (all renamed in Ghidra `program=DESKADV.EXE`; global doc far-ptr at DS:`1028:0a02`):
  - Loader: `IndyLoadDaw` 1010:a900 (DESKTOP.DAW chunk dispatcher; tag strings @1010:d756; on ENDF runs the
    Generate retry loop), chunk parsers `IndyParse{Tile 1010:3eac, Tnam b440, Zone 5450, Zaux b1ec, Zax2 b2c0,
    Zax3 b256, Char a3fe, Chwp b3ac, Caux b32a, Htsp b72c, Snds b4c2, Puz2 b10a, Actn b5d4}`; aux readers
    `IndyReadIzax` 1010:2462 (entities→zone+0x7c2, items→zone+0x7d4), `IndyReadIzx2` 1010:25c0 (→zone+0x7e2),
    `IndyReadIzx3` 1010:263c (→zone+0x7f0). `IndyLoadWorldState` 1010:b890 (SAVEGAME.WLD VERS/STUP/ENDF),
    `IndySetCurrentToIntroZone` 1010:ba3e.
  - Worldgen core: `IndyGenerate` 1010:8524, `IndyMakeWorldSeed` 1010:c10e, `IndyCarveQuestPath` 1010:6c5c,
    `IndyPlaceIslandStrips` 1010:7490, `IndyAssignQuestStepCells` 1020:1426, `IndyPlaceQuestNode` 1010:7f0c,
    `IndySelectPuzzle` 1010:7b58, `IndyPlacePuzzlesPass` 1010:9ebc, `IndyStartNewGameMaybe` 1020:0ed0.
  - Populates: `IndyPopulateGoalZone` 1010:5dac (type 10), `IndyPopulateTradeZone` 1010:6422 (0xf),
    `IndyPopulateTransactionZone` 1010:5f66 (0x10), `IndyPopulateSimpleZone` 1010:67ec (2..7),
    `IndyPopulateUsefulObjectZone` 1010:6580 (0x11/0x12).
  - Item helpers: `IndyZoneRequiresItem` 1010:5642 (0x7e2), `IndyZoneProvidesItem` 1010:5566 (0x7d4),
    `IndyZoneSpawnPoolHasItem` 1010:5842 (0x7f0), `IndyPickUnplacedProvidedItem` 1010:7a28,
    `IndyPickUnplacedSpawnItem` 1010:571e, `IndyFindItemInProvidedPool` 1010:593e, `IndyFillQuestItemSpot`
    1010:5a14, `IndyPlaceRewardInItemSpot` 1010:6260, `IndyFillSpawn` 1010:5bdc, `IndyPlaceItemOnLock`
    1010:610a, `IndyIsItemPlaced` 1010:6998, `IndyAddPlacedItemEntry` 1010:6b28 / `IndyRemovePlacedItemEntry`
    1010:6aae (doc+0x186 list), `IndyQueueItemForPlacement` 1010:69ea / `IndyUnqueueItemPlacement` 1010:6a34
    (doc+0x178 list), `IndyPickCellForItemZone` 1010:9b98, `IndyCheckZoneItemsAvailable` 1010:83bc,
    `IndyCollectZoneItems` 1010:8482, `IndyIsZoneUsed` 1010:6bc0 / `IndyMarkZoneUsed` 1010:6c3a (zone+8),
    `IndyIsPuzzleUsed` 1010:9b4e (doc+0x140 list), `IndyShuffleList` 1010:7daa, `IndyGetGridOrder` 1010:a3e2
    (ring table @DS:0x430 — IDENTICAL to Yoda's), `IndyGetZoneById` 1020:11b8, `IndyGetIslandOrientation`
    1010:3dc4.
  - Post/persist: `IndyMaterializePlacedItemTiles` 1020:07ae, `IndyFilterEnemyZonesFromPlacedList` 1020:06ca,
    `IndySavePlacedZoneList` 1020:0380 / `IndyLoadPlacedZoneList` 1018:eb1e, `IndySaveStoryHistory` 1020:0000 /
    `IndyLoadStoryHistory` 1018:e79e (obfuscated INI lists, rand()%255+1 key — Yoda SavePlanetTable-style),
    `IndyCacheSpecialTilePtrsMaybe` 1010:42be.
  - Palette: `IndyCyclePalette` 1018:8e40 (≡ Yoda `CDeskcppView::CyclePalette` — gated on `doc+0xc3c`, same
    ring ranges; enable flag set to 1 at `1010:506c` during palette init). Confirms Indy DOES cycle.
  - RNG: `IndyRand` 1008:635c (**standard MSVC LCG**: seed32@DS:1028:0cc8 = seed*214013+2531011, return
    (seed>>16)&0x7fff — the 0x343fd constant is split into 16-bit imm halves 0x43fd/0x0003, which is why a
    dword search missed it), `IndySrand` 1008:6344 (seed lo=arg, hi=0), `IndyTime` 1008:59b6 (DOS int21
    date/time), `IndyClockMaybe` 1008:5d86.
  See the H3 worldgen-RE session report for the full algorithm walkthrough (grid tokens, node types,
  difficulty tables, struct offsets).

## Milestone 4 — Indy worldgen REIMPLEMENTED + INTEGRATED (2026-07-09; builds, anchor 211, RUNTIME-UNVERIFIED)
The full Indy worldgen was decompiled from DESKADV.EXE and reimplemented as ~28 `CDeskcppDoc::Indy*`
methods in `src/Worldgen.cpp` (appended at end-of-file, entirely `#ifdef GAME_INDY`) + decls in
`Worldgen.h`. `Load()`'s post-load retry loop routes to `IndyGenerate` for Indy (no separate `Populate` —
IndyGenerate does plan + placement + materialize + play-state, matching DESKADV). `build-indy` compiles +
links (`yoda.exe` 470KB); **anchor still 211** (all changes GAME_INDY-guarded or Yoda-token-identical).
- **Prerequisite done (committed 24a247d):** Indy HTSP objects now load (routed to `ParseHtsp`) — required
  because the quest item pools live in DOOR_IN child zones reached via object type 9. `ReadIzaxIndy`'s
  single-pool wart (providedItemsB mirror) removed.
- **RNG:** standard MSVC LCG (CRT `rand`/`srand` used directly).
- **Integration shim** (top of the GAME_INDY block in Worldgen.cpp): `#define bool/true/false` (VC4.2
  pre-bool) + `#define` aliases wiring reused Yoda helpers (`IndyIsItemPlaced`→`IsItemPlaced`,
  `IndyZoneRequiresItem`→`ZoneRequiresItemMaybe`, `IndyShuffleList`→`WorldgenShuffleList`,
  `IndyQueueItemForPlacement`→`WorldgenPushZoneEntry`, `IndyAddPlacedItemEntry`→`WorldgenAddZoneEntry`,
  `IndyMarkZoneUsed`→`AddPlacedZoneId`, `IndyFilterEnemyZonesFromPlacedList`→`RemoveEmptyZonesFromPlacedList`)
  + no-op stubs for skipped INI persistence / DOS-time seed helper.
- **Doc-list reuse map:** Indy queued-items(doc+0x178)=`worldgenPendingZones`, placed-items(+0x186)=
  `worldgenRefZones`, used-zones(+0x14e)=`placedZoneIds`, per-step puzzle list(+0x132)=`goalTileList`,
  used-puzzles(+0x140)=`storyHistoryNevada`, used-required(+0x15c)=`uniqueRequiredItemsMaybe`. One new
  member: `int indyPlaceOnEdge` (doc+0xc40 edge gate).

### ⭐ WORLDGEN CONVERGES (2026-07-09) — the hang is FIXED; two root-cause bugs found via headless trace
The initial integration HUNG (unbounded retry: `IndyGenerate` returned 0 every seed). A `-DYODA_DEBUG`
headless trace (CrossOver wine DOES reach `Load()`/worldgen headless — a viable fast oracle) pinpointed the
GOAL node (type 10) failing on the first quest step with `passReq=1 passPick=1 passPuz=0` — no GOAL_PRIZE
puzzle matched. Two transcription bugs (both fixed, verified against the DESKADV decompilation of
`IndyGenerate` 1010:8524 + `IndyPlaceQuestNode` 1010:7f0c + `IndySelectPuzzle` 1010:7b58):
1. **`IndySelectPuzzle` matched the wrong item.** It compared the candidate puzzle's `itemA` against
   `reqItemA` (param_4, the *required* item), but DESKADV matches `itemA == param_5` (the **picked** item =
   `nWorldMissionKey`); param_4 is unused in the match. → `need == nWorldMissionKey`.
2. **The quest-chain item threading was mis-indexed.** DESKADV's `IndyPlaceQuestNode(gridOrder, reqItem,
   step-1, nodeType)` uses `param_5 = step-1` (the *order slot*) for the pick, `IndySelectPuzzle` bFirst
   (`step-1==0`), the `goalTileList[·]` write, and `IndyPopulateGoalZone`'s step-slot — but our code used
   `nOrder` (the grid ring 1–5) everywhere and PASS-2 passed `-1` for the slot. Fixed: cases 10/0xf/0x10 use
   `a5reqItem2` (=order-1) for all four; PASS-2 passes `order-1`. This threads the chain: step `order` writes
   `goalTileList[order-1]`, which step `order-1` reads as its `reqItem` (the downward-linking DESKADV does).
**Result (headless):** `Generate` SUCCEEDS on the first seed — full 8-step quest chain places + `IndyPlacePuzzlesPass`
+ materialize complete (`totalZones=9 nSteps=8`). Debug instrumentation removed; anchor still 211; clean
`build-indy` converges.

### ⭐ v59 — Indy BOOTS INTO A PLAYABLE WORLD (user-confirmed "gets in-game")
After worldgen converged, three more fixes got Indy rendering + interactive:
- **Reaches play mode (stuck-at-STUP fix).** IndyGenerate's tail replicates Yoda `Populate()`'s world-view
  handoff (nTargetZoneId=0, cameraX/Y=0x140, nFrameMode=0xb, bQuestCellsResident=1, BackupRecords,
  **pView->bBusy=0**). ⭐ Root cause of the STUP stall: OnTimer case-0xb with `bWorldInvalid==0` calls
  `WorldEntryStepMaybe`, which cannot reach step 10 (step-5 sets `nTransitionStep=-1` → loops 0→5 forever) — it
  relies on the zone ENTRY SCRIPT to advance nFrameMode, and Indy's ACTN scripts aren't distributed. WORKAROUND:
  set `bWorldInvalid=1` so case-0xb uses `ZoneTransitionStep` (climbs 0→10 → play mode nFrameMode=3, skipping the
  missing scripts); case-0xb self-clears it. ⚠ REVERT once ACTN is distributed.
  Found via the headless OnTimer trace (CrossOver wine fires the window timer headless — `-DYODA_DEBUG` YDBG logs).
- **Palette.** `IndyMasterPalette[1024]` (256 BGRX, from DESKADV via DesktopAdventures `indy_palette`) in
  DeskcppDoc.cpp; under GAME_INDY `pSysColorTable`→it + skip `bPaletteAnimEnabled=1` (Indy doesn't cycle).
- **Menus.** Save/Load/Replay were demo-gated (`DemoDisable`); enabled `GAME_INDY` in its `YODA_FULL` guard.

### ⭐ ACTN zone-script distribution DONE (2026-07-09) — scripts load; the entry-script/whip path is the real remaining delta
**ACTN is now distributed.** The Indy ACTN chunk is NOT a different internal format — it is the IDENTICAL keyed
`[zone_id(2), count(2), scripts...]` block as Yoda's per-zone IACT, only relocated to ONE global chunk after the
zones (the "one giant section"). Proven three ways: (1) DESKADV.EXE `IndyParseActn` 1010:b5d4 is structurally
identical to our `ParseActn` (same zone_id / -1-terminate / count / `IndyIactScriptRead` 1010:0170 = tag(4)+size(4)+
nCond(2)+conds×14B+nCmd(2)+cmds); condition record = `PUSH 0xe` (14 bytes), same as Yoda. (2) RAW-BYTE SIMULATION
of `DESKTOP.DAW`'s ACTN chunk (walk chunks tag(4)+len(4)+payload, VERS is a bare 8-byte tag+version, ZONE has a
4-byte chunklen prefix; then parse ACTN as the keyed record loop): the keyed parse consumes the chunk EXACTLY
(delta=0), 319 zones /
2825 scripts. (3) Headless YDBG confirmed the real load: `ACTN loaded: zones_with_scripts=319 total_scripts=2825`.
**Fix (anchor 211 held):** removed `ACTN` from the Indy dispatcher's length-skip list (`src/Worldgen.cpp` ~L4269) so
it falls through to the shared `ParseActn` — a one-line change; no `ParseActnIndy` needed.

### ⭐ v61 — the START-ZONE TARGET bug FIXED (root cause of "can't enter buildings" + the false entry-script trail)
The v59 pickup chased a non-existent "Indy entry-script" delta. The real bug: **the world-entry transition was
targeting the WRONG zone.** The IndyGenerate tail hardcoded `pView->nTargetZoneId = 0` (copied verbatim from Yoda's
`Populate()`, where the demo layout puts the intro zone at id 0). But Indy's start zone id is **dynamic** — the
generator places a MAP_START_AREA (type 11) zone at the player's spawn cell `(nStartX,nStartY)`. With `bWorldInvalid=1`,
`ZoneTransitionStep` step 5 sets `currentZone = GetZoneById(nTargetZoneId)` — so `=0` made the whole entry render and
run against **zone 0** (type 17, a random FIND_SOMETHING_USEFUL_BUILDING zone), whose objects/doors never matched
what the player saw. That is almost certainly why buildings couldn't be entered (the DOOR_IN objects belonged to the
wrong zone).
- **Fix (`src/Worldgen.cpp` IndyGenerate tail, GAME_INDY-guarded, anchor 211 held):**
  `pView->nTargetZoneId = GetZoneCell(nStartX, nStartY);` — the real start zone id.
- **Verified headlessly (YDBG):** start cell `(5,4)` → `startZoneId=120`; after the fix `currentZone` becomes
  **type 11 (MAP_START_AREA)** from transition step 6 on, and the transition self-climbs step 1→11 → play mode
  (`nFrameMode=3`). Before the fix it was type 17 (zone 0).
- **The `bWorldInvalid=1` self-climb is CORRECT for Indy, NOT a workaround to revert.** With the target fixed we
  inspected the REAL start zone (120): 45 scripts, cond histogram = GlobalVarEq×18 / BumpTile×16 / HasItem×11 /
  DragItem×5 / TempVarEq×2 / Walk×2 — **ZERO cond-0 (FirstEnter) / cond-1 (Enter)**. So Indy has no scripted intro
  entry; `WorldEntryStepMaybe` (which only escapes step 5 when an entry script advances nFrameMode) would loop
  forever. Yoda's Hoth intro zone DOES have a FirstEnter script — that's the whole reason Yoda uses the scripted
  path. DA confirms the shared opcode enum (`iact.c` triggers[] and `map.c` fire FirstEnter+Enter for BOTH games,
  no `is_yoda` gate), so the opcodes are NOT renumbered — there simply is no entry script to run. The v59 "no cond
  0/1" observation was correct but analysed the WRONG zone (0); the conclusion (chase entry semantics) was wrong.

### ⭐ v62 — five more bug fixes (New World infloop, palette, character animation, door crash) + whip finding
All GAME_INDY-guarded, anchor 211 held. Verified against DESKADV.EXE (three parallel RE agents) + headless YDBG.
1. **New World infinite loop / progress bar jumping (FIXED).** `StartGame` (OnNewWorld → StartGame) had a Generate
   loop that was NOT GAME_INDY-guarded — it ran Yoda `Generate()`, which never converges on Indy data → infinite
   reseed. `Load()` was guarded; `StartGame` was missed. Mirror Load's routing (`IndyGenerate` / skip Populate).
   Needed a GAME_INDY `IndyGenerate` decl in `DeskcppStub.h` (WorldgenHelpers compiles against the facade).
2. **Palette "browns cycling weirdly" (FIXED).** Indy animates DIFFERENT ranges than Yoda (v60's "same ranges"
   was wrong). DESKADV `IndyCyclePalette` 1018:8e40: EVERY tick ring-rotate [160..167],[224..228],[229..237] UP;
   ODD tick ring-rotate [238..243] DOWN + swap 244↔245; push single band [160..245] (0xa0,0x56). Indy has NO
   [10..14] band and none of Yoda's 198..207 swaps. Added a GAME_INDY branch in `CyclePalette` (early-return).
   sysPalette base = doc+0xc46, colortable ptr = doc+0x1046, enable = doc+0xc3c, tick counter = doc+0x50.
3. **Character animation garbled (FIXED).** Indy's frame layout is IDENTICAL to Yoda (24 shorts / stride 8 / same
   GetFrameTile — DESKADV FUN_1010_076e); the "21 frames / 7-per-bank" premise (from DesktopAdventures) was WRONG.
   The real bug: Indy's ICHR record (`0x4E`) has only TWO 2-byte fields after the name then a FULL `0x30` frame
   block — it DROPS Yoda's 3rd short + dword. Our shared `Character::Read` read Yoda's w3+dw, eating 6 frame bytes
   and shifting every frame by 3 shorts. Fixed the Indy branch to skip w3/dw and read the full 0x30 (DESKADV
   FUN_1010_069c reads 8+16+2+2 then 0x30). Total record size unchanged so CHWP still aligns.
4. **Door-entry crash (defensive guard added; NOT confirmed as the crash).** Headless (post-target-zone-fix) shows
   door entry COMPLETES: start zone 120 has 3 DOOR_IN objects (args 107/108/333), entering interior 107 (type 8)
   runs steps 0→10 fine. BUT the interior isn't in the 10×10 grid, so the grid search leaves cell (-1,-1) and
   `ZoneTransitionStep`'s UNguarded `mapGrid[nCellX+nCellY*10].flagSolved=1` writes `mapGrid[-11]` (OOB into the
   World struct — can corrupt a pointer → a later GDI crash the headless run doesn't trigger). WorldEntryStepMaybe
   guards this same store; added the guard to ZoneTransitionStep for Indy. ⚠ Needs USER visual re-test to confirm
   the door crash is gone; if not, get a GUI crash address (the crash is real-GDI-only, invisible headless).
5. **The whip does NOT appear (root cause found, fix pending).** DESKADV RE (agent): Indy does NOT seed a starting
   weapon/inventory at worldgen — `IndyGenerate`/`IndyStartNewGameMaybe` call NO add-to-inventory; the inventory
   CObArray (doc+0x78) is EMPTIED, never filled. The hero entity's HP is set (entity+0x90=0x78=120) but no weapon.
   So the whip comes from GAMEPLAY: either an OBJ_WEAPON pickup in a zone, or an IACT `CMD_AddItemToInv`. With the
   correct start zone now active, re-check whether the whip appears via pickup. (Also pending: wire the hero
   Character HP=120 in the IndyGenerate tail — currently only doc fields are set.)

### ⭐ v63 — New World infloop root-caused+FIXED; door crash root-caused (backtrace) + guarded
User re-test after v62: **palette FIXED, animations FIXED**; New World still infloops, door still crashes. Both
now root-caused via a saved GUI backtrace + headless repro (all GAME_INDY-guarded, anchor 211):
- **New World infinite load / progress-bar thrashing — FIXED.** `OnNewWorld → StartGame → LoadWorld()` re-parses
  DESKTOP.DAW to reset zone state, but `LoadWorld` is a SECOND copy of the DAW loader that only had the Yoda chunk
  dispatch — on the Indy DAW (global parallel-array aux) the Yoda `ParseZaux` walked off the global ZAUX and the
  loop never hit ENDF → infinite load. Ported `Load()`'s Indy branch into `LoadWorld` (ParseZauxIndy/ZAX2Indy/
  ZAX3Indy; skip ZAX4/IZAX/PNAM/ANAM; ZONE/HTSP/ACTN shared). Headless repro (inject OnNewWorld on 1st play tick):
  before = OnNewWorld never returns; after = returns + IndyGenerate converges (2nd seed). ⚠ LESSON: there are TWO
  DAW loaders (`Load` 0x4158 initial, `LoadWorld` 0x421fd0 New-World reset) — every load-format delta must be
  applied to BOTH.
- **Door-entry crash — root-caused, guarded (band-aid; deeper RE pending).** Saved GUI backtrace (YodaIndy/
  backtrace.txt) → PF write at yoda+0x6128. Mapped via OUR build's `/MAP` (NOT YodaDemo's layout!): the crash is in
  `Zone::IactRunCommands` `CMD_SetMapTile`: `tiles[(args[1]*18+args[0])*3+args[2]] = args[3]` with **args[1]=21087**
  (args=[7,21087,1,828]) → wildly OOB store. The script FORMAT is byte-identical to Yoda (conditions 14B, commands
  0xc+len+text — verified vs DESKADV FUN_1010_047e reading PUSH 0xc / PUSH 0x2), so args[1]=21087 is GENUINE data
  in some interior-zone SetMapTile. The byte-matched original does NOT bounds-check (relies on valid coords), so the
  real Indy engine must tolerate it. Guarded the tile index in SetMapTile/ClearTile/MoveMapTile for Indy. ⚠ This is
  a band-aid — the true root cause (why Indy interior scripts carry huge coords: opcode/executor semantics vs a
  script-keying quirk) is UNRESOLVED. Headless can't reproduce (a TransitionZoneDoor injection entered interior 107
  cleanly) — the crash needs a specific interior/trigger. Needs USER re-test + RE of Indy's command executor
  (find the DESKADV IactRunCommands twin; check if opcode 0 is SetMapTile for Indy or if it bounds-checks/uses the
  zone's real width instead of 18).

### ⭐ v64 — THE FUNDAMENTAL FIX: Indy IACT opcodes are RENUMBERED (fixes NPC dialog + door crash + entry gates)
User confirmed talking-to-NPCs also fails → the whole IACT system is misinterpreted, not a one-off. RE of the
DESKADV.EXE IACT RUNTIME (agent): runner `FUN_1010_2910` (=Zone::IactRun), executor `FUN_1010_2eb6`
(=Zone::IactRunCommands). ⭐ **The condition AND command OPCODES are RENUMBERED between Yoda and Indy** — but
record sizes (cond 14B / cmd 0xc+2+text), arg offsets (op@+4, args@+6..+0xe), the tile formula
`tiles[(y*18+x)*3+layer]`, event numbers (1=Walk 2=Bump 3=Drag 4/5=Enter), and every zone/script field offset are
IDENTICAL. Running Indy scripts through the Yoda opcode switches mis-dispatched everything:
- Indy **ClearTile=2** ran as Yoda **MoveMapTile=2** → the Yoda handler reads arg3/arg4 (uninitialized in an Indy
  ClearTile) as a destination coord → wild `tiles[]` write == the **door-entry crash** (the args[1]=21087 garbage).
- Indy **SayText=5 / ShowText=0x1c** ran as the wrong Yoda handlers == **silent NPCs**.
- Indy **FirstEnter=4 / Enter=5** ran as Yoda **Walk=4 / TempVarEq=5**, and Yoda's cond switch DEFAULTS-TO-PASS for
  unknown opcodes == building-entry scripts never gated (fired at wrong times / not at all).
**Fix (`src/IactScript.cpp`, GAME_INDY):** two lookup tables `kIndyCondToYoda[0x17]` / `kIndyCmdToYoda[0x24]`
translate each Indy opcode → its Yoda equivalent in `IactCondition::Read` / `IactCommand::Read`, so the byte-matched
Yoda interpreter runs unchanged. Verified by dumping the remapped script table: `s[0] C op=0`=FirstEnter,
`s[1] C op=1`=Enter, `C op=10`=CheckMapTile `[val,x,y,layer]`, `C op=2`=BumpTile `[3,11,584]`, `M op=0`=SetMapTile
`[3,10,0,1]`, `M op=12`=Random `[3]` — all sane. The full Indy→Yoda maps are in-source (from DESKADV jump tables
@1010:2f8a for cmds; the cond switch @1010:2910). ⚠ Key/high-impact opcodes are jump-table-CONFIRMED; a few rare
condition specials (Indy cond 0/8/9/0xb/0x14–0x16) and DrawOverlay's arg-order (Indy cmd 0x10 swaps a0/a1) are
best-guess TODOs (marked in-source) — refine if a specific script misbehaves. anchor 211 held (all guarded);
the earlier tile-write bounds guard in Iact.cpp stays as defense-in-depth.
⭐ LESSON: "shared engine, shared enum" was WRONG — DA's iact.h uses one enum for both games but the actual
BINARIES renumber the opcodes. Always confirm opcode semantics against DESKADV.EXE, not DA.

### ⭐ v65 — post-remap gameplay working (user: NPCs talk, whip found); fixed whip-vanish + Replay/New Story STUP
User confirmed the opcode remap works: NPCs talk, dialog shows, whip is findable. Two more fixes + one open quirk:
- **Whip vanished after one use — FIXED.** `FireWeaponStep` treats a weapon as reusable (never deplete `unk48` /
  never `RemoveItem`) only if `frames[7]==0x1fe` (Force) or `0x12` (Yoda lightsaber icon id). Indy's whip is a
  reusable melee ("extend") weapon flagged **TILE_LIGHTSABER (bit 18)** in tile metadata (DA checks that same flag
  for both games) but its icon tile id isn't 0x12 → it fell into the depletable path. Fixed: for Indy, key the
  reusable test off `GetTileData(frames[7])->flags & TILE_LIGHTSABER` (both the nStep==0 no-deplete branch and the
  removal guard). `DeskcppView.cpp` FireWeaponStep.
- **Replay Story / New Story hung at STUP — FIXED.** `OnReplayStory` (WorldgenHelpers.cpp) sets `bWorldInvalid=1`
  around StartGame then clears it to 0 after — which routes OnTimer case-0xb through `WorldEntryStepMaybe` (loops
  0→5 forever for Indy: no mode-advancing entry script) → stuck on the STUP graphic. The initial world + New World
  keep `bWorldInvalid=1` (IndyGenerate tail) → `ZoneTransitionStep` self-climb → play mode. Fixed: `#ifndef
  GAME_INDY` around the `bWorldInvalid=0` so Indy keeps it 1 (OnTimer clears it when the transition completes).
- ⏳ **OPEN: house-entry quirk.** Entering a building, a textbox shows one tile early, and the actual DOOR warp only
  triggers after backing out + re-entering onto the door tile. Likely a coordinate/trigger-cell offset — candidates:
  Indy interior zones are smaller than 18×18 and DA applies a center-shift (map.c center_shift_x/y) our engine may
  not, shifting the door/trigger cell; OR the FirstEnter/Enter vs BumpTile door-warp ordering. Needs RE of DESKADV's
  door-warp + the small-zone camera/centering. Not a blocker (playable with the workaround).

### ⭐ v66 — door bump/text/warp ordering FIXED (user hypothesis confirmed) + whip reusable check re-attempted
RE of DESKADV (agent, runner FUN_1010_2910 / cmd exec FUN_1010_2eb6 / bump handler **FUN_1018_733e**):
- **Door bump ordering — FIXED (user was right: Indy's IACT state machine differs).** After the BumpTile
  `IactRun(event 2)`, our `OnBumpTile` has a Yoda-only branch: `(nMask & 2) && !(nMask & 0x808)` → park in
  `nFrameMode=3 + bTextDialogShown=1 + bInputLocked=1` and return. DESKADV's `FUN_1018_733e` has NO such branch —
  a bump script's text command shows its dialog SYNCHRONOUSLY inside IactRun (frame-mode transiently 5), so the
  handler just aborts the move (`if(nFrameMode==2) nFrameMode=3; break`). That persistent lock ate the next user
  press for Indy → "home sweet home" appeared one step late + the warp needed a back-and-forth. Fixed: `#ifndef
  GAME_INDY` around the lock branch → Indy falls through to the plain move-abort. (Indy IACT bits differ too:
  suppress-restore = 0x8 not Yoda's 0x808; script-warp escalation = `mask & 0x800` → frameMode 6.)
- **Whip reusable check — re-attempted (bit 16, not bit 18).** ⭐ The agent proved DESKADV tests NO bit-18
  (TILE_LIGHTSABER) flag anywhere — that was a DesktopAdventures-reimpl assumption (my v65 fix never fired). The
  weapon-class flag DESKADV actually tests is **bit 16 = TILE_LIGHT_BLASTER** (`tile+0x406 & 1`). So for Indy,
  `FireWeaponStep` now treats any NON-blaster weapon tile as reusable (never deplete/remove). ⚠ BEST-EFFORT: the
  exact DESKADV weapon-fire routine wasn't fully traced (it's in the frame-mode dispatcher `FUN_1018_9c32` ←
  `FUN_1018_0000`); verify in play. If the whip still vanishes, trace FUN_1018_9c32's fire/attack state for the
  precise reusable-vs-deplete condition + any inventory-remove call, OR instrument (log currentWeapon->frames[7] +
  GetTileData(frames[7])->flags at the equip site) to get the whip tile's exact flags.
- ⭐ LESSON (reinforced): DesktopAdventures flag/enum assumptions (TILE_LIGHTSABER bit 18) do NOT match the real
  binary — confirm against DESKADV.EXE.

### ⭐ v67 — whip FIXED (user-confirmed) + sound path FIXED + window title (temp); door NARROWED to s26/SetPlayerPos
User after v66: whip persists ✅; door STILL broken. New items: no sound, Yoda title/icon.
- **Whip did no damage — FIXED.** `UseWeapon` (0x427d20) classifies the weapon into `nType` rifle(1)/saber(2)/
  melee(3); the whip is none of Yoda's ids (0x1ff/0x1fe/0x12) so it hit the `else nType = nType!=0 ? 3 : (int)
  pSavedItem` fallback — a garbage POINTER value when the weapon has no projectile tile → `switch(nType)` default →
  `goto done` (never calls DamageEntityAt). Fixed: for Indy, a non-blaster weapon (the whip) → `nType=3` (forward
  DamageEntityAt on the cell it cracks into). `src/Worldgen.cpp`.
- **Sound — FIXED.** `SoundInit` loads each wave from `"sfx\<name>"` (Yoda's subfolder), but Indy ships its WAVs in
  the game root → every `WaveMixOpenWave` failed silently. For Indy load by bare name (`szPath[0]=0`). DeskcppView.cpp.
- **Window title — TEMP runtime override.** Title/icon come from Yoda's copied `.rsrc`. Added a GAME_INDY
  `m_pMainWnd->SetWindowText("Indiana Jones' Desktop Adventures")` in InitInstance. ⚠ TEMPORARY — the proper fix
  (USER-directed) is to build the Indy config against Indy's OWN resources (title + icon + menus, a `.res` from the
  Indy assets); then this + [[indy-app-icon]] both go away. Deskcpp.cpp.
- **Door — root cause NARROWED (not yet fixed).** Dumped the start zone's 45 scripts (remapped opcodes + text) via a
  headless play-tick dump. The house door is **s26**: `C2(7,14,828)` [BumpTile the closed-door tile] →
  `ClearTile(7,14)` [open] + **`SetPlayerPos(7,14)`** [move the player ONTO the door cell] + RedrawTile +
  RenderChanges + `SayText("Ahh, my home away from home...")` + `FlagOnce`. So the whole open+move+text IS one bump
  script (fires synchronously on the bump — matching the user's "bump, door opens, indy moves forward, says home").
  ⭐ HYPOTHESIS for the remaining "walk back then forward to warp": `SetPlayerPos` moves the player onto the (now
  open) DOOR_IN cell via the camera, but our `CMD_SetPlayerPos` does NOT run the move-onto-object / DOOR_IN warp
  check (only a fresh user walk-onto triggers `TransitionZoneDoor`) — so the player stands on the door without
  warping and must step off + back on. The v66 input-lock removal was correct (RE-confirmed) but orthogonal to this.
  NEXT: RE Indy's SetPlayerPos command handler (Indy cmd 0x11; the agent tagged FUN_1010_e934 but that looked like a
  draw-player-tiles helper — find the true handler that sets the camera + note whether it triggers the door warp),
  or add a GAME_INDY DOOR_IN-warp check after SetPlayerPos lands the player on a DOOR_IN cell. Needs live iteration.

### ⭐ v68 — user re-test: whip damage / title / audio WORK; fixed mid-textbox Replay crash
User: whip damages ✅, title ✅, audio mostly ✅ (startup wav maybe a different name — minor). New + fixed:
- **Crash: Replay Story while a textbox is open — FIXED.** `OnUpdateReplayStory` / `OnUpdateLoadWorld` only called
  `DemoDisable` (enables Save/Load/Replay for Indy/full) with NO frame-mode guard — unlike `OnUpdateNewWorld`. So
  Replay/Load stayed enabled during a text dialog (frame-mode 5), letting a re-entrant world regen fire mid-textbox
  (text drew over STUP, crashed on dialog exit). For GAME_INDY/YODA_FULL, gray them during New World's busy modes
  (1/4/5/6/9/0xb). `src/WorldgenHelpers.cpp`.
- ⏳ **Startup wav (minor).** Audio mostly works (SNDS names load by bare filename now). The startup sound may map to
  a name Indy doesn't ship / a hardcoded id — low priority; check the SNDS id→name for the intro chime vs the WAVs
  in the run folder if it matters.

### ⭐ v69 — THE DOOR FIXED at the root: Indy IACT COMMAND opcodes were off-by-one 0x0b–0x14 (anchor 211)
The v67/v68 "s26/SetPlayerPos" hypothesis was the SYMPTOM, not the cause. RE'd the Indy command dispatcher
`FUN_1010_2eb6` (jump table 1010:2f85) case-for-case and found the `kIndyCmdToYoda` table (built in v64) had a
shifted cluster. ⭐ **Indy raw command 0x11 is `RedrawTile` (DrawZoneCell(x,y)+DrawPlayer), NOT SetPlayerPos** — so
s26's `RedrawTile(7,14)` (repaint the just-opened door cell) was executing as SetPlayerPos and TELEPORTING the player
ONTO the door cell. That bypassed the movement path, so the DOOR_IN warp (which fires in `OnBumpTile` only when the
player WALKS onto an empty t==-1 cell holding a DOOR_IN object → `FindObjectAt`→`TransitionZoneDoor`) never triggered
— hence "walk back then forward to enter". With 0x11 = RedrawTile, the player stays outside the now-open (ClearTile'd,
empty) door cell and walks forward into it naturally → warp fires. **No DOOR_IN-warp-after-SetPlayerPos hack needed**
(RE conclusion: the original engine warps ONLY on a player-initiated walk into the DOOR_IN cell; nothing in the
command dispatcher reaches the door-transition fn `FUN_1018_2e48`, whose sole caller is the walk/bump handler
`FUN_1018_733e`). Corrected 8 entries in `src/IactScript.cpp` (all verified vs the decompiled switch):

| Indy raw | DESKADV behavior (verified) | was→now (Yoda op) |
|---|---|---|
| 0x0b | PlaySound (`FUN_1010_e43c` WaveMix/MCI + result bit 0x1) | 0x08→**0x0a** |
| 0x0c | RenderChanges (`FUN_1018_0670` full redraw + bit 0x80) | 0x06→**0x08** |
| 0x0e | hide player (`doc+0xc38=1`) | 0x11→**0x10** (ReleaseCamera, bHidePlayer=1) |
| 0x0f | show player (`doc+0xc38=0`) | 0x10→**0x11** (LockCamera, bHidePlayer=0) |
| 0x11 | RedrawTile (DrawZoneCell+DrawPlayer) ⭐ | 0x12→**0x06** |
| 0x12 | SetPlayerPos (playerX/Y<<5 + camera clamp + bit 0x4) | 0x13→**0x12** |
| 0x13 | RedrawTiles rect (`FUN_1010_eade`+DrawPlayer) | 0x13→**0x07** |
| 0x14 | full-zone redraw (`FUN_1010_eb1c`) — no exact Yoda twin | 0xff→**0x08** (≈RenderChanges) |

Side effects the shift also caused (now fixed): script-triggered sounds were doing a redraw instead (0x0b), some
redraws were incomplete (0x0c/0x14), and scripted player hide/show during camera pans was inverted (0x0e/0x0f). The
0x00–0x0a, 0x0d, 0x10, 0x15–0x23 entries were re-verified CORRECT. ⚠ `0x13` rect arg-order vs `DrawZoneCellRect`
unverified (cosmetic). anchor 211 (GAME_INDY-guarded), build-indy links clean. ⏳ USER: visual re-test — the house
door should now warp on a single walk-through (no back-and-forth).

### ⭐ v70 — door CONFIRMED working; removed Indy's (nonexistent) ammo bar (anchor 211)
User (after v69): the house door warps on a single walk-through now ✅. New minor bug: the whip's ammo bar shows
solid black. Root cause: `DrawWeaponIcon` (0x428c40, the Yoda green/black weapon-charge column) keys the per-shot
"spent" height off `frames[7]` matching a Yoda ammo weapon (blaster/rifle/force/saber 0x12/0x1fe/0x1ff/0x200/0x201);
the whip matches none → `nMult=0` → `nHeight = 0x1e - ammo*0 = 0x1e` → the black spent-column fills the ENTIRE bar.
⭐ RE-confirmed (DESKADV.EXE, exhaustive): **Indy renders NO ammo bar.** The green fill color 0x91 appears NOWHERE
in game code (72,288-instruction scan); the HUD-refresh twin `FUN_1010_e542` draws bevels → health PIE meter
(`FUN_1018_a242`, GDI Pie() green/yellow/red tiers) → state-icon strip (`FUN_1018_9c32`) → inventory → the weapon
BOX (`FUN_1018_adf2`, which DOES exist = bevel + 30×30 equipped-weapon icon, no ammo geometry). No charge-column
function exists — consistent with the melee whip having no ammo. FIX: `#ifdef GAME_INDY return;` at the top of
`DrawWeaponIcon` (skip the bar); `DrawWeaponBox` stays (Indy has it). `src/Worldgen.cpp`; anchor 211 (guarded).
- **Weapon box CENTERING (v70 follow-up, user-noted):** without the ammo bar, Indy centers the weapon box over the
  whole box+ammo region — 16px left of Yoda's box (onto where Yoda's ammo bar sat) + 4px down. Exact coords from the
  DESKADV UI-rect init `FUN_1010_4666`: box `[left=0x180, top=0x100, right=0x1a0, bottom=0x120]` (vs Yoda
  `0x190/0xfc/0x1b0/0x11c`). Applied as a `#ifdef GAME_INDY` override of nWeaponBox{Left,Top,Right,Bottom} in the doc
  ctor (`src/DeskcppDoc.cpp`); anchor 211 (guarded).

### ⭐ v71 — milestone 5: proper Indy RESOURCES (app icon + title); CDeskcppDoc RECT/unk docs (anchor 211)
User: playtesting build-indy finds few remaining Indy issues; centering fixed by eye. Two items done:
- **Indy app icon + title + About credits (retires the temp `SetWindowText` hack + [[indy-app-icon]]).**
  `tools/make_res.py` (+ shared `tools/reslib.py`; supersedes make_indy_res.py) builds the extended-config `.res`
  = Yoda's `.rsrc` base (our code references YodaDemo's integer resource IDs, so a wholesale swap breaks the UI)
  with only game/variant resources overridden. `make_res.py <yoda> <out> --indy <DESKADV.EXE>` (GAME_INDY) /
  `--full <Yodesk.exe>` (YODA_FULL) / else extract_res.py (demo anchor). For GAME_INDY the app IDENTITY is Indy's:
  - `IDR_MAINFRAME == 2` for this MFC app (menu/icon/doc-template string all id 2; confirmed by
    `new CSingleDocTemplate(2, …)` in InitInstance — NOT the usual 128). App icon = GROUP_ICON id 2 → member ICON
    (Yoda→ICON 11, Indy→ICON 1). The tool parses `INDYDESK/DESKADV.EXE` (a **16-bit NE** — its own NE resource-table
    reader, since `extract_res.py` only walks a PE `.rsrc`), copies Indy's GROUP_ICON 2 + member icon verbatim (icon
    DIB + GROUP_ICON dir formats are identical NE↔PE), remaps its RT_ICON ordinal to a free id (901; Yoda uses 1..11),
    and drops Yoda's GROUP_ICON 2 / ICON 11.
  - **Title = "Desktop Adventures"** — the AUTHENTIC DESKADV.EXE title (string id 2 = doc-template title AND
    AFX_IDS_APP_TITLE 0xE000/57344; both = "Desktop Adventures", NOT "Indiana Jones' Desktop Adventures" — the temp
    override's guess). The tool patches both MFC string-table blocks. The runtime `SetWindowText` override is removed
    from `src/Deskcpp.cpp` (title now flows from the resource via the doc template).
  - **About dialog (id 100 = IDD_ABOUTBOX):** rewritten with Indy's title + credits ("Indiana Jones and his
    Desktop Adventures", "The Desktop Adventures Team" + Hal Barwood/Wayne Cline/Mark Crowley/Reed Derleth/Paul
    LeFevre/Tom Payne, "© 1996 LucasArts", caption "About Desktop Adventures"). Method: SUBSTITUTE Indy text into
    YodaDemo's OWN 32-bit DLGTEMPLATE via reslib's `parse_dlg32`/`build_dlg32` (round-trip byte-validated on both
    binaries) — avoids converting DESKADV's 16-bit dialog (16-bit ordinal-width ambiguity makes a faithful parse
    fiddly). Title static widened for the longer text; About icon → app icon (id 2). The Yoda credits are ONE
    multi-line static (7-control dialog: icon, title, copyright, OK, credits-block, version, group-box frame).
- **Full-Yoda About (YODA_FULL — "the full version shouldn't say Demo"):** `make_res.py --full <Yodesk.exe>`
  replaces DIALOG 100 with the retail Yodesk.exe's (a 32-bit PE → copies verbatim) so the About reads
  "Yoda(tm) Stories", not the demo's "Yoda(tm) Stories Demo". Verified: build-full links + its About has no "Demo".
  - Wired in `CMakeLists.txt`: INDY → `make_res.py --indy`; YODA_FULL → `make_res.py --full`; demo → extract_res.py.
    Anchor unaffected (demo uses extract_res.py; no `src/` changed for About — only tools/ + CMake; the earlier
    Deskcpp.cpp title edit is in a `#ifdef GAME_INDY` block empty for Yoda). ⏳ USER: confirm both About boxes.
  - Menus left as Yoda's (Indy menu id 2 may differ in text but our command IDs match Yoda's; a swap risks
    command-dispatch mismatch — deferred as optional).
- **CDeskcppDoc struct documentation** (separate cleanup, user-requested; codegen-neutral, anchor 211): 2 mystery
  RECTs identified + reader-verified (`rectAmmoBar` +0x32b4, `rectHealthDial` +0x32c4), 3 more int-quads → RECT,
  ~14 unks resolved from Ghidra readers. Details in CLAUDE.md v71 pickup.

### ⏭ NEXT (user visual re-test `./run_indy.sh` + full-Yoda)
1. **Indy icon + title + About** — v71 resources; confirm the titlebar shows "Desktop Adventures" + the Indy icon,
   and Help>About shows Indy's credits. Full-Yoda: About should read "Yoda(tm) Stories" (no "Demo").
2. Startup wav name (minor); hero-HP tail (entity+0x90=120 in IndyGenerate tail); the two rare/uncertain opcodes
   (0x13 rect arg-order, condition specials 0/8/9/0xb/0x14..0x16); INI replay persistence.
3. (optional) Indy menus (id 2) if the menu text should read Indy's.
Method: headless `-DYODA_DEBUG` YDBG oracle (guard GAME_INDY+YODA_DEBUG, revert before anchor check). The door
warp is GUI-only — a headless repro that injects TransitionZoneDoor does NOT reproduce the walk sequence.
Key DESKADV addrs this session: IndyCyclePalette 1018:8e40, IndyParseChar 1010:a3fe / char reader FUN_1010_069c /
GetFrameTile FUN_1010_076e, IndyGenerate tail 1010:8524 (hero HP entity+0x90=0x78), IndyStartNewGameMaybe 1020:0ed0.

## Gameplay-fidelity findings + tabled TODOs (2026-07-09, user played build-indy)
User-confirmed after ACTN distribution: **world loads, zone-to-zone movement works, palette looks correct**;
**buildings can't be entered** (= the entry-trigger work above); plus two tabled items:
- ✅ **Palette cycling — RESOLVED (Indy DOES cycle; v59 was wrong).** DESKADV.EXE has a CyclePalette twin
  `FUN_1018_8e40` — structurally IDENTICAL to Yoda's `CDeskcppView::CyclePalette` (same enable-flag gate,
  same ring ranges 0xec2.../0xfc2...) — and DESKADV **sets the enable flag to 1** during palette init
  (`1010:506c: MOV word ptr [doc+0xc3c], 0x1`, the OnNewDocument twin). So Indy cycles the palette exactly
  like Yoda. Fixed: `src/DeskcppDoc.cpp` now sets `bPaletteAnimEnabled = 1` for BOTH games (dropped the
  `#ifndef GAME_INDY` guard — anchor-safe, Yoda tokens identical). The DesktopAdventures `palette_animate()
  { if(!is_yoda) return; }` is a reimplementation inaccuracy, NOT the engine's behavior.
- ⏳ **TODO: player walk-frame FACING DIRECTION wrong for Indy (likely a frame-layout/enum delta).** Yoda's
  `Character::GetFrameTile` (`src/GameObjects.cpp` ~L164) indexes `frames[24]` = 3 anim banks × 8 facing dirs
  (banks at +0/+8/+16; row0=up, row1=down, rows2-4=left dy+3, rows5-7=right dy+6). But Indy's ICHR frame block
  is **0x2a bytes = 21 shorts** (vs Yoda's 0x30 = 24) — so Indy has a DIFFERENT per-bank direction count/order
  (21 = 3×7?), which shifts the facing index. Fix: RE DESKADV.EXE's `GetWalkFrameTile`/`GetFrameTile` twin for
  Indy's exact frame-row layout, then add a `GAME_INDY` branch in `Character::GetFrameTile` (and check
  `ParseChar`'s Indy frame count — currently reads 0x2a into a `frames[24]`). Visual oracle: `./run_indy.sh`.

### ⚠ (historical) RUN-TEST NEXT (user visual oracle): `./run_indy.sh` — does the generated Indy world RENDER + PLAY?
Worldgen converges, but headless can't verify rendering/playability. Remaining honest uncertainties:
1. **Hero HP / clock / UI-timer tail** — `// TODO(integration)`: the DESKADV tail resets the hero Character's
   HP (entity+0x90=120) + a UI timer; our tail sets doc fields only, so the player may spawn with wrong HP.
2. **Perpendicular-"clear" predicate** in edge-island growth + carve fork stamps (DESKADV `0x12f < neighbor`,
   modeled `>= PLAN_FORK_S`) — could still produce a slightly-malformed map layout.
3. **PlaceQuestNode case-1 edge arm** (object-less OR teleporter accept — pairing intent).
4. **`IndyPickUnplacedProvidedItem` bAvoidRange** now passes the order slot (was hardcoded 1) — the step-0
   item-exclusion range (0x21c–0x21f) path is now exercised; confirm no bad picks.
5. INI persistence (Save/Load placed-zone + story history) omitted — replays won't persist yet.
The DESKADV functions are all named in Ghidra (`program=DESKADV.EXE`) for re-decompiling any suspect piece;
the `-DYODA_DEBUG` headless trace (YDBG in DebugLog.h) is the proven fast oracle for worldgen-logic bugs.

## Anchor discipline
Every `GAME_INDY` guard's fall-through (no macro) must be the exact Yoda code, so `progress.py` stays 211
and all byte-match oracles pass. Same rule as H2. Only the extended `build-indy` config exercises Indy.

## v72 (2026-07-10) — MIDI music DONE (user-confirmed audible) + sound-id remap + intro zone 0x78

**Indy's music is MIDI played through MCI command strings** (user lead: "Indy uses MIDs"; RE'd from
DESKADV.EXE, all functions named in Ghidra: `IndySoundInit` 1018:4c54, `IndyPlaySound` 1010:e43c,
`IndyPlayThemeMusic` 1018:6dd0, `IndyStopAllMusic` 1018:6e34, `IndyViewOnUpdate` 1010:e1aa,
`IndyViewTeardownMaybe` 1010:dff0, `IndyOnToggleMusic` 1010:c092 — plate comments carry the detail).

The 16-bit model (mirrored in our port, all `GAME_INDY`-guarded):
- SNDS ids **0x00–0x0d = WAVs** (WaveMix, as before), **0x0e–0x11 = MIDs** (FLOURISH/THEME/DEFEAT/
  VICTORY), plus two hardcoded extras outside SNDS: **eerie.mid = id 0x12** and **eep.wav = id 0x13**
  (the 15th wave — twin of Yoda's id-0x25→handles[5] eep quirk). Enum `IndySoundId` in DeskcppView.h.
- MIDs: `mciSendString("open sequencer!<file> alias <NAME>")` at SoundInit (independent of the WaveMix
  session — DESKADV opens them even when WaveMix fails), `"play <NAME> from 1"` in PlaySound's music arm
  (gated by nMusicEnabled + per-id opened flag; 16-bit flag table = DGROUP word @0x53c+id*2),
  `"stop <NAME>"` on ToggleMusic-off + new game, `"close <NAME>"` in the view teardown.
- Startup: OnUpdate(399) opens+plays THEME (our port: SoundInit opens all, then the existing
  `PlaySound(0x3d)` remaps to THEME). DESKADV has a `themw3.mid` fallback when doc+0x56==0x1e
  (legacy VERS-0x1e-era data) — not reproduced, DESKTOP.DAW is VERS 0x200.
- New game (`IndyStartNewGameMaybe`): StopAllMusic then PlaySound(0x0e FLOURISH) — our StartGame's
  existing `SoundFlush(); PlaySound(0x3a)` gains the StopAllMusic and the remap does the rest.

**⭐ Sound-id remap (`Indy_MapSoundId`, DeskcppView.cpp):** the two games' SNDS tables DIFFER (Yoda
5=eep/6=nogo vs Indy 5=ROAR/6=DOOR/8=NOGO...), so every hardcoded Yoda id in shared code played the
WRONG Indy sound. Under GAME_INDY, `PlaySound` translates hardcoded Yoda ids (5→0x13 eep, 6→8 nogo,
0xb→7 explode, 0x3a/0x3d/0x3e/0x3f→FLOURISH/THEME/DEFEAT/VICTORY MIDs; Yoda-only concepts
0x1f–0x23/0x2a/0x2b/0x31/0x34/0x37 → −1 silent), while the
data-driven (Indy-native) ids — IACT `pCmd->args[0]`, `pWeapon->weaponCharId` — bypass via
**`PlaySoundData`** (demo: `#define PlaySoundData PlaySound`, token-neutral).

**✅ v73 remap VERIFIED against DESKADV ground truth** (agent sweep: both SNDS tables extracted from
YODESK.DTA / DESKTOP.DAW + ALL 28 `IndyPlaySound` call sites disassembled): Indy's WAV table ends at
0x0d (`IndyPlaySound` branches `if (id < 0xe)`; 0x0e–0x11 MIDI, 0x12 eerie, 0x13 eep hardcoded), so
every Yoda id ≥0x1f has no possible Indy WAV. Hardcoded ids DESKADV ever pushes: {0,1,3,4,8} SFX +
{0x0e,0x10,0x11} music + 0x13 eep — key twins: damage→0x13 @1010:1bd4, bump-NOGO→8 @1018:837f/83e6
(FUN_1018_733e = OnBumpTile twin), walk→3 @1018:b09b, win→0x11 @1018:21d7, lose→0x10 @1018:2497,
new-game→0x0e @1020:0fa9. IACT dispatcher @1010:33b6 pushes the RAW script arg (matches our
PlaySoundData bypass — no double remap). Every mapping CONFIRMED, none WRONG, none missing; the
Indy-only WAVs (ROAR/DOOR/WHIP/GUNSHOT/MACHETE/SPEAR/ARROW) are all data-driven and reachable.
Nit: Yoda SNDS order is 0x1f=saberout, 0x20=armed (both −1, no functional impact).

**Intro zone:** the v71 "hero-HP tail (entity+0x90=120)" was a misread — 16-bit view+0x90 is our
`nTargetZoneId`, and 0x78=120 is **Indy's intro zone id** (Yoda 0x5d=93); IndyStartNewGameMaybe pushes
0x78 both to the 5-step ZoneTransitionStep intro loop and view+0x90. Both StartGame literals now
GAME_INDY→0x78.

**Gotcha:** `mmsystem.h` defines `PlaySound`→`PlaySoundA` — `#undef PlaySound` after including it.
CMake links `winmm.lib` (all extended configs; unreferenced elsewhere). Anchor: 211 exact, link
0/0/exit0, bugscan 0/0/0 after all edits. USER-CONFIRMED: theme MIDI audibly plays on launch.

## v93 (2026-07-18) — locator/overview MAP tiles fixed (user: "map tiles handled differently")

**Symptom:** the L-key locator/overview map rendered garbage in Indy — random terrain/object tiles
(stone walls, foliage, a treasure chest, even a character portrait) instead of map-cell icons.

**Root cause:** `CacheUiTilePtrsMaybe` (DeskcppDoc.cpp, YODA 0x41a5d0) hardcodes the 20 UI/locator
tile catalog indices **817–837** — valid in Yoda's ~2128-tile catalog (demo 2128 / full 2123, nearly
identical) but WRONG for Indy's **1144-tile** DESKTOP.DAW, where those indices land on ordinary
terrain/object tiles. (Tile counts measured directly: TILE chunk len / 0x404.)

**Ground truth — `IndyCacheSpecialTilePtrsMaybe` (DESKADV 1010:42be):** caches 20 far-ptr (4-byte)
tile entries from the tile array (`doc+0x60`) into `doc+0x340..+0x38c`; **index = srcOffset/4**.
Recovered Indy indices (slot→idx): 0=377 1=375 2=363 3=364 4=365 5=366 6=367 7=371 8=373 9=369
10=368 11=372 12=374 13=370 14=376 15=639 16=408 17=1138 18=1131 19=1139. Slots **2–14 are a
contiguous locator-icon block whose deltas are BYTE-FOR-BYTE identical to Yoda's 817-based block**
(strong cross-validation of the slot mapping); slots 0,15–19 are the separate player-marker / cursor /
arrow specials at Indy-specific positions. Rendered from the DAW with `indy_palette` these are exactly
a coherent map-icon set (map-cell bg + circle / puzzle-piece / door-arch / edge+corner markers /
filled-star "you are here" [slot 15] / red-X / hollow-star / 4-way travel crosses).

**Second delta — `IndyDrawLocatorMap` (DESKADV 1010:bb60, ≡ DrawLocatorMap 0x423df0):** Indy fills the
canvas with palette index **0x4c** (its olive map-bg colour, `Canvas::Fill` = FUN_1010_11be) and draws
**NO per-cell background tile**; Yoda instead fills 0 and blits tile **0x344** under every cell (its
"blank map cell" graphic). Empty Indy cells therefore just show the 0x4c fill.

**Icon SELECTION unchanged:** `IndyGetLocatorIcon` (1010:402e) has the same zone-type→slot switch and
same 0..0x13 / −1 returns as our byte-matched `GetLocatorIconMaybe` — so no selector work needed, only
the tile-index resolution + fill/bg.

**Fix (both GAME_INDY-guarded, anchor fall-through = original Yoda):** DeskcppDoc.cpp
`CacheUiTilePtrsMaybe` #ifdef branch with the Indy indices; Worldgen.cpp `DrawLocatorMap` fills 0x4c and
`#ifndef GAME_INDY`-skips the 0x344 bg blit. Verified by rendering the recovered tiles from DESKTOP.DAW
(new = coherent icons, old 817–837 = terrain garbage). Anchor re-oracled: 211 exact / 99.17 %, bugscan
0 HIGH/0 SHIFT, vt 10 CLEAN, msg 11 CLEAN. Indy SDL build compiles clean. NOT yet live-screenshot in the
running map (opening the overview needs the locator item in inventory) — tile-render proof stands in.

## v93 tail — save/load investigation (Indy scripts): TWO distinct issues

**Issue A (user-reported): loading an Indy save deserializes scripts wrong.** Established: the
user's save is OUR OWN format — there are NO Indy `.wld` files anywhere, our reader
(Worldgen.cpp:5804) hard-requires magic `"YODASAV44"` and rejects everything else with
AfxMessageBox(0xe008), and our writer (5452) also stamps `"YODASAV44"` in the Yoda 32-bit layout
with no GAME_INDY branch. So the file was ACCEPTED ⇒ our format ⇒ self-consistent ⇒ scripts
*should* round-trip (write N doneFlags, read N doneFlags; the entity/object reads consume exactly
what was written, so the cursor reaches the script list at the right offset regardless of struct
sizes). Load flow: read magic/seed/planet → **StartGame(0,1) REGENERATES via IndyGenerate** → read
questItems/mapGrid → LoadZoneRecursive→ReadSavedState overlays doneFlags onto the regenerated
zones. LEADING HYPOTHESIS (needs the exact symptom to confirm): IndyGenerate re-run at load is not
bit-identical to the original generation (cf. the known unseeded-rand planet re-pick), so saved zone
state doesn't map onto regenerated zones (LoadZoneRecursive's `savedId!=child` early-out, or a
script-count mismatch). Alt: entity-count drift (save-time LIVE count vs regenerated count) →
`entities[i]` OOB (ReadSavedState does NOT grow entities, unlike objects) → corruption. PENDING: the
precise user symptom (events replay? crash? corrupt world?) to pick the branch.

**Issue B (found while investigating A — SEPARATE, real, latent): retail Indy `.sav` files can't be
loaded at all.** Agent RE (read twin FUN_1010_1dd4, write twin FUN_1010_2108, both disassembled
byte-by-byte): retail Indy's save is magic `"INDYSAV44"` (string @1010:d7c8) in a **uniformly 16-bit
format** — every count/flag Yoda writes as 4-byte `int` is 2 bytes, and fields are dropped: zone
header = 4 shorts (tempVar/randVar/doorReturnX/Y, NO globalVar/planet) = 8B vs our 20B; activatedFlag
2B; ZoneObj = state(2)+arg(2) only (4B vs our 12B); MapEntity = 16 contiguous shorts ent+0x4..+0x22
(32B, no waypoints/unk60, vs our 92B); each doneFlag 2B; LoadZoneRecursive child `savedFull` is 2B.
Struct OFFSETS are shared (type@4/state@8/x@0xa/y@0xc/arg@0xe) so a fix is serialization-width only,
not struct redef. worldSeed/currentPlanet stay 4-byte (`long`) even in 16-bit Indy. Fix shape (all
GAME_INDY-guarded, line-neutral) if we want retail-save compat: 16-bit branches in Zone::Read/Write
SavedState (Iact.cpp), LoadZoneRecursive/SaveZoneRecursive child header (WorldgenHelpers.cpp), and
Serialize magic `"INDYSAV44"` + 16-bit quest-cell/MapZone grid (Worldgen.cpp). DESKADV twins:
IndyLoadWorldState 1010:b890, reader FUN_1010_1dd4, writer FUN_1010_2108, LoadZoneRecursive twin
FUN_1020_0af0. NOTE: implementing B also changes what WE write — either re-stamp our saves INDYSAV44
16-bit (breaks existing our-format saves) or keep writing YODASAV44 and ALSO accept INDYSAV44 on read.

### v93 UPDATE — Issue A fix IMPLEMENTED (zone-state 16-bit field-set)

Implemented Parts 1 (Zone::ReadSavedState/WriteSavedState, Iact.cpp) of the Indy save spec as
GAME_INDY branches (`#ifdef GAME_INDY … return; #endif` at the top of each; anchor fall-through
100% untouched — 211 exact re-verified). The Indy zone-state record now matches retail
FUN_1010_1dd4/2108: 2-byte fields; zone header = tempVar/randVar/doorReturnX/doorReturnY only (NO
globalVar/planet); each ZoneObj = state+arg only (NO type/x/y); each MapEntity = its first 16 fields
only (charId,x,y,damageTaken,active,unk10,bulletX,bulletY,aiStepCounter,unk18,bRetreating,unk20,
bulletDX,bulletDY,bulletStep,seqIdx — NO waypoints/item/unk2c/bRefreshFrame/numItems/timer/wanderDir/
unk60); doneFlags 2B. This stops the loader OVERWRITING regenerated per-object type/x/y and zone
globalVar with stale saved values — the "loads OK, specific scripts broken" cause. Write/read are
exact mirrors ⇒ self-consistent round-trip (proven by construction; user to confirm the script
behaviour in playtest — headless save→load automation blocked by the SDL file-picker). Kept
YODASAV44 magic + 32-bit main-serialize for now (a self-consistent hybrid); the recursive
header full-flag + main-serialize stay 32-bit until Part 3 lands. Existing 32-bit-zone-state Indy
saves won't load (none exist on disk). REMAINING for full retail INDYSAV44 compat (issue B): magic
→ "INDYSAV44", recursive full-flag 2B (SaveZoneRecursive/LoadZoneRecursive + main-reader nArg), and
the 16-bit main-serialize (quest-cell 9-short record + doc tail) — Part-3 struct RE in progress
(MapZone/CDeskcppDoc offset→name).

### v93 FINAL — Issue B DONE: full retail INDYSAV44 read+write

Our GAME_INDY build now reads AND writes the real retail Indy save format (magic "INDYSAV44").
New GAME_INDY-only methods `CDeskcppDoc::IndyWriteWorldState` / `IndyReadWorldState` (Worldgen.cpp
tail) implement the full 16-bit main-serialize per the DESKADV spec: seed(4) only header (no
planet/bQuestCellsResident); ONE quest-item list; ONE 10x10 grid of 9 shorts/cell in order
[cellItemC,cellQuestSlot5,cellQuestSlot6,id,cellQuestSlot0,cellQuestSlot1,cellItemA,cellItemB,
zoneType]; ONE sentinel-terminated zone-recursive block; inventory count(2)+ids; compact doc tail
[zoneIdx,playerX,playerY,weaponIdx,cameraX,cameraY,health,maxHealth,difficulty](2 each)+
elapsed(4)+totalZones(2)+checksum(count2,sum2) — NO weaponState, NO goal-item pair. OnSaveWorld/
OnLoadWorld get a minimal `#ifdef GAME_INDY` branch to these (magic check accepts "INDYSAV44");
SaveZoneRecursive/LoadZoneRecursive full-flag narrowed to 2B for Indy. Per-zone state records go
through the v93 Zone::Write/ReadSavedState 16-bit branch (issue A). ⭐ VERIFIED: new `save_smoke`
harness (microfx/harness + PortableSDL.cmake) drives IndyWriteWorldState→IndyReadWorldState and
confirms round-trip across seeds 1/42/7 — playerX/playerY/health/difficulty/seed/totalZones all
survive (tail scalars surviving ⇒ the WHOLE file incl. every zone-state/script record stayed
cursor-aligned), no crash, ~3.5KB saves (vs Yoda ~14-90KB — the compact 16-bit format). Anchor
re-verified: 211 exact, bugscan 0/0, vt 10 CLEAN, msg 11 CLEAN, link 0 unresolved. ⚠ MEDIUM-confidence
fields (cell item/slot identities, single quest-list semantics, totalZones=doc+0x48) round-trip fine
for OUR saves (self-consistent) but reading a REAL retail Indy .sav is the remaining validation — no
retail .sav on hand to test. Docs: the two agent specs are condensed above under "INDYSAV44".
