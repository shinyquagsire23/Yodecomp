// Iact — the Iact .obj (0x405ae0–0x407cf4): Zone's second source file — .dta chunk readers
// (IZON/IZAX/IZX2/IZX3/IZX4), the .wld saved-state pair, and the IACT interpreter
// (IactProbeMove/IactRun/IactRunCommands). TU COMPLETE 2026-07-06: all 10 functions transcribed;
// 88% of original instructions byte-identical; ReadIzon+ReadZax4 exact under the current dial.
// Residuals are annotated per-function — all tie-break class (reg/cmp/schedule/slot), judged
// jointly at endgame per the TU-phase doctrine.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
#include "GameObjectClasses.h"
#include "IactScript.h"
#include "DeskcppStub.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#pragma intrinsic(strcmp)

// FUNCTION: YODA 0x00405ae0  [dial-breather: was exact; DIFF(7) since the real-GameView.h
//   de-dup (step 5, 2026-07-07, commit f1ca459 — WorldStub.h now includes the real GameView.h,
//   which Iact.cpp pulls in). align=0, one pure EBP<->EBX cycle (loop counter i vs the tile
//   walk-pointer). v36 root-cause + probes: it is Iact's FIRST emitted function, so its coloring
//   is seeded purely by the TU header/decl CONTEXT, not its body — loop forms (idx/while/do-while/
//   w-hoist) all INERT; /O2 uniquely correct (flag sweep: /Ox/O1/Og all catastrophic, none flip it).
//   v111 closes two MORE axes: tag[5..16] (lesson #36 buffer size) is flat at 7 for 5-8 and only
//   WORSENS after (20/16), and all 6 decl orders are flat at 7. This is header-phase displacement
//   from a deliberate correctness de-dup (retiring the
//   poisoning stub, lesson #22) — reverting would trade correctness for bytes. Joint-fixed-point
//   (G2 whole-image) resolves it; do NOT mangle this correct body. See tools/frontier.py.]
// Parses an IZON header up to the tile grid: dims/type/vars + the 18x18x3 tile rows.
// A mismatched tag rewinds the 8 header bytes (record is optional in the stream).
void Zone::ReadIzon(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;

    pFile->Read(tag, 4);
    tag[4] = 0;
    pFile->Read(&size, 4);
    if (strcmp(tag, "IZON") != 0) {
        pFile->Seek(-8, CFile::current);
        return;
    }
    pFile->Read(&width, 2);
    pFile->Read(&height, 2);
    pFile->Read(&type, 4);
#ifndef GAME_INDY
    pFile->Read(&globalVar, 2);
    pFile->Read(&planet, 2);
#endif
    // Indy's IZON header is 8 bytes (width+height+type) — it drops Yoda's globalVar+planet
    // (no planets). Verified: the per-zone stride in DESKTOP.DAW is exactly IZON size; reading
    // the extra 4 bytes overran every zone by 4 and cascaded into a total misalignment.
    for (i = 0; i < width; i++)
        pFile->Read(&tiles[i * 54], height * 6);
}

// [v120: the CONTAINER CALL FORM (lesson #48) is measured here and the current spelling is
//  POSITIVELY CONFIRMED — `objects.Add(o)` costs 12 B -> 19 at identical length, i.e. converting
//  would make an already-length-exact function worse. Dropping the `add` local (`for (i = n;
//  i < count; i++)`) costs ~570 B either way, so the count-difference local is confirmed too.]
// FUNCTION: YODA 0x00405bd0  [EFFECTIVE MATCH: DIFF(12) — a pure ebx<->esi 2-cycle in the LAST
//   loop (ebx = the scaled iactScripts index, esi = i), plus the `this` load sitting one push
//   earlier. Length (862) and the callee-save set both already match, so lesson #42/#43 do not
//   apply. v111 closed all THREE decl dials and the loop form: 12 decl set+orders (floor 12,
//   several at 18), all 32 hoist subsets of {n,add,e,p,k} (floor 12, monotonically worse with
//   more hoists), and 9 loop/named-local spellings — the countdown and unguarded do-while are
//   refuted by LENGTH (860/857 vs 862), confirming the `for`, and a named `IactScript *s` for the
//   cast chain costs 2 B. Read as the v105 TU-joint phase: the lever is not in this body.
//   ⚠ v112 re-opened this as a SET x ORDER INTERACTION and closed it again: the write mirror
//   went 20 -> 7 on exactly that (>=5 function-scope decls AND `i` last), so the same 51
//   configurations were measured here — every hoist subset of {n,add,e,p,k} with `i` pinned
//   LAST, at two orders. Floor is still 12 (11 configs tie it, the rest are 14-27). The two
//   mirror functions genuinely respond differently to the same dial; that asymmetry is the
//   v105 joint-phase reading, not a missed spelling.]
// Read this zone's runtime state from a .wld: vars + tile grid (bFull), activatedFlag,
// objects (grown with new ZoneObj as needed), entities, IACT script done-flags.
// Called by World::LoadZoneRecursive.
void Zone::ReadSavedState(CFile *pFile, int bFull)
{
    ZoneObj *o; int count;
    int i;

#ifdef GAME_INDY
    // Retail Indy (DESKADV FUN_1010_1dd4) stores the per-zone saved state in its native 16-bit
    // layout: every field our 32-bit code declares `int` is 2 bytes, and several fields are
    // DROPPED entirely — the zone header omits globalVar/planet, each ZoneObj saves only
    // state+arg (NOT type/x/y), and each MapEntity is only its first 16 fields (no waypoints/
    // item/timers). Reading the Yoda 32-bit layout (the code below) mis-restored those dropped
    // fields — overwriting regenerated per-object type/x/y and zone globalVar with stale values,
    // which is what broke specific scripts on load. GAME_INDY-only; anchor path is untouched.
    {
        short s16;
        if (bFull) {
            pFile->Read(&s16, 2); tempVar     = s16;
            pFile->Read(&s16, 2); randVar     = s16;
            pFile->Read(&s16, 2); doorReturnX = s16;
            pFile->Read(&s16, 2); doorReturnY = s16;
            for (i = 0; i < width; i++)
                pFile->Read(&tiles[i * 54], height * 6);
        }
        pFile->Read(&s16, 2); activatedFlag = s16;
        pFile->Read(&s16, 2); count = s16;
        int n = objects.GetSize();
        for (i = 0; i < count - n; i++) {
            ZoneObj *o = new ZoneObj;
            objects.SetAtGrow(objects.GetSize(), o);
        }
        for (i = 0; i < count; i++) {
            ZoneObj *o = (ZoneObj *)objects[i];
            pFile->Read(&o->state, 2);
            pFile->Read(&o->arg, 2);
        }
        if (bFull) {
            pFile->Read(&s16, 2); count = s16;
            for (i = 0; i < count; i++) {
                MapEntity *e = (MapEntity *)entities[i];
                pFile->Read(&e->charId, 2);
                pFile->Read(&e->x, 2);
                pFile->Read(&e->y, 2);
                pFile->Read(&e->damageTaken, 2);
                pFile->Read(&s16, 2); e->active      = s16;
                pFile->Read(&e->unk10, 2);
                pFile->Read(&e->bulletX, 2);
                pFile->Read(&e->bulletY, 2);
                pFile->Read(&e->aiStepCounter, 2);
                pFile->Read(&s16, 2); e->unk18       = s16;
                pFile->Read(&s16, 2); e->bRetreating = s16;
                pFile->Read(&s16, 2); e->unk20       = s16;
                pFile->Read(&e->bulletDX, 2);
                pFile->Read(&e->bulletDY, 2);
                pFile->Read(&e->bulletStep, 2);
                pFile->Read(&e->seqIdx, 2);
            }
            pFile->Read(&s16, 2); count = s16;
            for (i = 0; i < count; i++) {
                pFile->Read(&s16, 2);
                ((IactScript *)iactScripts[i])->doneFlag = s16;
            }
        }
    }
    return;
#endif
    if (bFull) {
        pFile->Read(&tempVar, 4);
        pFile->Read(&randVar, 4);
        pFile->Read(&doorReturnX, 4);
        pFile->Read(&doorReturnY, 4);
        pFile->Read(&globalVar, 2);
        pFile->Read(&planet, 2);
        for (i = 0; i < width; i++)
            pFile->Read(&tiles[i * 54], height * 6);
    }
    pFile->Read(&activatedFlag, 4);
    pFile->Read(&count, 4);
    int n = objects.GetSize();
    if (n < count) {
        int add = count - n;
        for (i = 0; i < add; i++) {
            o = new ZoneObj;
            objects.SetAtGrow(objects.GetSize(), o);
        }
    }
    for (i = 0; i < count; i++) {
        o = (ZoneObj *)objects[i];
        pFile->Read(&o->state, 2);
        pFile->Read(&o->arg, 2);
        pFile->Read(&o->type, 4);
        pFile->Read(&o->x, 2);
        pFile->Read(&o->y, 2);
    }
    if (bFull) {
        pFile->Read(&count, 4);
        for (i = 0; i < count; i++) {
            MapEntity *e = (MapEntity *)entities[i];
            pFile->Read(&e->charId, 2);
            pFile->Read(&e->x, 2);
            pFile->Read(&e->y, 2);
            pFile->Read(&e->damageTaken, 2);
            pFile->Read(&e->active, 4);
            pFile->Read(&e->unk10, 2);
            pFile->Read(&e->bulletX, 2);
            pFile->Read(&e->bulletY, 2);
            pFile->Read(&e->aiStepCounter, 2);
            pFile->Read(&e->unk18, 4);
            pFile->Read(&e->bRetreating, 4);
            pFile->Read(&e->unk20, 4);
            pFile->Read(&e->bulletDX, 2);
            pFile->Read(&e->bulletDY, 2);
            pFile->Read(&e->bulletStep, 2);
            pFile->Read(&e->seqIdx, 2);
            pFile->Read(&e->unk60, 2);
            pFile->Read(&e->item, 2);
            pFile->Read(&e->unk2c, 4);
            pFile->Read(&e->bRefreshFrame, 4);
            pFile->Read(&e->numItems, 4);
            pFile->Read(&e->timer, 2);
            pFile->Read(&e->wanderDir, 2);
            int *p = e->waypoints;
            for (int k = 4; k > 0; k--) {
                pFile->Read(p, 4);
                pFile->Read(p + 1, 4);
                p += 2;
            }
        }
        pFile->Read(&count, 4);
        for (i = 0; i < count; i++)
            pFile->Read(&((IactScript *)iactScripts[i])->doneFlag, 4);
    }
}

// FUNCTION: YODA 0x00405f30  [DIFF(20) -> DIFF(7) at v112, via lesson #38 (the decl SET
//   AND ORDER). Two levers, each killing an identifiable defect: (1) declaring `i` BEFORE
//   `count` fixed the backedge compare FORM in all three count loops at once (orig
//   `cmp [esp+count],reg; jg`, ours `cmp reg,[esp+count]; jl`) -> 20 to 14; (2) hoisting the
//   three object pointers to function scope, with `i` LAST, fixed the iactScripts loop's
//   ebx<->edi bijection -> 14 to 7. The oracle pins a FAMILY (any >=5 function-scope decls
//   with `i` last measure 7; which names are hoisted is irrelevant) so this is the idiomatic
//   member, chosen to mirror ReadSavedState's own block -- lesson #36 discipline.
//   Everything else is REFUTED BY LENGTH, which is what makes the surviving shape evidence:
//   inline cast chains instead of `o` emit 808 B, `objects.GetAt(i)` 826 B, and staging
//   `count = objects.GetSize()` before the activatedFlag write costs 53 B (all vs 830).
//   A separate index variable for the first loop is INERT, so the dial is not a second `i`.
//   Compare-operand order (`count > i`) is source-inert: 15/15 flat. Loop FORM inert too
//   (for/while/do-while all 830 B, 7 B). Residual = the objects loop's index/walker pair
//   only (orig edi=i, ebx=scaled walker; ours swapped) -- lesson #44's bijection class, and
//   the v112 joint search over this TU (54 combinations) proves no other function's decl
//   configuration reaches it.]
// Write mirror of ReadSavedState (counts staged through a local). Called by
// World::SaveZoneRecursive.
void Zone::WriteSavedState(CFile *pFile, int bFull)
{
    ZoneObj *o; MapEntity *e; int *p;
    int count; int i;

#ifdef GAME_INDY
    // Mirror of the GAME_INDY branch in ReadSavedState — writes the retail Indy 16-bit record
    // (DESKADV FUN_1010_2108): 2-byte fields, no globalVar/planet, ZoneObj = state+arg only,
    // MapEntity = first 16 fields only. Keeps our Indy save/load self-consistent and stops us
    // persisting the fields that clobbered regenerated state on load.
    {
        short s16;
        if (bFull) {
            s16 = (short)tempVar;     pFile->Write(&s16, 2);
            s16 = (short)randVar;     pFile->Write(&s16, 2);
            s16 = (short)doorReturnX; pFile->Write(&s16, 2);
            s16 = (short)doorReturnY; pFile->Write(&s16, 2);
            for (i = 0; i < width; i++)
                pFile->Write(&tiles[i * 54], height * 6);
        }
        s16 = (short)activatedFlag;  pFile->Write(&s16, 2);
        s16 = (short)objects.GetSize(); pFile->Write(&s16, 2);
        count = objects.GetSize();
        for (i = 0; i < count; i++) {
            ZoneObj *o = (ZoneObj *)objects[i];
            pFile->Write(&o->state, 2);
            pFile->Write(&o->arg, 2);
        }
        if (bFull) {
            s16 = (short)entities.GetSize(); pFile->Write(&s16, 2);
            count = entities.GetSize();
            for (i = 0; i < count; i++) {
                MapEntity *e = (MapEntity *)entities[i];
                pFile->Write(&e->charId, 2);
                pFile->Write(&e->x, 2);
                pFile->Write(&e->y, 2);
                pFile->Write(&e->damageTaken, 2);
                s16 = (short)e->active;      pFile->Write(&s16, 2);
                pFile->Write(&e->unk10, 2);
                pFile->Write(&e->bulletX, 2);
                pFile->Write(&e->bulletY, 2);
                pFile->Write(&e->aiStepCounter, 2);
                s16 = (short)e->unk18;       pFile->Write(&s16, 2);
                s16 = (short)e->bRetreating; pFile->Write(&s16, 2);
                s16 = (short)e->unk20;       pFile->Write(&s16, 2);
                pFile->Write(&e->bulletDX, 2);
                pFile->Write(&e->bulletDY, 2);
                pFile->Write(&e->bulletStep, 2);
                pFile->Write(&e->seqIdx, 2);
            }
            s16 = (short)iactScripts.GetSize(); pFile->Write(&s16, 2);
            count = iactScripts.GetSize();
            for (i = 0; i < count; i++) {
                s16 = (short)((IactScript *)iactScripts[i])->doneFlag;
                pFile->Write(&s16, 2);
            }
        }
    }
    return;
#endif
    if (bFull) {
        pFile->Write(&tempVar, 4);
        pFile->Write(&randVar, 4);
        pFile->Write(&doorReturnX, 4);
        pFile->Write(&doorReturnY, 4);
        pFile->Write(&globalVar, 2);
        pFile->Write(&planet, 2);
        for (i = 0; i < width; i++)
            pFile->Write(&tiles[i * 54], height * 6);
    }
    pFile->Write(&activatedFlag, 4);
    count = objects.GetSize();
    pFile->Write(&count, 4);
    for (i = 0; i < count; i++) {
        o = (ZoneObj *)objects[i];
        pFile->Write(&o->state, 2);
        pFile->Write(&o->arg, 2);
        pFile->Write(&o->type, 4);
        pFile->Write(&o->x, 2);
        pFile->Write(&o->y, 2);
    }
    if (bFull) {
        count = entities.GetSize();
        pFile->Write(&count, 4);
        for (i = 0; i < count; i++) {
            e = (MapEntity *)entities[i];
            pFile->Write(&e->charId, 2);
            pFile->Write(&e->x, 2);
            pFile->Write(&e->y, 2);
            pFile->Write(&e->damageTaken, 2);
            pFile->Write(&e->active, 4);
            pFile->Write(&e->unk10, 2);
            pFile->Write(&e->bulletX, 2);
            pFile->Write(&e->bulletY, 2);
            pFile->Write(&e->aiStepCounter, 2);
            pFile->Write(&e->unk18, 4);
            pFile->Write(&e->bRetreating, 4);
            pFile->Write(&e->unk20, 4);
            pFile->Write(&e->bulletDX, 2);
            pFile->Write(&e->bulletDY, 2);
            pFile->Write(&e->bulletStep, 2);
            pFile->Write(&e->seqIdx, 2);
            pFile->Write(&e->unk60, 2);
            pFile->Write(&e->item, 2);
            pFile->Write(&e->unk2c, 4);
            pFile->Write(&e->bRefreshFrame, 4);
            pFile->Write(&e->numItems, 4);
            pFile->Write(&e->timer, 2);
            pFile->Write(&e->wanderDir, 2);
            p = e->waypoints;
            for (int k = 4; k > 0; k--) {
                pFile->Write(p, 4);
                pFile->Write(p + 1, 4);
                p += 2;
            }
        }
        count = iactScripts.GetSize();
        pFile->Write(&count, 4);
        for (i = 0; i < count; i++)
            pFile->Write(&((IactScript *)iactScripts[i])->doneFlag, 4);
    }
}

// FUNCTION: YODA 0x00406270
// Parses an IZAX record: entity list (count read twice — the first is a placeholder field),
// then two word lists into providedItemsA/providedItemsB.
// len 410 vs extent 416: the -6 is EXACTLY the two `i = count;` sites below, 3 B each — the
// same single construct that accounts for all of ReadZax2 0x00406410 and ReadZax3 0x00406490.
// See the ReadZax2 note for the measured axes (all closed) and the one dictionary entry.
void Zone::ReadZaux(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    pFile->Read(&count, 2);
    if (count > 0) {
        n = count;
        for (i = 0; i < n; i++) {
            MapEntity *e = new MapEntity;
            pFile->Read(&e->charId, 2);
            pFile->Read(&e->x, 2);
            pFile->Read(&e->y, 2);
            pFile->Read(&e->item, 2);
            pFile->Read(&e->numItems, 4);
            pFile->Read(e->waypoints, 0x20);
            entities.SetAtGrow(entities.GetSize(), e);
        }
    }
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, see Zax2 note
        do {
            pFile->Read(&n, 2);
            providedItemsA.SetAtGrow(providedItemsA.GetSize(), n);
        } while (--i != 0);
    }
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, see Zax2 note
        do {
            pFile->Read(&n, 2);
            providedItemsB.SetAtGrow(providedItemsB.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406410
// Parses an IZX2 record: one word list into genCandidateA.
// ⭐ v127 QUANTIFIED IT: the ENTIRE 47 B residual is ONE construct worth exactly 3 bytes, and
// everything after it is the downstream shift. len 111 vs extent 114 — we are SHORT (lesson #49).
// At +0x03b the original emits `mov ax, word [esp+0x12]` + `movsx ebp, ax` (8 B) where we emit
// the single `movsx ebp, word [esp+0x12]` (5 B). Same family, same 3 B, at ReadZax3 0x00406490
// (textually this function, genCandidateA/B apart) and TWICE in ReadZaux 0x00406270 (-6).
// ⭐ The lesson-#53 dictionary has exactly ONE entry for the two-step form: Canvas::BlitFast
// 0x00408110 +0x03b, `mov dx, word [ebp+0x10]; movsx esi, dx` = `int rows = height;` — where
// `height` is a short that C-level ASSIGNMENT reaches on one path (`height = canvasH - destY;`)
// and the parameter slot on the other. So the two-step looks like the MERGE of two definitions
// of a short, and `count` here has only one (the CFile::Read through &count).
// ⛔ CLOSED axes — do not re-tread: all 120 leading-decl permutations (v109); 20+ statement
// forms (v99); and v127's `n = count; i = n;` / `i = n = count;` (all REFUTED BY LENGTH — they
// really store n, 119 vs the 114 extent), `i = n` alone, `int i` scoped inside the if,
// `(short)`/`(int)` casts, `count = count;` and a `short t = count;` temp — the last six all
// DEAD FLAT at 111 B / 47. The lever is the missing second DEFINITION of the short, not the
// spelling of this assignment.
void Zone::ReadZax2(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B). v99 SWEPT 20+ forms (see EOF note).
        do {
            pFile->Read(&n, 2);
            genCandidateA.SetAtGrow(genCandidateA.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406490
// Parses an IZX3 record: one word list into genCandidateB. Source clone of ReadZax2.
void Zone::ReadZax3(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B). v99 SWEPT 20+ forms (see EOF note).
        do {
            pFile->Read(&n, 2);
            genCandidateB.SetAtGrow(genCandidateB.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406510
// Parses an IZX4 record header and discards it (tag/size/one word). `this` is unused.
void Zone::ReadZax4(CFile *pFile)
{
    char  tag[5];
    int   size;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
}

#ifdef GAME_INDY
// Indy IZAX record (parallel-array layout — NOT a byte-match function; Indy-only). Format
// reverse-engineered from DESKTOP.DAW raw bytes and validated to consume ALL 366 records exactly:
//   tag(4)="IZAX", size(4), mission_spec(2), num_entries(2),
//   num_entries * { charId(2), x(2), y(2) }   (6-byte entities, vs Yoda's 44-byte),
//   count(2), items[count] (2 bytes each).
// Indy's IZAX carries only ONE item pool (Yoda has two: providedItemsA + providedItemsB). The
// worldgen quest builder's goal-zone placement needs items in BOTH branches, so we feed the single
// Indy pool to both providedItemsA and providedItemsB. (HYPOTHESIS pending DESKADV.EXE worldgen RE
// — Indy's actual two-branch item model is unconfirmed; this at least lets Generate proceed.)
void Zone::ReadIzaxIndy(CFile *pFile)
{
    char  tag[4];
    int   size;
    short mission;
    short count;
    short n;
    int   i;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&mission, 2);        // mission-specific flag (unused by worldgen)
    pFile->Read(&count, 2);          // number of entities
    for (i = 0; i < count; i++) {
        MapEntity *e = new MapEntity;
        pFile->Read(&e->charId, 2);
        pFile->Read(&e->x, 2);
        pFile->Read(&e->y, 2);
        entities.SetAtGrow(entities.GetSize(), e);
    }
    pFile->Read(&count, 2);          // provided-item pool size
    for (i = 0; i < count; i++) {
        pFile->Read(&n, 2);
        providedItemsA.SetAtGrow(providedItemsA.GetSize(), n);   // Indy is SINGLE-pool: worldgen
        // reads only providedItemsA (IZAX); the old providedItemsB mirror was a wart (removed).
    }
}
#endif

// FUNCTION: YODA 0x00406550  [WIP: DIFF(495), len 583 vs extent 557 (+26). found-vs-r EBP
//   contest: orig found=EBP/r=stack, ours r=EBP/found=stack (memory-form found tests cost the
//   bytes). Control flow verified line-by-line against the true disasm (take-order coord-then-
//   flag, two-store sign forms, compiler inc-ebp for the 2nd-probe found=1). n merged per orig
//   ESI reuse.
//   ⭐ v123 DECOMPOSED THE +26 EXACTLY (lesson #49 method) — every byte is that ONE contest and
//   there is no missing/extra CODE. Both frames are 12 bytes and hold {savedY:2, savedX:2,
//   this:4, ONE int:4}, so exactly one int local is memory-resident and the two images pick a
//   DIFFERENT one. Ledger: `found` in memory costs +33 (init `mov [esp+0xc],0` 8 B vs
//   `xor ebp,ebp` 2; four `mov [mem],1` 8 B vs `mov ebp,1` 5 / `inc ebp` 2; five `cmp [mem],0`
//   5 B vs `test ebp,ebp` 2), `r` in a register saves -14 (no store, `sub edi,ebp` and
//   `lea edi,[esi+ebp]` for the four uses), +2 for two `cmp [bForce],0` where the original
//   folds the live zero as `cmp [bForce],ebp` (lesson #39), and -6 because our `return 1`
//   early-exit is an INLINE epilogue where the original branches to a shared tail block.
//   ⭐ v137 READ THE ORIGINAL OUT END TO END (read-only, no compile) and the CONTROL FLOW is
//   confirmed IDENTICAL to this source instruction for instruction — so the +26 is the contest
//   and nothing else. Original register map: esi = dx then n (they coalesce), edi = tx,
//   ebx = ty, ebp = found; frame S0+0 savedY(w), S0+2 savedX(w), S0+4 this, S0+8 r — r stored
//   at +0x6f, INTERLEAVED into the first GetTile's argument setup. Two details worth keeping:
//   the merged 2nd-probe `inc ebp` at +0x1aa is cl exploiting `test ebp,ebp; je` (it knows
//   ebp==0 in that arm, so 1 byte instead of 5 — the three 1st-probe sites keep `mov ebp,1`);
//   and the six epilogues share bytes (`mov eax,2` then `jg` INTO the -1 return's pop run).
//   ⛔ v137 CLOSED A THIRD AXIS — `n`'s SCOPE (lesson #37), which the SET+ORDER sweep below
//   structurally cannot reach: declaring `int n` inside each of the three branches (3 short
//   ranges instead of 1 long one, the hoped-for re-ranking) measures 594 B at len 594 = +11
//   ALONE, and 495 @ +26 = exactly baseline when composed with the r-late statement order
//   (lesson #51 cross, 4 cells). Baseline reproduces at 495/583, so the Iact.cpp phase has not
//   rotated since v123.
//   ⛔ TWO MORE AXES CLOSED, do not re-tread: (1) decl SET+ORDER — 10 configurations (r,n first;
//   found last; found without initialiser; n,r swapped; savedX/savedY split; txty first) are
//   flat or WORSE, best is the baseline 495 at +26, and `int n, r;` costs +11 more. (2) the
//   STATEMENT ORDER around r's definition — moving `r = ...` after the savedX/savedY stores
//   (which is where the ORIGINAL emits its store, at +0x6f right before the GetTile call) buys
//   489 B but leaves the length at +26, i.e. below lesson #48's landing bar; the if/else
//   spelling of the ternary is inert. Per lesson #44 this is the scratch/EBP allocation class.]
// Movement sidestep probe: target = (x+dx, y+dy). Returns -1 out-of-bounds, 1 target free,
// 0 blocked, or a direction code (2=E 3=W 4=S 5=N) after sidestepping around the obstacle
// (side picked pseudo-randomly via GetTickCount parity; bForce accepts occupied sidesteps).
// a5 is unused (caller-pushed). Used by the entity AI in GameView::Tick.
int Zone::IactProbeMove(int x, int y, int dx, int dy, int a5, int bForce)
{
    int   found = 0;
    int   tx = x + dx;
    int   ty = y + dy;
    short savedX, savedY;
    int   r, n;

    if (tx < 0 || ty < 0 || tx >= width || ty >= height)
        return -1;
    r = (GetTickCount() & 1) == 0 ? 1 : -1;
    savedX = (short)tx;
    savedY = (short)ty;
    if ((short)GetTile(tx, ty, 1) < 0)
        return 1;
    if (dx == 0 && dy != 0) {
        n = tx - r;
        if (n >= 0 && ((short)GetTile(n, ty, 1) < 0 || bForce != 0)) {
            tx = n;
            found = 1;
        }
        if (!found) {
            n = r + tx;
            if (n < width && ((short)GetTile(n, ty, 1) < 0 || bForce != 0)) {
                tx = n;
                found = 1;
            }
        }
    } else if (dy == 0 && dx != 0) {
        n = ty - r;
        if (n >= 0 && ((short)GetTile(tx, n, 1) < 0 || bForce != 0)) {
            ty = n;
            found = 1;
        }
        if (!found) {
            n = r + ty;
            if (n < height && ((short)GetTile(tx, n, 1) < 0 || bForce != 0)) {
                ty = n;
                found = 1;
            }
        }
    } else {
        n = tx + 1;
        if (dx >= 0)
            n = tx - 1;
        if ((short)GetTile(n, ty, 1) < 0 || bForce != 0) {
            tx = n;
            found = 1;
        }
        if (!found) {
            n = ty + 1;
            if (dy >= 0)
                n = ty - 1;
            if ((short)GetTile(tx, n, 1) < 0 || bForce != 0) {
                ty = n;
                found = 1;
            }
        }
    }
    if (!found)
        return 0;
    if (ty < savedY)
        return 5;
    if (ty > savedY)
        return 4;
    if (tx < savedX)
        return 3;
    if (tx > savedX)
        return 2;
    return 0;
}

// FUNCTION: YODA 0x00406780  [STRUCTURALLY COMPLETE: DIFF(1538) at v115 (was 1548), len
//   2408/2408 — no length drift, so the whole residual is local redistribution + tie-breaks:
//   reg-rename 2/3-cycles (AllEnemiesDead loop, index-temp eax/ecx/edx), cmp operand directions
//   (TempVarEq/Ne, loop backedges jl<->jg), je/jne polarity (DragWrongItem 0x12), QuestSpot xor
//   placement + zero-reg reuse, CheckCellItems itemA/itemB emit order. All rotate with TU phase
//   (proven: probes inert or mirrored) — joint endgame. KEY CRACKS: the two-return tail
//   (duplicated epilogue) also fixed the {result,itemB,nScripts} slot 3-cycle (slot order is
//   usage-driven, NOT decl-driven); and BOTH of BumpTile's compares are IN-CONDITION
//   ASSIGNMENTS — (ty = y + dy) landed at G1, and v115 added (tx = dx + x), which killed the
//   long-standing add-vs-sub form at +0x14b (orig `mov eax,[dx]; add eax,[x]; cmp eax,[args0]`;
//   ours had emitted the algebraically-rewritten `mov eax,[args0]; sub eax,[dx]; cmp eax,[x]`).
//   ⚠ The original does NOT reuse tx in the tile index — it recomputes `dx + x` there, PROVEN
//   by length: every variant that substitutes tx into the subscript emits 2404 B, 4 short of
//   the original's 2408 (and 1800 B of diff). ⚠ MEASURED NEGATIVE at +0x15e (orig adds y then
//   dy, ours dy then y): all 7 spellings of the ty condition — (ty = dy + y), the reversed
//   compare direction, and both tx/ty decl orders — are DEAD FLAT at 1538, while dropping ty
//   entirely costs 252 B. Per lesson #45 a mirrored compare is never the condition's spelling.]
// The IACT condition interpreter: for each of the zone's scripts whose trigger conditions all
// pass for this frame event (1=walk-step 2=BumpTile 3=DragItem 4=enter-zone 5=enter-vehicle),
// run its commands. Runs with nFrameMode forced to 1 (script-busy); restores the caller's mode
// unless a command warped/spawned (result & 0x808). Opcode semantics: scrdoc.txt in
// ~/workspace/DesktopAdventures. NOTE the original's COND_CheckCellItems reuses the SCRIPT loop
// variable (idx) for its second inventory scan — a real bug (script iteration restarts from
// nInv+1 after that condition); reproduced faithfully. Engine-bug inventory: docs/engine-bugs.md.
int Zone::IactRun(int event, int x, int y, int dx, int dy, int a5, CDC *pDC, CDeskcppDoc *pWorld,
                  CDeskcppView *pView)
{
    int          found;      // per-case scratch: HasItem found-flag / QuestSpot counter /
    int          count;      //   CheckCellItems found pair (function-level, C-style decls)
    int          idx;        // script index (clobbered by CheckCellItems — see NOTE above)
    IactScript  *pScript;
    int          nScripts;
    int          result;
    int          itemB;
    int          i;          // condition index
    int          nConds;
    Tile        *pNeeded;
    Tile        *pTileA;
    Tile        *pTileB;
    int          savedMode;
    int          cellX;
    int          cellY;
    int          matched;

    result = 0;
    nScripts = iactScripts.GetSize();
    if (nScripts == 0)
        return 0;
    idx = 0;
    savedMode = pWorld->nFrameMode;
    pWorld->nFrameMode = 1;
    for (; idx < nScripts; idx++) {
        if (pWorld != NULL && pWorld->abortFrame != 0)
            break;
        pScript = (IactScript *)iactScripts[idx];
        if (pScript->doneFlag != 1) {
            matched = 1;
            nConds = pScript->conditions.GetSize();
            for (i = 0; i < nConds; i++) {
                if (!matched)
                    break;
                IactCondition *pCond = (IactCondition *)pScript->conditions[i];
                switch (pCond->opcode) {
                case COND_FirstEnter:
                    if (event == 4 && activatedFlag == 0 && pWorld != NULL) {
                        if (pWorld->FindZoneCellById((short)pWorld->GetZoneIndex(this),
                                                     &cellX, &cellY) != 0
                            && pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagSolved == 1)
                            matched = 0;
                    } else {
                        matched = 0;
                    }
                    break;
                case COND_Enter:
                    if (event != 4)
                        matched = 0;
                    break;
                case COND_BumpTile: {
                    int tx; int ty;
                    if (event != 2 || (tx = dx + x) != pCond->args[0] || pCond->args[1] != (ty = y + dy)
                        || tiles[(ty * ZONE_WIDTH + dx + x) * ZONE_LAYERS + 1] != pCond->args[2])
                        matched = 0;
                    break; }
                case COND_DragItem:
                    if (event == 3 && pCond->args[0] == x && pCond->args[1] == y) {
                        int t;
                        int n;
                        t = pCond->args[3];
                        if (t == -1)
                            n = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
                        else
                            n = tiles[(y * ZONE_WIDTH + x) * ZONE_LAYERS + pCond->args[2]];
                        if (n == t || t == -1) {
                            n = pCond->args[4];
                            if (n == -1)
                                n = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemA;
                            if (n >= 0 && pWorld->tileArray[n] == pWorld->pEquippedItem) {
                                if (n == 0x1ff)
                                    pView->DrawTileAt((short)pWorld->nWeaponHitX,
                                                      (short)pWorld->nWeaponHitY, -1);
                                break;
                            }
                        }
                    }
                    matched = 0;
                    break;
                case COND_Walk:
                    if (event != 1 || pCond->args[0] != x || pCond->args[1] != y
                        || tiles[(y * ZONE_WIDTH + x) * ZONE_LAYERS] != pCond->args[2])
                        matched = 0;
                    break;
                case COND_TempVarEq:
                    if (tempVar != pCond->args[0])
                        matched = 0;
                    break;
                case COND_RandVarEq:
                    if (pCond->args[0] != randVar)
                        matched = 0;
                    break;
                case COND_RandVarGt:
                    if (pCond->args[0] >= randVar)
                        matched = 0;
                    break;
                case COND_RandVarLs:
                    if (pCond->args[0] <= randVar)
                        matched = 0;
                    break;
                case COND_EnterVehicle:
                    if (event != 5)
                        matched = 0;
                    break;
                case COND_CheckMapTile:
                case COND_CheckMapTileVar:
                    if (tiles[(pCond->args[2] * ZONE_WIDTH + pCond->args[1]) * ZONE_LAYERS + pCond->args[3]]
                        != pCond->args[0])
                        matched = 0;
                    break;
                case COND_EnemyDead:
                    // sic: '<' not '<=' — args[0]==GetSize() reads past the array
                    // (docs/engine-bugs.md #2)
                    YODA_SIC_FIX(if (pCond->args[0] < 0 || entities.GetSize() == pCond->args[0]) { BUGLOG(("sic#2 EnemyDead: idx=%d n=%d, OOB read avoided\n", (int)pCond->args[0], (int)entities.GetSize())); matched = 0; break; }) if (entities.GetSize() < pCond->args[0]
                        || ((MapEntity *)entities[pCond->args[0]])->charId != -1)
                        matched = 0;
                    break;
                case COND_AllEnemiesDead: {
                    int n = entities.GetSize();
                    int alive = 0;
                    for (int j = 0; j < n; j++) {
                        if (((MapEntity *)entities[j])->charId >= 0)
                            alive++;
                    }
                    if (alive != 0)
                        matched = 0;
                    break; }
                case COND_HasItem:
                    if (pWorld != NULL) {
                        int n;
                        int nInv;
                        n = pCond->args[0];
                        if (n == -1)
                            n = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
                        if (n >= 0)
                            pNeeded = pWorld->tileArray[n];
                        nInv = pWorld->inventory.GetSize();
                        found = 0;
                        if (n >= 0 && nInv > 0) {
                            for (int j = 0; j < nInv; j++) {
                                if (((InvItem *)pWorld->inventory[j])->pTile == pNeeded)
                                    found = 1;
                            }
                        }
                        if (found != 0)
                            break;
                    }
                    matched = 0;
                    break;
                case COND_CheckEndItem: {
                    if (pWorld == NULL) {
                        matched = 0;
                        break;
                    }
                    int cell = pWorld->playerY * 10 + pWorld->playerX;
                    short b = pWorld->zones[cell].cellItemB;
                    if (pWorld->zones[cell].cellItemA == pCond->args[0])
                        matched = 1;
                    else
                        matched = (b == pCond->args[0]);
                    break; }
                case COND_CheckStartItem:
                    if (pWorld == NULL || pWorld->startItem != pCond->args[0])
                        matched = 0;
                    break;
                case COND_ZoneSolved:
                    if (pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagB == 0)
                        matched = 0;
                    break;
                case COND_GameInProgress:
                    if (pWorld->gameState != -1)
                        matched = 0;
                    break;
                case COND_GameCompleted:
                    if (pWorld->gameState != 1)
                        matched = 0;
                    break;
                case COND_HealthLs:
                    if (pWorld->healthHi * -100 - pWorld->healthLo + 0x191 >= pCond->args[0])
                        matched = 0;
                    break;
                case COND_HealthGt:
                    if (pWorld->healthHi * -100 - pWorld->healthLo + 0x191 <= pCond->args[0])
                        matched = 0;
                    break;
                case COND_CheckCellItem:
                    if (pWorld == NULL
                        || pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemC
                           != pCond->args[0])
                        matched = 0;
                    break;
                case COND_DragWrongItem:
                    if (event == 3 && pCond->args[0] == x && pCond->args[1] == y) {
                        int t;
                        int n;
                        t = tiles[(y * ZONE_WIDTH + x) * ZONE_LAYERS + pCond->args[2]];
                        if (pCond->args[3] == t || t == -1) {
                            n = pCond->args[4];
                            if (n == -1)
                                n = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemA;
                            if (n >= 0 && pWorld->tileArray[n] != pWorld->pEquippedItem
                                && pView->bWeaponIactActiveMaybe == 0) {
                                int id = pWorld->FindTile(pWorld->pEquippedItem);
                                if (id <= 0x202) {
                                    if (id >= 0x1fe || id == 0x12)
                                        matched = 0;
                                } else if (id >= 0x204 && (id <= 0x205 || id == 0x31a))
                                    matched = 0;
                                break;
                            }
                        }
                    }
                    matched = 0;
                    break;
                case COND_PlayerAtPos:
                    if (pWorld->cameraX / 0x20 != pCond->args[0]
                        || pWorld->cameraY / 0x20 != pCond->args[1])
                        matched = 0;
                    break;
                case COND_GlobalVarEq:
                    if (globalVar != pCond->args[0])
                        matched = 0;
                    break;
                case COND_GlobalVarLs:
                    if (globalVar >= pCond->args[0])
                        matched = 0;
                    break;
                case COND_GlobalVarGt:
                    if (globalVar <= pCond->args[0])
                        matched = 0;
                    break;
                case COND_ExperienceEq:
                    if (pWorld->completionCount != pCond->args[0])
                        matched = 0;
                    break;
                case COND_QuestSpotPresent:
                    found = 0;
                    count = objects.GetSize();
                    if (count > 0) {
                        matched = 0;
                        do {
                            ZoneObj *obj = (ZoneObj *)objects[found];
                            if (obj->x == pCond->args[0] && obj->y == pCond->args[1]
                                && obj->type == 0 && obj->state == 1) {
                                matched = 1;
                                break;
                            }
                            found++;
                        } while (found < count);
                    }
                    break;
                case COND_CheckCellItems: {
                    if (pWorld == NULL) {
                        matched = 0;
                        break;
                    }
                    int cell = pWorld->playerY * 10 + pWorld->playerX;
                    int itemA = pWorld->zones[cell].cellItemA;
                    itemB = pWorld->zones[cell].cellItemB;
                    if (itemA >= 0)
                        pTileA = pWorld->tileArray[itemA];
                    if (itemB >= 0)
                        pTileB = pWorld->tileArray[itemB];
                    int nInv = pWorld->inventory.GetSize();
                    found = 0;
                    count = 0;
                    if (itemA >= 0 && nInv > 0) {
                        for (int j = 0; j < nInv; j++) {
                            if (((InvItem *)pWorld->inventory[j])->pTile == pTileA)
                                found = 1;
                        }
                    }
                    if (itemB >= 0) {
                        // sic: the original reuses the SCRIPT index for this scan (see NOTE)
                        for (idx = 0; idx < nInv; idx++) {
                            if (((InvItem *)pWorld->inventory[idx])->pTile == pTileB)
                                count = 1;
                        }
                    }
                    if (found)
                        matched = 1;
                    else
                        matched = (count != 0);
                    break; }
                case COND_TempVarNe:
                    if (tempVar == pCond->args[0])
                        matched = 0;
                    break;
                case COND_RandVarNe:
                    if (pCond->args[0] == randVar)
                        matched = 0;
                    break;
                case COND_GlobalVarNe:
                    if (globalVar == pCond->args[0])
                        matched = 0;
                    break;
                case COND_ExperienceGt:
                    if (pWorld->completionCount <= pCond->args[0])
                        matched = 0;
                    break;
                }
            }
            if (matched && pScript->doneFlag == 0)
                result |= IactRunCommands(idx, pDC, pWorld, pView);
        }
    }
    if ((result & IACT_ZONE_INVALID) == 0) {
        pWorld->nFrameMode = savedMode;
        return result;
    }
    return result;
}

// FUNCTION: YODA 0x004070e0  [STRUCTURALLY COMPLETE: 849 main-body insns, 740 identical (87%).
//   Residual tie-breaks: dispX/dispY slot placement (orig -0x28/-0x2c amid the CString cluster,
//   ours -0x58/-0x5c — decl order proven inert, optimizer-internal), MoveCamera cx/cy reg-vs-
//   memory contest (orig cx=EDI/cy=-0x1c home; ours mirrored, +2-insn store-reload — same class
//   as IactProbeMove's EBP contest), ShowAll/HideAllEntities reg 3-cycles, AddItemToInv/
//   RemoveItemFromInv &&-chain load/test interleave, case-0 OR-schedule slot, init-store order.
//   KEY CRACKS: toupper(c) NOT _toupper (ctype.h macro = sub 0x20; orig CALLS the CRT function);
//   (short)(expr<<5) casts materialize 16-bit shl — route short args through int temps; ShowObject
//   needs a GetSize() temp; SayText scope order msg/name/head + Left/Right retbuf temps drive the
//   EH funclet layout (matched).]
// The IACT command executor: runs every command of iactScripts[scriptIdx] and returns the
// dirty-flags mask (0x1=sound 0x2=text 0x4=camera 0x8=spawn 0x10=objects 0x20=tiles 0x40=entities
// 0x80=full-redraw 0x100=player 0x200=game-over 0x400=inventory 0x800=zone-warp). SayText/ShowText
// substitute the current quest items' tile names for the \xa2/\xa5 placeholders, fixing the
// "a"->"an" article when the name starts with a vowel.
unsigned int Zone::IactRunCommands(int scriptIdx, CDC *pDC, CDeskcppDoc *pWorld, CDeskcppView *pView)
{
    unsigned int result;
    int          dispX;      // MoveCamera: cell to redraw behind the camera step
    int          dispY;
    int          n;          // SetPlayerPos scan target / MoveCamera camera-cell y
    IactScript  *pScript;
    int          targetX;    // MoveCamera destination
    int          targetY;
    int          camCellY;   // camera cell at entry (ReleaseCamera/LockCamera redraw)
    int          camCellX;
    int          doneOnce;   // set by CMD_FlagOnce -> doneFlag at exit
    int          nCmds;
    int          i;
    int          arrived;
    int          bCameraFree;
    int          textX;      // SayText/ShowText dialog position
    int          textY;

    result = 0;
    camCellX = pWorld->cameraX / 0x20;
    bCameraFree = 1;
    doneOnce = 0;
    camCellY = pWorld->cameraY / 0x20;
    pScript = (IactScript *)iactScripts[scriptIdx];
    nCmds = pScript->commands.GetSize();
    for (i = 0; i < nCmds; i++) {
        IactCommand *pCmd = (IactCommand *)pScript->commands[i];
        int op = pCmd->opcode;
        switch (op) {
        case CMD_SetMapTile:
        case CMD_SetMapTileVar:
            result |= IACT_TILES;
            // sic: the original does NOT bounds-check this tile write (verified in the byte-matched
            // disasm), relying on valid script coords. Some Indy interior-zone SetMapTile commands
            // carry out-of-range coords (e.g. y=21087) that crash the raw store; the real Indy
            // engine tolerates them, so guard the index for Indy only (Yoda fall-through = exact
            // original, anchor 211). TODO(indy): RE why Indy interior scripts have huge coords
            // (opcode semantics vs a script-keying/data quirk) for a true root-cause fix.
#if defined(GAME_INDY) || defined(YODA_BUGFIX)
            if ((unsigned)((pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]) < (unsigned)ZONE_CELL_COUNT)
#endif
            tiles[(pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]] = (short)pCmd->args[3]; YODA_SIC_FIX(else BUGLOG(("sic SetMapTile: OOB x=%d y=%d layer=%d\n", (int)pCmd->args[0], (int)pCmd->args[1], (int)pCmd->args[2]));)
            break;
        case CMD_ClearTile:
            result |= IACT_TILES;
#if defined(GAME_INDY) || defined(YODA_BUGFIX)
            if ((unsigned)((pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]) < (unsigned)ZONE_CELL_COUNT)
#endif
            tiles[(pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]] = -1; YODA_SIC_FIX(else BUGLOG(("sic ClearTile: OOB x=%d y=%d layer=%d\n", (int)pCmd->args[0], (int)pCmd->args[1], (int)pCmd->args[2]));)
            break;
        case CMD_MoveMapTile: {
#if defined(GAME_INDY) || defined(YODA_BUGFIX)
            if ((unsigned)((pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]) >= (unsigned)ZONE_CELL_COUNT
                || (unsigned)((pCmd->args[4] * ZONE_WIDTH + pCmd->args[3]) * ZONE_LAYERS + pCmd->args[2]) >= (unsigned)ZONE_CELL_COUNT)
            {
                YODA_SIC_FIX(BUGLOG(("sic MoveMapTile: OOB src=%d,%d dst=%d,%d layer=%d\n", (int)pCmd->args[0], (int)pCmd->args[1], (int)pCmd->args[3], (int)pCmd->args[4], (int)pCmd->args[2]));) result |= IACT_TILES;
                break;
            }
#endif
            short t = tiles[(pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]];
            tiles[(pCmd->args[1] * ZONE_WIDTH + pCmd->args[0]) * ZONE_LAYERS + pCmd->args[2]] = -1;
            result |= IACT_TILES;
            tiles[(pCmd->args[4] * ZONE_WIDTH + pCmd->args[3]) * ZONE_LAYERS + pCmd->args[2]] = t;
            break; }
        case CMD_DrawOverlayTile:
            if (pWorld->tileArray[pCmd->args[2]] != NULL) {
                int x = pCmd->args[0] << 5;
                int y = pCmd->args[1] << 5;
                pWorld->pCanvas->BlitMasked((char *)pWorld->tileArray[pCmd->args[2]]->pixels,
                                            TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, (short)x, (short)y, 0);
                result |= IACT_TILES;
            }
            break;
        case CMD_SayText:
        case CMD_ShowText:
            if (pWorld->abortFrame == 0) {
                if (op == CMD_SayText) {
                    textX = pWorld->cameraX + 0x10;
                    textY = pWorld->cameraY + 0x10;
                } else if (op == CMD_ShowText) {
                    textX = pCmd->args[0] * 0x20 + 0x10;
                    textY = pCmd->args[1] * 0x20 + 0x10;
                }
                CString msg;
                msg = pCmd->text;
                int pos = msg.FindOneOf("\xa2");
                if (pos >= 0) {
                    short item = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemA;
                    if (item >= 0) {
                        CString name(pWorld->tileArray[item]->name);
                        CString head = msg.Left(pos);
                        char c = name[0];
                        if (c >= 'A')
                            c = (char)toupper(c);
                        if ((c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U')
                            && YODA_SIC_FIX((pos >= 2 || (BUGLOG(("sic#4 SayText: article probe at pos=%d, pre-buffer read avoided\n", pos)), 0)) &&) (head[pos - 2] == 'A' || head[pos - 2] == 'a')) {
                            head.SetAt(pos - 1, 'n');
                            head += " ";
                        }
                        head += name;
                        head += msg.Right(msg.GetLength() - pos - 1);
                        msg = head;
                    }
                }
                pos = msg.FindOneOf("\xa5");
                if (pos >= 0) {
                    short item = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
                    if (item >= 0) {
                        CString name(pWorld->tileArray[item]->name);
                        CString head = msg.Left(pos);
                        char c = name[0];
                        if (c >= 'A')
                            c = (char)toupper(c);
                        if ((c == 'A' || c == 'E' || c == 'I' || c == 'O' || c == 'U')
                            && YODA_SIC_FIX((pos >= 2 || (BUGLOG(("sic#4 ShowText: article probe at pos=%d, pre-buffer read avoided\n", pos)), 0)) &&) (head[pos - 2] == 'A' || head[pos - 2] == 'a')) {
                            head.SetAt(pos - 1, 'n');
                            head += " ";
                        }
                        head += name;
                        head += msg.Right(msg.GetLength() - pos - 1);
                        msg = head;
                    }
                }
                pView->ShowTextDialog(msg, textX, textY, 0);
                result |= IACT_TEXT;
            }
            break;
        case CMD_RedrawTile:
            pView->DrawZoneCell((short)pCmd->args[0], (short)pCmd->args[1]);
            pWorld->DrawPlayer();
            break;
        case CMD_RedrawTiles:
            pView->DrawZoneCellRect(pCmd->args[0], pCmd->args[1], pCmd->args[2], pCmd->args[3]);
            pWorld->DrawPlayer();
            break;
        case CMD_RenderChanges:
            pView->DrawGameArea(pDC);
            result |= IACT_FULL_REDRAW;
            break;
        case CMD_WaitTicks: {
            long t = clock();
            long end = pCmd->args[0] * 100 + t;
            while (end > t)
                t = clock();
            break; }
        case CMD_PlaySound:
            if (pView != NULL) {
                pView->PlaySoundData(pCmd->args[0]);
                result |= IACT_SOUND;
            }
            break;
        case CMD_TransitionIn:
            result |= IACT_SOUND;
            break;
        case CMD_Random:
            randVar = rand() % pCmd->args[0] + 1;
            break;
        case CMD_SetTempVar:
            tempVar = pCmd->args[0];
            break;
        case CMD_AddTempVar:
            tempVar += pCmd->args[0];
            break;
        case CMD_ReleaseCamera:
            pWorld->bHidePlayer = 1;
            pView->RedrawPlayerCellMaybe();
            pView->DrawZoneCell((short)camCellX, (short)camCellY);
            result |= IACT_PLAYER;
            break;
        case CMD_LockCamera:
            pWorld->bHidePlayer = 0;
            pView->RedrawPlayerCellMaybe();
            pView->DrawZoneCell((short)camCellX, (short)camCellY);
            pWorld->DrawPlayer();
            result |= IACT_PLAYER;
            break;
        case CMD_SetPlayerPos:
            pView->DrawZoneCell((short)(pWorld->cameraX / 0x20), (short)(pWorld->cameraY / 0x20));
            pWorld->cameraX = pCmd->args[0] << 5;
            pWorld->cameraY = pCmd->args[1] << 5;
            camCellX = pCmd->args[0];
            camCellY = pCmd->args[1];
            if (pView->bIactZoneEntryMaybe == 0)
                pWorld->DrawPlayer();
            if (pWorld->bHidePlayer != 0) {
                int j = 0;
                int nObjs = objects.GetSize();
                if (nObjs > 0) {
                    n = pCmd->args[0];
                    do {
                        ZoneObj *obj = (ZoneObj *)objects[j];
                        if (obj->x == n && obj->y == pCmd->args[1]) {
                            bCameraFree = 0;
                            break;
                        }
                        j++;
                    } while (j < nObjs);
                }
            }
            if (bCameraFree != 0)
                pWorld->UpdateCamera();
            result |= IACT_CAMERA;
            if (pView->bIactZoneEntryMaybe != 0)
                return result;
            break;
        case CMD_MoveCamera: {
            targetX = pCmd->args[2];
            targetY = pCmd->args[3];
            arrived = 0;
            int cx = pCmd->args[0];
            if (cx == -1)
                cx = pWorld->cameraX / 0x20;
            dispX = cx;
            n = pCmd->args[1];
            if (n == -1)
                n = pWorld->cameraY / 0x20;
            dispY = n;
            do {
                if (targetX == cx && targetY == n) {
                    arrived++;
                } else {
                    if (targetY != n) {
                        int step = -1;
                        if (targetY - n >= 0)
                            step = 1;
                        dispY = n;
                        n += step;
                        pWorld->cameraY = n * 0x20;
                    }
                    if (targetX != cx) {
                        int step = -1;
                        if (targetX - cx >= 0)
                            step = 1;
                        dispX = cx;
                        cx += step;
                        pWorld->cameraX = cx * 0x20;
                    }
                    pView->DrawZoneCell((short)dispX, (short)dispY);
                    pWorld->UpdateCamera();
                    pWorld->DrawPlayer();
                    pView->DrawGameArea(pDC);
                    long t = clock();
                    long end = pCmd->args[4] * 100 + t;
                    while (end > t)
                        t = clock();
                }
                result |= IACT_CAMERA;
            } while (arrived == 0);
            pView->DrawGameArea(pDC);
            break; }
        case CMD_FlagOnce:
            doneOnce = 1;
            break;
        case CMD_ShowObject: {
            int t = pCmd->args[0];
            int nObjs = objects.GetSize();
            if (t >= 0 && nObjs > t)
                ((ZoneObj *)objects[t])->state = 1;
            result |= IACT_OBJECTS;
            break; }
        case CMD_HideObject: {
            int t = pCmd->args[0];
            int nObjs = objects.GetSize();
            if (t >= 0 && nObjs > t) {
                result |= IACT_OBJECTS;
                ((ZoneObj *)objects[t])->state = 0;
            }
            break; }
        case CMD_ShowEntity: {
            // sic: no bounds check, unlike ShowObject (docs/engine-bugs.md #3)
            YODA_SIC_FIX(if (pCmd->args[0] < 0 || pCmd->args[0] >= entities.GetSize()) { BUGLOG(("sic#3 ShowEntity: OOB idx=%d (n=%d)\n", (int)pCmd->args[0], (int)entities.GetSize())); break; }) MapEntity *p = (MapEntity *)entities[pCmd->args[0]];
            if (p != NULL) {
                p->active = 1;
                result |= IACT_ENTITIES;
            }
            break; }
        case CMD_HideEntity: {
            YODA_SIC_FIX(if (pCmd->args[0] < 0 || pCmd->args[0] >= entities.GetSize()) { BUGLOG(("sic#3 HideEntity: OOB idx=%d (n=%d)\n", (int)pCmd->args[0], (int)entities.GetSize())); break; }) MapEntity *p = (MapEntity *)entities[pCmd->args[0]];
            if (p != NULL) {
                p->active = 0;
                result |= IACT_ENTITIES;
            }
            break; }
        case CMD_ShowAllEntities: {
            int nEnts = entities.GetSize();
            for (int j = 0; j < nEnts; j++) {
                MapEntity *p = (MapEntity *)entities[j];
                if (p != NULL) {
                    result |= IACT_ENTITIES;
                    p->active = 1;
                }
            }
            break; }
        case CMD_HideAllEntities: {
            int nEnts = entities.GetSize();
            for (int j = 0; j < nEnts; j++) {
                MapEntity *p = (MapEntity *)entities[j];
                if (p != NULL)
                    p->active = 0;
            }
            result |= IACT_ENTITIES;
            break; }
        case CMD_SpawnItem:
            if (pWorld != NULL && pView != NULL) {
                pView->nPickupX = pCmd->args[1];
                pView->nPickupY = pCmd->args[2];
                if (pCmd->args[0] < 0) {
                    int t = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemC;
                    pView->nPickupTileId = t;
                    if (t < 0)
                        break;
                } else {
                    pView->nPickupTileId = pCmd->args[0];
                }
                result |= IACT_SPAWN;
                pView->pPickupObj = NULL;
                pView->nTransitionStep = 0;
                pView->bBlinkState = 0;
                pWorld->nFrameMode = 9;
            }
            break;
        case CMD_AddItemToInv:
            if (pWorld != NULL && pCmd->args[0] >= 0 && pView != NULL) {
                pView->AddItemToInv(pWorld->tileArray[pCmd->args[0]]);
                result |= IACT_INVENTORY;
            }
            break;
        case CMD_RemoveItemFromInv:
            if (pWorld != NULL) {
                if (pCmd->args[0] >= 0) {
                    if (pView != NULL) {
                        pView->RemoveItem(pWorld->tileArray[pCmd->args[0]]);
                        result |= IACT_INVENTORY;
                    }
                } else {
                    short item = pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].cellItemA;
                    if (YODA_SIC_FIX(pView != NULL &&) item >= 0 && pWorld->tileArray[item] != NULL) {
                        pView->RemoveItem(pWorld->tileArray[item]);
                        result |= IACT_INVENTORY;
                    }
                }
            }
            break;
        case CMD_MarkZoneSolved:
            pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagA = 1;
            pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagB = 1;
            pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagC = 1;
            pWorld->zones[pWorld->playerY * 10 + pWorld->playerX].flagD = 1;
            break;
        case CMD_WinGame:
            if (pWorld != NULL) {
                result |= IACT_GAME_OVER;
                pWorld->abortFrame = 1;
            }
            break;
        case CMD_LoseGame:
            if (pView != NULL) {
                pView->AddHealth(-300);
                if (pWorld != NULL) {
                    result |= IACT_GAME_OVER;
                    pWorld->abortFrame = -1;
                }
            }
            break;
        case CMD_WarpToMap:
            pWorld->cameraX = pCmd->args[1] << 5;
            pWorld->cameraY = pCmd->args[2] << 5;
            pView->nTargetZoneId = pCmd->args[0];
            pView->TransitionZoneScript(pCmd->args[3], pCmd->args[0]);
            result |= IACT_ZONE_WARP;
            break;
        case CMD_SetGlobalVar:
            globalVar = (short)pCmd->args[0];
            break;
        case CMD_AddGlobalVar:
            globalVar += (short)pCmd->args[0];
            break;
        case CMD_SetRandVar:
            randVar = pCmd->args[0];
            break;
        case CMD_AddHealth:
            pView->AddHealth(pCmd->args[0]);
            break;
        }
    }
    if (doneOnce == 1)
        pScript->doneFlag = 1;
    return result;
}

// ═══ v99 NEGATIVE RESULT — ReadZax2 0x406410 / ReadZax3 0x406490, the `mov ax`/`movsx` pair ═══
// The originals widen the element count as `mov ax,[esp+0x12]; movsx ebp,ax` (8 B) where we emit
// the folded `movsx ebp, word ptr [esp+0x12]` (5 B) — the ONLY structural diff in either function
// (46/46 insns, align=16, reg_pen=0). Do not re-chase it from the source side: a v99 probe sweep
// under the anchor flags tried short/int loop counters, for- vs do-while, explicit casts, decl
// reordering, `register short`, unsigned+cast, ternaries, post-decrement, count reused after the
// loop, and short temp chains (t=count; u=t; i=u) — ALL fold to the 5-byte form. The one form that
// DOES stage through AX is assigning into an ADDRESS-TAKEN short (`n = count; i = n;`), and that
// necessarily emits a `mov word ptr [esp+0x10],ax` store the original does not have. Merging count
// into `n` (one scratch short) is also wrong: the original keeps TWO slots, count@0x12 + n@0x10.
// ⇒ classify as allocation/scheduling state (the dial), not a source fact. Contrast lesson #34,
// where an equally 'unreachable-looking' register copy DID turn out to be a source construct —
// which is why this one was re-swept before being parked again.
// ⏮ v120 RE-SWEPT IT A THIRD TIME on FOUR axes v99/v109 never touched, because the seam is worth
// +3 exact (0x406410, 0x406490 and BOTH of ReadZaux 0x406270's tail loops carry the identical
// idiom — 4 sites, one construct — and ReadZaux's -6 length is exactly two of them). ALL FLAT:
//   · the CONTAINER CALL FORM (lesson #48): `genCandidateA.Add(n)` is byte-IDENTICAL to
//     `SetAtGrow(GetSize(), n)` here — a third confirmation that Add is INERT on a CWordArray of
//     values, and the original's push order (element first, `this` LAST via lea) is the
//     SetAtGrow fingerprint anyway, so the current spelling is positively confirmed.
//   · the decl SET (lesson #38/#45): adding a 6th local `short m;` at each of the 6 positions in
//     the leading block and writing `m = count; i = m;` — dead flat at 47 B in all 6. cl's FRONT
//     end copy-propagates the intermediate away, which is why no temp chain can ever reach it.
//   · forced-truncation expressions that keep the intermediate type at `short`:
//     `(short)(count + 0 | * 1 | - 0 | >> 0)`, `(short)(unsigned short)count`, `(int)(short)(int)count`,
//     a ternary and a comma expression — all 10 fold to the same 5-byte `movsx r32, mem16`.
//   · the decl TYPE of `i` — and this one is the informative negative. `short i` DOES produce the
//     16-bit load (`mov bp, word ptr [esp+0x12]`, len 112, diff 46) but then keeps the counter
//     16-bit (`dec bp`), where the original decrements 32-bit. So cl will emit EITHER the folded
//     `movsx r32,mem16`+`dec r32` (int) OR `mov r16,mem16`+`dec r16` (short); the original's
//     THIRD shape — 16-bit load into a scratch, `movsx` into a 32-bit counter, `dec r32` — was
//     not reachable from any of the ~45 spellings now tried across v99/v109/v120.
// ⇒ The +3 is one instruction-SELECTION decision, not a missing statement. Park stands; do not
// re-open it without a genuinely new mechanism (the axes above are all recorded as spent).
