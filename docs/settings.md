# YodaDemo.exe — settings / registry persistence

The `.obj` at **0x41b2f0–0x41bee0** is the **`CWinApp`-derived app class** (`CDeskcppApp`). Settings
live as fields on the app object and persist to the **Windows registry** through MFC
`CWinApp::WriteProfileInt`/`GetProfileInt` (MFC maps these to `HKCU\Software\<company>\<app>` once
`SetRegistryKey` is called), under the **`[OPTIONS]`** section.

## `Settings_Save` (0x41b2f0)
Writes every setting with `CWinApp::WriteProfileInt(app, "OPTIONS", "<name>", app-field)`:

| Key | app field | meaning |
|---|---|---|
| `PlaySound`  | +0x330c | SFX on/off |
| `PlayMusic`  | +0x3310 | MIDI music on/off |
| `Difficulty` | +0x331c | difficulty level |
| `GameSpeed`  | +0x3324 | game speed |
| `WorldSize`  | +0x3328 | generated world size |
| `Count`      | +0x332c | **completion count** — # times the game has been beaten (persists across sessions). IACT `COND_Experience*` checks it to gate repeat-playthrough item upgrades (force/lightsaber); this is what DA calls "experience". Field named `completionCount` |
| `LCount`     | +0x33a8 | last count |
| `HScore`     | +0x33ac | high score |
| `LScore`     | +0x33b0 | last score |
| `Terrain`    | +0x2e3c | planet/terrain type (also the world's `currentPlanet`) |

After the profile writes it serializes additional state. **Correction (2026-07-04):** the settings live
on the **`World` doc**, not a separate app object — `Settings_Save` is `__fastcall(World *param_1)` and
writes `param_1->playSound/playMusic/difficulty/...` while calling `CWinApp::WriteProfileInt(app, …)` on
the *app* (`this`) as the registry writer. Confirmed by `Player_Move`, which gates audio on
`doc->playSound`/`doc->playMusic`. All nine fields are now named in the `World` struct at the offsets in
the table above (the 0x33xx block only fits the 0x33c0 doc, not a small CWinApp).

## Load
No `GetProfile`-heavy loader exists in the app region (all functions call `GetProfile` < 3×), so the
**demo** appears to start from defaults and only *save* settings; the full game would read them back
per-key in `InitInstance`. (`WorldSize`/`Terrain`/`Difficulty` feed `Worldgen_*`; `PlaySound`/`PlayMusic`
gate the audio in `Sound_Init`/`Player_Move`.)
