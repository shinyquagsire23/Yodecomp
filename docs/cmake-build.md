# CMake build (Phase H1) — extended, configurable builds of the engine

`CMakeLists.txt` + `toolchain/vc42.cmake` stand up a CMake build on top of the wine-wrapped
Visual C++ 4.2 toolchain. This is the **foundation for Phase H** (H2 full Yodesk, H3 Indy, H4 SDL):
it exposes the extension config matrix as CMake options while keeping the byte-exact demo as the
default, preserved corner.

## Quick start
```sh
cmake -B build-cmake -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake     # -G Ninja is picked up if present
cmake --build build-cmake                                            # -> build-cmake/yoda.exe
cmake --build build-cmake --target run                              # copy into YodaDemo/ + launch under CrossOver wine
```
Verified 2026-07-08: the default build produces a 454 KB PE32 GUI `yoda.exe` that loads and runs
under wine (enters its window message loop, 0 unresolved imports).

## Config matrix (Phase H axes)
| Option | Values | Default | Effect |
|---|---|---|---|
| `YODA_GAME` | `YODA` / `INDY` | `YODA` | `INDY` appends `-D GAME_INDY` |
| `YODA_VARIANT` | `DEMO` / `FULL` | `DEMO` | `FULL` appends `-D YODA_FULL` |
| `YODA_PLATFORM` | `WIN32` / `SDL` | `WIN32` | `SDL` is Phase H4 → configure-time `FATAL_ERROR` for now |

```sh
cmake -B build-full -DCMAKE_TOOLCHAIN_FILE=toolchain/vc42.cmake -DYODA_VARIANT=FULL
```

## The byte-match anchor is preserved (the governing principle)
The **default corner** (`YODA` + `DEMO` + `WIN32`) emits the *exact* flag set
`tools/link_exe.sh` uses — `/nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS` —
and invokes the wrapper the same way (cwd=`src/`, bare-basename source → identical `Z:\` provenance),
so its objects are **byte-for-byte** those of the 211-exact anchor build.

Every extension axis is **additive**: it only appends a `-D`, and only when it diverges from the
default. The default adds nothing, so its preprocessed tokens are identical to the anchor. Guards
added in H2–H4 must keep the fall-through (no macro) equal to the anchor path:
`#ifdef GAME_INDY`/`#else`→yoda, `#ifdef YODA_FULL`/`#else`→demo, `#ifndef YODA_PORTABLE`→win32/MFC.

**Proven (2026-07-08):** all 13 TUs' reloc-masked `.text` compiled by CMake's default config equals
the `build/*.obj` produced by the byte-match harness (per-named-COMDAT compare). `progress.py` stays
211 exact / 99.17 % coverage. The fidelity gate remains `tools/{link_exe,verify,progress}.py`; this
CMake build is for the *extended* configs and is only a convenience mirror of the anchor.

## Why custom-command based (not CMake's MSVC ruleset)
cl 10.20 (`_MSC_VER 1020`) predates CMake's MSVC auto-detection; the compiler is a 32-bit Windows PE
run under wine through `toolchain/bin/{cl,link,lib}`. Making CMake's built-in MSVC compile/link rules
drive that (compiler-id probe, `/showIncludes` scanning, modern link-flag/manifest injection) is
fragile and would risk perturbing the anchor. So the project is `LANGUAGES NONE` and every compile /
resource / link step is an `add_custom_command()` invoking the same wrappers, in the same shape, as
`link_exe.sh`. `toolchain/vc42.cmake` only declares the Windows/x86 target and hands over the wrapper
paths + VC tree location.

Build steps (mirror `link_exe.sh`): compile each TU (deps: its `.cpp` + all `src/*.h`) →
`extract_res.py` copies the original `.rsrc` into `yoda.res` → build the `wavmix32` import stub from
committed source → static-MFC link (`NAFXCW` + `LIBCMT` + Win32 imports + wavmix32 + resources).

## Notes / limits
- The WIN32 build needs the original `YodaDemo/YodaDemo.exe` (gitignored, copyrighted) for its
  `.rsrc`; configure `FATAL_ERROR`s with guidance if absent.
- Header dependency is coarse: touching any `src/*.h` rebuilds every TU (the TUs are tightly coupled
  through shared headers — matches reality without a `/showIncludes` scanner).
- Objects go to `build-cmake/obj/` — **separate** from the harness `build/`, so oracles are unaffected.
- `VCDIR=<alt-vc> cmake --build build-cmake` A/B-tests an alternate compiler (the wrappers read the env).
- The CMake image's `.text` is ~+6.6 KB vs `link_exe.sh`'s (link-order layout only; both are `/OPT:REF`
  builds). Byte-identical *image* is Phase G2, which is compiler-wall-blocked and parked — not an H1 goal.
