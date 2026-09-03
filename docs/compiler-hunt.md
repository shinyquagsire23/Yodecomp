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

**v52c — interim-cl hunt ATTEMPTED, no obtainable candidate (public archives exhausted).** The target window
is tiny: VC 4.0 shipped Dec 1995 (cl 5270), VC 4.1 was the Feb-1996 subscription upgrade (cl 6038). The
original adopted MOST of the 4.0→4.1 allocator changes (211 like 6038) but not 3 (like 5270) ⇒ a build between,
closest to 6038 = a VC 4.1 BETA (Jan 1996) or a Q1-1996 MSDN Development Platform VC press. Checked archive.org:
the ONLY Q1-1996 MSDN item is `msdn-disc3-premium-release-0296` (Feb 1996) = the NT **DDK** disc, no VC
compiler; the earliest VC compiler presses on archive.org are July 1996 (4.1/6038, already have) + Aug 1996
(4.2/6166). VC 4.1 betas would live on **BetaArchive** — but the USER searched it (2026-07-08) for `4.0a`, `4.0
subscription`, and `4.1 beta`: **ALL returned NOTHING.** So the interim build is not obtainable on BetaArchive
either. CHECKED + RULED OUT the
archive.org item `MSDNDeveloperFront` ("MSDN Developer Network January 1996", the Q1-96 Level-2 Development
Platform): scanned SETUP.iso + both numbered MSDN discs — they're SDK/DDK/OLE2-SDK/DDK, NO `MSDEV/BIN` compiler
(VC++ was a SEPARATE product, not part of MSDN Level 2; the whole set is SDKs/DDKs/BackOffice/Library). Don't
re-download it. ⇒ **the obtainable
x86 VC 4.x line is fully exhausted (5270/6038/6166 all tested); no public interim build to A/B.** If a VC 4.1
beta / Q1-1996 MSDN Development Platform VC disc is ever sourced (BetaArchive), drop it at `toolchain/vc4X/` and
`VCDIR=<it> python3 tools/progress.py` — if it flips the 3 keeping the 19 (exact→214+), that's the app-cl and
proves the mixed-toolchain (interim-cl + 4.2 libs) theory. Until then the app-cl axis stays UNDETERMINED and the
compiler lever is parked.
⚠ **v96: the app-cl axis is now effectively SETTLED as pure-4.2** — the 2 remaining discordant
functions are 4.2-reachable via the declaration dial, so no mixed toolchain is required to explain
the data. ~~And "211 stands as the achievable exact count with our toolchain" is RETRACTED: 215.~~
⛔ **v101: that last sentence is itself RETRACTED — 215 was a harness artifact (see the v101 section
below). No dial position beats the baseline.** The pure-4.2 conclusion is unaffected: it rests on the
static-library fingerprint (v52), not on the dial.

## ⛔ v101 (2026-09-02) — THE v96 RE-OPENING IS RETRACTED: "+4/−0 at 215" WAS A HARNESS ARTIFACT

**Verdict: there is NO free-gain dial position. 234 (the v100-corrected count) is the BEST position
known, and every perturbation only LOSES.** The v96 "seven missing symbols" quest is closed.

**Root cause — a THIRD instance of the v100 "the instrument itself can lie" bug, in the one tool the
whole re-opening rested on.** `tools/dialsweep.py`'s `exact_set()` still carried the PRE-v100 COMDAT
filter: it dropped lib-owned COMDATs *even when a marker explicitly names one by mangled hint*. Those
markers then fell back POSITIONALLY inside `match.pair_by_name`, each stealing the COMDAT the next
marker wanted — the same 28-mis-pair cascade through `DeskcppView.cpp` that v100 fixed in
`progress.py`/`idiomscan.py`. ⚠ `membertest.py`, `headersweep.py` and `enumfieldtest.py` ALL measure
through `dialsweep.exact_set()`, so **every sweep number this project has ever published inherited the
bug.** Fixed in the one shared place (v101); `dialsweep`'s baseline now agrees with the anchor at 234
(pre-fix it reported the familiar 211).

**Re-measured on the corrected instrument** (`--all-tus`, dial header `Worldgen.h`, n=0..8):

| n | v96 claimed | v101 extern | v101 struct | v101 typedef |
|---|---|---|---|---|
| 0 | 211 baseline | **234** | 234 | 234 |
| 3 | 213 (+0x423110) | 228 (−6, +0) | 228 (−6, +0) | 228 (−6, +0) |
| 6 | 214 | 227 (−7, +0) | 227 (−7, +0) | 227 (−7, +0) |
| 7 | **215, +4 gained / 0 lost** | 227 (−7, **+0**) | 227 (−7, **+0**) | 227 (−7, **+0**) |
| 8 | 214 | 229 (−5, +0) | 229 (−5, +0) | 229 (−5, +0) |

The three kinds still agree exactly with each other (v96's "validated 4 ways" property — the dial IS
pure symbol count, that part survives). What does NOT survive is the **+4/−0 free gain**, i.e. precisely
the signature CLAUDE.md designates as "the fingerprint of truth". The only function ever made exact by
any position is `0x40a320` (BlitTile), always at a cost of 8 losses — a TRADE, i.e. the fingerprint of
PADDING. Nothing here justifies adding a declaration.

**Why v96 saw a gain that was not there.** v96b decomposed the deficit as "`DeskcppView.cpp` is ~6-8
symbols short → gains `0x40ebe0`, `0x40fca0`". **Both of those functions are ALREADY BYTE-EXACT at the
corrected baseline** — they are among the 17 that the v100 pairing fix recovered. The dial positions
that appeared to "gain" them were just positions where the mis-pairing cascade happened to land
differently. The `Worldgen.cpp` half is likewise unsupported: `0x423110` (ParseZaux) is still non-exact
with **78 differing bytes out of 116** — not a near-miss that one declaration could flip — and
`0x41f830` (CheckZoneItemsAvailable) sits at 9.

**Consequences.**
- The "find the real seven symbols" hunt has no evidence behind it. Do not resume it. (Adding a
  genuinely-missing declaration remains correct on its own merits — just not to chase a number.)
- The v97 "members are INERT" result was measured through the same broken `exact_set()` and should be
  treated as UNVERIFIED (v99 had already partially retracted it from the other direction: adding a
  member FUNCTION to `CDeskcppView` demonstrably rotated Worldgen.cpp).
- The de-facto rule stands and is now better evidenced: **the dial is an instrument, never a knob.**

**Reproduce:** `YODA_PAIR_WARN=0 python3 tools/dialsweep.py --all-tus --kinds extern --max 8`.

## ⭐ v96 (2026-07-26) — HUNT RE-OPENED, then the discriminator FAILED (honestly): idiom test has NO POWER

**Why re-opened:** the v95/v96 de-hex sweep proved the codegen dial is driven by state ENTIRELY OUTSIDE
a function — an **empty** `#include` file rotates `Worldgen.cpp`; an `enum` at the tail of `DeskcppView.h`
(which Worldgen.cpp never references) cost **6** byte-exact functions. v52b's "the 3 resist faithful
source-steering" was therefore a search over a much smaller space than the real one: it tried only LOCAL
levers (param order, decl order, scope brackets, body spelling), because nobody knew the ambient
declaration environment was a lever at all. And all 3 of the 4.0-wins live in Worldgen — the most
dial-sensitive TU we have.

**The test that was run (new `tools/idiomscan.py`):** a different C2 build should change at least one
INSTRUCTION-SELECTION idiom (switch lowering, div-by-constant magic multiply, inline memset thresholds,
setcc-vs-branch, lea-vs-add) somewhere across dozens of functions; a different internal STATE never does.
So classify every non-exact function by asmscore `align` (structure with registers normalized out),
`reg_pen` (is the register difference one consistent bijection), and the MNEMONIC MULTISET delta.

**Result vs our 4.2 — clean, and it is NOT evidence:**
| class | meaning | n |
|---|---|---|
| A PURE-REGALLOC | align==0, mnemonics identical, reg_pen==0 | 1 |
| B REGALLOC-MESSY | align==0, mnemonics identical, reg_pen>0 | 15 |
| C SCHEDULED | mnemonics identical, align>0 (reordered) | 25 |
| D SOURCE/IDIOM | mnemonic multisets differ (unfinished source) | 134 |

⇒ **41 functions differ from the original by register allocation and/or scheduling ONLY** (identical
instruction multisets; 16 of them perfectly aligned). That replaces the long-quoted "~48" estimate with a
measured set. The other 134 are simply not-yet-faithful source — not a compiler question.
**Among the 16 structurally-aligned functions: ZERO instruction-selection deltas.**

**⚠ THE CALIBRATION KILLED THE INFERENCE.** Re-ran the same scan with the VC **4.0** backend
(`toolchain/vc40mix/` = 4.0 BIN + 4.2 INCLUDE/LIB/MFC, reproducing the documented A/B; cl 10.00.5270):
4.0 ALSO shows `align==0 with idiom-signal delta: 0`. Across the **173 functions non-exact under BOTH**
backends, the idiom-signal delta differs on only **3**, and all three are trivial (one `lea`, one `test`).
⇒ **MSVC's instruction selection is effectively FROZEN across the 4.x line; only the register allocator
and scheduler changed.** So "our 4.2 output shows no idiom differences from the original" CANNOT
distinguish same-compiler from a different-4.x-compiler. The premise the test rested on is false, for this
compiler family. Recorded as a negative result — do not re-derive it.

**⭐ BUT: the hunt's evidentiary base is WEAKER than this doc claims — the "3 VC4.0-wins" is now 2.**
`DetonateAdjacentTiles` 0x428680 — "the observation that started it", the doc's *sharp case* — is **NO
LONGER byte-exact under VC 4.0**. Verified today under BOTH 4.0 header sets:
`align=0 reg_pen=4 identity_miss=60 byte_diff=60` (pure vc40) / `77` (vc40mix); insns 377/377 either way.
`ParseZaux` 0x423110 and `ZoneHasIzxItemMaybe` 0x41bfa0 DO still go exact under 4.0, as documented.
Our `Worldgen.cpp` source drifted since v52 (many sessions, incl. the de-hex sweep) and the 4.0 match went
with it. That is exactly the fragility expected if the 4.0 "wins" are **coincidental allocator landings**
rather than a compiler signature — a symmetric ESI/EDI tie-break has a real chance of matching by luck, so
4.0 winning 2-3 of ~41 while LOSING 19 is an unremarkable tail, not a fingerprint.

**Where that leaves the hypothesis (honest status):** interim-cl and the ambient-dial hypothesis are
**observationally equivalent under static residual analysis** — both predict exactly "pure register-allocation
residuals, no idiom differences". No amount of further disassembly comparison can separate them.
The ONLY discriminator left is the **DIAL SWEEP**, and it is now well-motivated and cheap:
- move the dial NEUTRALLY (append declarations to an already-included header's tail — zero token change
  inside any function body, zero line change at EOF), K positions;
- per position recompile just the ONE TU and asmscore the targets (not a full progress.py);
- targets = the 16 `align==0` functions, especially the 2 surviving 4.0-wins.
**If ANY dial position makes one of them exact under 4.2, the interim-cl hypothesis is unnecessary** and we
gain functions. If a broad sweep never does while 4.0 hits them at dial position 0, that is a far stronger
statement than v52b could make. Direct evidence the dial has this power: this session an enum in an
unrelated header moved 6 functions across the exactness line, and **24 functions flip multiset-identity
between the 4.0 and 4.2 backends**.

### v96b — LOCALIZED: the deficit is TWO TUs, and it points at `Worldgen.h`

`tools/headersweep.py` sweeps EVERY shared header and compares reach fingerprints (a header only
perturbs the TUs that transitively include it; the baseline is reused for the rest, so a position
costs reach x ~8s instead of a 13-TU rebuild). n=1..10 plain symbols, project-wide:

| header | reach | exact @ n=1..10 |
|---|---|---|
| **Worldgen.h** | 4 TUs | 210 209 211 213 210 **214 215 214** 214 211 |
| **Deskcpp.h** | 6 TUs | 210 209 211 213 210 **214 215 214** 214 211 (IDENTICAL — its 2 extra TUs are inert) |
| **TextDialog.h** | 2 TUs | 211 211 211 212 211 **213 213 213** 212 210 |
| DeskcppView.h | 6 | always TRADES |
| IactScript.h | 6 | always TRADES |
| MapZone.h | 7 | trades |
| GameObjectClasses.h | 8 | mostly LOSES |
| Canvas.h | 8 | mostly LOSES |
| DeskcppStub.h | 2 | no free gain; loses |

**Decomposition by owning TU:**
- `DeskcppView.cpp` is ~6-8 symbols short -> gains **0x40ebe0, 0x40fca0**
- `Worldgen.cpp` is exactly **7** short -> gains **0x423110** (n>=3) and **0x41f830** (only at n=7)
- **Every other TU is ALREADY at its correct dial position** — Iact, WorldgenHelpers, GameObjects,
  IactScript, DeskcppDoc only ever LOSE when perturbed. This is a localized gap, not a codebase-wide
  fudge factor, and it means most of our header state is already right.

`TextDialog.h` is the clean control: it reaches DeskcppView but not Worldgen, and delivers exactly
DeskcppView's two functions and neither of Worldgen's — the model behaves as predicted.

⇒ **the missing symbols must be visible to DeskcppView.cpp AND Worldgen.cpp but NOT to
Iact.cpp/WorldgenHelpers.cpp** (or those would regress). Exactly one header has that reach:
**`Worldgen.h`**. ⚠ parsimonious, not unique — DeskcppView could need 6 from one place and Worldgen 7
from another and they would overlap in this measurement.

**NEXT (real RE, not sweeping):** inventory the ORIGINAL's file-scope globals in the worldgen
`.data`/`.bss` region from Ghidra against the set we actually declare. `Worldgen.cpp` carries many
(`genZoneTypeScratch`, `genSkipTeleCheck`, `genCellQuestSlot6Scratch`, ...); unmodelled ones are
candidates backed by INDEPENDENT evidence — add them because they are real and let the dial move as a
consequence. If that does not account for ~7, look at forward declarations and at enum FIELD COUNTS
(an enum costs tag + fields, so a mis-transcribed enum is a quantified dial error; `TileFlags` carries
TILE_PLAYER/TILE_ENEMY/TILE_FRIENDLY as COMMENTS rather than enumerators).

**Reproduce:** `python3 tools/idiomscan.py --csv out.csv` (add `VCDIR=$PWD/toolchain/vc40mix` for the
calibration). ⚠ `tools/idiomscan.py` must slice the original at OUR trimmed COMDAT length — do NOT use
`toolchain/test/app_funcs.txt` extents as a byte-comparison basis (that table is for marker-coverage
accounting and has bogus entries, e.g. 0x416620 listed as 1 byte; slicing to it decoded 0 instructions and
fabricated a whole function of phantom "idiom delta"). The tool now ASSERTS that align==0 + equal
instruction counts implies an empty mnemonic delta, so that class of bug fails loudly.

**⛔ HUNT CLOSED (2026-07-08) — no obtainable interim compiler exists on ANY accessible source.**
⚠ **SUPERSEDED v96 / RE-CORRECTED v101: closing it was right, and the v96 reason was WRONG.**
~~Both surviving 4.0-only functions go byte-exact under our own 4.2 at the right dial position, and
215 > 211 is reachable with zero regressions.~~ ⛔ v101: measured on a fixed `dialsweep`, no dial
position is reachable with zero regressions at all, and `0x423110` differs in 78 of its 116 bytes.
Do not resume compiler hunting (that verdict stands on the v52 static-library fingerprint); do NOT
resume the "missing symbols" hunt either — pursue the residuals as ordinary source-fidelity work
(`tools/residuals.py` ranks them on the anchor's own byte oracle). archive.org
(public VC presses = 4.0/4.1/4.2, all tested; Jan-96 MSDN Level-2 = no VC) AND BetaArchive (user searched 4.0a /
4.0 subscription / 4.1 beta → nothing) are both exhausted. The app-cl question (interim-cl mixed-toolchain vs
pure-4.2 + 3 source-locked funcs) is therefore UNFALSIFIABLE with available artifacts — do NOT spend more time
hunting compilers. If a VC 4.x beta/interim ever surfaces from a private collection, the A/B recipe above still
applies; otherwise 211 + effective is the standing content deliverable and the path forward is G2 whole-image.

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
