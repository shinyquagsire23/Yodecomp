# The compiler hunt — finding the exact VC++ 4.2 build that compiled YodaDemo.exe

**Premise (corrected v50):** we are NOT missing a compiler. `toolchain/vc42/` is a genuine VC++ 4.2 —
`CL.EXE` driver **version 10.20.6166**, `C1XX.EXE` (C++ front end), `C2.EXE` (the codegen backend where
register allocation happens). We compile with it via `toolchain/bin/cl`. It byte-matches **211/534** app
functions exactly, which proves it is the correct MAJOR compiler.

**The open problem:** ~48 functions differ from the original ONLY in register allocation (symmetric ESI/EDI /
ECX/EDX role swaps). v37–v40 exhausted the source-side levers (flags, PCH, COMDAT set, emission order, decl
context) trying to flip them. ⭐ **CORRECTED v52: the choice is NOT intrinsic — it is COMPILER-BUILD-sensitive.**
The original was built with a slightly DIFFERENT VC 4.x build whose allocator makes the opposite symmetric
choice on some functions — proven, not conjectured (see the v52 result below).

## ⭐ RESOLUTION v52 (2026-07-08) — the toolchain IS VC 4.2 (proven by the STATIC LIBS); residuals are SOURCE-side
**Final answer, after the compiler A/B AND a static-library fingerprint (the decisive evidence):** the original
`YodaDemo.exe` was built with **VC 4.2 throughout** — cl 10.20.6166 (our exact compiler) + 4.2 headers + 4.2
libs + LINK 3.10. Therefore, by the fresh-TU determinism axiom (identical compiler+source ⇒ identical bytes),
**the ~48 non-exact "reg-coloring" residuals are SOURCE-FIDELITY differences, NOT immovable compiler tie-breaks.**

**How the static libs settled it (the clincher — do this kind of check first next time):** the statically-linked
CRT (`LIBCMT.LIB`) and MFC (`NAFXCW.LIB`) code in the exe's LIBRARY region (~0x429000–0x44b000) is MS-prebuilt
and version-specific. Window-matched it (20-byte reloc-tolerant windows, `tools/`-style) against vc40/vc41/vc42
libs: **1404 windows are unique to vc42's libs, ZERO unique to vc40 or vc41** — e.g. a 96-byte `_makepath`-family
CRT run at VA 0x42a9ff is verbatim in vc42/LIBCMT.LIB and absent from 4.0/4.1. So the LIBRARY axis = VC 4.2,
unambiguously. Headers = 4.2 (211-max is under 4.2 MFC headers). App-cl = 4.1/4.2-class (211) not 4.0 (195). All
axes converge on VC 4.2 = OUR toolchain.

**⚠ RETRACTS the "interim build in (5270,6038)" theory** (written earlier this session from the compiler A/B
alone). The 3 functions VC 4.0 compiles byte-exact that 4.2 doesn't (`DetonateAdjacentTiles` 0x428680, `ParseZaux`
0x423110, `ZoneHasIzxItemMaybe` 0x41bfa0) are NOT evidence of a different original compiler — the libs disprove
that. They are 3 functions where **our source is subtly non-faithful** and VC 4.0's *different* allocator
coincidentally lands on the original's registers. **VC 4.0 is a useful ORACLE** (it confirms the target register
assignment is reachable → a faithful source form exists), not the build that shipped the game.

**Corrects v39 too:** "reg-coloring intrinsic, not source-fixable" — the practical observation (the source levers
tried in v37–v40 didn't reach them) stands, but the NATURE is source-fidelity, and the DetonateAdjacentTiles
param-swap→exact result already proved the registers are source-reachable (the faithful form is just unfound). So
the residual class is a set of hard SOURCE puzzles (attackable, with 4.0 as an oracle), not a compiler wall.

**Attempted next lever (Fable Q1 lever-2) — v52b RESULT: the 3 RESIST faithful source-steering under 4.2.**
Took the stab with 4.0's byte-exact output as the oracle; all three are structurally stuck:
- `DetonateAdjacentTiles` 0x428680 — the contested ESI/EDI hold the two **params** x/y (created in fixed sig
  order; no local to reposition). v39 already proved only the UNFAITHFUL param-swap flips it. Lever-2 N/A.
- `ParseZaux` 0x423110 — the **lesson-#7 clone family** (byte-identical source to `ParseZax2`/`ParseZax3`);
  can't differentiate identical source faithfully (fixing one rotates the clones).
- `ZoneHasIzxItemMaybe` 0x41bfa0 — decl-order swap (`int i=0` before `int nCount=...GetSize()`) REGRESSED
  structurally (align 0→22), not a clean reg flip; also a sibling family (`ZoneFindInIzxList`).
⇒ No faithful 4.2 source form readily reproduces them. **This TEMPERS the "residuals are source-attackable"
optimism above and RE-OPENS the interim-cl possibility** (I over-retracted it): the compiler and libraries are
SEPARABLE axes, so a MIXED toolchain — VC 4.2 libs/headers/linker (proven) + an app-cl that resolves these ~3
tie-breaks the 4.0 way while resolving the other 211 the 4.2 way — is fully consistent with the lib fingerprint.
Detonate is the sharp case: body-fixed (v39) + not-faithfully-4.2-reachable + 4.0 produces it from our faithful
source ⇒ under strict determinism the app-cl is likely NOT bit-identical to our 6166 on these tie-breaks.
**Honest status: UNDETERMINED between (1) a slightly-different interim app-cl [4.2 libs] — re-motivates the
narrow cl hunt in (5270,6038); and (2) pure 4.2 with 3 source-locked functions beyond our search.** 211 stands
as the achievable exact count with our toolchain. The 16-bit Indy / retail-Yoda source witnesses could still
reveal true decl order or clone differences that discriminate (1) vs (2).

**Toolchains kept locally** (all gitignored `/toolchain/vc4*/`, for A/B + the user's other projects):
`toolchain/vc40/` cl 5270, `toolchain/vc41/` cl 6038, `toolchain/vc42/` cl 6166 (the canonical one).

---
### Appendix — the compiler A/B that (before the lib check) suggested an interim build
Tested the WHOLE retail x86 4.x line via the `VCDIR` A/B (4.2 MFC headers kept, to isolate the backend from the
header dial, lesson #26). The PE **linker** 3.10 pins LINK.EXE not CL.EXE (Fable), so earlier-cl objects link
fine with 4.2's LINK 3.10 + NAFXCW.LIB — hence testing point releases was valid:

| build | cl | C2.EXE md5 | exact | vs our 4.2 |
|---|---|---|---|---|
| VC 4.0 | 10.00.5270 | 958c47f9… | **195** | wins 3, loses 19 (DIFFERENT set) |
| VC 4.1 | 10.10.6038 | 6d07c3f7… | 211 | **byte-identical set to 4.2** |
| VC 4.2 (ours) | 10.20.6166 | dcd69f1d… | 211 | baseline |
| VC 4.2 Enterprise | 10.20.6166 | dcd69f1d… | — | md5-identical to ours (v51) |

**The observation that started it:** `DetonateAdjacentTiles` 0x428680 — a PARKED "intrinsic" ESI↔EDI residual
(v39) — is **byte-EXACT under VC 4.0's compiler**, non-exact under 4.1/4.2. (Initially read as "compiler-fixable
⇒ interim build"; the lib check above reinterprets it as "our source is non-faithful; 4.0 is a coincidental
oracle.") The 4.0-vs-4.2 exact-set diff (stable core 192, **union 214**):
- **VC 4.0 wins 3** (exact under 4.0, not 4.2), all in the Worldgen TU, all previously parked reg-coloring:
  `DetonateAdjacentTiles` 0x428680, `ParseZaux` 0x423110 (the lesson-#7 rotation example), `ZoneHasIzxItemMaybe`
  0x41bfa0.
- **VC 4.2 wins 19** (exact under 4.2, not 4.0): ReadIzon, Clear, Fill, FindTile, DrawGameArea, FindEntityAt,
  IactScript ctor/Read, ParseActn, ParseHtsp, OnToggleSound/Music, Randomize, OnNewWorld, LoadStoryHistoryAlaska,
  RemoveZoneEntry2, WorldgenPickItemFromZone, FindAdjacentGateDirMaybe, OnOpenDocument, FindEntityAt.

**What this pins:** our SAME source, compiled under 4.0 vs 4.2, produces different bytes on 22 swing functions;
the ORIGINAL binary matches the 4.2 bytes on 19 and the 4.0 bytes on 3 — INSIDE the same TU (Worldgen, one
compiler). So the original's C2 allocator is NEITHER 4.0 nor 4.2: it has the 6038-era behavior on 211 functions
but the 5270-era behavior on 3. ⇒ **the target is a cl build strictly in (5270, 6038), very close to 6038** — an
interim VC 4.0-era release (service pack / MSDN mid-1996 refresh with cl 10.0x–10.1x) not in the retail
4.0/4.1/4.2 line. 4.1 (6038) already == 4.2, so the 3-function transition happened just before 6038.

**Status of the hunt:** NARROWED, not closed. The obtainable retail x86 4.x line is exhausted (4.0/4.1/4.2 all
tested). Remaining candidate = an interim cl 10.0x/10.1x build between 5270 and 6038 (MSDN Level-2/subscription
discs Jan–Jun 1996, VC 4.0 SP, or a 4.1 beta). If found, A/B it via `VCDIR`; if it flips the 3 while keeping the
19, it is THE build and exact → 214+. Toolchains kept locally: `toolchain/vc40/` (cl 5270), `toolchain/vc41/`
(cl 6038), `toolchain/vc42/` (cl 6166) — all gitignored (`/toolchain/vc4*/`).

**Alternative lever (Fable Q1-2):** the 3 4.0-wins are now proven register-reachable, so a faithful
decl-POSITION source search under 4.2 (declare-at-first-use vs hoisted; scope brackets around late-lifetime
locals — the allocator keys on frontend symbol-creation order) MIGHT reproduce 4.0's choice under 4.2. Untried;
uses 4.0's exact output as the oracle. Worth a focused attempt on Detonate/ParseZaux/ZoneHasIzxItem.

---
## (SUPERSEDED v51 note — kept for history) "no obtainable x86 candidate exists"
The v51 pass concluded the candidate space was empty, but only tested 4.2 *editions* (Pro/Enterprise, both
6166). It missed the point-release axis (linker pins LINK not CL). v52 above corrects it. Still valid from v51:
VC 4.2b (cl 10.20.6312) is RISC/Alpha-only (targets Alpha, link 4.20 ≠ 3.10 — useless for x86); VC 5.0/VS97 link
5.x ≠ 3.10. Those remain ruled out; the live target is the (5270,6038) interim build.

## Why a different build was plausible (rationale — now answered by the HUNT RESULT above)
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
