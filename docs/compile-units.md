# YodaDemo.exe — app-region compile-unit / module map (first pass)

Derived 2026-07-04 by segmenting the app region (`0x401000–0x429000`, 534 functions) on the
signal that **MSVC emits each translation unit's `.rdata`/`.data` (vtables, strings, FP consts,
globals) contiguously, in the same order as its `.text`.** A function's lowest own-data reference
therefore steps up at each `.obj` boundary. Shared base vtable `0x44ccc4` (`CObject`, ref'd by 16
funcs) was excluded as noise. Reproduce with `tools/segment_cus.py` over `toolchain/test/cu_refs2.txt`;
themes from `cu_strings.txt`, call-flow from `cu_calls.txt`.

Granularity caveat: adjacent tiny `.rdata` steps (e.g. `44b148→44b160`, one small vtable apart) may be
separate classes *within one `.obj`*, so treat fine splits as class-level, the big blocks as modules.

## Major modules (the architecture)
| .text range | funcs | module (theme from strings) | key strings |
|---|---|---|---|
| `0x401ac0–0x4042b0` | 69 | **GameData** — reads `YodaDemo.dta`; 9 doc-methods typed `World*` (`World_FindTile` 0x403aa0, `World_GetTileData` 0x403a40, `Game_RefreshZone` 0x403ae0) | `GameData`, `Nevada`, `Alaska`, `Oregon` |
| `0x408c60–0x40a560` | 26 | **Core utilities** — called by everyone, no strings | (none) |
| `0x40a560–0x418700` | **107** | **Game UI / view / hints / sound** | `OPTIONS`, `MS Sans Serif`, `MIDILoad`, `Super Jedi!`, `goyoda`/`gojedi` (cheats), `This is a DOOR/WEAPON/EWOK…` tile hints, `Congratulations! You've won…` |
| `0x419730–0x41b2f0` | 20 | **Logging** | `c:\yodalog.txt` |
| `0x41b2f0–0x41bee0` | 28 | **Settings / registry** | `GameSpeed`, `LCount`, `LScore`, `HScore` |
| `0x41c340–0x429000` | **130** | **World gen + `.wld` save/load** | `*.wld`, `World Files (*.wld)`, `ASAV44` (save sig), `!!!!No Place to put Find Puzzle!!!` |

### Interaction / wrapping (inter-segment call counts)
- **UI/view (`40a560`)** is the top layer → calls Core `408c60` (39×), WorldGen `41c340` (32×), GameData `401ac0` (22×).
- **WorldGen (`41c340`)** ↔ mutually coupled with UI and Core → calls GameData `401ac0` (25×), Core `408c60` (14×), UI `40a560` (9×).
- **GameData (`401ac0`)** is the lower data layer → calls WorldGen `41c340` (8×), Core `408c60` (3×).
- **Core (`408c60`)** is shared infrastructure used by all three big modules.

Rough layering: **UI → WorldGen → GameData**, with **Core** shared throughout (like OpenJKDF2's
`sith*` wrapping `rd*`). The `World_*` score unit (`0x401450–0x401ab9`, see CLAUDE.md) sits just
below GameData in .text and is its own tiny group (FP pool `0x44b09c`).

## Full 27-segment table (class/module granularity)
```
.text range        funcs  .rdata block   .data     note
401180-4011d0      2      44b050         -         MFC helper class (vtable 44b050)
4011d0-401390      12     44b068         -         MFC helper class(es) (ctor/dtor + scalar-deleting dtors)
401390-401490      4      44b080         -         MFC helper class; incl World_UpdateScore(401450)
401490-401ac0      5      44b09c         -         World_* score unit (FP pool 44b09c) ✅bytematched 401490
401ac0-4042b0      69     -              4560b0    ** GameData module **
4042b0-404480      9      44b148         -         small class
404480-404670      1      -              4560f0    (singleton)
404670-404c80      9      44b160         -         small class
404c80-404da0      4      44b178         -         small class
404da0-404ed0      5      44b190         -         small class
404ed0-405150      6      44b1a8         -         small class
405150-405ae0      18     44b1c0         -         module (no strings)
405ae0-4070e0      9      -              4560fc    (data-anchored)
4070e0-408110      17     -              456104    module (calls WorldGen/Core heavily)
408110-4086b0      3      -              459e28    small
4086b0-408c60      3      44b578         -         small
408c60-40a560      26     44b638         459458    ** Core utilities **
40a560-418700      107    44b748         4560f8    ** Game UI / view / hints / sound **
418700-418b10      9      44bc68         -         small class
418b10-418c30      5      44bc80         -         small class
418c30-418dd0      6      44bc98         459558    small class
418dd0-419120      8      44bd30         -         small class
419120-419730      8      44bf30         459e20    small class
419730-41b2f0      20     44c130         456210    ** Logging (yodalog.txt) **
41b2f0-41bee0      28     44c438         -         ** Settings/registry **
41bee0-41c340      9      44d064         -         small
41c340-429000      130    44d2b4         4560f8    ** WorldGen + .wld save/load **
```

## Subdivision of the big modules (call-graph cut analysis, 2026-07-04)
Cutting each module where few intra-module call edges cross (candidate `.obj` seams):
- **GameData** (`0x401ac0–0x4042b0`) → **~10 small sub-`.obj`s** of 4–13 funcs (separate `.dta` type
  parsers), roughly: `401ac0`, `401e96`, `402276`, `402656`, `4029b2`, `402d02`, `403052`, `403250`,
  `4037a0`, `403ae0`. These align with the `.data` global steps (4560b0/4560b4…).
- **UI/view/sound** (`0x40a560–0x418700`) → one tightly-coupled **~82-func cluster** `0x40a560–0x416220`
  (the main CView/window class incl. the ~10.8 KB `FUN_0040b270` window proc) + small tail helper groups
  (`416220`, `4169bf`, `416b90`, `417f19`, `418230`, `418560`).
- **WorldGen/save** (`0x41c340–0x429000`) → essentially **one monolithic ~126-func `.obj`**
  `0x41c340–0x428c40` + a 4-func tail. Very tightly coupled — the world generator is one big TU.

Takeaway: the two giants are near-monolithic single TUs; GameData is many small TUs. (Cut analysis is
approximate — the call graph is sparse, so treat sub-seams as hints, not hard boundaries.)

## Asset parser (`.DTA`) — inside the WorldGen/save module (2026-07-04)
Cross-referenced with `~/workspace/DesktopAdventures/src/assets.c` + `scrdoc.txt` (user's engine
recreation). The `WorldGen` module is really **world-load/save + the `.DTA` asset loader**:
- **`Dta_Load` (0x422670)** — main `.dta` loader / chunk dispatcher. Opens the data file via `CFile`,
  reads IFF-like 4-char chunk tags and dispatches. Tag table at `.data 0x456890-0x456908`:
  `VERS STUP ZONE IZON ZAUX IZAX ZAX2/3/4 HTSP ACTN IACT PUZ2 IPUZ SNDS TILE CHAR CHWP TNAM ENDF`.
- **Per-chunk handlers**: the contiguous run **`FUN_00422f60 .. FUN_004236b0`** (11 small functions)
  are the per-chunk parsers (one per tag, in dispatch order) — a clean `Dta_Parse*` sub-unit to name
  next by decoding the dispatch order. Plus `FUN_00424380`, `FUN_00425e30` (larger zone/tile data).
- `FUN_00421fd0` (refs zone tags) and `FUN_00423850` (VERS/STUP/ENDF) are related loaders (likely the
  world save-writer / header reader) — verify before naming.
- Chunk semantics come straight from `assets.c` (e.g. TILE = `(32*32)+4` bytes/tile; ZONE/IZON =
  maps + index; CHAR/CHWP = characters + weapons; ACTN/IACT/PUZ2 = scripts/puzzles per `scrdoc.txt`).

This is the highest-value area to decompile next — it's data-driven, mostly small integer/parse
functions (good match targets), and fully documented by DesktopAdventures.

## Named compile-unit outline (anchors + proximity, 2026-07-04)
Overlaying the named functions onto the data-ref segments identifies most `.obj`s. **Working-outward
rule:** unnamed `FUN_*` in a segment almost certainly belong to that segment's theme (one `.obj` = one
source file, emitted contiguously). Approximate boundaries:

| .text range | CU (from its named anchors) |
|---|---|
| `0x401180–0x401450` | small MFC helper/exception classes (ctor/dtor boilerplate) |
| `0x401450–0x401ab9` | **World / game-score** — `World_UpdateScore`/`Calc*Score`/`GetZoneCell` |
| `0x401ac0–0x405150` | **GameData** + more small classes (`.dta` state; see module map above) |
| `0x405150–0x405ae0` | **Zone class** — `Zone_Ctor`/`Dtor`/`GetTile`/`SetTile`/`GetEdgeCode`/`FindObjectAt` (18) |
| `0x405ae0–0x4070e0` | **Zone runtime + IACT scripts** — `Iact_ReadIzon`/`ReadZaux`/`ReadZax2-4`, `Iact_Run` |
| `0x4070e0–0x408110` | **IACT commands + rendering** — `Iact_RunCommands` (cmd executor) + `Render_Blit` (+ ~15 helpers) |
| `0x408c60–0x40a560` | **Player / game core** — `Game_OnWalk`, `Game_MovePlayer` (+ movement helpers) |
| `0x40a560–0x418700` | **Game UI/view (big, 107)** — `Game_OnDragItem`, `Game_OnBumpTile`, window proc `FUN_0040b270` |
| `0x419730–0x41b2f0` | **Logging** (yodalog.txt)   ·   `0x41b2f0–0x41bee0` **Settings/registry** |
| `0x41c340–0x429000` | **.DTA loader + Worldgen (130)** — all `Dta_Parse*`/`Dta_Load`/`Dta_ReadZone`, `Worldgen_*` |

So: IACT/script functions cluster with the zone readers (`0x405ae0–0x4070e0`); rendering is its own
`.obj` right after; player movement is the `0x408c60` `.obj`; the drag/bump handlers + main window proc
are the big `0x40a560` view `.obj`. Refine each by decompiling a couple of its unnamed `FUN_*`.

### View `.obj` (0x40a560–0x418700) — identified functions (string + call-graph anchors)
- `Game_Tick` (0x40b270) — per-frame update + inlined enemy AI (see game-logic.md)
- `Game_OnDragItem` (0x4102d0, item-drag + "This is a WEAPON…" hints), `Game_OnBumpTile` (0x413df0)
- `Game_CheckCheat` (0x415820 — `goyoda`→Invincible, `gojedi`→Super Jedi)
- `View_OnOptions` (0x416030 — OPTIONS + MIDILoad), `View_OptionsDialog` (0x411180)
- `Game_ShowWinMessage` (0x40f4b0 — "Well done, Luke!"), `View_DrawText` (0x40f060 — MS Sans Serif)
- `View_FUN_0040b160` — entity/character draw (iterates `doc->characters`)
- Message handlers (OnDraw/OnTimer/OnKeyDown/OnLButton…) are wired at runtime (SetTimer/message map),
  not a Ghidra-visible table — reach them by decompiling from these anchors. `Game_Tick` has no direct
  callers ⇒ it's the registered timer/idle proc.

**Propagation pass (2026-07-04):** `GameView` struct (CView subclass, `doc@0x44`/`frameCounter@0xb0`)
typed onto **23 of the 29** view methods that touch `doc@0x44`/`zone@0x2c0`, so they decompile against
`this->doc->currentZone->…`. Newly named from the clarified bodies: `Game_Tick` (0x40b270),
`Game_DrawEntities` (0x40b160), `View_DrawMap` (0x40ed90, camera-visible tile render), `Game_UseTile`
(0x40a710, tile-type interaction dispatch), `View_HandleInput` (0x412250, iactBusy-gated input),
`Game_ClassifyTile` (0x40fca0, gameState-aware tile→code). The rest are typed but not yet
individually named (camera/scroll/GDI helpers) — name incrementally as their `doc->` accesses clarify.

## Next refinements
- Subdivide the two giant modules (UI 107, WorldGen 130) — they're likely several `.obj`s each; the
  giant `FUN_0040b270` (~10.8 KB) lives in the UI module and is probably the main window proc.
- Confirm the small-class cluster `0x4042b0–0x405150` (vtables 44b148…44b1a8) — likely MFC-derived
  document/view/exception classes; decompile ctors to name them.
- The `GameData` state names (Nevada/Alaska/Oregon) suggest `.dta` section/tileset labels — cross-ref
  `~/workspace/DesktopAdventures` for the `.dta`/zone format.
