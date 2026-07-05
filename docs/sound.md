# YodaDemo.exe — sound (SFX via WAVMIX32) & music

Sound effects go through **WAVMIX32.DLL** — LucasArts' software wave mixer (imports `WaveMixInit`,
`WaveMixOpenWave`, `WaveMixPlay`, `WaveMixOpenChannel`, …). Music is MIDI (loaded via the OPTIONS path).

## Data
- **`SNDS` chunk → `Dta_ParseSnds` (0x4233f0)** — reads the list of sound-file names (`.wav` paths) as
  `CString`s into **`World.soundNames` @doc+0xe4** (an inline `CString` array).

## Engine
- **`Sound_Init` (0x411520)** — `WaveMixInit()` → stores the session handle in **`GameView.soundSession`
  @view+0xc4**; then walks `doc->soundNames` and `WaveMixOpenWave`s each `.wav`. Idempotent (returns if
  `soundSession` already set). One-time audio setup.
- **Playback** — `WaveMixPlay` plays a loaded wave on a channel. Called from `Player_Move` (0x409060,
  movement/action SFX) and the IACT `CMD_PlaySound` (opcode 0xa) command path.
- **`Sound_Flush` (0x413bf0)** — `WaveMixFlushChannel` (stop/flush queued sounds).
- **Teardown** — the view dtor `FUN_00408c60` (Core `.obj`, vtable 0x44b638) `WaveMixActivate(…,0)` +
  `WaveMixFreeWave` (per wave) + `WaveMixCloseChannel`/`WaveMixCloseSession`, then clears `soundSession`.

## Music
- the **MIDILoad** registry flag (OPTIONS section) is persisted on exit by `GameView::ConfirmExitMaybe` (0x416030, was mislabeled `View_OnOptions`) and lazily read by `GameView::ProcessWalkMaybe` (0x411180); the actual audio on/off is `World::ToggleSound`/`ToggleMusic`. (`0x416030` refs
  `MIDILoad`) — separate from the WAVMIX SFX path.

## Fields modeled
- `GameView.soundSession` @0xc4 (WaveMix session HANDLE)
- `World.soundNames` @0xe4 (CString[] of `.wav` paths)
