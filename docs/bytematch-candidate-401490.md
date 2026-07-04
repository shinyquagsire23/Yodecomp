# Bytematch candidate — `FUN_00401490` (zone-completion score)

First real reversing target for proving the toolchain (Phase 2). Prepared 2026-07-04.

## What it is
A leaf function that computes a **world-completion score**. It scans a 10×10 grid
(100 zone records) starting at `this+0x4B4`, stride `0x34` (52 bytes) per record, counts
"solved" zones, divides by the total zone count (`this+0x58`), scales to a percentage, and
returns a score of 30–300 in 30-point steps by 10 % band.

- Calling convention: `__fastcall`/`__thiscall` — the struct pointer arrives in **ECX**.
- **Pure leaf: zero `CALL` instructions.** Only relocations are to a `.rdata` float/double
  constant pool at `0x0044B09C … 0x0044B140`.
- Caveat: **x87 floating point heavy** (FLD/FCOMP/FNSTSW/FIDIV/FMUL). Per Fable, FP is the
  *harder* first-match case (x87 instruction selection + constant-pool ordering). It is still a
  clean leaf and very matchable; if the first green proves stubborn, fall back to a pure-integer
  leaf. See `../CLAUDE.md` Phase 2.

## Compiler / flags (decomp.me)
- Platform: **win32** (MSVC), Compiler: **MSVC 4.20** (`cl` 10.20).
- Flags to try first (DevStudio 4.x static-MFC Release default):
  `/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS"`
- Brute-force axis if not green: `/O2` vs `/O1`, `/Oy` vs `/Oy-`.

## Reconstructed C (matching-oriented, first draft — validate on decomp.me)
```c
/* Zone record: 0x34 (52) bytes. Only the fields this function touches are named. */
typedef struct Zone {
    short exists;      /* +0x00  short, >0 means the zone is populated        */
    char  pad02[0x1e];
    int   field20;     /* +0x20  == 1 when ... (solved flag A)                */
    int   field24;     /* +0x24  == 1 when ... (solved flag B)                */
    char  pad28[0x0c];
} Zone;                /* sizeof == 0x34 */

typedef struct World {
    char  pad00[0x58];
    int   totalZones;  /* +0x58  divisor: total number of zones in the world  */
    char  pad5c[0x4b4 - 0x5c];
    Zone  zones[100];  /* +0x4b4  10 x 10 grid                                 */
} World;

/* ECX = this */
int __fastcall World_CalcCompletionScore(World *this)
{
    int   y, x;
    Zone *pZone = this->zones;
    int   score = 0;         /* -> EDI: the return value AND the reused zero      */
    float count = 0.0f;      /* zeroed by storing EDI(0) into the float slot      */

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (pZone->exists > 0 && pZone->field24 == 1 && pZone->field20 == 1)
                count = count + 1.0f;
            pZone = (Zone *)((char *)pZone + 0x34);
        }
    }

    {
        float pct = (count / (float)this->totalZones) * 100.0f;
        /* else-if chain with ONE trailing `return score`: keeps `score` (EDI) live
           across every band so MSVC 4.2 can't fold `mov edi,VAL; mov eax,edi` down to
           `mov eax,VAL`. It tail-duplicates the `mov eax,edi`+epilogue into each band. */
        if      (90.1 <= pct && pct <= 100.0) score = 300;
        else if (80.1 <= pct && pct <=  90.0) score = 270;
        else if (70.1 <= pct && pct <=  80.0) score = 240;
        else if (60.1 <= pct && pct <=  70.0) score = 210;
        else if (50.1 <= pct && pct <=  60.0) score = 180;
        else if (40.1 <= pct && pct <=  50.0) score = 150;
        else if (30.1 <= pct && pct <=  40.0) score = 120;
        else if (20.1 <= pct && pct <=  30.0) score =  90;
        else if (10.1 <= pct && pct <=  20.0) score =  60;
        else if ( 0.0 <= pct && pct <=  10.0) score =  30;
    }
    return score;
}
```
Notes for matching:
- **Return value must be a single variable (`score` -> EDI), not direct `return CONST`.** The original
  routes every band through EDI (`mov edi,VAL; mov eax,edi`), keeps EDI=0 as the default, and reuses
  that 0 to zero the float local. Using `return 300;` directly (v1) produced `mov eax,300`, a spurious
  `xor eax,eax` default, no `push ebx`, and put the loop's `1` in EDI instead of EBX. The single-var
  form cascades all of those into place.
- The original hoists `1.0f` onto the x87 stack before the loop (`FLD [0x44b09c]`) and keeps the
  accumulator live in a register — this is `/O2` behavior. If the compiler emits `FLD1/FADD` inside
  the loop instead, that's an `/O1` vs `/O2` tell — adjust flags.
- Band comparisons must be emitted top-down in source order (they are, above) so the double
  constant pool (`0x44b0a8…`) lands in the same order.
- `this->totalZones` uses `FIDIV` (integer divide against x87) — keep the divisor an `int`, not a cast.

## Target assembly (paste into decomp.me)
The full decomp.me target — GNU-`as` `.intel_syntax noprefix` with labels for every jump target —
is in **`target-401490.s`** (215 lines). Function spans `0x00401490 … 0x004016C4` (565 bytes).

Caveats when pasting:
- The `fld/fcomp qword ptr [0x44b0xx]` operands reference the **`.rdata` double/float constant pool**
  (`0x44B09C…0x44B140`). Your compiled version references its own local constants at different
  addresses — decomp.me flags these as relocations and they should be treated as matching, not real
  diffs. The *ordering* of the pool is what must match (band thresholds top-down).
- The two-operand x87 forms `fld st(1)` / `fadd st(1)` / `fstp st(2)` are the `/O2` accumulator-on-the-
  x87-stack idiom. If your C emits `fld1`+`fadd` per iteration instead, that's the `/O1`-vs-`/O2` tell.

### Ground-truth bytes (565 = 0x235)
```
83 ec 08 b8 0a 00 00 00 53 56 57 8d b1 b4 04 00
00 33 ff 89 7c 24 0c d9 44 24 0c d9 05 9c b0 44
00 ba 0a 00 00 00 bb 01 00 00 00 66 83 3e 00 7e
...  (full hex: regenerate from YodaDemo.exe file offset 0x890, len 0x235)
```
Re-dump the human-readable disassembly anytime with:
`curl "http://localhost:8089/disassemble_function?program=YodaDemo.exe&address=00401490"`

## How to prove the match
1. Open https://decomp.me , New scratch → platform **win32**, compiler **MSVC 4.20**, flags above.
2. Paste the target disassembly as the target and the C above as the source.
3. Iterate flags/source until the diff is 100 %. Record the winning flag set in `CLAUDE.md` Phase 2.
