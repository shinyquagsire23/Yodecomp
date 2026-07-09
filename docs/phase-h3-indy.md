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
- **Named DESKADV.EXE functions so far:** _(none yet — milestone-2 deltas were recovered from raw DAW
  bytes, not DESKADV code. Name them here when the worldgen/logic deltas force DESKADV decompilation.)_

## Anchor discipline
Every `GAME_INDY` guard's fall-through (no macro) must be the exact Yoda code, so `progress.py` stays 211
and all byte-match oracles pass. Same rule as H2. Only the extended `build-indy` config exercises Indy.
