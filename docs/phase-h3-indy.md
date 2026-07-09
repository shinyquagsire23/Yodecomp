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

**Remaining for a clean full parse (still Yoda-format under GAME_INDY ⇒ will misread):** `PUZ2` (Indy
record lacks Yoda's `unk3`/`item_b`), `CHAR` (record `0x54`→`0x4E`), `CHWP`/`CAUX`/`TNAM`/name lengths
(`24`→`16`), and `TILE`/`SNDS` (verify shared). Then **2b** distribute the global HTSP objects + ACTN
IACT lump + aux back to zones; **3** Indy palette; **4** Indy worldgen (no planets — `Generate`/
`LoadWorld`/`WorldgenSelectPuzzle` planet logic is Yoda-specific and will fail as-is).

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
