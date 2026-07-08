# The compiler hunt — finding the exact VC++ 4.2 build that compiled YodaDemo.exe

**Premise (corrected v50):** we are NOT missing a compiler. `toolchain/vc42/` is a genuine VC++ 4.2 —
`CL.EXE` driver **version 10.20.6166**, `C1XX.EXE` (C++ front end), `C2.EXE` (the codegen backend where
register allocation happens). We compile with it via `toolchain/bin/cl`. It byte-matches **211/534** app
functions exactly, which proves it is the correct MAJOR compiler.

**The open problem:** ~48 functions differ from the original ONLY in register allocation (symmetric ESI/EDI /
ECX/EDX role swaps). v37–v40 exhausted the source-side levers (flags, PCH, COMDAT set, emission order, decl
context) trying to flip them, and concluded the choice is intrinsic to *this build's* register allocator. Since
our compiler byte-matches 211 functions, the residual is most likely that **the original was built with a
slightly DIFFERENT 4.2 sub-build** whose allocator makes the opposite symmetric choice — NOT a source bug.

## Why a different build is plausible
- **The binary is dated 1997-02-18** — squarely between VC++ 4.2 (1996) and VC++ 5.0 (VS97, **1997-04-28**).
- **The linker is 3.10** (4.2-era; VC 5.0 shipped a newer linker) — so it's a 4.2-family toolchain, not 5.0.
- **Multiple 4.2 cl builds exist:** VC 4.0 = cl `10.00.5270`; base VC 4.2 = cl `10.20.6166` (ours);
  **VC 4.2b = cl `10.20.6312`** (the Alpha/RISC edition build number per KB Q164951; the x86 4.2b build is a
  later 10.20.63xx). A studio actively on the bleeding edge (LucasArts was doing hand-MMX in the Canvas blits,
  working on Jedi Knight/DF2 in 1997) plausibly ran a later 4.2 subscription/service-pack build than 6166.
- ~146 build-number delta (6166→6312) implies real codegen changes between 4.2 and 4.2b.

## Candidate builds to hunt (Internet Archive / MSDN subscription discs)
1. **VC++ 4.2b (x86)** — the update to 4.2. Look for cl `10.20.63xx`. Highest priority.
2. **VC++ 4.2 "subscription" / Service Pack refreshes** — MSDN Professional/Level-2 discs from late 1996 /
   early 1997 shipped periodic VC 4.2 refreshes with bumped build numbers between 6166 and 6312.
3. **A VC 5.0 BETA** (Feb 1997 was VS97 beta season) — LOWER priority (its linker is newer than the observed
   3.10, so probably not the one), but worth a codegen A/B if found.
- **Search terms:** "Visual C++ 4.2" / "4.2b" ISO, "MSDN" 1996/1997 subscription disc, "Visual C++ 4.2
  Professional", jeffpar kbarchive Q164951 (RISC Edition version table lists per-edition build numbers).
- **What to grab:** ideally the whole VC dir (BIN + INCLUDE + LIB + MFC — headers affect codegen via decls,
  lesson #26), but for a quick codegen A/B only **BIN/C1XX.EXE + BIN/C2.EXE** need to change (keep our
  INCLUDE/LIB/MFC to isolate the backend). Drop it at e.g. `toolchain/vc42b/`.

## The A/B test (infra is READY — v50)
The `toolchain/bin/{cl,link,lib}` wrappers now honor a **`VCDIR`** env override (default `toolchain/vc42`).
So testing a candidate is one command from repo root:

```sh
VCDIR=/absolute/path/to/candidate-vc-dir python3 tools/progress.py
```

`progress.py` recompiles every TU through the wrapper and prints the byte-exact function count.

**Go / no-go:**
- **exact > 211** → the candidate is CLOSER to the original's compiler. If it's the exact build, expect a large
  jump (many reg-coloring functions flip to exact at once, since they share the ESI/EDI wall).
- **exact == 211** (byte-identical to our output) → same codegen as 6166; not the one.
- **exact < 211** → a wrong/older build; discard.
- Fast focused check (no full rebuild): the bellwether is **DetonateAdjacentTiles 0x428680** (Worldgen,
  the pure 60-byte ESI↔EDI swap). `VCDIR=<cand> python3 tools/asmscore.py src/Worldgen/Worldgen.cpp 0x428680`
  — if `reg_pen`/`identity_miss` drop to 0, the candidate's allocator matches the original. Also
  GetZoneIndex 0x423dc0 (ECX↔EDX) and ReenableHotspotObjects 0x40ebe0 (ESI↔EDX).

## If a better build is found
1. Point the default at it (or keep both and pick per-need). Re-baseline progress.py / verify.py / the oracles.
2. Re-run g2 (absolute layout may also improve — the length walls are the same reg-alloc wall; a build that
   fixes register allocation likely fixes the length divergences too, unblocking whole-image layout).
3. Update CLAUDE.md (the reg-coloring ceiling lessons #29/#39 + the milestone) — the "212 ceiling" would lift.

## If no better build is ever found
211 exact + effective, a runnable /OPT:REF image, and all .rdata content validated is the standing deliverable.
The reg-coloring delta is then genuinely a compile-time artifact of build 6166 vs the original's build.
