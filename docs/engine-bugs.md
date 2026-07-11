# Original engine bugs (YodaDemo.exe)

Genuine defects in the ORIGINAL 1997 binary, discovered during byte-matching. These are **not**
transcription errors — the byte-exact goal means our source reproduces them faithfully, so each
one is a trap for anyone reading the decomp ("that looks wrong" — yes, it is, and it must stay).
Every entry is verified against the disassembly at the given address. When a bug is reproduced
in `src/`, the source carries a `// sic:` or NOTE comment pointing here.

Scope note: demo-build *limitations* (goal=108 hardcode, grayed Save/Load/Replay, pre-seeded
Alaska story list) are intentional and documented in CLAUDE.md's GameData notes, not here.

## YODA_BUGFIX — guarded fixes for production builds (2026-07-10, v75)

CMake option `YODA_BUGFIX` (default **ON** for every non-anchor config — FULL / INDY / SDL;
forced OFF + hard error on the byte-match anchor). Under `-D YODA_BUGFIX`, the `YODA_SIC_FIX(x)`
/ `YODA_SIC_RETURN(x)` / `BUGLOG((fmt, ...))` macros (defined identically at the tail of
Worldgen.h / DeskcppStub.h / DeskcppDoc.h — keep the three copies in sync) activate CRASH/UB/LEAK
fixes at the `// sic:` sites and append a line to **yoda_bugfix.log** (cwd) whenever the ORIGINAL
engine would have hit the edge. In anchor builds every macro expands to empty / bare `return;` on
the same source line — preprocessed tokens and line numbers are unchanged (PTRINT trick; anchor
re-verified 211 after every touched TU).

**Policy: crash class only.** Behavior-shaping bugs stay faithful — fixing them would change
rand() consumption / worldgen output / script scheduling and break seed-parity with the real
game (the H4 M0 digest oracle depends on reproducing them). Verified: the fixed build's worldgen
digest is byte-identical to the unfixed build at the same seed/INI.

| bug | status under YODA_BUGFIX |
|---|---|
| #1 script-index clobber | **kept** (script scheduling = behavior; no log — would fire per evaluation) |
| #2 EnemyDead OOB | **fixed** (idx<0 or ==count → condition fails) + log |
| #3 Show/HideEntity OOB | **fixed** (skip) + log; SetMapTile/ClearTile/MoveMapTile OOB guards widened from GAME_INDY-only to all bugfix builds + log |
| #4 SayText/ShowText article pre-buffer read | **fixed** (probe skipped when pos<2) + log |
| #5 IactRun prologue pWorld deref | **kept** (callers always valid; restructuring = codegen risk for zero gain) |
| #6 RemoveItemFromInv missing pView check | **fixed** (guard added) |
| #7 unreachable OOM dialogs | **kept** (dead code, harmless) |
| #8 unguarded pCanvas->Clear() | **fixed** (guard) + log |
| #9 16-of-20 UI tile ptr clear | **fixed** (clears all 20) |
| #10 shuffle guard never fires | **kept** (fix would change rand() sequence → worldgen parity); no log (fires every shuffle) |
| #11 rollback removes item1a twice | **log-only** (fix would change worldgen; hits ARE seen in the wild — seed 0x2a logs two) + Indy mirror logged (`sic#11i`) |
| #12 teleporter scan result discarded | **kept** (worldgen behavior; log would be per-candidate spam) |
| #13 uninitialized nMask (×2 sites) | **fixed** (zero-init) + log when the IactRun was skipped |
| #14 OnDragItem Artoo DC+palette leak | **fixed** (released on both arms) + log |
| UpdateDragCursor pBitmap leak (unnumbered) | **fixed** (deleted on Attach failure) + log |
| SaveGame/LoadGame pDlg deref before null check (unnumbered) | **fixed** (store guarded; the existing null check then reports) |
| GetZoneById off-planet **-1 sentinel** (hardening, not a bug — the game never queries off-planet ids) | **fixed** (returns NULL) + log |
| #15 `CDeskcppView::Tick`/`DrawEntities` unbounded `charId` (2026-07-11, v84; defensive, not the live crash's cause) | **fixed** (skip entity) + log |
| #16 `ShowWinMessage` Yoda tile-id 780/2034 OOB on Indy's smaller tile catalog (2026-07-11, v84; the ACTUAL live-crash cause) | **fixed** (short-circuit before `GetAt`) + log |

## Confirmed bugs

### 1. `COND_CheckCellItems` clobbers the script-loop index — script iteration restarts
- **Where:** `Zone::IactRun`, condition opcode 0x1e, 0x406f54/0x406f66 (stores to `[esp+0x14]`).
- **What:** the condition's second inventory scan reuses the OUTER script-loop variable as its
  loop counter (`for (idx = 0; idx < nInv; idx++)`). The compiler's final-value replacement makes
  it visible: `idx = 0` is stored, then `idx = nInv` before a countdown loop.
- **Impact:** after any script evaluates a CheckCellItems condition, the script loop continues
  from `nInv + 1` (or 1 if the inventory scan was skipped) instead of the current script index —
  scripts can be re-run or skipped depending on inventory size. Latent in the demo because zones
  rarely combine CheckCellItems with many scripts.
- **Reproduced:** `src/Iact/Iact.cpp` IactRun, `// sic:` comment at the scan.

### 2. `COND_EnemyDead` bounds check off-by-one
- **Where:** `Zone::IactRun`, opcode 0xb, 0x406ac0: `CMP [ESI+0x7d8], EAX; JL fail`.
- **What:** the guard is `entities.GetSize() < args[0]` — strict less-than. When
  `args[0] == GetSize()`, the guard passes and `entities.m_pData[args[0]]` reads one element
  past the end of the array.
- **Impact:** out-of-bounds heap read; the garbage pointer is then dereferenced for `charId`.
  Only triggers on malformed/edge IACT data (`EnemyDead N` where N == entity count).

### 3. `CMD_ShowEntity` / `CMD_HideEntity` have no bounds check at all
- **Where:** `Zone::IactRunCommands`, opcodes 0x17/0x18, 0x4078e1/0x407908:
  `MOV EAX,[ECX+0x7d4]; MOV EAX,[EAX+EDX*4]` with EDX = args[0], unguarded.
- **What:** unlike `CMD_ShowObject`/`CMD_HideObject` (0x15/0x16), which check
  `0 <= idx < objects.GetSize()`, the entity variants index `entities.m_pData[args[0]]`
  directly. Only a NULL check on the fetched pointer follows.
- **Impact:** any script with an out-of-range entity index reads (and conditionally writes
  `active` through) a wild pointer.

### 4. `CMD_SayText`/`CMD_ShowText` article fix can read before the string
- **Where:** `Zone::IactRunCommands`, opcodes 4/5, 0x407358 / 0x407477:
  `MOV AL, [EAX + ESI - 2]` with ESI = placeholder position.
- **What:** the "a → an" article fix reads `head[pos - 2]` (the character two before the
  `\xa2`/`\xa5` placeholder). If the placeholder sits at position 0 or 1 of the message, this
  reads up to 2 bytes before the CString buffer (into the CStringData header).
- **Impact:** harmless in practice (reads length/refcount bytes, only compared against 'a'/'A'),
  but it is an out-of-bounds read on attacker^Wdesigner-controlled data.

### 5. `Zone::IactRun` dereferences pWorld before its own NULL checks
- **Where:** 0x4067bc: `MOV EAX,[EDI+0x5c]` (nFrameMode save) executes unconditionally.
- **What:** the function saves/sets `pWorld->nFrameMode` in the prologue, yet the script loop
  and half the condition handlers carefully test `pWorld != NULL` (e.g. FirstEnter, HasItem,
  CheckEndItem). The prologue dereference makes all those later checks dead code: a NULL
  pWorld crashes before any of them run. Same inconsistency in `IactRunCommands` (cameraX read
  at 0x40710b precedes any check).
- **Impact:** none at runtime (callers always pass a valid doc) — but it proves the NULL checks
  were cargo-culted per-condition rather than a real contract.

### 6. `CMD_RemoveItemFromInv` cellItemA branch skips the pView NULL check
- **Where:** `Zone::IactRunCommands`, opcode 0x1d, 0x407abc (negative-args branch).
- **What:** the explicit-tile branch (`args[0] >= 0`) guards `pView != NULL` before calling
  `GameView::RemoveItem`; the `args[0] < 0` (use current quest item) branch calls it with no
  such check.
- **Impact:** NULL pView + a remove-current-item command = crash. Same never-happens-in-practice
  caveat as #5, same evidence of inconsistent guarding.

### 7. `World::ParseTilesMaybe` — the out-of-memory message box is unreachable
- **Where:** 0x41a030, catch handler at 0x41a164/0x41a178.
- **What:** the TRY around `new Tile` has ONE catch (`CException*`, per the EH tables at
  FuncInfo 0x452438) whose body is `m_pException = e; THROW_LAST();` followed by
  `AfxMessageBox(0xe01e); AfxAbort();` — but THROW_LAST() rethrows, so the message box and
  abort are dead code the compiler faithfully emitted (MSVC 4.2 does no DCE). On OOM the
  user never sees the error dialog; the exception propagates raw.
- **Reproduced:** `src/WorldDoc/WorldDoc.cpp` ParseTilesMaybe, `// sic:` at the catch body.

### 8. `World::OnNewDocument` — unguarded `pCanvas->Clear()` after a guarded allocation
- **Where:** 0x41bb10, tail (Canvas creation block).
- **What:** the Canvas is created under TRY and only `SetPalette` is guarded by
  `pCanvas != NULL`; the following `Clear()` call is unconditional. If the 0x43c-byte
  allocation ever failed, this dereferences NULL.
- **Reproduced:** `src/WorldDoc/WorldDoc.cpp` OnNewDocument, `// sic:` comment.

### 9. `World::OnNewDocument` — only 16 of the 20 cached UI tile pointers are cleared
- **Where:** 0x41bb10 tail: the zero loop over World+0x460 runs 0x10 iterations, but
  `CacheUiTilePtrsMaybe` (0x41a5d0) fills 20 slots (+0x460..+0x4b0). The last 4 cached
  pointers survive a re-init with stale values until the next CacheUiTilePtrs call.
- **Reproduced:** `src/WorldDoc/WorldDoc.cpp` OnNewDocument, `// sic:` comment.

### 10. `World::WorldgenShuffleList` — the "already moved?" guard never fires
- **Where:** 0x41efc0-ish (phase 2 of the shuffle at 0x41ef90): the second pass tests
  `pList->GetAt(k) != -1` — but `CWordArray::GetAt` returns a WORD that zero-extends
  (`xor eax,eax; mov ax,[...]; cmp eax,-1`), so the comparison is ALWAYS true. Elements
  already scattered in pass 1 (slots set to 0xffff) are "re-scattered": the retry loop
  scans for a free temp slot and rand()-places the 0xffff sentinel itself.
- **Impact:** wasted rand() calls and sentinel values written into random temp slots; the
  shuffle still terminates because pass 3 copies the temp back wholesale. The list ends up
  shuffled but with the sentinel handling doing extra no-op work each generation.
- **Reproduced:** `src/Worldgen/Worldgen.cpp` WorldgenShuffleList, `// sic:` comment
  (`!= -1` kept verbatim; the other comparisons in the function use 0xffff and are fine).

### 11. `World::WorldgenPlaceItemForLockChainMaybe` — failure path removes item1a twice, never item2
- **Where:** 0x41d1df/0x41d1e7 (in 0x41d0c0): when `CheckZoneItemsAvailable` fails after both
  `WorldgenAddZoneEntry(item1a, ...)` and `WorldgenAddZoneEntry(item2, ...)` registered their
  items, the rollback calls `RemoveZoneEntry2` TWICE with `item1a` (both `PUSH EBX`) — `item2`
  stays registered. Classic copy-paste slip; the second call should pass `item2`.
- **Impact:** a stale `item2` entry survives in the worldgenRefZones dedup set, so that item is
  treated as "already placed" for the rest of generation — it can suppress later placements of
  the same item on this seed.
- **Also here:** both `if (item1 >= 0)` / `if (next >= 0)` guards test a zero-extended WORD as
  int, so they are ALWAYS true (same family as #10) — kept verbatim with `// sic:` comments.
- **Reproduced:** `src/Worldgen/Worldgen.cpp` WorldgenPlaceItemForLockChainMaybe
  (`RemoveZoneEntry2(item1a);` twice, `// sic:` comment).

### 12. `World::PlaceQuestNode` — the ENEMY_TERRITORY teleporter scan discards its result

- **Where:** 0x41f2a1–0x41f2cb (the `case 1:` arm of the placement switch, genSkipTeleCheck != 0
  branch).
- **What:** when the teleporter-distance check is enabled, the candidate zone is accepted
  immediately if it has NO objects (`JLE 0x41f757` -> return zoneId). Otherwise the object list
  is scanned for an existing `OBJ_TELEPORTER` (type 0xd) — but BOTH scan outcomes (found via
  `JZ 0x41f737`, and exhausted via `JMP 0x41f737`) fall to the switch break and just skip the
  zone. The intended "accept if no teleporter present" return is missing, so any
  ENEMY_TERRITORY zone that contains objects can never be picked in this mode.
- **Impact:** teleporter placement silently restricted to object-free zones; on maps without
  such zones the placer exhausts its candidate list and returns 0xffff.
- **Reproduced:** `src/Worldgen/Worldgen.cpp` PlaceQuestNode (`// sic:` comment on the scan).

### 13. `GameView::ZoneTransitionStep` — step 10 reads an uninitialized IACT result mask

- **Where:** 0x409b2c (the only write to `[EBP-0x1a]`) vs 0x409b30 / 0x409b5c (unconditional
  `TEST byte ptr [EBP-0x1a], 0x4 / 0x20` reads), inside the `nStep == 10` arm.
- **What:** the step-10 arm declares a local result mask for its final zone-entry
  `Zone::IactRun(5, ...)`, but the call is guarded (`bSkipEntryIactMaybe == 0 &&
  bWorldInvalidMaybe == 0`) while the two flag tests afterward (`& 4` → UpdateCamera,
  `& 0x20` → DrawWholeZone) are not. When the IactRun is skipped the mask is stack garbage —
  whatever the earlier ring-blit loop left in that slot — so the redraw decisions at step 10
  are nondeterministic in that path.
- **Impact:** benign-looking (worst case a spurious UpdateCamera/DrawWholeZone or a missing
  one right after a skipped entry script); invisible in normal play because the flag is only
  set around scripted zone entries.
- **Reproduced:** `src/GameView/GameView.cpp` ZoneTransitionStep (`unsigned short nMask;`
  deliberately left uninitialized, `// sic:` comment).

### 14. `GameView::OnDragItem` — Artoo help categories 0x13/0x14 leak the DC and selected palette

- **Where:** 0x00410a7d / 0x00410a98 (the two `PlaySound(6); return;` arms of the
  ClassifyTile switch inside the drag-Artoo branch).
- **What:** the function opens with `GetDC()` + `SelectPalette(pWorld->pPalette, 0)`. Every
  other exit path re-selects the old palette and calls `ReleaseDC`; these two arms jump
  straight to the epilogue (0x410f70) after destroying the CString — the DC is never
  released and the game palette stays selected into it.
- **Impact:** latent resource leak (one DC per drag onto a category-0x13/0x14 target);
  Win9x-era common DCs made this mostly invisible.
- **Reproduced:** `src/GameView/GameView.cpp` OnDragItem (`return; // sic` comments on both
  case arms).

### 15. `CDeskcppView::Tick` / `DrawEntities` — unbounded `MapEntity::charId` read (defensive; not the live crash's cause — see #16)

- **Where:** `Tick` (the `if (nCharId < 0) goto NEXT_ENT;` guard + the two
  `pWorld->characters.GetAt(nCharId)` sites it protects) and `DrawEntities`
  (`pWorld->characters.GetAt(pEnt->charId)`), `src/DeskcppView.cpp`.
- **What:** both functions only reject a NEGATIVE `charId`; nothing checks it against
  `pWorld->characters.GetSize()`. `MFC`'s real `CObArray::GetAt` has no bounds check in a
  release (`/NDEBUG`) build either — it silently reads past the array (UB, usually harmless
  garbage) — but our microfx `CObArray::GetAt` (`microfx/include/afxwin.h:324`) asserts, so an
  OOB index is a hard crash under the SDL port instead of the original's silent corruption.
- **Impact:** applied defensively while chasing a live-reported (2026-07-11 playtest) crash
  ("talking to an indoor NPC" / "holding Up" in GAME_INDY reliably crashes with `Assertion
  failed: (i >= 0 && i < m_nSize), function GetAt, file afxwin.h, line 324`). A backtrace
  capture (`MfxArrayOOBTrap`, see #16) proved the ACTUAL culprit is `ShowWinMessage`'s
  hardcoded tile ids, not `charId` — this guard is harmless hardening, kept for defense in
  depth, but was NOT the fix. Root cause + fix: #16.
- **Reproduced:** `src/DeskcppView.cpp` `Tick`/`DrawEntities`, `// sic:`-style `YODA_SIC_FIX`
  guards (`sic#15`) — skip the entity and `BUGLOG` the `charId`/count instead of reading OOB.
  Only the two sites flagged by investigation are guarded (the `nCharId`-indexed reads); the
  `weaponCharId`-indexed `GetAt` calls in the same functions are NOT yet guarded (lower
  suspicion, and the `ALIVE_ENT` site sits in a dense goto-heavy block where adding a new label
  needs its own care) — a real "still open" TODO if the crash recurs after this fix.

### 16. `CDeskcppView::ShowWinMessage` — Yoda-hardcoded tile ids 780/2034 exceed Indy's tile catalog

- **Where:** `src/DeskcppView.cpp:4331` (`pWorld->tiles.GetAt(780)`) and `:4392`
  (`pWorld->tiles.GetAt(2034)`), the two special-item arms at the top of `ShowWinMessage`
  (called from `OnBumpTile` on every interactive-cell bump — walking into an NPC counts).
- **What:** these constants are literal indices into Yoda's tile catalog (comparing the
  currently-equipped item's `Tile*` against a specific hardcoded tile — a Yoda-only special
  item, per the function's own comment: "tile 780 (the demo's goal item)" / "tile 2034 with
  goalItemTileId 0xbd"). Neither is `GAME_INDY`-ifdef'd. Indy's tile catalog (from
  `DESKTOP.DAW`) is much smaller — a live crash capture (`MfxArrayOOBTrap` in
  `microfx/include/afxwin.h`, temporary diagnostic added while chasing this) showed
  `CObArray::GetAt(i=2034) n=1144` — `pWorld->tiles` only has 1144 entries, so index 2034 is
  read OOB **unconditionally**, on essentially every bump (the `780` arm almost never matches
  since it's Yoda's own item, so control flow reaches the `2034` arm's `GetAt` — evaluated
  BEFORE the `goalItemTileId == 0xbd` check due to left-to-right `&&` — on nearly every call).
- **Impact:** live-reported (2026-07-11 playtest) — crashes on bumping/talking to an indoor
  NPC in GAME_INDY, and reproduces even just holding an arrow key into any interactive cell.
  Confirmed fixed: live retest after the guard below no longer crashes (same repro steps).
- **Root-cause note:** the *correct* fix is DESKADV.EXE's own equivalent special-item tile ids
  (Indy has no Yoda "Force" item, so these arms are presumably dead for Indy — but the real
  binary must either never reach `GetAt` with an OOB constant, or use Indy-catalog-sized ids).
  Not yet RE'd against DESKADV.EXE — the guard below is a crash-class fix (policy: intro),
  not a behavior-accuracy fix; these two arms may still be semantically WRONG for Indy (just
  no longer crashing).
- **Reproduced:** `src/DeskcppView.cpp` `ShowWinMessage`, `YODA_SIC_FIX` guards (`sic#16`) —
  short-circuit `tiles.GetSize() > 780` / `> 2034` before the `GetAt` call (so an undersized
  catalog just falls through to the next arm, same as "item not equipped"), `BUGLOG` on the
  OOB case.

## Compiler quirks that LOOK like bugs (they aren't)

- **Dead stores to the script index** (`idx = 0` / `idx = nInv` in #1) are the compiler's
  init-store + final-value replacement for a memory-homed loop variable — the countdown itself
  runs in a register. Recognize the pattern before "fixing" it.
- **`QuestSpotPresent` leaves `matched` untouched when the zone has no objects** (0x406e62
  JLE straight to the loop increment): an empty object list neither passes nor fails the
  condition — matched keeps its prior value. Looks accidental, is faithful control flow.

## How to add entries

Verify against the disassembly (address + instruction), state the impact honestly (most of
these are latent), and cross-link the `src/` reproduction site with a `// sic:` comment. Keep
demo limitations and TU-phase/codegen artifacts out — those live in CLAUDE.md.
