// GameData — the second doc-TU source file (0x401ac0–0x4042b0): story-history registry
// persistence, world-map helpers, menu update handlers, asset accessors.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS (static MFC).
// v1 = nops + accessors + CCmdUI handlers. TODO v2: LoadStoryHistory*/SaveStoryHistory* (registry
// sextet), RemoveEmptyZonesFromPlacedList, PlaceZoneObjectTiles, Save/LoadZoneRecursive,
// OnReplayStory, StartGame, RefreshZone, BuildQuestPath.
#include "WorldStub.h"
#include <time.h>

// Demo-limiting helper: the three permanently-grayed menu items (Save/Load/Replay) share an
// inlined disable call — the EAX staging of the pointer arg is the inlining fingerprint.
static __inline void DemoDisable(CCmdUI *p)
{
    p->Enable(0);
}

// FUNCTION: YODA 0x00402660
void World::Nop1()
{
}

// FUNCTION: YODA 0x00403060
void World::Nop2()
{
}

// FUNCTION: YODA 0x00403070  [EFFECTIVE MATCH: DIFF(24) — i/n register 2-cycle (ebx/edi) in both
//   loops + funclet-window skew (len 206 incl. our EH stubs vs Ghidra body 188). Probes inert.]
// Rebuild placedZoneIds in place, dropping zones whose type is Empty(1).
void World::RemoveEmptyZonesFromPlacedList()
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

// FUNCTION: YODA 0x00403140
// Stamp a zone's visible objects into tile layer 1. Types 0/1/2/5/6/7/8 place their tile if
// active and the cell is empty; type 0xb forces tile 0x1cb.
void World::PlaceZoneObjectTiles(short zoneId)
{
    if (zoneId >= 0) {
        ZoneStub *z = zoneObjects[zoneId];
        if (z != 0) {
            int n = z->objects.GetSize();
            for (int i = 0; i < n; i++) {
                ZoneObjStub *o = (ZoneObjStub *)z->objects[i];
                switch (o->type) {
                case 0:
                case 1:
                case 2:
                case 5:
                case 6:
                case 7:
                case 8:
                    if (o->state == 1 && o->visible >= 0) {
                        int t = (short)z->GetTile(o->x, o->y, 1);
                        if (t < 0)
                            z->SetTile(o->x, o->y, 1, o->visible);
                    }
                    break;
                case 0xb:
                    if (o->state == 1) {
                        o->visible = 0x1cb;
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

// FUNCTION: YODA 0x00403250  [EFFECTIVE MATCH: DIFF(16) — id/x-counter register 2-cycle
//   (orig id=DX,x=ESI; ours swapped). Decl hoisting/order inert. Allocator tie-break.]
// Locate the world-map cell holding zone `id`; outputs grid coords.
int World::FindZoneCellById(short id, int *pX, int *pY)
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
unsigned char World::GetExitDirections()
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
void World::SaveZoneRecursive(CFile *f, short zoneId, int bFull)
{
    ZoneStub *z = zoneObjects[zoneId];
    f->Write(&zoneId, 2);
    int full = bFull;
    f->Write(&full, 4);
    z->WriteSavedState(f, full);
    int n = z->objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObjStub *o = (ZoneObjStub *)z->objects[i];
        if (o->type == 9 && o->visible >= 0)
            SaveZoneRecursive(f, o->visible, full);
    }
}

// FUNCTION: YODA 0x00403450  [EFFECTIVE MATCH: DIFF(6) at exact length — residual register roles;
//   the child-local (o->visible cached across the Reads) was the structural crack, cf. HitEntityAt.]
// .wld load mirror: read + verify each door child id before recursing.
void World::LoadZoneRecursive(CFile *f, short zoneId, int bFull)
{
    short savedId;
    int savedFull;
    ZoneStub *z = zoneObjects[zoneId];
    z->ReadSavedState(f, bFull);
    int n = z->objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObjStub *o = (ZoneObjStub *)z->objects[i];
        short child = o->visible;      // cached in a callee-saved reg across the Read calls
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
void World::OnUpdateFileSave(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403520
void World::OnUpdateAppExit(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6 && nFrameMode != 5)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403550
void World::OnUpdateHideMe(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403580
void World::OnUpdateNewWorld(CCmdUI *pCmdUI)
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
void World::OnUpdateLoadWorld(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403610
// File>Replay Story: permanently grayed in the demo (the handler is still linked).
void World::OnUpdateReplayStory(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403620
// File>Replay Story: confirm if a game is in progress, pick the story to replay (current goal or
// the planet history's most recent), then rebuild the world around it with a fresh seed.
void World::OnReplayStory()
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
        bWorldInvalid = 0;
        return;
    }
    nRequestedGoalItem = -1;
    nFrameMode = savedMode;
}

// FUNCTION: YODA 0x004037a0  [WIP: DIFF(254) at exact length 666 — structure ~97% (172/178 insns
//   aligned); residuals: grid-store increment placement ([edx-0x34] pattern), gen-loop layout,
//   early-out jcc direction. Probes: c-pointer grid (+22B worse), decl swaps inert.]
// Begin a game session: reset player state, walk-in animation, camera/inventory reset, clear both
// map grids, load assets, then (unless restoring a save) generate + populate the world.
int World::StartGame(unsigned int nSeed, int bSkipGenerate)
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
    GameViewStub *v = (GameViewStub *)GetNextView(pos);
    if (v != 0) {
        for (int i = 0; i < 5; i++) {
            v->OnWalk(0x5d, (short)i);
            long c = clock();
            while (clock() < c + 100)
                ;
        }
        nFrameMode = 0;
        v->unk118 = 0;
        v->SoundFlush();
        v->PlayerMove(0x3a);
    }
    FindSpecialZoneMaybe();
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
            mz->field2c = -1;
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
            mz[100].field2c = -1;
            mz++;
        }
    }
    if (LoadWorldMaybe() == 0)
        return 0;
    unk2e34 = 0;
    if (bSkipGenerate == 0) {
        int ok = 0;
        unsigned int seed = nSeed;
        do {
            if (Generate(seed) == 0)
                seed = Randomize();
            else
                ok = ok + 1;
        } while (ok == 0);
        BackupZoneGrid();
        Populate();
    }
    v->bBusy = 0;
    if (bSkipGenerate == 0)
        nFrameMode = 0xb;
    unk3378 = 0;
    unk2e60 = 0;
    return 1;
}

// FUNCTION: YODA 0x00403a40
Tile *World::GetTileData(int idx)
{
    if (idx >= 0 && idx < tileCount)
        return tileArray[idx];
    return 0;
}

// FUNCTION: YODA 0x00403a70
ZoneStub *World::GetZoneById(short id)
{
    if (id >= 0 && id < zoneCount)
        return zoneObjects[id];
    return 0;
}

// FUNCTION: YODA 0x00403aa0  [EFFECTIVE MATCH: DIFF(4) — orig reuses dead ECX (this) for the
//   walk pointer; ours colors it EDX. cmp-flip inert. Allocator tie-break.]
int World::FindTile(void *pTile)
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
void World::RefreshZone()
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
                    int t = (short)((ZoneStub *)currentZone)->GetTile(cx, cy, 0);
                    if (t >= 0)
                        pCanvas->BlitFast(tileArray[t]->pixels, 0x20, 0x20, 0x20, destX, destY);
                    t = (short)((ZoneStub *)currentZone)->GetTile(cx, cy, 1);
                    if (t >= 0) {
                        Tile *pt = tileArray[t];
                        if ((pt->flags & 1) != 0)
                            pCanvas->BlitMasked((char *)pt->pixels, 0x20, 0x20, destX, destY, 0);
                        else
                            pCanvas->BlitFast(pt->pixels, 0x20, 0x20, 0x20, destX, destY);
                    }
                    t = (short)((ZoneStub *)currentZone)->GetTile(cx, cy, 2);
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
