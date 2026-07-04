# YodaDemo.exe — runtime game logic (script execution & player events)

The gameplay loop turns player input into **IACT script triggers** that run zone action scripts.
Cross-referenced with `~/workspace/DesktopAdventures/src/iact.c` + `scrdoc.txt` / `SCRIPTS.md`.

## Script/IACT interpreter
- **`Iact_Run` (0x00406780)** — evaluates a zone's IACT scripts. `__thiscall`; `param_1` = **event type**
  (`2`=BumpTile, `3`=DragItem, `4`=Walk, `5`=variant). It switches on each script's **condition opcode**
  (`record+4`, values `0x00-0x23` per `scrdoc.txt`: FirstEnter/Enter/BumpTile/DragItem/Walk/TempVarEq/
  RandVar*/EnemyDead/HasItem/CheckEndItem/HealthLs/GlobalVar*/…). When a script's conditions match the
  event, its command list runs (SetMapTile/ShowText/PlaySound/SpawnItem/WarpToMap/AddHealth/… `0x00-0x25`).
  Uses `Zone_FindObjectAt` (0x403250) and the zone tile grid.

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
- **`Render_DrawTileSprite` (0x004070e0)** — draws a tile/sprite to a `CDC`, using `doc->tileArray`
  (+0x84), the camera position (`+0x3330`/`+0x3334`), and the hero grid position. Big switch over
  tile/sprite kind.

## Game tick & enemy AI
- **`Game_Tick` (0x0040b270)** — the main per-frame update (~10.8 KB, **no callers** ⇒ a registered
  timer/idle callback). `void __fastcall(view*)`. Drives everything each frame:
  - **Player**: `Player_FUN_00409060` (step/move), `Player_FUN_00409460` (tile-collision, reads all 3
    layers via `Zone_GetTile`).
  - **Enemy/monster AI**: **inlined here** — `_rand` is called **36×** for random monster movement/
    behavior, with tile checks and `Zone_SetTile` to move them. There is NO separate AI function; the
    monolithic tick updates all entities (typical 1997 MFC game).
  - **Scripts**: `Iact_FUN_00406550` (a `GetTickCount`-based timed script/position helper) and the
    `Game_On*` event handlers → `Iact_Run`.
  So "character driving" for both player and enemies lives in `Game_Tick`; decompiling its per-entity
  loops (there are several, guarded by `_rand`) is the way to pull out the enemy AI.

## Still to map
- The **command executor** switch (SetMapTile/ShowText/… `0x00-0x25`) — inside/near `Iact_Run`.
- The individual `Game_Tick` sub-loops (player step, monster AI, projectile/attack) — currently one
  giant function; splitting it is the next decomp step for the gameplay core.
