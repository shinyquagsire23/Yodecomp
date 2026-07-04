# YodaDemo.dta — asset file format & parser map

Yoda Stories' game data lives in `YodaDemo.dta` (demo) / `YODESK.DTA` (full), an **IFF-like
tagged-chunk** file. Reverse-engineered here by cross-referencing YodaDemo.exe's loader with the
user's engine recreation `~/workspace/DesktopAdventures` (`src/assets.c`, `scrdoc.txt`, `SCRIPTS.md`).

## Loader
`Dta_Load` (**0x00422670**) — opens the `.dta` via MFC `CFile` (path from `AfxGetModuleState`),
shows a progress bar, then loops: **read a 4-byte tag**, inline-`strcmp` it against the tag table,
and dispatch to a per-chunk parser. All reads go through the `CFile` vtable: **`Read` = vtable slot
`+0x3c`** (`(**(code**)(*pFile + 0x3c))(buf, len)`); the seek/tell helper is slot `+0x30`.

The read tag is compared against the tag table in **`.data 0x456890–0x456908`** (4 bytes each):
`ENDF ACTN HTSP ZAX3 ZAX2 ZAUX VERS ZONE PUZ2 SNDS CAUX CHWP CHAR TNAM TILE STUP` (+ index tags
`IZON`/`IPUZ` @ 0x4560f0). `VERS`/`STUP`/`TILE` are read inline; the rest dispatch to handlers.

## Tag → handler map (verified from the dispatch in `Dta_Load`)
| Tag  | Handler (renamed)        | Addr     | Parses (per assets.c / scrdoc.txt) |
|------|--------------------------|----------|-----------------------------------|
| ZONE | `Dta_ParseZone`          | 0x422f60 | count + N zones/maps → array @ `doc+0x94/0x98`; per-zone via `Dta_ReadZone` (0x426a00) |
| ZAUX | `Dta_ParseZaux`          | 0x423110 | zone auxiliary data |
| ZAX2 | `Dta_ParseZax2`          | 0x423210 | zone auxiliary 2 |
| ZAX3 | `Dta_ParseZax3`          | 0x423190 | zone auxiliary 3 |
| HTSP | `Dta_ParseHtsp`          | 0x4236b0 | hotspots (per-zone interaction points) |
| ACTN | `Dta_ParseActn`          | 0x423510 | zone action scripts (opcodes → scrdoc.txt) |
| PUZ2 | `Dta_ParsePuz2`          | 0x422fd0 | puzzles |
| SNDS | `Dta_ParseSnds`          | 0x4233f0 | sound-file name list (CString paths) |
| CAUX | `Dta_ParseCaux`          | 0x423290 | character auxiliary |
| CHWP | `Dta_ParseChwp`          | 0x423300 | character weapons |
| TNAM | `Dta_ParseTnam`          | 0x423380 | tile names: short id + 24-byte name → `tile+0x408` (CString) |
| ENDF | (triggers Worldgen)      | —        | end-of-data marker → runs world generation, NOT a parser |
| VERS | inline                   | —        | version `long` |
| STUP | inline                   | —        | startup graphic (uses palette) |
| TILE | inline                   | —        | tiles: `(32*32)+4` bytes each (graphics) |
| CHAR | `Dta_ParseChar`          | 0x421e70 | character base records → `doc->characters[id]` (World+0xc0) |

**Character subsystem** (records at `doc->characters` = `World+0xc0`, indexed by char id):
`Dta_ParseChar` (0x421e70, CHAR) reads the base character records; then `Dta_ParseCaux` (0x423290, CAUX)
and `Dta_ParseChwp` (0x423300, CHWP) attach auxiliary + weapon data to `characters[id]` (both loop:
read `short id`; `id<0` skips, else index `characters[id]` and read that record's data). Format details
in `~/workspace/DesktopAdventures/src/character.c` (animation frames, health, weapon frames, etc.).

**On `ENDF`:** when `Dta_Load` reads the end-of-data tag it doesn't parse a chunk — it kicks off the
**random world generation** (the "WorldGen" megafunction area):
- `Worldgen_Randomize` (0x424380) — seeds the RNG from `GetCursorPos`+`time()`+`clock()` (`srand`
  @0x42a640) and packs `rand()` bytes into a value; begins randomizing the world.
- `Worldgen_Populate` (0x425e30) — places entities/items into zones using `rand()` + document methods
  (`Zone_*` accessors via vtable +0x68/+0x6c; helpers `FUN_00421460`, `FUN_004269a0`).
So the `.DTA` **load** (Dta_*) and the **world generation** (Worldgen_*) are driven from the same
`Dta_Load` loop — asset load first, then generate on ENDF.

All handlers are `__thiscall(CDocument *doc, CFile *pFile)`. The document object (the `this`) holds the
parsed asset arrays; known offsets so far:
- `doc+0x84` → tile array base (TNAM writes names at `tile[id]+0x408`)
- `doc+0x94/0x98` → zone pointer array (`SetSize` + per-zone pointers)
- `doc+0x32f8`, `+0x32d4/0x32d8` → load-state / progress-bar geometry

## ZONE records (maps) — `Dta_ParseZone` → `Dta_ReadZone` → Zone class
`Dta_ParseZone` (0x422f60) reads a `short` count, `SetSize`s the zone array at `doc+0x94/0x98`, then
loops `Dta_ReadZone` (0x426a00) per zone. `Dta_ReadZone`:
1. Reads a `short` (zone id / planet) + a `long`. Only keeps the zone if its planet matches
   `doc+0x2e3c` **or** the id is in a hardcoded allow-list (demo-included zones: 0x4c–0x4d, 0x5d–0x60,
   0x10f, 0x1d7, 0x217, 0x282, …) — this is how the **demo** ships a subset of the full game's zones.
2. `operator new(0x848)` → **`Zone_Ctor`(obj, 0x12, 0x12)** — an 18×18 grid zone object (sizeof 0x848),
   which builds 6 `CDWordArray`s (3 tile layers + overlay + hotspots + actions).
3. `Iact_ReadIzon`(obj, pFile) reads the IZON body, then loops reading sub-records.

**ZONE record layout** (from `~/workspace/DesktopAdventures/src/map.c`, `assets.h`):
- `u16 width, height` (18×18), flags, `u8 area_type` — `{UNUSED, DESERT, SNOW, FOREST, UNUSED, SWAMP}`.
- 3 tile layers **low / middle / high**, each `width*height` × `u16` tile ids, + an overlay layer.
- **Hotspots** (`object_info`, gated by `htsp_offset`): types `{QUEST_ITEM_SPOT, SPAWN, THE_FORCE,
  VEHICLE_TO/FROM, LOCATOR, ITEM, PUZZLE_NPC, WEAPON, DOOR_IN/OUT, LOCK, TELEPORTER, XWING_FROM/TO}`.
- **Zone/map flags** (purpose): `{NOP, ENEMY_TERRITORY, FINAL_DESTINATION, ITEM_FOR_ITEM,
  FIND_SOMETHING_USEFUL_NPC, ITEM_TO_PASS, FROM/TO_ANOTHER_MAP, INDOORS, INTRO_SCREEN, FINAL_ITEM,
  MAP_START_AREA, VICTORY/LOSS_SCREEN, …, FIND_THE_FORCE}`.
- **IACT** action scripts per zone (`num_iacts`); opcodes documented in `scrdoc.txt` (`FirstEnter`,
  `BumpTile`, `CheckEndItem`, `HasItem`, `HealthLs`, …).
- The **IZON index** (`izon_data` in assets.h): offsets to izon/izax/izx2/izx3/izx4/htsp/iact bodies +
  `num_iacts` + `iact_offsets[0x100]`.

### Zone object layout (class @ 0x405150, vtable 0x44b1c0, sizeof 0x848)
Recovered from `Zone_GetTile`/`Zone_SetTile`/`Zone_FindObjectAt`/`Zone_GetEdgeCode`:
```
+0x00  vtable (0x44b1c0)
+0x04  int   type          (== 8 => special, e.g. indoor)
+0x0c  short width          (= 0x12 / 18)
+0x0e  short height         (= 0x12 / 18)
+0x10  u16   tiles[H][W][3] flat grid; index = ((y*18 + x)*3 + layer)*2 + 0x10
                            layer 0=low 1=middle 2=high; size 18*18*3*2 = 0x708 (ends +0x718)
+0x7a8 CObArray objects     m_pData @+0x7ac, m_nSize @+0x7b0
                            object record: +0x08 type(==1 active), +0x0a x, +0x0c y
+0x7d0 CObArray (2nd list)  m_pData @+0x7d4, m_nSize @+0x7d8
... (6 CObArray/CDWordArray members total: tile-layer helpers + objects/hotspots/actions)
```
Named methods: `Zone_Ctor` (0x405150), `Zone_Dtor` (0x4054d0), `Zone_ScalarDtor` (0x405300),
`Zone_GetTile` (0x405430), `Zone_SetTile` (0x405480), `Zone_FindObjectAt` (0x405330, find object by
x,y with type==1), `Zone_GetEdgeCode` (0x405380, classify a coord as in-bounds / off-edge direction —
used for map-to-map transitions), `Iact_ReadIzon` (0x405ae0).

### Zone-loading pipeline (named 2026-07-04)
`Dta_ReadZone` (0x426a00) `new`s a Zone, calls `Zone_Ctor` (0x405150, 18×18) + `Iact_ReadIzon`
(0x405ae0, IZON body), then the per-tier aux readers — all also invoked later by the `Dta_ParseZax*`
chunk parsers, one per stored zone object:
- `Iact_ReadZaux` (0x406270) — ZAUX record   `Iact_ReadZax2` (0x406410) — ZAX2
- `Iact_ReadZax3` (0x406490) — ZAX3           `Iact_ReadZax4` (0x406510) — ZAX4
Each is `__thiscall(Zone *zone, CFile *pFile)` reading that tier's data into the zone. So the aux
chunk parsers (`Dta_ParseZaux/Zax2/Zax3`) just iterate `doc->zoneObjects[i]` and call the matching
`Iact_ReadZax*`. (In `src/Dta/Dta.h` these are modeled as `ZoneAux::Load*` — a masked reloc, so the
name doesn't affect the byte-match; the real target is a `Zone` method.)

### Struct/type modeling in the Ghidra DB (2026-07-04)
Defined & applied: `World` (game document — `zones MapZone[100]@0x4b4`, `totalZones`, `score`,
`zoneObjects Zone**@0x98`, `zoneCount@0x9c`, `tileArray@0x84`, `currentPlanet@0x2e3c`, `counter@0x3320`),
`MapZone` (0x34 overview cell), `Zone` (0x848 map class), `ZoneObj`, and `CFile`+`CFile_vtbl` (`Read`
fn-ptr @ vtbl+0x3c). Applied `this`/param types across the World-score, Zone, and Dta-parser functions
so the decompiler emits `this->zoneObjects[i]`, `this->tiles[..]`, `pFile->vtbl->Read(..)` directly.

### Module dependency (wrapping)
`Dta_Load` (0x422670, world-load module) → `Dta_ParseZone` → `Dta_ReadZone` → **Zone class**
(`Zone_Ctor`/`Iact_ReadIzon`, the 0x405150 module, vtable 0x44b1c0) → MFC `CDWordArray`. This is the
`sith*`-wraps-`rd*` style layering: the `.DTA` loader wraps the Zone class wraps MFC containers.

## Next
- Name `Dta_ReadZone` (0x426a00) internals + the ZONE record struct (from `scrdoc.txt` zone layout).
- Resolve CHAR / TILE handlers (0x424380 / 0x425e30) and the VERS/STUP inline reads.
- These parsers are small and data-driven → good byte-match targets for a `src/Dta/` module next.
