# YodaDemo.exe — runtime game logic (script execution & player events)

The gameplay loop turns player input into **IACT script triggers** that run zone action scripts.
Cross-referenced with `~/workspace/DesktopAdventures/src/iact.c` + `scrdoc.txt` / `SCRIPTS.md`.

## IACT script system — two phases (fully mapped 2026-07-04)
Each zone owns a **`CObArray` of IACT scripts** (`zone->iactScripts` @+0x7c0, count @+0x7c4). A script
record has a **conditions list** (`+0xc` = condition count) and a **done flag** (`+0x2c`, set by the
FlagOnce command so a one-shot script won't refire). Execution is two functions:

### Phase 1 — `Iact_Run` (0x00406780): evaluate conditions
`__thiscall(Zone *this, int event, x, y, dx, dy, …, World *doc, view)`; `event` = `2`=BumpTile,
`3`=DragItem, `4`=Walk, `5`=variant. Outer loop over `iactScripts`, inner loop over each script's
conditions, **`switch(cond->opcode)`** (`cond+4`; args at `cond+8/+0xc/+0x10/+0x14/+0x18`). If **all**
conditions pass and the script isn't done, calls `Iact_RunCommands`. Condition opcodes (verified vs
`scrdoc.txt`, with the offsets they read):
| op | name | reads |
|----|------|-------|
| 0xb | EnemyDead | `zone->entities[arg]` alive? (+0x7d4/0x7d8) |
| 0xc | AllEnemiesDead | loops `zone->entities` |
| 0xd | HasItem | `doc->inventory[]` (+0xac/0xb0) |
| 0xe/0xf | CheckEnd/StartItem | tile at player pos / `doc->startItem` (+0x2e38) |
| 0x11/0x12 | GameInProgress/Completed | `doc->gameState` (+0x68 == -1/1) |
| 0x13/0x14 | HealthLs/Gt | `doc` health = `healthHi*-100 - healthLo + 0x191` (+0x3314/0x3318) |
| 0x18 | PlayerAtPos | camera `doc+0x3330/0x3334` |
| 0x19–0x1b,0x21 | GlobalVar Eq/Ls/Gt/Ne | `zone->globalVar` (+0x844) |
| 0x1c,0x23 | Experience Eq/Gt | `doc->experience` (+0x332c) |
| 0x1d | (object check) | loops `zone->objects` (+0x7ac/0x7b0) |
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
`{ zoneId:short (-1 skips); zone = doc->zoneObjects[id]; scriptCount:short; SetSize zone->iactScripts;
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
Each maps a player action to an `Iact_Run` event type on the current zone (`doc...->+0x2c0`):
| Function | Addr | Event | Fires when |
|---|---|---|---|
| `Game_OnBumpTile`  | 0x413df0 | 2 BumpTile | player bumps a tile/sprite |
| `Game_OnDragItem`  | 0x4102d0 | 3 DragItem | an inventory item is dragged onto a tile (switches on tile/item type) |
| `Game_OnWalk`      | 0x409650 | 4 Walk / 5 | player walks onto a tile |
| `Game_MovePlayer`  | 0x409c10 | 4 Walk / 5 | player movement step |

`FUN_004037a0` (movement/interaction dispatcher) calls `Game_OnWalk`; the chain climbs toward the main
window proc / game tick (the ~10.8 KB `FUN_0040b270`, jt~70 — still to map).

Player/hero grid position is `doc+0x2e20` (x) / `doc+0x2e24` (y); the current zone pointer hangs off the
view (`+0x2c0`). Characters live in `doc->characters` (World+0xc0).

## Rendering
- **`Iact_RunCommands` (0x004070e0)** — the IACT command executor (see above); its many draw-commands
  use `doc->tileArray` (+0x84), camera (`+0x3330`/`+0x3334`), hero grid pos — which is why it first
  read as a renderer.
- **`Render_Blit` (0x00408000)** — `BitBlt` wrapper (dest DC, x,y,w,h, src DC, srcX,srcY).

## Game tick & enemy AI
- **`Game_Tick` (0x0040b270)** — the main per-frame update (~10.8 KB, **no callers** ⇒ a registered
  timer/idle callback). `void __fastcall(view*)`. Drives everything each frame:
  - **Player**: `Player_Move` (step/move), `Player_CheckWalkable` (tile-collision, reads all 3
    layers via `Zone_GetTile`).
  - **Enemy/monster AI**: **inlined here** (no separate AI function). Iterates the zone's spawned-entity
    list `zone->entities` (a 2nd `CObArray`, `m_pData@+0x7d4`, count `@+0x7d8`), each a **`MapEntity`**:
    ```
    MapEntity (sizeof ~0x40):
      +0x04 short charId    index into doc->characters (<0 = empty slot)
      +0x06/+0x08 homeX/Y   spawn position
      +0x0c int   state
      +0x12/+0x14 x/y       current grid position
      +0x24 short timer     behavior cooldown
      +0x38/+0x3a dx/dy     pending movement delta
      +0x3c short animFlag
    ```
    Per entity per frame: look up its char def (`doc->characters[charId]`), `_rand()` (36× total) to
    pick a move, **`Iact_ProbeMove`(zone,x,y,dx,dy)** (0x406550) to test tile walkability + animation
    timing (`GetTickCount`), then `Zone_SetTile` + `Player_Move` to relocate/animate, and `Iact_Run` to
    fire the entity's scripts. `MapEntity` + `Zone.entities` are now modeled in the Ghidra DB.
  - **Scripts**: `Iact_ProbeMove` (a `GetTickCount`-based timed script/position helper) and the
    `Game_On*` event handlers → `Iact_Run`.
  So "character driving" for both player and enemies lives in `Game_Tick`; decompiling its per-entity
  loops (there are several, guarded by `_rand`) is the way to pull out the enemy AI.

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
  └─ View_FUN_0040b160               entity/character draw (iterates doc->characters, Zone_SetTile)
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

## Still to map
- Naming the remaining condition/command opcode cases against `scrdoc.txt` args (e.g. `SetTempVar`,
  `WarpToMap`, `AddItemToInv`) — the switch structure is mapped; per-case bodies can be annotated.
- The individual `Game_Tick` sub-loops (player step, monster AI, projectile/attack) — currently one
  giant function; splitting it is the next decomp step for the gameplay core.
- `View_FUN_0040b160` (entity draw) and the rest of the `View`/CView `.obj`.
