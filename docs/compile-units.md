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
| `0x401ac0–0x4042b0` | 69 | **GameData** — doc(`World`)-method CU: story-history registry persistence, doc menu handlers, zone save/load recursion, asset accessors, worldgen helpers. Fully RE'd + renamed 2026-07-05 (see the GameData CU section below) | `GameData`, `Nevada`, `Alaska`, `Oregon` |
| `0x408c60–0x40a560` | 26 | **Core utilities** — called by everyone, no strings | (none) |
| `0x40a560–0x418700` | **107** | **Game UI / view / hints / sound** | `OPTIONS`, `MS Sans Serif`, `MIDILoad`, `Super Jedi!`, `goyoda`/`gojedi` (cheats), `This is a DOOR/WEAPON/EWOK…` tile hints, `Congratulations! You've won…` |
| `0x419730–0x41b2f0` | 20 | **Utility + debug log** — `Log_Write` (0x419cb0) appends to `c:\yodalog.txt` (fopen/fputs/fclose); rest is a utility class. Most log callers compiled out under NDEBUG (only `Worldgen_PlacePuzzle`'s "No Place to put Find Puzzle" survives) | `c:\yodalog.txt` |
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
418dd0-419120      8      44bd30         -         Dlg_    CDialog subclass (Dlg_Ctor)
419120-419730      8      44bf30         459e20    Frame_  CFrameWnd/CMainFrame (Frame_OnCreate, PreCreateWindow)
419730-41b2f0      20     44c130         456210    App_    CDeskcppApp (CWinApp: App_Ctor) + Log_Write + doc-render helpers
41b2f0-41bee0      28     44c438         -         Settings_ CDeskcppApp settings (Settings_Save, WriteProfileInt)
41bee0-41c340      9      44d064         -         Wld_    .wld save/load helper class (serializes world objects; ctor reached from Wld_Serialize)
41c340-~427490    ~110   44d2b4         4560f8    ** CDeskcppDoc TU (World:: ns): World::Generate (0x41f960) worldgen + World::Load/Parse* (dta) + World::Serialize (wld). this=World* **
~427490-4292e7    ~20    -              -         ** CDeskcppView draw/inventory tail (GameView:: ns), NOT doc: DrawHealthDial/Needle/AddHealth, DrawWeaponBox/Icon, BlitViewportDither, AddItemToInv, RemoveItem, GameView::FUN_00427d20 (weapon+health render). this=GameView* (pWorld@0x44, unkHWnd@0x1c) **
NOTE (2026-07-05): namespace migration folded the doc sub-modules into World:: (namespace must == struct for auto-this). The ~0x427490 boundary splits doc (World) from the view-render tail (GameView) — several tail funcs were mis-this-typed and corrected.
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

## GameData CU (0x401ac0–0x4042b0) — fully RE'd + renamed (2026-07-05)
All real functions are `World` (CDeskcppDoc) methods — this `.obj` is a second doc-class source file
(think `gamedata.cpp`), NOT a standalone module class. Rest of the range = 8-byte EH funclets
(`<Parent>_ehN`). Boundary confirmed: preceded by the World-scorers fragment (ends 0x401ab9), followed
by the Records TU (`Puzzle::Ctor` 0x4042b0); string cluster `.data 0x4560b0–0x4560e8`
(`GameData`/`Nevada`/`Alaska`/`Oregon`/format strings) is CU-local.

**⭐ Planet-table semantics solved:** `Nevada`/`Alaska`/`Oregon` registry keys (section `GameData`) are
per-planet **story histories** — CWordArrays of recently played story **goal-item ids** (planet 1=Nevada
=desert, 2=Alaska=ice (the demo planet), 3=Oregon=forest; matches `AreaType`). Line format
`<worldSeed>_<obfKey>_<count>_v0_.._v9`, values obfuscated by a per-line additive `rand()%255+1` key;
the seed prefix is written but discarded on load; lists trimmed to ≤3. `World::Generate` loads the
current planet's list when no story is requested, appends the built story's goal item (demo hardcodes
108; also pre-appends 0xbd/0xc5 to the Alaska list by completionCount) and saves it back on success.
**File>Replay Story (cmd 0x800B)** = `World::OnReplayStory` (0x403620, recovered from a code gap):
takes `nCurrentGoalItemMaybe@0x33a0` (>0) or the last history element, puts it in
`nRequestedGoalItemMaybe@0x6c`, and runs `StartGame(Randomize(), 0)` — same story goal, fresh layout
(msgs 0xE009 confirm / 0xE01C "no story saved to replay").

| addr | function | role |
|---|---|---|
| 0x401ac0/0x401ea0/0x402280 | `World::LoadStoryHistoryNevada/Alaska/Oregon` | registry → `storyHistory*@0x284/298/2ac` (de-obfuscate) |
| 0x402670/0x4029c0/0x402d10 | `World::SaveStoryHistoryNevada/Alaska/Oregon` | write-back twins (obfuscate, seed prefix) |
| 0x402660/0x403060 | `World::Nop1/Nop2` | **empty (single RET)** — compiled-out debug hooks called by Generate; the TU contains empty member functions (matching note) |
| 0x403070 | `World::FilterEnemyZonesFromListMaybe` | rebuild `placedZoneIds@0x220` dropping `MAP_ENEMY_TERRITORY(1)` zones |
| 0x403140 | `World::PlaceZoneObjectTiles` | stamp visible ZoneObjs (OBJ types 0/1/2/5/6/7/8, 0xb→tile 0x1cb) into tile layer 1 |
| 0x403250 | `World::FindZoneCellById` | 10×10 grid search → (x,y) |
| 0x4032c0 | `World::GetExitDirections` | compass bitmask W8/E4/N1/S2 of neighbour zones |
| 0x4033b0/0x403450 | `World::SaveZoneRecursive`/`LoadZoneRecursive` | .wld zone snapshot write/read, recursing OBJ_DOOR_IN(9) child rooms; call `Zone::WriteSavedState`/`ReadSavedState` (0x405f30/0x405bd0) |
| 0x403510..0x403610 | `World::OnUpdate{FileSave,AppExit,HideMe,NewWorld,LoadWorld,ReplayStory}` | **recovered from the 0x403501–0x40379f gap**: CCmdUI Enable handlers (msgmap 0x44c2d0; Save/Load/Replay grayed in the demo); menu ids from the RT_MENU resource |
| 0x403620 | `World::OnReplayStory` | ON_COMMAND 0x800B (see above) |
| 0x4037a0 | `World::StartGame(nSeed, bSkipGenerate)` | was `ResetGameStateMaybe`; full session start: reset state/grids, `LoadWorldMaybe`, Generate-loop+`Populate` (unless bSkipGenerate), RET 8 |
| 0x403a40/0x403a70/0x403aa0 | `World::GetTileData`/`GetZoneById`/`FindTile` | asset accessors (clean) |
| 0x403ae0 | `World::RefreshZone` | redraw currentZone's 3 tile layers into the Canvas |
| 0x403c80 | `World::BuildQuestPathMaybe` | worldgen 10×10 grid puzzle-path layout (called only by Generate) |

Doc-TU handlers renamed from the same message-map evidence: `World::OnNewWorld` (0x424450, was
`ShowError`), `OnSaveWorld` (0x424540, was `FileDialog`), `OnLoadWorld` (0x424fc0, was `Serialize`),
`OnToggleSound/Music` + updates (0x4242a0..0x424360).

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

## Certain MFC vtable methods named (2026-07-04)
From the vtables + `CRuntimeClass` structs, these boilerplate/entry virtuals are named with certainty:
- `CDeskcppView_GetRuntimeClass` (0x408560), `CMainFrame_GetRuntimeClass` (0x419070),
  `CDeskcppDoc_GetRuntimeClass` (0x419f40) — pattern `MOV EAX,<CRuntimeClass>; RET`.
- `CDeskcppDoc_CreateObject` (0x419ed0) — from `CRuntimeClass.m_pfnCreateObject` (+0xc).
- `App_InitInstance` (0x4198c0) — the sole `AddDocTemplate` caller (CWinApp::InitInstance override).
- `Wld_Serialize` (0x424fc0) = `CDeskcppDoc::Serialize` (ASAV44 `.wld`); `View_DrawScreen` (0x409110)
  is the view paint (calls `Dta_Load` — the `.dta` loads **lazily** on first draw, not OnOpenDocument).

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
- `GameView::ConfirmExitMaybe` (0x416030 — quit/new-adventure prompt; persists OPTIONS/MIDILoad on exit; was `View_OnOptions`), `GameView::ProcessWalkMaybe` (0x411180 — walk-step handler, was `View_OptionsDialog`)
- `Game_ShowWinMessage` (0x40f4b0 — "Well done, Luke!"), `View_DrawText` (0x40f060 — MS Sans Serif)
- `View_FUN_0040b160` — entity/character draw (iterates `pWorld->characters`)
- Message handlers (OnDraw/OnTimer/OnKeyDown/OnLButton…) are wired at runtime (SetTimer/message map),
  not a Ghidra-visible table — reach them by decompiling from these anchors. `Game_Tick` has no direct
  callers ⇒ it's the registered timer/idle proc.

**Propagation pass (2026-07-04):** `GameView` struct (CView subclass, `doc@0x44`/`frameCounter@0xb0`)
typed onto **23 of the 29** view methods that touch `doc@0x44`/`zone@0x2c0`, so they decompile against
`this->pWorld->currentZone->…`. Newly named from the clarified bodies: `Game_Tick` (0x40b270),
`Game_DrawEntities` (0x40b160), `View_DrawMap` (0x40ed90, camera-visible tile render), `Game_UseTile`
(0x40a710, tile-type interaction dispatch), `View_OnLButtonUp` (0x412250, iactBusy-gated input),
`Game_ClassifyTile` (0x40fca0, gameState-aware tile→code). The rest are typed but not yet
individually named (camera/scroll/GDI helpers) — name incrementally as their `pWorld->` accesses clarify.

## Next refinements
- Subdivide the two giant modules (UI 107, WorldGen 130) — they're likely several `.obj`s each; the
  giant `FUN_0040b270` (~10.8 KB) lives in the UI module and is probably the main window proc.
- Confirm the small-class cluster `0x4042b0–0x405150` (vtables 44b148…44b1a8) — likely MFC-derived
  document/view/exception classes; decompile ctors to name them.
- The `GameData` state names (Nevada/Alaska/Oregon) suggest `.dta` section/tileset labels — cross-ref
  `~/workspace/DesktopAdventures` for the `.dta`/zone format.


**Heritage & demo-limiting note (2026-07-05):** the registry keys `Nevada`/`Alaska`/`Oregon` are Indiana
Jones' Desktop Adventures engine leftovers (US-state planet slots). In Yoda Stories: slot 1 = Tatooine
(desert), slot 2 = Hoth (ice — the demo's planet), slot 3 = Endor (forest). Demo-limiting measures in this
CU (not present in the full game): goal item hardcoded to 108, Alaska history pre-seeded (0xbd/0xc5 by
completionCount), File>Save/Load/Replay menu items force-disabled — plus most content stripped from the
demo DTA itself. Don't over-interpret these as engine semantics.
