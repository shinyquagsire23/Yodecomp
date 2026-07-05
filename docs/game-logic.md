# YodaDemo.exe — runtime game logic (script execution & player events)

The gameplay loop turns player input into **IACT script triggers** that run zone action scripts.
Cross-referenced with `~/workspace/DesktopAdventures/src/iact.c` + `scrdoc.txt` / `SCRIPTS.md`.

## IACT script system — two phases (fully mapped 2026-07-04)
Each zone owns a **`CObArray` of IACT scripts** (`zone->iactScripts` @+0x7c0, count @+0x7c4). A script
record has a **conditions list** (`+0xc` = condition count) and a **done flag** (`+0x2c`, set by the
FlagOnce command so a one-shot script won't refire). Execution is two functions:

### Phase 1 — `Iact_Run` (0x00406780): evaluate conditions
Full typed signature: `int Iact_Run(Zone *this, int event, int x, int y, int dx, int dy, int arg5,
CDC *pDC, World *pWorld, GameView *view)`; `event` = `2`=BumpTile, `3`=DragItem, `4`=Walk, `5`=variant.
Now reads idiomatically — `pWorld->zones[pWorld->playerY*10 + pWorld->playerX].flagSolved`, `pWorld->inventory`, etc. Outer loop over `iactScripts`, inner loop over each script's
conditions, **`switch(cond->opcode)`** (`cond+4`; args at `cond+8/+0xc/+0x10/+0x14/+0x18`). If **all**
conditions pass and the script isn't done, calls `Iact_RunCommands`. Condition opcodes (verified vs
`scrdoc.txt`, with the offsets they read):
| op | name | reads |
|----|------|-------|
| 0xb | EnemyDead | `zone->entities[arg]` alive? (+0x7d4/0x7d8) |
| 0xc | AllEnemiesDead | loops `zone->entities` |
| 0xd | HasItem | `pWorld->inventory[]` (+0xac/0xb0) |
| 0xe/0xf | CheckEnd/StartItem | tile at player pos / `pWorld->startItem` (+0x2e38) |
| 0x11/0x12 | GameInProgress/Completed | `pWorld->gameState` (+0x68 == -1/1) |
| 0x13/0x14 | HealthLs/Gt | `pWorld` health = `healthHi*-100 - healthLo + 0x191` (+0x3314/0x3318) |
| 0x18 | PlayerAtPos | camera `pWorld+0x3330/0x3334` |
| 0x19–0x1b,0x21 | GlobalVar Eq/Ls/Gt/Ne | `zone->globalVar` (+0x844) |
| 0x1c,0x23 | Experience Eq/Gt | `pWorld->completionCount` (+0x332c) — **not** RPG XP; it's the # of times the game has been beaten (registry `Count`). These conditions gate repeat-playthrough upgrade events (force powers, lightsaber) |
| 0x16 | `CheckCellItem` (was DA Unk16) | `zones[playerCell].cellItemC` (+0x10) == arg0 |
| 0x1d | `QuestSpotPresent` (was DA Unk1d) | loops `zone->objects` for `type==OBJ_QUEST_ITEM_SPOT` at coords |
| 0x1e | `CheckCellItems` (was DA Unk1e) | reads `zones[playerCell].cellItemA/B` (+0xc/0xe) as `tileArray` indices |
| 0x15 | (no handler) | unimplemented in the demo |
| 0x1f,0x20 | TempVarNe/RandVarNe | `zone->tempVar/randVar` (+0x834/+0x838) |
| 0x22 | CheckMapTileVar | zone tile grid |

### Phase 2 — `Iact_RunCommands` (0x004070e0): execute actions
`__thiscall(Zone *this, int scriptIndex, CDC*, World *doc, view)` — indexes
`this->iactScripts[scriptIndex]` and **`switch(cmd->opcode)`** over `0x00-0x25` (matches
DesktopAdventures `commands[]`): `0`SetMapTile `1`ClearTile `4`SayText `5`ShowText `a`PlaySound
`c`Random(`_rand`) `f`SetMapTileVar `10/11`Release/LockCamera `12`SetPlayerPos(`camera=arg<<5`)
`13`MoveCamera `14`FlagOnce `15`ShowObject `1b`SpawnItem … `25`AddHealth. Most commands draw, so it
lives in the render-heavy `.obj` (0x4070e0–0x408110) with `Render_Blit`. *(Formerly mis-named
`Render_DrawTileSprite` before the two-phase structure was understood.)*

**DA-authoritative enums (2026-07-04):** `IactCmdOp` (all 38 `CMD_*` 0x00–0x25) and `IactCondOp` (all
36 `COND_*` 0x00–0x23) are now defined in the DB from DA `iact.h` (`IACT_CMDS`/`IACT_TRIGGERS`) — the
exact names, applied to `IactCommand.opcode`/`IactCondition.opcode` (the jump-table switch still shows
numeric case labels, a Ghidra rendering limit). `Iact_RunCommands` params typed `(Zone*, int, CDC*,
World* pWorld, GameView* view)`.

**Note on opcode names:** DA's IACT names come from *strings that leaked out of the DAT* (the packer
left uninitialized memory holding original command-name fragments). Whether a name leaked depended on
how many args a command used — commands that filled their arg buffer overwrote the name, so DA's
`Unk*`/`*_MAYBE` opcodes are its *guesses*, not reads. YodaDemo's handlers are authoritative, so a few
are renamed from behavior: **CMD** `0x1e` `MarkZoneSolved` (was OpenShow — sets `flagA/B/+1c/+28=1` on
the player's map cell), `0x1f` `WinGame` (`abortFrame=1`), `0x20` `LoseGame` (`abortFrame=-1` + `-300`);
**COND** `0x10` `ZoneSolved` (checks the flag `CMD_MarkZoneSolved` sets). Confirmed DA's `_MAYBE`s:
`0x11` `GameInProgress` (`pWorld->gameState`@0x68 `== -1`), `0x12` `GameCompleted` (`gameState == 1`).
`gameState@0x68`: -1 = in progress, 1 = won. (Win/lose direction of `0x1f`/`0x20` inferred from the
`-300` penalty — verify against the game.) **Inventory** confirmed via the item commands: `CMD_AddItemToInv` (0x1c) →
`Game_AddItemToInv` (0x428f50) does `SetAtGrow(&pWorld->inventory, pWorld->inventoryCount, tileWrapper)`;
`CMD_RemoveItemFromInv` (0x1d) → `Game_RemoveItem`. `pWorld->inventory` = **CObArray @0xa8** of held items
(Tile-wrappers, each item's `Tile*` at wrapper+4, name CString at +8), `inventoryCount` @0xb0.

### In-memory record structs (modeled in the DB 2026-07-04)
The file format (DesktopAdventures `iact.c`) is `condCount:u16`, `conditions[7×u16]` (opcode + 6 args),
`cmdCount:u16`, `commands[opcode + 5 args + strlen:u16 + string]`. YodaDemo widens the `u16`s to `int`
in memory and stores conditions/commands as **arrays of pointers**:
```
IactScript (0x30):  read into zone->iactScripts (CObArray @zone+0x7c0/0x7c4)
  +0x08 IactCondition** conditions   +0x0c int condCount
  +0x1c IactCommand**   commands      +0x20 int cmdCount
  +0x2c int doneFlag                  (set to 1 by the FlagOnce command → one-shot)
IactCondition (0x1c):  +0x04 int opcode   +0x08 int args[5]
IactCommand   (0x20):  +0x04 int opcode   +0x08 int args[5]   +0x1c CString text (ShowText/SayText)
```
(`conditions`/`commands` are actually embedded `CObArray`s — the objects sit at `script+0x04` and
`script+0x18`, so `m_pData` lands at +0x08/+0x1c and `m_nSize` at +0x0c/+0x20.)

### The IACT reader (found 2026-07-04) — the ACTN chunk carries all zones' scripts
**`Dta_ParseActn` (0x423510)** is the IACT script reader (not just "action scripts"): it loops
`{ zoneId:short (-1 skips); zone = pWorld->zoneObjects[id]; scriptCount:short; SetSize zone->iactScripts;
per script: new IactScript(0x30) }`. The per-record readers are a small class cluster at
**0x418700–0x418dd0** (now `Iact_*`):
- `Iact_ScriptCtor` (0x418700) + `Iact_ReadScript` (0x4188d0) — reads condCount then cmdCount,
  allocating each record and calling its reader
- `Iact_ConditionCtor` (0x418b10) + `Iact_ReadCondition` (0x418be0) — `new(0x1c)`, reads opcode+5 args
- `Iact_CommandCtor` (0x418c30) + `Iact_ReadCommand` (0x418d40) — `new(0x20)`, reads opcode+5 args+string
So the pipeline is **ACTN load → `IactScript` records per zone → `Iact_Run`/`Iact_RunCommands` execute**.
After typing, `Iact_Run`/`Iact_RunCommands` decompile as `this->iactScripts[i]->condCount`,
`script->conditions[..]`, `script->commands[..]`, `script->doneFlag = 1`, `this->tempVar`, `this->tiles[..]`.
`MapEntity` corrected to its real size **0x64** (the entity reader `Iact_ReadZaux` `new`s 0x64-byte
records into `zone->entities`; also fills `+0x26`, `+0x28`, and a `[0x20]` block @+0x40).

## Player event handlers (input → `Iact_Run`)
Each maps a player action to an `Iact_Run` event type on the current zone (`pWorld->currentZone` +0x2c0):
| Function | Addr | Event | Fires when |
|---|---|---|---|
| `Game_OnBumpTile`  | 0x413df0 | 2 BumpTile | player bumps a tile/sprite |
| `GameView::OnDragItem`  | 0x4102d0 | 3 DragItem | drop-resolution for a dragged item (switches on item/target tile). Handles weapon use AND the **R2-D2 (Artoo) hint system** — dragging Artoo onto an object shows his speech balloon: a switch on the target category (0..0x14) picks a hint string `strArtooHelp` (resources 0xe020..0xe038) → `ShowTextDialog`. Also plays palette-cycling effects |
| `Game_OnWalk`      | 0x409650 | 4 Walk / 5 | player walks onto a tile |
| `Game_MovePlayer`  | 0x409c10 | 4 Walk / 5 | player movement step |

`FUN_004037a0` (movement/interaction dispatcher) calls `Game_OnWalk`; the chain climbs toward the main
window proc / game tick (the ~10.8 KB `FUN_0040b270`, jt~70 — still to map).

**Combat — `GameView::UseWeapon` (0x427d20, from `GameView::UseTile` 0x40a710):** the player attacks with
`pWorld->currentWeapon` (a `Character*` weapon def). Damage = `weapon->frames[0x22]` scaled by `pWorld->difficulty`
(`<0x32` ⇒ `×(10 − diff/5)`, min 1); sets `pWorld->equippedItem = tileArray[weapon->name[10]]`; branches on the
weapon type id (`weapon->name[10]`: 0x12/0x1fe/0x1ff/0x200…) and calls `Puzzle::FUN_00404910` to apply the
hit, then repaints. (Health is applied separately via `GameView::AddHealth` 0x427690, IACT cmd 0x25.)

**Zone transitions** — two handlers move the player between the 10×10 grid's zones via paired hotspot
objects (`ZoneObj.type` = `ObjType`), and both drive the **per-zone save snapshot**:
| Handler | Trigger | Hotspot pair | On the state |
|---|---|---|---|
| `GameView::TransitionZoneDoor` (0x40e9d0) | `OnBumpTile` (walk into a door) | `OBJ_DOOR_IN`(9) → `OBJ_DOOR_OUT`(0xa) | records return pos in the source zone, `World::EnterZone` |
| `GameView::TransitionZoneXWing` (0x40e7c0) | `DrawObjects` (reach an X-Wing spot) | `OBJ_XWING_TO`(0xf) ⇄ `OBJ_XWING_FROM`(0xe) | **arrive→`World::RestoreRecords`**, **depart→`World::BackupRecords`** |
Both: target zone id = `hotspot->+0xe`; find the paired object in the target via `World::GetZoneById`
(0x403a70, bounds-checked `zoneObjects[id]`); set `pWorld->cameraX/Y = paired.x/y << 5`, `field90_0x60`
= direction, `bIactBusy = 6`. The backup/restore is why saves stay sparse (see worldgen.md).

Player/hero grid position is `doc+0x2e20` (x) / `doc+0x2e24` (y); the current zone pointer hangs off the
view (`+0x2c0`). Characters live in `pWorld->characters` (World+0xc0).

## Rendering pipeline (named via GameView propagation 2026-07-04)
```
View_DrawScreen (0x409110)      full redraw: SelectPalette(pWorld->palette @+0x2c4) + panels + viewport
  └ View_DrawGameArea (0x40a200)   blit the 0x120×0x120 (9×9-tile) play viewport
       ├ View_DrawMap (0x40ed90)      camera-visible tile grid (pWorld->cameraX>>5 … Zone_GetTile+tileArray)
       ├ View_DrawObjects (0x40ec30)  visible zone->objects
       ├ Game_DrawEntities (0x40b160) placed entities via pWorld->characters[ent->charId]
       └ View_DrawTileAt (0x40a3a0)   one tile/sprite at grid (x,y) (grid→pixel <<5)
  └ Render_Blit (0x408000)          int Render_Blit(Canvas*, CDC* dest, destX,destY,w,h,srcX,srcY)
                                     → BitBlt(dest->m_hDC, …, this->hdc, …, SRCCOPY)
```
`pWorld->palette` (World+0x2c4, CPalette*) is selected by every draw fn. `Iact_RunCommands`' draw-commands
reuse the same tile/camera machinery (why it first read as a renderer).

**Full prototypes set (2026-07-04):** the whole path is now typed — `View_DrawScreen`/`View_DrawGameArea`
`(GameView*, CDC*)`, `View_DrawMap`/`View_DrawObjects`/`Game_DrawEntities(GameView*)`, `View_DrawTileAt`
`(GameView*, short x,y,frame)`, `Render_Blit`/`Canvas_Blit`/`Canvas_Init`/`Canvas_GetSize` as `Canvas*`
methods. `pWorld->canvas` is the `Canvas` (0x43c DIBSection) blitted to the paint `CDC`. (Consolidated a
duplicate `CDC` type — the empty `/Demangler/CDC` placeholder now carries the real `m_hDC@4` layout.)

## Window layout & the inventory scroll area (mapped 2026-07-05 from `Game_RemoveItem`)

The main window (a `CDeskcppView` = our `GameView`) has four regions:
- **Menu bar** — new adventure / load save / options (map size) / audio.
- **Play viewport** (left) — 9×9 tiles × 3 layers, drawn by the `View_DrawScreen` pipeline above.
- **Inventory** (right) — a scrollable **two-column list** of held items; each row is the item's name
  text plus a bitmap of the item. Backed by `pWorld->invArray` (**CObArray @ World+0xa8**) of Tile-wrapper
  objects (item's `Tile*` at wrapper+4, name CString at +8); `pWorld->inventory`(m_pData)@0xac,
  `pWorld->inventoryCount`(m_nSize)@0xb0.
- **Below the inventory** — **4 direction arrows** (zone transitions): `GameView::DrawDirectionArrows`
  (0x4270f0) fills the arrow-panel rect `World.nArrowBox{L,T,R,B}@0x32e4/e8/ec/f0` and draws an enabled/
  disabled arrow icon per direction from `GameData::GetExitDirections(pWorld)` (0x4032c0) — a bitmask
  (**W=8, E=4, N=1, S=2**) set when the neighbour cell in the 10×10 grid `exists`; indoor zones (flags
  8/9/0xd/0xe) get none. The **weapon box** (drag target from
  the inventory; rect = `World.nWeaponBox{Left,Top,Right,Bottom}` @0x32a4/a8/ac/b0, `GameView::DrawWeaponBox`
  0x428ac0 draws the frame, `GameView::DrawWeaponIcon` 0x428c40 BitBlts `pWorld->currentWeapon`@0x2e2c), and a
  **health circle** — rect `World.nHealthDial{Left,Top,Right,Bottom}` @0x32c4/c8/cc/d0, drawn by
  `GameView::DrawHealthDial` (0x427490, two `Chord()` pie-halves) + `GameView::DrawHealthNeedle` (0x4278a0,
  pens colored by `pWorld->healthHi`). `GameView::AddHealth` (0x427690) is the health logic (IACT cmd 0x25).

### The inventory scroll bar — `InvScrollBar` (custom `CScrollBar` subclass)
`GameView.pInvScrollBar` (**+0x40**, `InvScrollBar*`) points to the inventory scroll bar control. It is a
CWnd-derived object with two extra members:

| off | field | meaning |
|---|---|---|
| 0x1c | `m_hWnd` | CWnd handle of the scroll bar (used with `SetScrollRange`/`Pos`, nBar=SB_CTL) |
| 0x3c | `scrollMax` | max scroll position = `max(0, inventoryCount - 7)` (the list shows **7 rows**) |
| 0x40 | `scrollPos` | current scroll position, clamped to `[0, scrollMax]` |

`Game_RemoveItem` (0x429150) and `Game_AddItemToInv` (0x428f50) resize the bar after every add/remove:
`inventoryCount < 8` → range `[0,1]`, `scrollMax=0`; else range `[0, count-7]`, `scrollMax=count-7`.

`InvScrollBar_OnVScroll` (0x409360) is the **reflected** `WM_VSCROLL` handler (`this==pScrollBar`; it was
mis-attributed to `GameView` by the bulk this-typing — the offset-0x40 collision with `GameView.field_40`
is coincidental). It steps `scrollPos` by nSBCode (SB_LINE ±1, SB_PAGE ±7, SB_THUMB = absolute), clamps to
`[0, scrollMax]`, calls `SetScrollPos(m_hWnd)`, then `GameView_DrawText` on the **parent** GameView to
repaint the two-column list at the new offset.

## Game tick & enemy AI
- **`GameView::Tick` (0x0040b270)** — the per-entity update / enemy-AI step, **called by**
  **`GameView::UpdateFrameMaybe`** (0x40d470, the actual per-frame loop — it drives Tick×5 + `CyclePalette`×6
  ambient palette anim + `DrawGameArea`×4 render + win/lose via `abortFrame`). Tick itself, per frame:
  - **Player**: `Player_Move` (step/move), `Player_CheckWalkable` (tile-collision, reads all 3
    layers via `Zone_GetTile`).
  - **Enemy/monster AI**: **inlined here** (no separate AI function). Iterates the zone's spawned-entity
    list `zone->entities` (a 2nd `CObArray`, `m_pData@+0x7d4`, count `@+0x7d8`), each a **`MapEntity`**:
    ```
    MapEntity (sizeof ~0x40):
      +0x04 short charId    index into pWorld->characters (<0 = empty slot)
      +0x06/+0x08 homeX/Y   spawn position
      +0x0c int   state
      +0x12/+0x14 x/y       current grid position
      +0x24 short timer     behavior cooldown
      +0x38/+0x3a dx/dy     pending movement delta
      +0x3c short animFlag
    ```
    Per entity per frame: look up its char def (`pWorld->characters[charId]`), `_rand()` (36× total) to
    pick a move, **`Iact_ProbeMove`(zone,x,y,dx,dy)** (0x406550) to test tile walkability + animation
    timing (`GetTickCount`), then `Zone_SetTile` + `Player_Move` to relocate/animate, and `Iact_Run` to
    fire the entity's scripts. `MapEntity` + `Zone.entities` are now modeled in the Ghidra DB.
  - **Scripts**: `Iact_ProbeMove` (a `GetTickCount`-based timed script/position helper) and the
    `Game_On*` event handlers → `Iact_Run`.
  So "character driving" for both player and enemies lives in `GameView::Tick`; decompiling its per-entity
  loops (there are several, guarded by `_rand`) is the way to pull out the enemy AI.
- **Structure mapped (2026-07-05).** Now `GameView::Tick` (`this=GameView*`, `this->pWorld`). Two switch
  axes drive it:
  - **Per-entity movement AI (mapped 2026-07-05; corrects an earlier wrong note).** The `switch` reads a
    **per-char-def movement-type field** at `Character+0x36` (the decompiler writes `this_01->frames + 0x1a`
    off `frames@0x1c`; it is **NOT** `flags>>16` — cases 6/7/9/10 rule that out; the DA `ICHR_BEHAVIOR`
    flags/`CharBehavior` enum is a *separate* field). Cases are **1,2,3,4,6,7,8,9,10** (no 5).
    **Common per-entity frame:** each `MapEntity` has a cooldown `timer@0x24`; while >0 it decrements and
    waits, at 0 it picks a step per its move-type, `Zone::IactProbeMove` tests walkability, `PlayerMove`
    relocates it (nested `switch(dir)`), and if it reaches the player it melee-attacks via
    `AddHealth(-damage)` (damage = `char frames+0x22` = `Char+0x3e`). The move-types, by what picks the step:
    | val | movement style |
    |---|---|
    | 1 | passive random wander (coin-flip whether to move) |
    | 2 | erratic wander (heavy `_rand`) |
    | 3 | random, else drift toward home (`ent+0x6/0x8`) |
    | **4** | **direct chase** — deterministic step toward the target, no `_rand` (the aggressive hunter) |
    | 6 | patterned (mod-3 cycle on a counter) |
    | 7 | random wander |
    | 8 | very erratic (`_rand`×9) |
    | 9 | random wander |
    | 10 | chase + attack; reads state `ent+0x3e`, double `PlayerMove`/`AddHealth` (likely a ranged/projectile attacker) |
    This switch *is* the whole enemy AI — there is no separate AI function. (Exact 1↔difficulty mapping and
    the 6/10 specifics are best confirmed in-game; the chase (4) vs wander (1/2/7/8/9) split is certain.)
  - **Game-mode branches:** `switch(pWorld->field_0x30)` (3×) — a per-frame mode/phase field at
    `World+0x30` (distinct from `gameState@0x68`); TBD which modes (intro/play/cutscene?).
  Hot callees (counts): `Zone::GetTile`×20, `PlayerMove`/`AddHealth`×10, `Zone::IactProbeMove`/
  `World::GetTileData`×9, `Zone::SetTile`×8, `World::FindTile`×3, `_rand`×37.

## Game-loop call structure (traced 2026-07-04 — reveals the module layering)
```
Game_Tick (View .obj, 0x40b270, timer/idle)
  ├─ Player_Move (0x409060)          entity move by direction (deltas @doc+0x330c/0x3310)
  ├─ Player_CheckWalkable (0x409460) reads all 3 tile layers → blocked?
  ├─ Iact_ProbeMove               timed script/position helper (GetTickCount)
  ├─ Zone_GetTile / Zone_SetTile     tile read/write
  └─ _rand ×36                       inlined enemy/monster AI (random movement)

Game_OnWalk (0x409650) / Game_MovePlayer (0x409c10)   [near-identical: two movement variants]
  ├─ Player_TryStep (0x409610) → Player_CheckWalkable   walkability-gated move
  ├─ Iact_Run (0x406780)             fire Walk/step scripts
  ├─ Render_Blit (0x408000) + GetDC/SelectPalette/ReleaseDC   immediate on-screen draw
  ├─ World_GetZoneCell (0x401a80)    overview-grid lookup
  └─ View_FUN_0040b160               entity/character draw (iterates pWorld->characters, Zone_SetTile)
```
**Layering confirmed:** `View`/tick → `Player` (movement+draw glue) → `Iact` (scripts) + `Render`
(blit) + `World`/`Zone` (data) + GDI. The Player `.obj` (0x408c60–0x40a560) is the action layer that
stitches scripts, world data, and drawing together; `Render` (0x4070e0–0x408110) is pure blitting
(`Iact_RunCommands` draw-cmds, `Render_Blit`).

## IACT — end to end (complete)
`ACTN` chunk → `Dta_ParseActn` (0x423510) reads `IactScript`/`IactCondition`/`IactCommand` records
(reader class @0x418700–0x418dd0) into `zone->iactScripts` → **`Iact_Run`** (0x406780) evaluates a
script's conditions on a player/entity event → on pass, **`Iact_RunCommands`** (0x4070e0) executes its
commands. Structs + reader/executor functions are all modeled/named in the DB; opcode tables verified
against `scrdoc.txt` + `iact.c`.

## Struct model & upward propagation (2026-07-04)
Opcode **enums** in the DB: `IactCondOp` (`COND_*`, 0x00–0x23) and `IactCmdOp` (`CMD_*`, 0x00–0x25),
applied to `IactCondition.opcode` / `IactCommand.opcode`. (The executors walk the record arrays by raw
byte offset, so their `switch` doesn't fold to `->opcode`; the enums still name the opcode space and
render wherever a record is cleanly typed. Case→name mapping is in the two opcode tables above.)

**`GameView`** (the MFC CView subclass; `this` of the game-loop methods): `+0x44 World* doc`,
`+0xb0 short frameCounter`. Typing it makes `view->pWorld->…` cascade everywhere. `World` field map grew
to include the game-state offsets pinned from IACT + the loop: `currentZone@0x2c0`, `playerX/Y@0x2e20/24`,
`cameraX/Y@0x3330/34`, `iactBusy@0x5c`, plus the earlier health/inventory/experience/gameState fields.

Propagating `GameView*`/`World*`/`Zone*` upward turned the giant game functions idiomatic and let them
be named at a glance:
- `Game_Tick` (0x40b270): `this->frameCounter++`, `pWorld->currentZone->entities[i]`, `pWorld->characters[..]`,
  `pWorld->cameraX >> 5`.
- `Game_DrawEntities` (0x40b160, was `View_FUN_0040b160`): draws each placed entity via
  `pWorld->characters[ent->charId]` + `Zone_SetTile`.
- `Game_OnBumpTile` / `GameView::OnDragItem`: now `this->pWorld->…`.
This is the pattern to keep applying: type one `this`, name the fields it reveals, and the next caller
up decompiles for free.

## Still to map
- The individual `Game_Tick` sub-loops (player step, monster AI, projectile/attack) — currently one
  giant function; splitting it is the next decomp step for the gameplay core.
- The rest of the `GameView`/CView `.obj` (menu/command handlers, OnDraw) — keep propagating `GameView*`.
