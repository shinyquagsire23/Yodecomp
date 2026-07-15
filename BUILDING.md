# Building Yodecomp

This project is a decompilation + multi-platform port of LucasArts' *Desktop Adventures*
engine (Yoda Stories / Indiana Jones' Desktop Adventures). Most people want the **portable
SDL build** (`microfx`) — it runs natively on macOS / Linux / Windows and in the browser
(WASM), and needs no wine or vintage toolchain. That's what this guide leads with; the
original **byte-matching WIN32/MFC** build is documented in a collapsible section further down.

| Target | Toolchain | Runs on |
|---|---|---|
| **SDL native** (`microfx`) | your host C++ compiler + SDL3 | macOS / Linux / Windows |
| **WASM** | Emscripten (bundles the SDL3 port) | browser |
| WIN32 / MFC (byte-match anchor) | Visual C++ 4.2 under wine | Windows / wine |

> **Nothing in this repo is a distributable game.** You must supply your own legal copy of the
> original game data + executables — see [What you must supply](#what-you-must-supply). The
> **Yoda Stories demo** data is freely redistributable and is the quickest way to a working build.

---

## What you must supply

The build **extracts window resources (icon, cursors, strings, About box) from the original
executables automatically** at configure time — you do *not* extract anything by hand. All you
provide is the original files, in the folders below (all gitignored, never committed):

| You provide | Used for | Extraction |
|---|---|---|
| `YodaDemo/YodaDemo.exe` | resource base for **every** build (also the Ghidra RE target) | auto (CMake → `extract_res.py`) |
| the game's **data folder** (`.DTA`/`.DAW` + `sfx/`) | loaded at runtime from the run folder | none — used as-is |
| `Yoda Stories/Yodesk.exe` *(full build only)* | retail Yoda icon / title / About | auto (`make_res.py --full`) |
| `INDYDESK/DESKADV.EXE` *(Indy build only)* | Indy icon / title / About | auto (`make_res.py --indy`) |

`YodaDemo/YodaDemo.exe` is required for **all** configs (its resource IDs are baked into the
engine code), so grab the freely-distributed Yoda Stories demo even if you're targeting the
full/Indy game.

### The run folder (game data)

The engine finds its data next to the **executable** (it derives the data directory from
`GetModuleFileName`, not the current directory). Each config has a matching run folder that holds
that game's data; populate it from your own legal copy. The engine loads a single packed
`.DTA`/`.DAW` archive plus a sibling `sfx/` folder of `.WAV` sound effects (Indy also has loose
`.MID` music). These ship with the game — the archive only stores the sound *filenames*, not the
audio itself, so `sfx/` is a real folder you copy over.

<details>
<summary>Per-config run-folder contents</summary>

```
YodaDemo/                     Yoda Stories DEMO (freely redistributable)
  YodaDemo.exe                original demo binary — REQUIRED (resource base for all builds)
  YodaDemo.dta                demo game data
  sfx/                        66 sound-effect WAVs (armed.wav, blaster.wav, ...)
  wavemix.ini                 WaveMix mixer config

YodaFull/                     retail YODA STORIES (full game)
  YODESK.DTA                  retail game data (~4.6 MB)
  sfx/                        sound-effect WAVs
  wavemix.ini

YodaIndy/                     retail INDIANA JONES' DESKTOP ADVENTURES
  DESKTOP.DAW                 Indy game data (~2.36 MB)
  *.WAV                       loose sound effects (ARROW.WAV, DOOR.WAV, GUNSHOT.WAV, ...)
  *.MID                       music (THEME.MID, EERIE.MID, VICTORY.MID, ...)

INDYDESK/DESKADV.EXE          original Indy binary — Indy resource base (also the Ghidra target)
Yoda Stories/Yodesk.exe       original retail Yoda binary — full-game resource base
```

(The `--full` / `--indy` resource EXE paths can be repointed by editing `cmake/PortableSDL.cmake`
/ `CMakeLists.txt`, but the defaults expect these locations.)
</details>

---

## Prerequisites

Common: **CMake** ≥ 3.16 and **Python 3** (the resource + verify tools are plain Python — no pip
packages). Ninja is used automatically if installed.

| Target | Extra |
|---|---|
| SDL native | a host C++17 compiler and **SDL3** (`SDL3_mixer` optional → adds MIDI/full audio). SDL2 works as a fallback; with no SDL at all you get a silent `null` backend. |
| WASM | **Emscripten** (`emcc`/`emcmake`). SDL3 comes from Emscripten's own port (`--use-port=sdl3`) — do **not** install it yourself. |

macOS: `brew install cmake ninja sdl3 sdl3_mixer emscripten`.

---

## Build: SDL native

Configure **without** a toolchain file (the SDL build uses your host compiler); the config matrix
picks the game + variant:

```sh
# Full Yoda Stories, native:
cmake -B build-sdl -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL
cmake --build build-sdl                       # -> build-sdl/yoda (native binary)
cmake --build build-sdl --target run          # stage yoda into YodaFull/ + launch it

# Demo:
cmake -B build-sdl-demo  -DYODA_PLATFORM=SDL
cmake --build build-sdl-demo --target run     # -> runs from YodaDemo/

# Indiana Jones:
cmake -B build-sdl-indy  -DYODA_PLATFORM=SDL -DYODA_GAME=INDY
cmake --build build-sdl-indy --target run     # -> runs from YodaIndy/
```

The `run` target copies the freshly built `yoda` into the run folder matching the config
(`YodaDemo/` · `YodaFull/` · `YodaIndy/`) — where your game data lives — and launches it there
(the shell scripts `run_sdl.sh` / `run_sdl_indy.sh` do the same thing by hand).

Config axes:

| Option | Values | Default | Effect |
|---|---|---|---|
| `YODA_PLATFORM` | `WIN32` / `SDL` | `WIN32` | `SDL` selects this portable build |
| `YODA_GAME` | `YODA` / `INDY` | `YODA` | — |
| `YODA_VARIANT` | `DEMO` / `FULL` | `DEMO` | — |

The build auto-selects a backend from what CMake finds: **SDL3** preferred (add `SDL3_mixer` for
full audio incl. MIDI), then SDL2, else a silent `null` backend. Details: `docs/phase-h4-sdl.md`.

**Visual Studio / MSVC:** the SDL build uses your host compiler, so it works with the Visual
Studio generator too — point CMake at an SDL3 install (e.g. via vcpkg or `-DCMAKE_PREFIX_PATH`).
VS is a *multi-config* generator, so pass `--config` when building:
```sh
cmake -B build-sdl -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL
cmake --build build-sdl --config Release --target run
```
(The executable suffix — `yoda.exe` on Windows — is handled automatically.)

The `microfx` shim is written to compile under MSVC (POSIX/GNU calls are behind portable C++17 or
`_WIN32` guards). It's built + run regularly on clang/macOS; native-Windows link/runtime hasn't
been smoke-tested yet, so treat MSVC as "should build, please report issues."

---

## Build: WASM (browser)

```sh
emcmake cmake -B build-wasm -DYODA_PLATFORM=SDL -DYODA_VARIANT=FULL
cmake --build build-wasm                      # -> build-wasm/yoda.html/.js/.wasm[/.data]
cd build-wasm && python3 -m http.server 8777  # open http://localhost:8777/yoda.html
```

Two asset modes:
- **`-DYODA_WASM_PRELOAD=ON`** (default) — bakes the game data into `yoda.data` (needs the
  `.DTA`/`sfx` present at configure time). Self-contained page; good for local testing.
- **`-DYODA_WASM_PRELOAD=OFF`** — shippable page with **no game data**; an in-page folder picker
  asks the user for their game folder at load. Use this for anything you distribute.

`-fexceptions` and ASYNCIFY are set automatically. See the "WASM build/debug" section of
`CLAUDE.md` for the node/puppeteer test harnesses and architecture notes.

---

<details>
<summary><b>Build: WIN32 / MFC — the byte-match anchor build (needs Visual C++ 4.2 + wine)</b></summary>

This target reproduces the original 1997 `YodaDemo.exe` codegen. It is only needed for
decompilation work (per-function byte-matching); the SDL/WASM builds above give you a playable
game without any of it.

### Host prerequisites
- **CrossOver** or mainline **wine** with a working 32-bit (WoW64) subsystem. Verified host:
  CrossOver's `wine32on64` on Apple Silicon macOS. Mainline `wine-stable` (Homebrew) is a free
  fallback.
- A **Visual C++ 4.2** tree in `toolchain/vc42/` — see the collapsible section below.

### Build

```sh
# Demo (the byte-exact anchor — YODA + DEMO + WIN32):
cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake
cmake --build build-cmake
cmake --build build-cmake --target run          # copy into YodaDemo/ + launch under wine

# Retail full Yoda Stories:
cmake -B build-full  -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_VARIANT=FULL
cmake --build build-full && ./run_full.sh

# Indiana Jones:
cmake -B build-indy  -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_GAME=INDY
cmake --build build-indy && ./run_indy.sh
```

> ⚠ wine's `cl`/`link` are serialized (`JOB_POOL wine=1`) — parallel wine cl deadlocks the
> wineserver. If a build wedges, `pkill -9 -f yoda.exe` and re-run.

### Sound at runtime
The build **generates a silent `wavmix32.dll`** from the committed non-copyrighted stub
(`toolchain/wavmix32/`), so the WIN32 build runs with no sound out of the box. For real audio,
drop the game's genuine `WAVMIX32.DLL` next to the EXE — imports are by name, so it's a drop-in
replacement. (SDL/WASM do their own audio and don't use WAVMIX32.)

See `docs/cmake-build.md` for why this build is custom-command based rather than using CMake's
MSVC ruleset.
</details>

<details>
<summary><b>Providing the Visual C++ 4.2 toolchain (<code>toolchain/vc42/</code>)</b></summary>

Only the WIN32/MFC build needs this. Visual C++ 4.2 (cl 10.20 / 1997) is abandonware — obtain the
CD image (e.g. WinWorld "Microsoft Visual C++ 4.2", or an MSDN disc on archive.org). **Do not run
SETUP** (its 16-bit installer fights wine) — extract/copy the tree into `toolchain/vc42/`:

```sh
hdiutil attach VC42.iso
cp -R /Volumes/.../MSDEV/VC/{BIN,INCLUDE,LIB,MFC} toolchain/vc42/
# or:  7z x VC42.iso
```

Keep `BIN/` intact (`CL.EXE` needs its sibling `C1/C1XX/C2/MSPDB` DLLs). The path must contain
**no spaces**. When populated, `toolchain/vc42/` should look like:

```
toolchain/vc42/
  BIN/          CL.EXE C1.EXE C1XX.EXE C2.EXE LINK.EXE LIB.EXE DUMPBIN.EXE
                CVPACK.EXE CVTRES.EXE  MSPDB*.DLL  (~114 files — keep the whole BIN/)
  INCLUDE/      stdio.h windows.h ... (Win32 SDK + CRT headers)
  LIB/          LIBCMT.LIB (static CRT)  KERNEL32.LIB USER32.LIB GDI32.LIB ... (Win32 import libs)
  MFC/
    INCLUDE/    afxwin.h afxext.h afxcmn.h afxcoll.h ...
    LIB/        NAFXCW.LIB (static retail MFC, ~7.5 MB)  NAFXCWD.LIB ...
    SRC/        MFC source (lets the compiler emit matching message-map / vtable codegen)
```

The critical pieces the build links are `MFC/LIB/NAFXCW.LIB`, `LIB/LIBCMT.LIB`, and the Win32
import libs — you never list these by hand; `afx.h` emits the right `#pragma comment(lib, ...)`.

`toolchain/bin/{cl,link,lib}` are thin **bash** wrappers around the VC4.2 PEs. **Invoke them
directly** (`toolchain/bin/cl ...`), never `wine toolchain/bin/cl`. Smoke-test once populated:

```sh
toolchain/bin/cl /nologo /c /O2 toolchain/test/hello.c && echo OK
```

> **Linker version note.** Our `LINK.EXE` stamps PE linker version 4.20 while the original reads
> 3.10; this is masked by the byte-match harness and affects only a byte-identical *whole EXE* (a
> parked goal). See `toolchain/README.md` and `docs/compiler-hunt.md`.
</details>

<details>
<summary><b>How resources are built (reference)</b></summary>

Each config's window resources are compiled into a `yoda.res` from the original executables you
supplied, keeping YodaDemo's integer resource IDs (the engine depends on them) while overriding
only the identity resources (icon/title/About). CMake runs these for you:

- Demo — `tools/extract_res.py YodaDemo/YodaDemo.exe yoda.res` (copies `.rsrc` verbatim).
- Full / Indy — `tools/make_res.py YodaDemo/YodaDemo.exe yoda.res --full "Yoda Stories/Yodesk.exe"`
  (or `--indy INDYDESK/DESKADV.EXE`).

For the SDL/WASM builds the `.res` is embedded as a C array (`tools/bin2c.py`) and parsed at
runtime by `microfx/src/res/mfxres.cpp`.
</details>

<details>
<summary><b>Verifying the byte-match anchor (decomp work)</b></summary>

If you touch shared engine code, re-run the anchor oracles (all must stay green — see `CLAUDE.md`);
these require the VC4.2 toolchain and `YodaDemo/YodaDemo.exe`:

```sh
python3 tools/progress.py       # 211 exact / 99.17 %
tools/link_exe.sh               # 0 unresolved / 0 duplicates / exit 0
python3 tools/bugscan.py --all  # 0 HIGH / 0 SHIFT
python3 tools/vtcheck.py        # 10 classes CLEAN
python3 tools/msgcheck.py       # 11 maps CLEAN
```
</details>

---

See `CLAUDE.md` for the full development protocol and `docs/` for per-subsystem notes
(`cmake-build.md`, `phase-h4-sdl.md`, `sound.md`, `dta-format.md`, …).
