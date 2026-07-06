# Original engine bugs (YodaDemo.exe)

Genuine defects in the ORIGINAL 1997 binary, discovered during byte-matching. These are **not**
transcription errors — the byte-exact goal means our source reproduces them faithfully, so each
one is a trap for anyone reading the decomp ("that looks wrong" — yes, it is, and it must stay).
Every entry is verified against the disassembly at the given address. When a bug is reproduced
in `src/`, the source carries a `// sic:` or NOTE comment pointing here.

Scope note: demo-build *limitations* (goal=108 hardcode, grayed Save/Load/Replay, pre-seeded
Alaska story list) are intentional and documented in CLAUDE.md's GameData notes, not here.

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
