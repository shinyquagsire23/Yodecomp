// GameData — the second doc-TU source file (0x401ac0–0x4042b0): story-history registry
// persistence, world-map helpers, menu update handlers, asset accessors.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS (static MFC).
// v1 = nops + accessors + CCmdUI handlers. TODO v2: LoadStoryHistory*/SaveStoryHistory* (registry
// sextet), RemoveEmptyZonesFromPlacedList, PlaceZoneObjectTiles, Save/LoadZoneRecursive,
// OnReplayStory, StartGame, RefreshZone, BuildQuestPath.
#include "DeskcppStub.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#pragma intrinsic(strcpy)

// Pointer-through-int walk of paZonePtrGrid (StartGame): the grid holds Zone*/Tile* — on the
// 64-bit portable build the walker must be pointer-sized. The anchor config preprocesses
// PTRINT back to the ORIGINAL `int` token (same device as Worldgen.cpp/DeskcppView.cpp).
#ifdef YODA_PORTABLE
#define PTRINT intptr_t
#else
#define PTRINT int
#endif

// Demo-limiting helper: the three permanently-grayed menu items (Save/Load/Replay) share an
// inlined disable call — a non-static inline MEMBER, which is what stages the arg via EAX.
__inline void CDeskcppDoc::DemoDisable(CCmdUI *p)
{
#if defined(YODA_FULL) || defined(GAME_INDY)
    p->Enable(1);        // full/Indy: Save/Load World + Replay Story are available
#else
    p->Enable(0);
#endif
}

// FUNCTION: YODA 0x00401ac0  [EFFECTIVE MATCH: DIFF(2) — the values-loop back-edge cmp operand
//   PROVEN PHASE DRIFT (2026-07-05): the ORIGINAL's three loaders oscillate jg/jl/jg at this
//   site with identical source (orig Nevada+Oregon = cmp [count],eax;jg, Alaska = cmp eax,
//   [count];jl); ours emits jl x3. Both while-forms + a 2^3 combo sweep canonicalize to jl —
//   the choice is TU-phase, not source. Ours matches Alaska; N+O carry 2B each. Endgame item.
//   order (cmp [count],eax vs cmp eax,[count]); both source directions + do-while emit ours.
//   Lesson-#6 instruction selection. Cracks that got here: GetProfileString(...,"0") default,//   v109: the decl axis is closed too — the leading block is only {buf,pApp} and its single
//   permutation is flat at 2 B, consistent with the phase-drift reading above.
//   Find("_") > 0 arm inline-first, int v = atoi(left) - obfKey (int-width sub + temp-slot
//   sharing), guarded do-while.]
// Load the planet-1 story history from registry [GameData] Nevada0..N. Line format
// "<seed>_<obfKey>_<count>_v0_..": seed parsed but discarded; obfKey subtracted from each value.
// Reads until a missing key (default "0"); trims the list to <= 3 entries.
void CDeskcppDoc::LoadStoryHistoryNevada()
{
    char buf[32];
    CWinApp *pApp = AfxGetApp();
    CString prefix("Nevada");
    CString key;
    int i = 0;
    int done = 0;
    do {
        sprintf(buf, "%d", i);
        i++;
        key = prefix;
        key += buf;
        CString line = pApp->GetProfileString("GameData", key, "0");
        if (*(const char *)line == '0') {
            done = done + 1;
        }
        else {
            CString left = line.Left(line.Find("_"));
            CString tmp;
            atol(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int obfKey = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int count = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            int j = 0;
            if (count > 0) {
                do {
                    if (line.Find("_") > 0)
                        left = line.Left(line.Find("_"));
                    else
                        left = line;
                    int v = atoi(left) - obfKey;
                    storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), (short)v);
                    if (j < count - 1) {
                        tmp = line.Right(line.GetLength() - line.Find("_") - 1);
                        line = tmp;
                    }
                    j++;
                } while (count > j);
            }
        }
    } while (done == 0);
    if (storyHistoryNevada.GetSize() > 3)
        storyHistoryNevada.RemoveAt(0, 1);
}

// FUNCTION: YODA 0x00401ea0  [perpetual dial-breather: flipped exact<->DIFF(2) at EVERY
//   de-dup step (MapZone out, Canvas in, GameView-stub-retirement out — all 2026-07-07).
//   One 2-byte tie-break rides the TU dial; do not chase. G1.]
// Load the planet-2 story history from registry [GameData] Alaska0..N. Line format
// "<seed>_<obfKey>_<count>_v0_..": seed parsed but discarded; obfKey subtracted from each value.
// Reads until a missing key (default "0"); trims the list to <= 3 entries.
void CDeskcppDoc::LoadStoryHistoryAlaska()
{
    char buf[32];
    CWinApp *pApp = AfxGetApp();
    CString prefix("Alaska");
    CString key;
    int i = 0;
    int done = 0;
    do {
        sprintf(buf, "%d", i);
        i++;
        key = prefix;
        key += buf;
        CString line = pApp->GetProfileString("GameData", key, "0");
        if (*(const char *)line == '0') {
            done = done + 1;
        }
        else {
            CString left = line.Left(line.Find("_"));
            CString tmp;
            atol(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int obfKey = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int count = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            int j = 0;
            if (count > 0) {
                do {
                    if (line.Find("_") > 0)
                        left = line.Left(line.Find("_"));
                    else
                        left = line;
                    int v = atoi(left) - obfKey;
                    storyHistoryAlaska.SetAtGrow(storyHistoryAlaska.GetSize(), (short)v);
                    if (j < count - 1) {
                        tmp = line.Right(line.GetLength() - line.Find("_") - 1);
                        line = tmp;
                    }
                    j++;
                } while (count > j);
            }
        }
    } while (done == 0);
    if (storyHistoryAlaska.GetSize() > 3)
        storyHistoryAlaska.RemoveAt(0, 1);
}

// FUNCTION: YODA 0x00402280
// Load the planet-3 story history from registry [GameData] Oregon0..N. Line format
// "<seed>_<obfKey>_<count>_v0_..": seed parsed but discarded; obfKey subtracted from each value.
// Reads until a missing key (default "0"); trims the list to <= 3 entries.
void CDeskcppDoc::LoadStoryHistoryOregon()
{
    char buf[32];
    CWinApp *pApp = AfxGetApp();
    CString prefix("Oregon");
    CString key;
    int i = 0;
    int done = 0;
    do {
        sprintf(buf, "%d", i);
        i++;
        key = prefix;
        key += buf;
        CString line = pApp->GetProfileString("GameData", key, "0");
        if (*(const char *)line == '0') {
            done = done + 1;
        }
        else {
            CString left = line.Left(line.Find("_"));
            CString tmp;
            atol(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int obfKey = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            left = line.Left(line.Find("_"));
            int count = atoi(left);
            tmp = line.Right(line.GetLength() - line.Find("_") - 1);
            line = tmp;
            int j = 0;
            if (count > 0) {
                do {
                    if (line.Find("_") > 0)
                        left = line.Left(line.Find("_"));
                    else
                        left = line;
                    int v = atoi(left) - obfKey;
                    storyHistoryOregon.SetAtGrow(storyHistoryOregon.GetSize(), (short)v);
                    if (j < count - 1) {
                        tmp = line.Right(line.GetLength() - line.Find("_") - 1);
                        line = tmp;
                    }
                    j++;
                } while (count > j);
            }
        }
    } while (done == 0);
    if (storyHistoryOregon.GetSize() > 3)
        storyHistoryOregon.RemoveAt(0, 1);
}

// FUNCTION: YODA 0x00402660
void CDeskcppDoc::Nop1()
{
}

// FUNCTION: YODA 0x00402670  [DIFF(2) x3 (this + Alaska + Oregon) — was 611 B each until v110.
//   ⭐ THE CRACK (lesson #42, the LICM/CSE-temp dial): the `rem` loop used to read
//       int idx = k + lineNo * 10;   ... storyHistory*[idx] ...  (in BOTH arms)
//   A NAMED TEMP for a subexpression that is loop-INVARIANT in part lets cl hoist that part
//   out of the loop: it materialised lineNo*10 in EBX before the loop (`lea ecx,[eax+eax*4];
//   lea ebx,[ecx*2]`), which cost a callee-saved register (`push ebx`/`pop ebx`), leaked into
//   the OTHER loop's arms (arm 2 grabbed EBX for the data pointer, so the two arms landed the
//   buf pointer in different registers), broke the cross-jump of the shared
//   [lea buf; inc esi; push buf; call sprintf] tail (+16 B, ours 858 vs orig 842) and rotated
//   the {lineNo,base,rem} frame slots into a 3-cycle. Repeating the subscript expression
//   INLINE in both arms instead makes cl CSE it to just above the branch — exactly what the
//   original does (`mov eax,[lineNo]; cmp esi,edi; lea edx,[eax+eax*4]; lea edx,[esi+edx*2]`,
//   recomputed every iteration) — and all four symptoms vanish at once: 611 -> 22.
//   ⇒ When a residual shows an extra callee-saved push + asymmetric if/else arms + a failed
//   tail merge, suspect a CSE TEMP you introduced that the original never had.
//   ⭐ Then `k = 0;` AFTER `line += buf;` (not before it) at BOTH init sites: 22 -> 13 -> 2.
//   Lesson #39 exactly — the increment/zero-store's POSITION relative to the neighbouring call
//   decides whether cl schedules `xor esi,esi` into the call setup or ahead of it.
//   RESIDUAL = 2 B, the `k < last` cmp operand order (`cmp esi,edi;jge` vs ours `cmp edi,esi;
//   jle`) plus, in Alaska/Oregon, the backedge `while (k < rem)` mirror. PARKED, and now for a
//   MEASURED reason rather than a guess: (i) the three twins DISAGREE WITH EACH OTHER in our
//   build (Nevada S1 wrong/S2 right, Oregon S1 right/S2 wrong, Alaska both wrong) on identical
//   source, which is v105's "the source is not the variable" signature; (ii) 8 in-function
//   spellings (compare mirrors, guard form, do-while/backedge forms, `last` decl position) are
//   dead flat at 2/4/2; (iii) an UPSTREAM token perturbation in the Load* twins that visibly
//   moved those functions moved these three by exactly 0 — so the v105/v106 joint-phase lever
//   does not reach here either. Same family as the 0x401ac0 note below: MSVC 4.2 phase drift
//   that the ORIGINAL binary exhibits too.]
// Write storyHistoryNevada back to registry [GameData] Nevada0..N: 10 values per line, each
// obfuscated by +obfKey (rand()%255+1, stored as field1); worldSeed as the decimal prefix.
void CDeskcppDoc::SaveStoryHistoryNevada()
{
    char buf[32];
    int fullLines;
    int rem;
    int k;
    int lineNo;
    int obfKey;
    CWinApp *pApp = AfxGetApp();
    CString line;
    CString prefix("Nevada");
    obfKey = rand() % 0xff + 1;
    int n = storyHistoryNevada.GetSize();
    if (n > 3) {
        storyHistoryNevada.RemoveAt(0, 1);
        n = storyHistoryNevada.GetSize();
    }
    if (n >= 0) {
        fullLines = n / 10;
        lineNo = 0;
        rem = n % 10;
        if (fullLines > 0) {
            int base = 0;
            do {
                sprintf(buf, "%ld_", worldSeed);
                line = buf;
                sprintf(buf, "%d_", obfKey);
                line += buf;
                strcpy(buf, "10_");
                line += buf;
                k = 0;
                do {
                    if (k < 9)
                        sprintf(buf, "%d_", storyHistoryNevada[base + k] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryNevada[base + k] + obfKey);
                    k++;
                    line += buf;
                } while (k < 10);
                sprintf(buf, "%d", lineNo);
                {
                    CString key = prefix + buf;
                    pApp->WriteProfileString("GameData", key, line);
                }
                base += 10;
                lineNo++;
            } while (lineNo < fullLines);
        }
        if (rem > 0) {
            sprintf(buf, "%d", lineNo);
            CString key = prefix + buf;
            sprintf(buf, "%ld_", worldSeed);
            line = buf;
            sprintf(buf, "%d_", obfKey);
            line += buf;
            sprintf(buf, "%d_", rem);
            line += buf;
            k = 0;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryNevada[k + lineNo * 10] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryNevada[k + lineNo * 10] + obfKey);
                    k++;
                    line += buf;
                } while (k < rem);
            }
            pApp->WriteProfileString("GameData", key, line);
        }
    }
}

// FUNCTION: YODA 0x004029c0
// Write storyHistoryAlaska back to registry [GameData] Alaska0..N: 10 values per line, each
// obfuscated by +obfKey (rand()%255+1, stored as field1); worldSeed as the decimal prefix.
void CDeskcppDoc::SaveStoryHistoryAlaska()
{
    char buf[32];
    int fullLines;
    int rem;
    int k;
    int lineNo;
    int obfKey;
    CWinApp *pApp = AfxGetApp();
    CString line;
    CString prefix("Alaska");
    obfKey = rand() % 0xff + 1;
    int n = storyHistoryAlaska.GetSize();
    if (n > 3) {
        storyHistoryAlaska.RemoveAt(0, 1);
        n = storyHistoryAlaska.GetSize();
    }
    if (n >= 0) {
        fullLines = n / 10;
        lineNo = 0;
        rem = n % 10;
        if (fullLines > 0) {
            int base = 0;
            do {
                sprintf(buf, "%ld_", worldSeed);
                line = buf;
                sprintf(buf, "%d_", obfKey);
                line += buf;
                strcpy(buf, "10_");
                line += buf;
                k = 0;
                do {
                    if (k < 9)
                        sprintf(buf, "%d_", storyHistoryAlaska[base + k] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryAlaska[base + k] + obfKey);
                    k++;
                    line += buf;
                } while (k < 10);
                sprintf(buf, "%d", lineNo);
                {
                    CString key = prefix + buf;
                    pApp->WriteProfileString("GameData", key, line);
                }
                base += 10;
                lineNo++;
            } while (lineNo < fullLines);
        }
        if (rem > 0) {
            sprintf(buf, "%d", lineNo);
            CString key = prefix + buf;
            sprintf(buf, "%ld_", worldSeed);
            line = buf;
            sprintf(buf, "%d_", obfKey);
            line += buf;
            sprintf(buf, "%d_", rem);
            line += buf;
            k = 0;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryAlaska[k + lineNo * 10] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryAlaska[k + lineNo * 10] + obfKey);
                    k++;
                    line += buf;
                } while (k < rem);
            }
            pApp->WriteProfileString("GameData", key, line);
        }
    }
}

// FUNCTION: YODA 0x00402d10
// Write storyHistoryOregon back to registry [GameData] Oregon0..N: 10 values per line, each
// obfuscated by +obfKey (rand()%255+1, stored as field1); worldSeed as the decimal prefix.
void CDeskcppDoc::SaveStoryHistoryOregon()
{
    char buf[32];
    int fullLines;
    int rem;
    int k;
    int lineNo;
    int obfKey;
    CWinApp *pApp = AfxGetApp();
    CString line;
    CString prefix("Oregon");
    obfKey = rand() % 0xff + 1;
    int n = storyHistoryOregon.GetSize();
    if (n > 3) {
        storyHistoryOregon.RemoveAt(0, 1);
        n = storyHistoryOregon.GetSize();
    }
    if (n >= 0) {
        fullLines = n / 10;
        lineNo = 0;
        rem = n % 10;
        if (fullLines > 0) {
            int base = 0;
            do {
                sprintf(buf, "%ld_", worldSeed);
                line = buf;
                sprintf(buf, "%d_", obfKey);
                line += buf;
                strcpy(buf, "10_");
                line += buf;
                k = 0;
                do {
                    if (k < 9)
                        sprintf(buf, "%d_", storyHistoryOregon[base + k] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryOregon[base + k] + obfKey);
                    k++;
                    line += buf;
                } while (k < 10);
                sprintf(buf, "%d", lineNo);
                {
                    CString key = prefix + buf;
                    pApp->WriteProfileString("GameData", key, line);
                }
                base += 10;
                lineNo++;
            } while (lineNo < fullLines);
        }
        if (rem > 0) {
            sprintf(buf, "%d", lineNo);
            CString key = prefix + buf;
            sprintf(buf, "%ld_", worldSeed);
            line = buf;
            sprintf(buf, "%d_", obfKey);
            line += buf;
            sprintf(buf, "%d_", rem);
            line += buf;
            k = 0;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryOregon[k + lineNo * 10] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryOregon[k + lineNo * 10] + obfKey);
                    k++;
                    line += buf;
                } while (k < rem);
            }
            pApp->WriteProfileString("GameData", key, line);
        }
    }
}

// FUNCTION: YODA 0x00403060
void CDeskcppDoc::Nop2()
{
}

// FUNCTION: YODA 0x00403070  [DIFF(26) -> DIFF(24) at v113 via the LOOP FORM (lesson #40).
//   Loop 1 is the house guarded countdown, NOT a `for (i = 0; i < n; i++)`: the original's
//   backedge is `add edi,2; dec ebx; jne` under a `test ebx,ebx; jle` guard. Switching to it
//   fixed a REAL defect, not just a byte count — `this` now lands in ESI as the original has
//   it, which removed four this-relative load diffs (+0x03e/+0x069/+0x084/+0x093).
//   ⚠ LENGTH does not discriminate here (both forms emit 206), because cl already strength-
//   reduces our `for` into a countdown; the register assignment is what moves.
//   Loop 2 keeps the plain `for`: its guard reads keep.m_nSize from MEMORY and reloads it,
//   which our current spelling already produces. Both countdown spellings of loop 2 are
//   WORSE (27-29 B) and the register-guarded form is REFUTED BY LENGTH (204 vs 206).
//   PARKED at 24 B: a clean ebx<->edi 2-cycle (orig n->ebx i->edi; ours reversed) plus an
//   entry schedule shift (the original loads n between the callee-save pushes). Closed axes:
//   32 decl set x order configurations over {i,id,j,m} x i-position — all flat at 24.]
// Rebuild placedZoneIds in place, dropping zones whose type is Empty(1).
void CDeskcppDoc::RemoveEmptyZonesFromPlacedList()
{
    int n = placedZoneIds.GetSize();
    CWordArray keep;
    if (n > 0) {
        int i = 0;
        do {
            unsigned short id = placedZoneIds[i];
            if (zoneObjects[id]->type != 1)
                keep.SetAtGrow(keep.GetSize(), id);
            i++;
            n--;
        } while (n != 0);
    }
    placedZoneIds.SetSize(0, -1);
    int m = keep.GetSize();
    for (int j = 0; j < m; j++)
        placedZoneIds.SetAtGrow(placedZoneIds.GetSize(), keep[j]);
}

// FUNCTION: YODA 0x00403140
// Stamp a zone's visible objects into tile layer 1. Types 0/1/2/5/6/7/8 place their tile if
// active and the cell is empty; type 0xb forces tile 0x1cb.
// NOTE: `z`/`o`/`t` are declared at FUNCTION scope in THIS order, and ONE `t` serves both switch
//   arms. All three facts are load-bearing (loop-body-scoped `o`/`t` or a second `t2` costs 22 B;
//   declaring `z` after `o` costs 22 B). See CLAUDE.md "declaration SCOPE is a reg-alloc dial".
void CDeskcppDoc::PlaceZoneObjectTiles(short zoneId)
{
    Zone *z;
    ZoneObj *o;
    int t;

    if (zoneId >= 0) {
        z = zoneObjects[zoneId];
        if (z != 0) {
            int n = z->objects.GetSize();
            for (int i = 0; i < n; i++) {
                o = (ZoneObj *)z->objects[i];
                switch (o->type) {
                case 0:
                case 1:
                case 2:
                case 5:
                case 6:
                case 7:
                case 8:
                    if (o->state == 1 && o->arg >= 0) {
                        t = (short)z->GetTile(o->x, o->y, 1);
                        if (t < 0)
                            z->SetTile(o->x, o->y, 1, o->arg);
                    }
                    break;
                case 0xb:
                    if (o->state == 1) {
                        o->arg = 0x1cb;
                        t = (short)z->GetTile(o->x, o->y, 1);
                        if (t < 0)
                            z->SetTile(o->x, o->y, 1, 0x1cb);
                    }
                    break;
                }
            }
        }
    }
}

// FUNCTION: YODA 0x00403250  [DIAL-SENSITIVE: byte-exact under the 2026-07-06 RecordClasses.h
//   decl set; DIFF(16) again since the real-GameView.h de-dup (step 5, 2026-07-07). Proven
//   correct; settles at G1.]
// Locate the world-map cell holding zone `id`; outputs grid coords.
int CDeskcppDoc::FindZoneCellById(short id, int *pX, int *pY)
{
    if (id >= 0 && id < zoneCount) {
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++) {
                if (zones[y * 10 + x].id == id) {
                    *pX = x;
                    *pY = y;
                    return 1;
                }
            }
        return 0;
    }
    return 0;
}

// FUNCTION: YODA 0x004032c0
// Bitmask of map exits from the player's cell: W=8 E=4 N=1 S=2. Indoor/special zone types
// (8/9/0xd/0xe) have no map exits. Consumed by GameView::DrawDirectionArrows.
unsigned char CDeskcppDoc::GetExitDirections()
{
    if (currentZone == 0)
        return 0;
    int t = currentZone->type;
    if (t == 8 || t == 9 || t == 0xd || t == 0xe)
        return 0;
    int x = playerX;
    int y;
    if (x < 0 || (y = playerY) < 0)
        return 0;
    unsigned char dirs = 0;      // byte-width: the original accumulates flags in AL
    if (x > 0 && zones[y * 10 + x - 1].id >= 0)
        dirs = 8;
    if (x < 9 && zones[y * 10 + x + 1].id >= 0)
        dirs = dirs | 4;
    if (y > 0 && zones[y * 10 + x - 10].id >= 0)
        dirs = dirs | 1;
    if (y < 9 && zones[y * 10 + x + 10].id >= 0)
        return dirs | 2;
    return dirs;
}

// FUNCTION: YODA 0x004033b0
// [EXACT at v107. The 6 B "walker/counter ebx<->ebp 2-cycle" was NOT a register tie-break and
// NOT a declaration-scope dial (all 9 hoist/order/decl-set variants measured 6 B, dead flat):
// it was the LOOP FORM. The original uses the house `i++ / n-- / while (n != 0)` countdown
// under a separate `n > 0` guard — the recipe PlacePuzzle's delete loops already document —
// which yields the DEC/JNE that `for (i = 0; i < n; i++)` never produces. All five spellings
// of that idiom measure 0 B; the guard is load-bearing (an unguarded do-while emits 145 bytes
// against the original's 149). GetAt vs operator[] and the i/n decl order are inert here.
// ⚠ JOINT-PHASE DEBT: this token change re-rolls the TU phase and costs LoadZoneRecursive
// below its last byte (see its note). Confirmed token-driven, not lesson #23 — a LINE-NEUTRAL
// spelling of the same change trades identically. Net project count unchanged at 250.]
// .wld save: zone id + full flag + Zone::WriteSavedState, recursing into door-linked rooms.
void CDeskcppDoc::SaveZoneRecursive(CFile *f, short zoneId, int bFull)
{
    Zone *z = zoneObjects[zoneId];
    f->Write(&zoneId, 2);
    int full = bFull;
#ifdef GAME_INDY
    { short sfull = (short)full; f->Write(&sfull, 2); }   // retail Indy full-flag is 16-bit
#else
    f->Write(&full, 4);
#endif
    z->WriteSavedState(f, full);
    int n = z->objects.GetSize();
    if (n > 0) {
        int i = 0;
        do {
            ZoneObj *o = (ZoneObj *)z->objects[i];
            if (o->type == 9 && o->arg >= 0)
                SaveZoneRecursive(f, o->arg, full);
            i++;
            n--;
        } while (n != 0);
    }
}

// FUNCTION: YODA 0x00403450  [EFFECTIVE MATCH: DIFF(1) at exact length — v102: was DIFF(7). The
//   crack was EVALUATION ORDER, not register roles: pre-caching `o->arg` into the local hoisted
//   its load ABOVE the type test, where cl emits it after. Assigning inside the && keeps the
//   callee-saved cache across the Reads AND the original's order (7 B -> 1 B). Mirrors
//   SaveZoneRecursive above, which already reads o->arg inside the condition. The last byte is
//   the savedId/child cmp operand order — a pure commutative tie-break, canonicalized either way.
//   ⚠ v107: this was EXACT until SaveZoneRecursive's countdown form landed above; that token
//   change re-rolls the TU's joint register phase (v105) and puts this back to DIFF(1) on that
//   same inert cmp byte. 16 spellings probed under the new phase (decl set/order for i/o/child,
//   savedId/savedFull order, cmp mirror, == form): floor is 1 B, unreachable from this
//   function's own source. The countdown is structurally WRONG here — it emits 165 bytes vs the
//   original's 177 — so the two mirrors genuinely differ in loop form. OPEN: the compensating
//   fix must sit UPSTREAM of line 613 in this TU (v106 downstream-only); RemoveEmptyZones-
//   FromPlacedList 0x403070 was swept (10 variants) and is inert, so look further up.]
// .wld load mirror: read + verify each door child id before recursing.
void CDeskcppDoc::LoadZoneRecursive(CFile *f, short zoneId, int bFull)
{
    short savedId;
    int savedFull;
    Zone *z = zoneObjects[zoneId];
    z->ReadSavedState(f, bFull);
    int n = z->objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObj *o = (ZoneObj *)z->objects[i];
        short child;               // cached in a callee-saved reg across the Read calls
        if (o->type == 9 && (child = o->arg) >= 0) {
            f->Read(&savedId, 2);
#ifdef GAME_INDY
            { short sfull; f->Read(&sfull, 2); savedFull = sfull; }   // retail Indy full-flag is 16-bit
#else
            f->Read(&savedFull, 4);
#endif
            if (savedId != child)
                return;
            LoadZoneRecursive(f, child, savedFull);
        }
    }
}

// FUNCTION: YODA 0x00403510
// File>Save World: permanently grayed in the demo.
// [EXACT (v99): the grayed trio's EAX staging is the fingerprint of inlining a non-static
//  MEMBER — the implicit `this` nominally holds ECX, so the arg must land in EAX first. The
//  old file-scope `static __inline` helper folded to `mov ecx,[esp+4]`; a member does not.]
void CDeskcppDoc::OnUpdateFileSave(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403520
void CDeskcppDoc::OnUpdateAppExit(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6 && nFrameMode != 5)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403550
void CDeskcppDoc::OnUpdateHideMe(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403580
void CDeskcppDoc::OnUpdateNewWorld(CCmdUI *pCmdUI)
{
    switch (nFrameMode) {
    case 1:
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xb:
        pCmdUI->Enable(0);
        return;
    case 7:
        if (nMapChangeReason == 4 || nMapChangeReason == 1)
            pCmdUI->Enable(0);
        else
            pCmdUI->Enable(1);
        return;
    default:
        pCmdUI->Enable(1);
        return;
    }
}

// FUNCTION: YODA 0x00403600
// File>Load World: permanently grayed in the demo.
void CDeskcppDoc::OnUpdateLoadWorld(CCmdUI *pCmdUI)
{
#if defined(GAME_INDY) || defined(YODA_FULL)
    if (nFrameMode == 1 || nFrameMode == 4 || nFrameMode == 5 || nFrameMode == 6
        || nFrameMode == 9 || nFrameMode == 11) { pCmdUI->Enable(0); return; }
#endif
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403610
// File>Replay Story: permanently grayed in the demo (the handler is still linked).
void CDeskcppDoc::OnUpdateReplayStory(CCmdUI *pCmdUI)
{
#if defined(GAME_INDY) || defined(YODA_FULL)
    // Replay/Load regenerate the world; when enabled (full/Indy) they must be grayed during the
    // same busy frame-modes New World gates on — especially a text dialog (mode 5). Otherwise the
    // player can trigger a re-entrant world regen while a textbox is open (the text drew over the
    // STUP graphic and it crashed on dialog exit). Demo path (DemoDisable -> Enable(0)) unaffected.
    if (nFrameMode == 1 || nFrameMode == 4 || nFrameMode == 5 || nFrameMode == 6
        || nFrameMode == 9 || nFrameMode == 11) { pCmdUI->Enable(0); return; }
#endif
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403620
// File>Replay Story: confirm if a game is in progress, pick the story to replay (current goal or
// the planet history's most recent), then rebuild the world around it with a fresh seed.
void CDeskcppDoc::OnReplayStory()
{
    int answer = 6;
    int savedMode = nFrameMode;
    nFrameMode = 0;
    if (gameState == 0)
        answer = AfxMessageBox(IDS_CONFIRM_REPLAY, MB_YESNO, 0);
    if (answer == IDYES) {
        if (nCurrentGoalItem > 0) {
            nRequestedGoalItem = nCurrentGoalItem;
        }
        else {
#ifdef GAME_INDY
            // Indy has ONE story history ([GameData] Wyoming<N>, kept in the storyHistoryAlaska
            // slot) — DESKADV OnReplayStory (FUN_1020_0dc8) loads it directly, no planet switch;
            // most-recent entry becomes the requested goal (v85 INI replay persistence).
            IndyLoadStoryHistory();
            {
                int n = storyHistoryAlaska.GetSize();
                if (n > 0) {
                    nRequestedGoalItem = storyHistoryAlaska[n - 1];
                }
                else {
                    AfxMessageBox(IDS_ERR_NO_STORY_SAVED, 0, -1);
                    return;
                }
            }
#else
            switch (currentPlanet) {
            case 1: {
                LoadStoryHistoryNevada();
                int n = storyHistoryNevada.GetSize();
                if (n > 0) {
                    nRequestedGoalItem = storyHistoryNevada[n - 1];
                }
                else {
                    AfxMessageBox(IDS_ERR_NO_STORY_SAVED, 0, -1);
                    return;
                }
                break;
            }
            case 2: {
                LoadStoryHistoryAlaska();
                int n = storyHistoryAlaska.GetSize();
                if (n > 0) {
                    nRequestedGoalItem = storyHistoryAlaska[n - 1];
                }
                else {
                    AfxMessageBox(IDS_ERR_NO_STORY_SAVED, 0, -1);
                    return;
                }
                break;
            }
            case 3: {
                LoadStoryHistoryOregon();
                int n = storyHistoryOregon.GetSize();
                if (n > 0) {
                    nRequestedGoalItem = storyHistoryOregon[n - 1];
                }
                else {
                    AfxMessageBox(IDS_ERR_NO_STORY_SAVED, 0, -1);
                    return;
                }
                break;
            }
            }
#endif // GAME_INDY (single-history replay above; Yoda per-planet switch here)
        }
        goalTileList.SetSize(0, -1);
        placedZoneIds.SetSize(0, -1);
        bWorldInvalid = 1;
        bStartingGame = 1;
        int ok = StartGame(Randomize(), 0);   // nested: the 0 is pushed before Randomize runs
        bStartingGame = 0;
        if (ok == 0) {
            nFrameMode = 12;
            return;
        }
#ifndef GAME_INDY
        // Indy has no mode-advancing zone-entry script, so world entry must use the self-climbing
        // ZoneTransitionStep path (bWorldInvalid stays 1, as IndyGenerate's tail set it) — clearing
        // it to 0 here routes OnTimer case-0xb through WorldEntryStepMaybe, which loops 0->5 forever
        // and hangs at the STUP graphic (the Replay/New Story bug). OnTimer clears it when the
        // transition completes. Yoda keeps this store (its intro zone HAS a scripted entry).
        bWorldInvalid = 0;
#endif
        return;
    }
    nRequestedGoalItem = -1;
    nFrameMode = savedMode;
}

// FUNCTION: YODA 0x004037a0  [WIP: DIFF(79), align=34, insns 178/178 — was DIFF(254) under the
//   old SHIFTED MapZone stub, whose off-by-4 grid displacements were silently poisoning every
//   cell store; the vptr-true MapZone.h de-dup (2026-07-07) fixed those. Remaining residuals:
//   grid-store increment placement, gen-loop layout, early-out jcc direction (as before).]
// Begin a game session: reset player state, walk-in animation, camera/inventory reset, clear both
// map grids, load assets, then (unless restoring a save) generate + populate the world.
int CDeskcppDoc::StartGame(unsigned int nSeed, int bSkipGenerate)
{
    bHidePlayer = 1;
    healthHi = 1;
    healthLo = 1;
    weaponState[0] = 0;
    currentWeapon = 0;
    weaponState[1] = 0;
    gameState = 0;
    weaponState[2] = 0;
    abortFrame = 0;
    weaponState[3] = 0;
    nMapChangeReason = 1;
    nFrameMode = 7;
    nCurrentAmmo = 0;
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *v = (CDeskcppView *)GetNextView(pos);
    if (v != 0) {
        for (int i = 0; i < 5; i++) {
#ifdef GAME_INDY
            v->ZoneTransitionStep(0x78, (short)i);   // Indy intro zone 120 (DESKADV
                                                     // IndyStartNewGameMaybe pushes 0x78)
#else
            v->ZoneTransitionStep(0x5d, (short)i);
#endif
            long c = clock();
            while (clock() < c + 100)
                ;
        }
        nFrameMode = 0;
        v->nTransitionStep = 0;
        v->SoundFlush();
#ifdef GAME_INDY
        Indy_MidiStopAll();   // DESKADV stops the MCI sequencers here; PlaySound(0x3a)
                              // remaps to FLOURISH.MID (the twin pushes 0x0e)
#endif
        v->PlaySound(0x3a);
    }
    SetCurrentToIntroZone();
    cameraY = 0;
    cameraX = 0;
    UpdateCamera();
    bWorldReady = 1;
    nFrameMode = 7;
    nMapChangeReason = 1;
    v->DrawGameArea(0);
    if (bSkipGenerate == 0)
#ifdef GAME_INDY
        v->nTargetZoneId = 0x78;   // Indy intro zone 120 (16-bit view+0x90 = 0x78)
#else
        v->nTargetZoneId = 0x5d;
#endif
    int n = inventory.GetSize();
    for (int j = 0; j < n; j++) {
        CObject *p = inventory[j];
        if (p)
            delete p;
    }
    inventory.SetSize(0, -1);
    UpdateAllViews(0, 0, 0);
    PTRINT *pg = paZonePtrGrid;
    MapZone *mz = zones;
    for (int r = 0; r < 10; r++) {
        for (int col = 0; col < 10; col++) {
            *pg = 0;
            mz->id = -1;
            pg++;
            mz->cellQuestSlot0 = -1;
            mz->cellQuestSlot1 = -1;
            mz->zoneType = -1;
            mz->cellItemA = -1;
            mz->cellItemB = -1;
            mz->cellItemC = -1;
            mz->cellQuestSlot5 = -1;
            mz->cellQuestSlot6 = -1;
            mz->flagSolved = 0;
            mz->flagA = 0;
            mz->flagB = 0;
            mz->flagC = 0;
            mz->flagD = 0;
            mz->field30 = -1;
            mz[100].id = -1;
            mz[100].cellQuestSlot0 = -1;
            mz[100].cellQuestSlot1 = -1;
            mz[100].zoneType = -1;
            mz[100].cellItemA = -1;
            mz[100].cellItemB = -1;
            mz[100].cellItemC = -1;
            mz[100].cellQuestSlot5 = -1;
            mz[100].cellQuestSlot6 = -1;
            mz[100].flagSolved = 0;
            mz[100].flagA = 0;
            mz[100].flagB = 0;
            mz[100].flagC = 0;
            mz[100].flagD = 0;
            mz[100].field30 = -1;
            mz++;
        }
    }
    if (LoadWorld() == 0)
        return 0;
    unk2e34 = 0;
    if (bSkipGenerate == 0) {
        int ok = 0;
        unsigned int seed = nSeed;
        do {
            // Indy: IndyGenerate does plan+placement+materialize+play-state (no separate
            // Populate), same as the Load() dispatcher. The Yoda #else path is byte-identical
            // to the original (anchor-safe). Without this guard, New World -> StartGame ran the
            // Yoda Generate, which never converges on Indy data -> infinite reseed loop.
#ifdef GAME_INDY
            if (IndyGenerate(seed) == 0)
#else
            if (Generate(seed) == 0)
#endif
                seed = Randomize();
            else
                ok = ok + 1;
        } while (ok == 0);
#ifndef GAME_INDY
        BackupZoneGrid();
        Populate();
#endif
    }
    v->bBusy = 0;
    if (bSkipGenerate == 0)
        nFrameMode = 11;
    unk3378 = 0;
    unk2e60 = 0;
    return 1;
}

// FUNCTION: YODA 0x00403a40
Tile *CDeskcppDoc::GetTileData(int idx)
{
    if (idx >= 0 && idx < tileCount)
        return tileArray[idx];
    return 0;
}

// FUNCTION: YODA 0x00403a70
Zone *CDeskcppDoc::GetZoneById(short id)
{
    if (id >= 0 && id < zoneCount YODA_SIC_FIX(&& (zoneObjects[id] != (Zone *)-1 || (BUGLOG(("sic GetZoneById(%d): off-planet -1 slot, returning NULL\n", (int)id)), 0))))
        return zoneObjects[id];
    return 0;
}

// FUNCTION: YODA 0x00403aa0  [EFFECTIVE MATCH: DIFF(4) — orig reuses dead ECX (this) for the
//   walk pointer; ours colors it EDX. cmp-flip inert. Allocator tie-break.]
int CDeskcppDoc::FindTile(void *pTile)
{
    int r = -1;
    int n = tileCount;
    int i = 0;
    if (n > 0) {
        Tile **p = tileArray;
        do {
            if (*p == (Tile *)pTile) {
                r = i;
                break;
            }
            p++;
            i++;
        } while (i < n);
    }
    return r;
}

// FUNCTION: YODA 0x00403ae0  [WIP: DIFF(70), len 410 vs 416 — cracked so far: faithful destX/destY
//   short accumulators + per-iteration currentZone re-reads + int-promoted GetTile results +
//   BlitMasked-arm-first. Residual: a movsx-before-add on the accumulators (int-accum probe worse)
//   + push/mov scheduling at the blit sites.]
// Redraw the whole current zone into the offscreen canvas: 3 layers per cell; layers 1/2 use the
// masked blit for game-object tiles.
void CDeskcppDoc::RefreshZone()
{
    if (currentZone == 0) {
        pCanvas->Clear();
        return;
    }
    short cy = 0;
    if (currentZone->width > 0) {
        short destY = 0;
        do {
            short cx = 0;
            if (currentZone->height > 0) {
                short destX = 0;
                do {
                    int t = (short)((Zone *)currentZone)->GetTile(cx, cy, 0);
                    if (t >= 0)
                        pCanvas->BlitFast(tileArray[t]->pixels, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, destX, destY);
                    t = (short)((Zone *)currentZone)->GetTile(cx, cy, 1);
                    if (t >= 0) {
                        Tile *pt = tileArray[t];
                        if ((pt->flags & 1) != 0)
                            pCanvas->BlitMasked((char *)pt->pixels, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, destX, destY, 0);
                        else
                            pCanvas->BlitFast(pt->pixels, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, destX, destY);
                    }
                    t = (short)((Zone *)currentZone)->GetTile(cx, cy, 2);
                    if (t >= 0) {
                        Tile *pt = tileArray[t];
                        if ((pt->flags & 1) != 0)
                            pCanvas->BlitMasked((char *)pt->pixels, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, destX, destY, 0);
                        else
                            pCanvas->BlitFast(pt->pixels, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, TILE_PIXEL_SIZE, destX, destY);
                    }
                    destX = destX + 0x20;
                    cx = cx + 1;
                } while (cx < currentZone->height);
            }
            destY = destY + 0x20;
            cy = cy + 1;
        } while (cy < currentZone->width);
    }
}

// FUNCTION: YODA 0x00403c80  [NEAR MATCH: main code 0x537 vs orig 0x52e (+9B, 421/420 insns);
//   all five phases structurally converged (this-spill prologue, jump tables, walkers,
//   parity sbb/and idiom, swap epilogue). Residual loci: (a) prologue push/arg-load interleave
//   (paGrid binds EDI vs orig ESI — reg 3-cycle); (b) the phase-3 0x66 check emits a word-cmp
//   vs orig movsx+int-cmp (a one-case `switch(paGrid[..])` reproduces movsx+ECX exactly but
//   perturbs two other sites — net wash, kept the idiomatic if); (c) phase-5 zero-order +
//   swap-block scheduling. Same allocator tie-break class as the savers — endgame item.
//   Cracks that got here: helpers are __thiscall World members with unused this (orig
//   reloads ECX at every call site); `int count;` declared AFTER the three counters (fixed
//   ALL count-vs-target cmp operand orders); literal forms x<=1 / x<8 / x>0&&x<9 / >=3 /
//   >=0x96; walker arms != 0x68 inc-first; target=...+1 then a separate -=3; store order
//   fx,fy,found=1; found-flag reuse in the swap loop (no re-store of 1); int-local `cell`
//   for the 1/0x12c pair.]
// Worldgen plan-grid quest-path pass (called by Generate): counts the special plan cells,
// derives a random quest-step target, converts blockade-adjacent (0x12d-0x130) and
// gate-adjacent (0x66/0x68) 300-cells into 0x132 quest steps with sequential order ids,
// tops up with random ring-3+ placements, then swaps the final step out of the start ring.
// Returns the number of quest steps placed.
int CDeskcppDoc::BuildQuestPathMaybe(short *paGrid, short *paOrder)
{
    int nItems = 0;
    int nGates = 0;
    int nEmpty = 0;
    int count;
    short *p = paGrid;
    int x;
    int y;
    for (y = 0; y < 10; y++) {
        for (x = 0; x < 10; x++) {
            switch (*p) {
            case 0x65:
                nItems++;
                break;
            case 1:
            case 0x68:
            case 0x12c:
                nEmpty++;
                break;
            case 0x12d:
            case 0x12e:
            case 0x12f:
            case 0x130:
                nGates++;
                break;
            }
            p++;
        }
    }
    int target = rand() % (nEmpty / 5 + 1) + nEmpty / 4 - nGates - nItems + 1;
    target -= 3;
    if (target < 4)
        target = 4;
    count = 0;
    y = 0;
    p = paGrid - 1;
    short *po = paOrder - 2;
    for (; y < 10; y++) {
        for (x = 0; x < 10; x++) {
            switch (p[1]) {
            case 0x12d:
                if (p[0] == 0x12c) {
                    if (x <= 1 || p[-1] != 0x12c) {
                        p[0] = 0x132;
                        po[1] = (short)count;
                    }
                    else {
                        p[-1] = 0x132;
                        po[0] = (short)count;
                    }
                    count++;
                }
                break;
            case 0x12e:
                if (p[2] == 0x12c) {
                    if (x < 8 && p[3] == 0x12c) {
                        p[3] = 0x132;
                        po[4] = (short)count;
                    }
                    else {
                        p[2] = 0x132;
                        po[3] = (short)count;
                    }
                    count++;
                }
                break;
            case 0x12f:
                if (p[-9] == 0x12c) {
                    if (y <= 1 || p[-0x13] != 0x12c) {
                        p[-9] = 0x132;
                        po[-8] = (short)count;
                    }
                    else {
                        p[-0x13] = 0x132;
                        po[-0x12] = (short)count;
                    }
                    count++;
                }
                break;
            case 0x130:
                if (p[0xb] == 0x12c) {
                    if (y < 8 && p[0x15] == 0x12c) {
                        p[0x15] = 0x132;
                        po[0x16] = (short)count;
                    }
                    else {
                        p[0xb] = 0x132;
                        po[0xc] = (short)count;
                    }
                    count++;
                }
                break;
            }
            p++;
            po++;
        }
    }
    for (y = 0; y < 10; y++) {
        for (x = 0; x < 10; x++) {
            if ((int)paGrid[y * 10 + x] != 0x66)
                continue;
            switch (FindAdjacentGateDirMaybe(x, y, paGrid)) {
            case 1: {
                int cx = x - 1;
                int stop = 0;
                do {
                    if (cx < 0) {
                        cx = 0;
                        stop++;
                    }
                    else if (paGrid[y * 10 + cx] != 0x68) {
                        cx++;
                        stop++;
                    }
                    else {
                        cx--;
                    }
                } while (stop == 0);
                paGrid[y * 10 + cx] = 0x132;
                paOrder[y * 10 + cx] = (short)count;
                break;
            }
            case 2: {
                int cy = y - 1;
                int stop = 0;
                do {
                    if (cy < 0) {
                        cy = 0;
                        stop++;
                    }
                    else if (paGrid[cy * 10 + x] != 0x68) {
                        cy++;
                        stop++;
                    }
                    else {
                        cy--;
                    }
                } while (stop == 0);
                paGrid[cy * 10 + x] = 0x132;
                paOrder[cy * 10 + x] = (short)count;
                break;
            }
            case 3: {
                int cx = x + 1;
                int stop = 0;
                do {
                    if (cx > 9) {
                        cx = 9;
                        stop++;
                    }
                    else if (paGrid[y * 10 + cx] != 0x68) {
                        cx--;
                        stop++;
                    }
                    else {
                        cx++;
                    }
                } while (stop == 0);
                paGrid[y * 10 + cx] = 0x132;
                paOrder[y * 10 + cx] = (short)count;
                break;
            }
            case 4: {
                int cy = y + 1;
                int stop = 0;
                do {
                    if (cy > 9) {
                        cy = 9;
                        stop++;
                    }
                    else if (paGrid[cy * 10 + x] != 0x68) {
                        cy--;
                        stop++;
                    }
                    else {
                        cy++;
                    }
                } while (stop == 0);
                paGrid[cy * 10 + x] = 0x132;
                paOrder[cy * 10 + x] = (short)count;
                break;
            }
            default:
                continue;
            }
            count++;
        }
    }
    int done = 0;
    int attempts = 0;
    do {
        if (count >= target)
            done++;
        if (attempts > 200)
            done++;
        if (attempts < 50) {
            x = rand() % 10;
            if (x > 0 && x < 9)
                y = (rand() % 2 == 0) ? 9 : 0;
            else
                y = rand() % 10;
        }
        else {
            x = rand() % 10;
            y = rand() % 10;
        }
        if (count >= target)
            break;
        if (GetZoneGridOrder(x, y) >= 3 || attempts >= 0x96) {
            int cell = paGrid[y * 10 + x];
            if (cell == 1 || cell == 0x12c) {
                int bLeftOk = 0;
                int bRightOk = 0;
                int bUpOk = 0;
                int bDownOk = 0;
                if (x == 0 || paGrid[y * 10 + x - 1] != 0x132)
                    bLeftOk = 1;
                if (x == 9 || paGrid[y * 10 + x + 1] != 0x132)
                    bRightOk = 1;
                if (y == 0 || paGrid[y * 10 + x - 10] != 0x132)
                    bUpOk = 1;
                if (y == 9 || paGrid[y * 10 + x + 10] != 0x132)
                    bDownOk = 1;
                if (bLeftOk && bRightOk && bUpOk && bDownOk) {
                    paGrid[y * 10 + x] = 0x132;
                    paOrder[y * 10 + x] = (short)count;
                    count++;
                }
            }
            if (count >= target)
                break;
            attempts++;
        }
    } while (done == 0);
    int last = count - 1;
    int found = 0;
    int fx = 0;
    int fy = 0;
    for (y = 0; y < 10; y++) {
        for (x = 0; x < 10; x++) {
            if (paOrder[y * 10 + x] == last && GetZoneGridOrder(x, y) < 3) {
                fx = x;
                fy = y;
                found = 1;
                break;
            }
        }
        if (found)
            break;
    }
    if (found) {
        for (y = 0; y < 10; y++) {
            for (x = 0; x < 10; x++) {
                if (paOrder[y * 10 + x] >= 0 && GetZoneGridOrder(x, y) >= 3
                    && paOrder[y * 10 + x] != last) {
                    short t = paOrder[y * 10 + x];
                    paOrder[y * 10 + x] = (short)last;
                    found = 0;
                    paOrder[fy * 10 + fx] = t;
                    break;
                }
            }
            if (!found)
                return count;
        }
    }
    return count;
}
