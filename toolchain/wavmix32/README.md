# WAVMIX32 import library (built from a non-copyrighted stub)

The demo imports 10 functions from **WAVMIX32.DLL** (Microsoft's 1993/94 realtime wave-mixing DLL
— the shipped sound mixer). To link `yoda.exe` (Phase G0, `tools/link_exe.sh`) we need an import
library exposing those symbols. Rather than redistribute Microsoft's copyrighted `wavmix32.lib`,
we build our own from committed, non-copyrighted source:

- **`wavmix32.def`** — module-definition file: the DLL name + the 10 exported function names (an
  interface list — facts, not copyrightable expression).
- **`wavmix32_stub.c`** — do-nothing `WINAPI` stubs with the same names/signatures, so the build
  produces both a correctly stdcall-decorated import lib **and** a runnable (silent) stub DLL.

`tools/link_exe.sh` builds them automatically (generated artifacts are gitignored):

    cl  /nologo /c /MT /O2 /DWIN32 /DNDEBUG /D_WINDOWS  wavmix32_stub.c
    link /DLL /NOENTRY /DEF:wavmix32.def /OUT:wavmix32.dll /IMPLIB:wavmix32.lib \
         wavmix32_stub.obj kernel32.lib

The resulting `wavmix32.lib` provides the stdcall-decorated symbols our objects reference
(`_WaveMixInit@0`, `_WaveMixActivate@8`, …) and imports them from `WAVMIX32.DLL` **by their
undecorated names**, exactly like the original binary. So the linked EXE's import table matches
the original, and at runtime the generated no-op `wavmix32.dll` lets the game run **silently**.

**For real sound:** drop a genuine `WAVMIX32.DLL` (shipped with Yoda Stories, or built from the
Microsoft Wavemix SDK) next to the EXE — the imports are by name, so it is a drop-in replacement.
