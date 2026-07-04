# YodaDemo.exe — struct registry (single source of truth)

Every struct is **traced to its allocation** so the size is correct, and defined **once** in the Ghidra
DB (no duplicates). When a function still decompiles with non-idiomatic casts, the fix is to model the
struct it touches — then re-verify the decompilation reads like human C++.

Legend: **TRACED** = size confirmed at an `operator_new`/alloc site; **INFERRED** = size bounded by
observed field accesses (not yet pinned to an alloc — a TODO).

| Struct | Size | Provenance | What / key fields |
|---|---|---|---|
| `Zone` | **0x848** | TRACED — `operator_new(0x848)` in `Dta_ReadZone` (0x426acf) → `Zone_Ctor` | 18×18 map. `tiles[972]@0x10`, `objects@0x7ac`, `iactScripts@0x7c0`, `entities@0x7d4`, `tempVar@0x834`/`randVar@0x838`/`globalVar@0x844` |
| `Tile` | **0x40c** | TRACED — `operator_new(0x40c)` in `Game_OnWalk`/`Game_MovePlayer`/`FUN_0041a030` → `Tile_Ctor` (0x404da0) | 32×32 graphic. `pixels[0x400]@0`, `flags@0x404`, `name` CString@0x408 |
| `MapEntity` | **0x64** | TRACED — `operator_new(100)` in `Iact_ReadZaux` (0x406270) | spawned monster/NPC. `charId@4`, `homeX/Y@6/8`, `x/y@0x12/14`, `dx/dy@0x38/3a`, `[0x20]@0x40` |
| `IactScript` | **0x30** | TRACED — `operator_new(0x30)` in `Dta_ParseActn`/`Iact_ReadScript` | `conditions@8`(CObArray)`/condCount@0xc`, `commands@0x1c/cmdCount@0x20`, `doneFlag@0x2c` |
| `IactCondition` | **0x1c** | TRACED — `operator_new(0x1c)` in `Iact_ReadScript` | `opcode@4` (`IactCondOp`) + `args[5]@8` |
| `IactCommand` | **0x20** | TRACED — `operator_new(0x20)` in `Iact_ReadScript` | `opcode@4` (`IactCmdOp`) + `args[5]@8` + `text` CString@0x1c |
| `MapZone` | **0x34** | TRACED — grid stride at `World+0x4b4` (100 records) | overview-grid cell. `exists@0`, `flagSolved@0x18`, `flagA/B@0x20/24` |
| `Puzzle` | **0x2c** | TRACED — `operator_new(0x2c)` in `Dta_ParsePuz2` (0x422fd0) → `Puzzle_Read` (0x404480) → `World.puzzles@0xd0` | item-for-item quest. `type@0`, `itemA@0x10`(needed), `itemB@0x12`(reward), `text1..5`@0x18–0x28 (dialogue CStrings) |
| `Canvas` | **0x43c** | TRACED — `operator_new(0x43c)` (`Settings_FUN_0041bb10`/`Dta_FUN_00426c40`) → `Canvas_Init` (0x407df0) → `World.canvas@0x3270` | DIBSection offscreen buffer. `hdc@0`, `hbitmap@0xc`, BITMAPINFOHEADER@0x10 (`width@0x14`,`height@0x18`,`bitCount@0x1e`), `palette[256]@0x38`, `pBits@0x438` (separate DIB alloc) |
| `CDC` | 0x10 | model-only (MFC) — only `m_hDC@4` matters | MFC device context; `Render_Blit`'s dest param is `CDC*` (`BitBlt(dest->m_hDC, …)`) |
| `World` | **0x33c0** | TRACED — `CDeskcppDoc` `CRuntimeClass.m_nObjectSize` @0x44c2b0 | the CDocument game doc (real MFC name `CDeskcppDoc`). `tileArray@0x84`(Tile**), `zoneObjects@0x98`(Zone**), `characters@0xc0`, `currentZone@0x2c0`, `playerX/Y@0x2e20/24`, `cameraX/Y@0x3330/34`, health/inventory/score/experience (see game-logic.md) |
| `GameView` | **0x310** | TRACED — `CDeskcppView` `CRuntimeClass.m_nObjectSize` @0x44b228 | the CView subclass (`CDeskcppView`). `doc@0x44`(World*), `frameCounter@0xb0`, `soundSession@0xc4`, plus ~30 input/drag members typed from `View_HandleInput` (`draggedTile@0x140`(Tile*), `dragX/Y/Layer@0x104/8/c`, `dragActive@0x148`, …) |
| `ZoneObj` | **0x10** | TRACED — `operator_new(0x10)` in `Dta_ParseHtsp` (0x4236b0, HTSP reader) → `zone->objects@0x7a8` | a placed hotspot/object. `type@8`,`x@0xa`,`y@0xc` |
| `CFile` | 0x40 | model-only (MFC) — only `vtbl@0` + `Read`@vtbl+0x3c matter for the match | MFC file; `CFile_vtbl.Read` is a fn-ptr |

## Enums
- `IactCondOp` (`COND_*`, 0x00–0x23) — condition opcodes per `scrdoc.txt`; applied to `IactCondition.opcode`
- `IactCmdOp` (`CMD_*`, 0x00–0x25) — command opcodes per `iact.c` `commands[]`; applied to `IactCommand.opcode`

## Real MFC class names (from `CRuntimeClass`)
The app's own classes are `CDeskcpp*` ("Deskcpp" = Desktop Adventures C++): `CDeskcppDoc` (=`World`,
0x33c0), `CDeskcppView` (=`GameView`, 0x310). Descriptive names `World`/`GameView` are kept in the DB
for readability; the CRuntimeClass structs live at 0x44c2b0 / 0x44b228 and pin the sizes. To pin any
other MFC-derived class size, find its `CRuntimeClass {char* name, int nObjectSize, …}` in `.rdata`.

## Correction: `World.canvas` IS a fixed struct (2026-07-04)
Earlier I wrongly concluded the canvas was a computed-size buffer. Tracing `Render_Blit`'s `this`
argument → it's **`doc->canvas`** (@doc+0x3270), a fixed **`Canvas` = 0x43c** DIBSection wrapper
(`operator_new(0x43c)` in `Settings_FUN_0041bb10`/`Dta_FUN_00426c40` → `Canvas_Init` 0x407df0). The
pixels are a **separate** DIB allocation (`CreateDIBSection` fills `pBits@0x438`), not inline — which
is why the earlier alloc scan didn't see a giant block. Now modeled and typed (see registry).

## Open items
- `World+0x88` `tileCount`, the tile-array `CObArray`/`CPtrArray` object at doc+0x80 — confirm the class.
- Map the `Canvas` header fields (pixel base, W/H around +0x438) if the render path needs more clarity —
  but keep it a header-only model (pixels are variable-length inline).
