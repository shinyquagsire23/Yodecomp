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

### ⏭ NEXT = the entry-script → nFrameMode advance (the whip + scripted first-entry + revert the bWorldInvalid workaround)
Distributing ACTN did NOT by itself fix the whip or let us revert the `bWorldInvalid=1` workaround. Verified
headlessly (v59): with `bWorldInvalid=0`, OnTimer case-0xb → `WorldEntryStepMaybe` STILL loops 0→5→0 forever —
step 5 unconditionally sets `nTransitionStep=-1`, and only an **entry-triggered IACT script advancing nFrameMode**
breaks the cycle. `IactRun(4)`/`IactRun(5)` at step 5 fire only scripts whose condition is `COND_FirstEnter`(0)/
`COND_Enter`(1) with `event==4` (`src/Iact.cpp` ~L481). But the Indy start zone (id 0, type 17) has 9 scripts and
**none are cond 0/1 under Yoda opcode numbering** — script[0] = `cond 4` (COND_Walk) / `cmd 13` (CMD_MoveCamera).
So no entry script matches, nFrameMode never advances, and the workaround (ZoneTransitionStep, which reaches play
mode WITHOUT running entry scripts) stays. **Hypotheses to chase next (in order):**
1. **Indy IACT trigger/opcode semantics.** DA (`~/workspace/DesktopAdventures/src/iact.c`) uses the SAME command
   enum names for both games, so opcodes may NOT be wholesale-renumbered — but the ENTRY trigger may map differently
   (Indy's "first enter" could be a different condition opcode, or `IactRun`'s `event` arg for entry differs). RE
   DESKADV.EXE's `IactRun`-equivalent condition switch to confirm which condition opcode = zone entry for Indy, and
   add a `GAME_INDY` branch in `Zone::IactRun` if needed.
2. **The whip / starting weapon.** `currentWeapon` starts 0; it becomes a weapon only when the whip is in inventory
   and selected (`src/DeskcppView.cpp` ~L1580; weapon tiles 0x1ff–0x205 / a Character with `frames[7]==0x12`). The
   whip must be granted at world start — either by an entry script's `CMD_AddItemToInv` (blocked by #1) or by the
   worldgen tail directly (DESKADV `IndyGenerate` tail — check if it seeds a starting weapon / inventory item; the
   hero-HP TODO at `IndyGenerate` tail is the same "tail sets player state" area).
Method: the headless `-DYODA_DEBUG` YDBG oracle (proven this session) + DESKADV.EXE RE. `tmp/actn_sim.py` is the
ACTN raw-byte simulator if the format ever needs re-checking.

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
