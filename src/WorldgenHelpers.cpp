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

// Demo-limiting helper: the three permanently-grayed menu items (Save/Load/Replay) share an
// inlined disable call — the EAX staging of the pointer arg is the inlining fingerprint.
static __inline void DemoDisable(CCmdUI *p)
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
//   Lesson-#6 instruction selection. Cracks that got here: GetProfileString(...,"0") default,
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

// FUNCTION: YODA 0x00402670  [EFFECTIVE MATCH x3 (this + Alaska + Oregon): main body 824B vs orig
//   808B; head/tail/frame layout & instruction stream converged (buf@-0x54, fullLines@-0x34,
//   obfKey@-0x18, key temp among named locals — the inner-scoped `CString key` block was the
//   crack: a bare `prefix + buf` temp gets a frame-bottom slot, breaking the layout by 4B).
//   Residual = ONE coupled allocator artifact: {lineNo,base,rem} int slots are a 3-cycle
//   (-0x1c/-0x20/-0x24 rotated) which desymmetrizes the two sprintf arms (arm2 grabs EBX ->
//   push ebx + failed cross-jump of the shared [lea buf; inc; push; call] tail = the 16B).
//   Exhausted: decl order/position (4 variants), ternary-of-calls, arms swapped, k++ placement,
//   v-temp homing, loader-form phase knobs (2^3 sweep — zero effect). Slot triple is IR temp
//   numbering = full-TU/endgame territory (Records precedent). NOTE the orig loaders' own
//   backedge cmp oscillates jg/jl/jg across identical source — MSVC 4.2 phase drift is REAL
//   in the original binary too; don't chase per-function.]
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
                k = 0;
                line += buf;
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
            k = 0;
            line += buf;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    int idx = k + lineNo * 10;
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryNevada[idx] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryNevada[idx] + obfKey);
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
                k = 0;
                line += buf;
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
            k = 0;
            line += buf;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    int idx = k + lineNo * 10;
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryAlaska[idx] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryAlaska[idx] + obfKey);
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
                k = 0;
                line += buf;
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
            k = 0;
            line += buf;
            if (rem > 0) {
                int last = rem - 1;
                do {
                    int idx = k + lineNo * 10;
                    if (k < last)
                        sprintf(buf, "%d_", storyHistoryOregon[idx] + obfKey);
                    else
                        sprintf(buf, "%d", storyHistoryOregon[idx] + obfKey);
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

// FUNCTION: YODA 0x00403070  [EFFECTIVE MATCH: DIFF(24) — i/n register 2-cycle (ebx/edi) in both
//   loops + funclet-window skew (len 206 incl. our EH stubs vs Ghidra body 188). Probes inert.]
// Rebuild placedZoneIds in place, dropping zones whose type is Empty(1).
void CDeskcppDoc::RemoveEmptyZonesFromPlacedList()
{
    int n = placedZoneIds.GetSize();
    CWordArray keep;
    for (int i = 0; i < n; i++) {
        unsigned short id = placedZoneIds[i];
        if (zoneObjects[id]->type != 1)
            keep.SetAtGrow(keep.GetSize(), id);
    }
    placedZoneIds.SetSize(0, -1);
    int m = keep.GetSize();
    for (int j = 0; j < m; j++)
        placedZoneIds.SetAtGrow(placedZoneIds.GetSize(), keep[j]);
}

// FUNCTION: YODA 0x00403140  [DIAL-SENSITIVE: byte-exact under the 2026-07-06 RecordClasses.h
//   decl set; DIFF(10) again since the real-GameView.h de-dup (step 5, 2026-07-07) rotated the
//   TU dial. Proven correct; settles at G1.]
// Stamp a zone's visible objects into tile layer 1. Types 0/1/2/5/6/7/8 place their tile if
// active and the cell is empty; type 0xb forces tile 0x1cb.
void CDeskcppDoc::PlaceZoneObjectTiles(short zoneId)
{
    if (zoneId >= 0) {
        Zone *z = zoneObjects[zoneId];
        if (z != 0) {
            int n = z->objects.GetSize();
            for (int i = 0; i < n; i++) {
                ZoneObj *o = (ZoneObj *)z->objects[i];
                switch (o->type) {
                case 0:
                case 1:
                case 2:
                case 5:
                case 6:
                case 7:
                case 8:
                    if (o->state == 1 && o->arg >= 0) {
                        int t = (short)z->GetTile(o->x, o->y, 1);
                        if (t < 0)
                            z->SetTile(o->x, o->y, 1, o->arg);
                    }
                    break;
                case 0xb:
                    if (o->state == 1) {
                        o->arg = 0x1cb;
                        int t2 = (short)z->GetTile(o->x, o->y, 1);
                        if (t2 < 0)
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

// FUNCTION: YODA 0x004033b0  [EFFECTIVE MATCH: DIFF(6) — walker/counter ebx<->ebp 2-cycle.]
// .wld save: zone id + full flag + Zone::WriteSavedState, recursing into door-linked rooms.
void CDeskcppDoc::SaveZoneRecursive(CFile *f, short zoneId, int bFull)
{
    Zone *z = zoneObjects[zoneId];
    f->Write(&zoneId, 2);
    int full = bFull;
    f->Write(&full, 4);
    z->WriteSavedState(f, full);
    int n = z->objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObj *o = (ZoneObj *)z->objects[i];
        if (o->type == 9 && o->arg >= 0)
            SaveZoneRecursive(f, o->arg, full);
    }
}

// FUNCTION: YODA 0x00403450  [EFFECTIVE MATCH: DIFF(6) at exact length — residual register roles;
//   the child-local (o->arg cached across the Reads) was the structural crack, cf. HitEntityAt.]
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
        short child = o->arg;      // cached in a callee-saved reg across the Read calls
        if (o->type == 9 && child >= 0) {
            f->Read(&savedId, 2);
            f->Read(&savedFull, 4);
            if (savedId != child)
                return;
            LoadZoneRecursive(f, child, savedFull);
        }
    }
}

// FUNCTION: YODA 0x00403510
// File>Save World: permanently grayed in the demo.
// [EFFECTIVE MATCH: DIFF(6) x3 for the grayed trio — the original stages pCmdUI through EAX
//  (mov eax,[esp+4]; mov ecx,eax; mov edx,[eax]) where ours loads ECX directly. Local-copy,
//  inline-helper, and cast forms all fold to ours. 18 bytes total; park.]
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
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403610
// File>Replay Story: permanently grayed in the demo (the handler is still linked).
void CDeskcppDoc::OnUpdateReplayStory(CCmdUI *pCmdUI)
{
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
        answer = AfxMessageBox(0xe009, 4, 0);
    if (answer == 6) {
        if (nCurrentGoalItem > 0) {
            nRequestedGoalItem = nCurrentGoalItem;
        }
        else {
            switch (currentPlanet) {
            case 1: {
                LoadStoryHistoryNevada();
                int n = storyHistoryNevada.GetSize();
                if (n > 0) {
                    nRequestedGoalItem = storyHistoryNevada[n - 1];
                }
                else {
                    AfxMessageBox(0xe01c, 0, -1);
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
                    AfxMessageBox(0xe01c, 0, -1);
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
                    AfxMessageBox(0xe01c, 0, -1);
                    return;
                }
                break;
            }
            }
        }
        goalTileList.SetSize(0, -1);
        placedZoneIds.SetSize(0, -1);
        bWorldInvalid = 1;
        bStartingGame = 1;
        int ok = StartGame(Randomize(), 0);   // nested: the 0 is pushed before Randomize runs
        bStartingGame = 0;
        if (ok == 0) {
            nFrameMode = 0xc;
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
            v->ZoneTransitionStep(0x5d, (short)i);
            long c = clock();
            while (clock() < c + 100)
                ;
        }
        nFrameMode = 0;
        v->nTransitionStep = 0;
        v->SoundFlush();
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
        v->nTargetZoneId = 0x5d;
    int n = inventory.GetSize();
    for (int j = 0; j < n; j++) {
        CObject *p = inventory[j];
        if (p)
            delete p;
    }
    inventory.SetSize(0, -1);
    UpdateAllViews(0, 0, 0);
    int *pg = paZonePtrGrid;
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
        nFrameMode = 0xb;
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
    if (id >= 0 && id < zoneCount)
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
                        pCanvas->BlitFast(tileArray[t]->pixels, 0x20, 0x20, 0x20, destX, destY);
                    t = (short)((Zone *)currentZone)->GetTile(cx, cy, 1);
                    if (t >= 0) {
                        Tile *pt = tileArray[t];
                        if ((pt->flags & 1) != 0)
                            pCanvas->BlitMasked((char *)pt->pixels, 0x20, 0x20, destX, destY, 0);
                        else
                            pCanvas->BlitFast(pt->pixels, 0x20, 0x20, 0x20, destX, destY);
                    }
                    t = (short)((Zone *)currentZone)->GetTile(cx, cy, 2);
                    if (t >= 0) {
                        Tile *pt = tileArray[t];
                        if ((pt->flags & 1) != 0)
                            pCanvas->BlitMasked((char *)pt->pixels, 0x20, 0x20, destX, destY, 0);
                        else
                            pCanvas->BlitFast(pt->pixels, 0x20, 0x20, 0x20, destX, destY);
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
