// Worldgen TU (0x41c340–0x429000): worldgen + .wld save/load + .dta load (doc class source file).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include <stdlib.h>
#include <time.h>
#include "Worldgen.h"

// FUNCTION: YODA 0x0041c3b0
// Recursive: does zoneId (or a DOOR_IN-linked child zone) list itemId in genCandidateB (IZX3)?
// itemId == -1 means "any": the list just has to be non-empty.
int World::ZoneProvidesItem(short zoneId, short itemId)
{
    int found = 0;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    int nCount = pZone->genCandidateB.GetSize();
    if (itemId == -1)
    {
        found = 1;
        if (nCount <= 0)
            return 0;
    }
    else
    {
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                if ((short)pZone->genCandidateB.GetAt(i) == itemId)
                {
                    found = 1;
                    break;
                }
                i++;
            } while (nCount > i);
        }
        if (found == 0)
        {
            int nObjs = pZone->objects.GetSize();
            int j = 0;
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj != NULL && pObj->type == 9)
                    {
                        if (pObj->visible >= 0)
                            found = ZoneProvidesItem(pObj->visible, itemId);
                        if (found == 1)
                            return found;
                    }
                    j++;
                } while (j < nObjs);
            }
        }
    }
    return found;
}

// FUNCTION: YODA 0x0041c490
// Recursive lookup of itemId in cobArray4 (sel==0) or cobArray5 (sel!=0); returns the stored
// value or -1, descending DOOR_IN child zones.
int World::ZoneFindInIzxList(short zoneId, short itemId, int sel)
{
    short v;
    int result = -1;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    if (sel != 0)
    {
        int nCount = pZone->cobArray5.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                v = (short)pZone->cobArray5.GetAt(i);
                if (itemId == v)
                {
                    result = v;
                    break;
                }
                i++;
            } while (i < nCount);
        }
    }
    else
    {
        int nCount = pZone->cobArray4.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                v = (short)pZone->cobArray4.GetAt(i);
                if (itemId == v)
                {
                    result = v;
                    break;
                }
                i++;
            } while (i < nCount);
        }
    }
    if (result < 0)
    {
        int nObjs = pZone->objects.GetSize();
        int j = 0;
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == 9)
                {
                    if (pObj->visible >= 0)
                        result = ZoneFindInIzxList(pObj->visible, itemId, sel);
                    if (result >= 0)
                        break;
                }
                j++;
            } while (nObjs > j);
        }
    }
    return result;
}

// FUNCTION: YODA 0x0041d670
// Global item de-dup: is itemId already recorded in the worldgen ref-zone list?
int World::IsItemPlaced(short itemId)
{
    int nCount = worldgenRefZones.GetSize();
    if (nCount > 0)
    {
        int i = 0;
        do
        {
            WorldgenZoneEntry *pEntry = (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            if (pEntry->zoneId == itemId)
                return 1;
            i++;
        } while (i < nCount);
    }
    return 0;
}

// FUNCTION: YODA 0x0041d6b0
// Push-front onto the pending-zone worklist.
void World::WorldgenPushZoneEntry(short zoneId, short val)
{
    WorldgenZoneEntry *pEntry = new WorldgenZoneEntry(zoneId, val);
    if (pEntry != NULL)
        worldgenPendingZones.InsertAt(0, pEntry, 1);
}

// FUNCTION: YODA 0x0041d740
// Linear-search the pending-zone worklist by zoneId; RemoveAt + delete the entry.
void World::RemoveZoneEntry(short zoneId)
{
    int nCount = worldgenPendingZones.GetSize();
    int i = 0;
    if (nCount > 0)
    {
        do
        {
            WorldgenZoneEntry *pEntry = (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            if (pEntry->zoneId == zoneId)
            {
                worldgenPendingZones.RemoveAt(i, 1);
                delete pEntry;
                return;
            }
            i++;
        } while (i < nCount);
    }
}

// FUNCTION: YODA 0x0041d7a0
// Same as RemoveZoneEntry but on the ref-zone dedup set.
void World::RemoveZoneEntry2(short zoneId)
{
    int nCount = worldgenRefZones.GetSize();
    int i = 0;
    if (nCount > 0)
    {
        do
        {
            WorldgenZoneEntry *pEntry = (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            if (pEntry->zoneId == zoneId)
            {
                worldgenRefZones.RemoveAt(i, 1);
                delete pEntry;
                return;
            }
            i++;
        } while (i < nCount);
    }
}

// FUNCTION: YODA 0x0041d800
// Dedup-append to the ref-zone set: scan for zoneId, append a new entry only if absent.
void World::WorldgenAddZoneEntry(short zoneId, short val)
{
    BOOL bFound = FALSE;
    int nCount = worldgenRefZones.GetSize();
    int i = 0;
    if (nCount > 0)
    {
        do
        {
            if (((WorldgenZoneEntry *)worldgenRefZones.GetAt(i))->zoneId == zoneId)
            {
                bFound = TRUE;
                break;
            }
            i++;
        } while (i < nCount);
    }
    if (!bFound)
    {
        WorldgenZoneEntry *pEntry = new WorldgenZoneEntry(zoneId, val);
        if (pEntry != NULL)
            worldgenRefZones.SetAtGrow(worldgenRefZones.GetSize(), pEntry);
    }
}

// FUNCTION: YODA 0x0041d8d0
// Zone unavailable/already placed: no such zone, or its id is in placedZoneIds.
int World::IsZoneUsed(short zoneId)
{
    if (zones.GetAt(zoneId) == NULL)
        return 1;
    int nCount = placedZoneIds.GetSize();
    int result = 0;
    for (int i = 0; i < nCount; i++)
    {
        if ((short)placedZoneIds.GetAt(i) == zoneId)
        {
            result = 1;
            break;
        }
    }
    return result;
}

// FUNCTION: YODA 0x0041d920
// Record a zone id as placed this generation.
void World::AddPlacedZoneId(short zoneId)
{
    placedZoneIds.SetAtGrow(placedZoneIds.GetSize(), zoneId);
}

// FUNCTION: YODA 0x0041e920
// Collect the zone's IZX2/IZX3 item ids not already placed, random-pick one; if none, recurse
// into DOOR_IN child zones.
int World::WorldgenPickItemFromZone(short zoneId, short a2, int sel)
{
    int result = -1;
    unsigned int n = 0;
    if (a2 == 0)   // sic: n is dead until reused below (kept by the original compiler)
        n = 1;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    CWordArray candidates;
    if (sel != 0)
    {
        short nItems = (short)pZone->cobArray5.GetSize();
        if (nItems > 0)
        {
            for (int i = 0; i < nItems; i++)
            {
                int v = pZone->cobArray5.GetAt(i);
                if (IsItemPlaced(v) == 0)
                {
                    // sic: identical arms — the original tests the (dead) a2 flag and does the
                    // same append either way; the compiler cross-jumps, leaving a dead cmp.
                    if (n == 0)
                        candidates.SetAtGrow(candidates.GetSize(), v);
                    else
                        candidates.SetAtGrow(candidates.GetSize(), v);
                }
            }
        }
    }
    else
    {
        short nItems = (short)pZone->cobArray4.GetSize();
        if (nItems > 0)
        {
            for (int i = 0; i < nItems; i++)
            {
                int v = pZone->cobArray4.GetAt(i);
                if (IsItemPlaced(v) == 0)
                {
                    if (n == 0)
                        candidates.SetAtGrow(candidates.GetSize(), v);
                    else
                        candidates.SetAtGrow(candidates.GetSize(), v);
                }
            }
        }
    }
    int nCand = candidates.GetSize();
    if (nCand > 0)
        result = candidates.GetAt(rand() % nCand);
    if (result < 0)
    {
        n = pZone->objects.GetSize();
        int j = 0;
        if ((int)n > 0)
        {
            for (; j < (int)n; j++)
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj->type == 9)
                {
                    result = WorldgenPickItemFromZone(pObj->visible, a2, sel);
                    if (result >= 0)
                        break;
                }
            }
        }
    }
    return result;
}

// FUNCTION: YODA 0x0041ef90
// Fisher-Yates-style shuffle of a CWordArray: scatter each element into a random empty slot of a
// temp array (0xffff = empty sentinel), then copy back.
void World::WorldgenShuffleList(CWordArray *pList)
{
    short nSize = (short)pList->GetSize();
    short i = 0;
    if (nSize > 0)
    {
        CWordArray temp;
        int nInt = nSize;
        temp.SetSize(nInt, -1);
        while (i < nSize)
        {
            temp.SetAt(i, 0xffff);
            i++;
        }
        i = 0;
        short k;
        if (nSize > 0)
        {
            do
            {
                int r = rand();
                short slot = (short)(r % nInt);
                if (temp.GetAt(slot) == 0xffff)
                {
                    temp.SetAt(slot, pList->GetAt(i));
                    pList->SetAt(i, 0xffff);
                }
                    i++;
            } while (nSize > i);
        }
        k = nSize - 1;
        if (k >= 0)
        {
            do
            {
            if (pList->GetAt(k) != -1)   // sic: WORD zero-extends, never == -1 (engine bug)
            {
                short nMoved = 0;
                BOOL bAnyEmpty = FALSE;
                do
                {
                    if (nSize > 0)
                    {
                        unsigned short *pSlot = temp.GetData();
                        int m = nInt;
                        do
                        {
                            if (*pSlot == 0xffff)
                                bAnyEmpty = TRUE;
                            pSlot++;
                            m--;
                        } while (m != 0);
                    }
                    if (!bAnyEmpty)
                        break;
                    int r = rand();
                    short slot = (short)(r % nInt);
                    if (temp.GetAt(slot) == 0xffff)
                    {
                        nMoved++;
                        temp.SetAt(slot, pList->GetAt(k));
                        pList->SetAt(k, 0xffff);
                    }
                } while (nMoved == 0);
            }
            k--;
            } while (k >= 0);
        }
        for (i = 0; nSize > i; i++)
            pList->SetAt(i, temp.GetAt(i));
    }
}

// FUNCTION: YODA 0x0041f830
// Recursively verify a quest sub-tree is satisfiable: object types 6-8 must reference items not
// already placed; DOOR_IN (9) recurses into the child zone.
int World::CheckZoneItemsAvailable(short zoneId)
{
    int ok = 1;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    int nObjs = pZone->objects.GetSize();
    int i = 0;
    if (nObjs > 0)
    {
        do
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            switch (pObj->type)
            {
            case 6:
            case 7:
            case 8:
                if (pObj->visible >= 0 && IsItemPlaced(pObj->visible) != 0)
                    ok = 0;
                break;
            case 9:
                if (pObj->visible >= 0)
                    ok = CheckZoneItemsAvailable(pObj->visible);
                break;
            }
            if (ok == 0)
                break;
            i++;
        } while (nObjs > i);
    }
    return ok;
}

// FUNCTION: YODA 0x0041f8e0
// Recursively gather all zones a quest branch references into the ref-zone dedup set.
void World::WorldgenCollectZoneRefs(short zoneId)
{
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone != NULL)
    {
        int nObjs = pZone->objects.GetSize();
        if (nObjs > 0)
        {
            for (int i = 0; i < nObjs; i++)
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
                switch (pObj->type)
                {
                case 6:
                case 7:
                case 8:
                    if (pObj->visible >= 0)
                        WorldgenAddZoneEntry(pObj->visible, -1);
                    break;
                case 9:
                    if (pObj->visible >= 0)
                        WorldgenCollectZoneRefs(pObj->visible);
                    break;
                }
            }
        }
    }
}

// FUNCTION: YODA 0x00421460
// Snapshot the active 10x10 MapZone grid into the backup grid (sparse-save baseline).
void World::BackupZoneGrid()
{
    int n = 0;
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            mapGrid[n + 100].id = mapGrid[n].id;
            mapGrid[n + 100].cellQuestSlot0 = mapGrid[n].cellQuestSlot0;
            mapGrid[n + 100].cellQuestSlot1 = mapGrid[n].cellQuestSlot1;
            mapGrid[n + 100].zoneType = mapGrid[n].zoneType;
            mapGrid[n + 100].cellItemA = mapGrid[n].cellItemA;
            mapGrid[n + 100].cellItemB = mapGrid[n].cellItemB;
            mapGrid[n + 100].cellItemC = mapGrid[n].cellItemC;
            mapGrid[n + 100].cellQuestSlot5 = mapGrid[n].cellQuestSlot5;
            mapGrid[n + 100].cellQuestSlot6 = mapGrid[n].cellQuestSlot6;
            mapGrid[n + 100].flagSolved = mapGrid[n].flagSolved;
            mapGrid[n + 100].flagA = mapGrid[n].flagA;
            mapGrid[n + 100].flagB = mapGrid[n].flagB;
            mapGrid[n + 100].flagC = mapGrid[n].flagC;
            mapGrid[n + 100].flagD = mapGrid[n].flagD;
            mapGrid[n + 100].field30 = mapGrid[n].field30;
            n++;
        }
    }
}

// FUNCTION: YODA 0x00421520
// Restore the active 10x10 MapZone grid from the backup (replay/reset + sparse-save baseline).
void World::RestoreGridFromBackup()
{
    int n = 0;
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            mapGrid[n].id = mapGrid[n + 100].id;
            mapGrid[n].cellQuestSlot0 = mapGrid[n + 100].cellQuestSlot0;
            mapGrid[n].cellQuestSlot1 = mapGrid[n + 100].cellQuestSlot1;
            mapGrid[n].zoneType = mapGrid[n + 100].zoneType;
            mapGrid[n].cellItemA = mapGrid[n + 100].cellItemA;
            mapGrid[n].cellItemB = mapGrid[n + 100].cellItemB;
            mapGrid[n].cellItemC = mapGrid[n + 100].cellItemC;
            mapGrid[n].cellQuestSlot5 = mapGrid[n + 100].cellQuestSlot5;
            mapGrid[n].cellQuestSlot6 = mapGrid[n + 100].cellQuestSlot6;
            mapGrid[n].flagSolved = mapGrid[n + 100].flagSolved;
            mapGrid[n].flagA = mapGrid[n + 100].flagA;
            mapGrid[n].flagB = mapGrid[n + 100].flagB;
            mapGrid[n].flagC = mapGrid[n + 100].flagC;
            mapGrid[n].flagD = mapGrid[n + 100].flagD;
            mapGrid[n].field30 = mapGrid[n + 100].field30;
            n++;
        }
    }
}

// FUNCTION: YODA 0x004215e0
// Contains-test on the goal tile list.
int World::IsTileInGoalList(unsigned int tileId)
{
    int result = 0;
    int i = 0;
    int nCount = goalTileList.GetSize();
    for (; i < nCount; i++)
    {
        if (goalTileList.GetAt(i) == tileId)
        {
            result = 1;
            break;
        }
    }
    return result;
}

// FUNCTION: YODA 0x00421e50
// Worldgen grid-cell traversal/placement priority (static 10x10 dword table). `this` is unused.
int World::GetZoneGridOrder(int x, int y)
{
    return gWorldgenGridOrderTable[x + y * 10];
}

// FUNCTION: YODA 0x00422f40
BOOL World::IsModified()
{
    return m_bModified;
}

// FUNCTION: YODA 0x00422f50
void World::SetModifiedFlag(BOOL bModified)
{
    m_bModified = bModified;
}

// FUNCTION: YODA 0x00423d20
// Find the INTRO zone (map_flags 9), make it current and refresh (StartGame).
void World::SetCurrentToIntroZone()
{
    int nCount = zones.GetSize();
    for (int i = 0; i < nCount; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone->type == 9)
        {
            currentZone = pZone;
            RefreshZone();
            return;
        }
    }
}

// FUNCTION: YODA 0x00423d60
// Read the STUP chunk: a 288x288 8-bit canvas snapshot streamed row-by-row (dest stride 0x240).
void World::ReadStupCanvas(CFile *pFile)
{
    if (pCanvas == NULL)
    {
        pFile->Seek(0x14400, CFile::current);
        return;
    }
    int nRows = 0x120;
    char *pRow = (char *)pCanvas->GetData();
    do
    {
        pFile->Read(pRow, 0x120);
        pRow += 0x240;
        nRows--;
    } while (nRows != 0);
}

// FUNCTION: YODA 0x00423dc0
// Index of a Zone* in the zone list, or -1. (Ghidra name: EnterZone.)
int World::GetZoneIndex(Zone *pZone)
{
    int i = 0;
    if (zones.GetSize() > 0)
    {
        do
        {
            if (zones.GetAt(i) == (CObject *)pZone)
                return i;
            i++;
        } while (i < zones.GetSize());
    }
    return -1;
}

// FUNCTION: YODA 0x00423f50
// Clamp the visible 288x288 window to the camera position (small zones pin to full window).
void World::UpdateCamera()
{
    if (currentZone->width == 9)
    {
        nViewRight = 0x120;
        nViewBottom = 0x120;
        nViewLeft = 0;
        nViewTop = 0;
        return;
    }
    if (cameraX <= 0x80)
        nViewLeft = 0;
    else if (cameraX > 0x1a0)
        nViewLeft = 0x120;
    else
        nViewLeft = cameraX - 0x80;
    if (cameraY <= 0x80)
        nViewTop = 0;
    else if (cameraY > 0x1a0)
        nViewTop = 0x120;
    else
        nViewTop = cameraY - 0x80;
    nViewRight = nViewLeft + 0x120;
    nViewBottom = nViewTop + 0x120;
}


// FUNCTION: YODA 0x004242a0
// Audio menu: toggle sound; opening the first sound session lazily.
void World::OnToggleSound()
{
    nSoundEnabled = (nSoundEnabled == 0);
    POSITION pos = GetFirstViewPosition();
    GameView *pView = (GameView *)GetNextView(pos);
    if (pView != NULL && nSoundEnabled != 0 && pView->soundSession == 0)
        pView->SoundInit();
}

// FUNCTION: YODA 0x004242f0
void World::OnUpdateToggleSound(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(nSoundEnabled);
}

// FUNCTION: YODA 0x00424310
// Audio menu: toggle music.
void World::OnToggleMusic()
{
    nMusicEnabled = (nMusicEnabled == 0);
    POSITION pos = GetFirstViewPosition();
    GameView *pView = (GameView *)GetNextView(pos);
    if (pView != NULL && nMusicEnabled != 0 && pView->soundSession == 0)
        pView->SoundInit();
}

// FUNCTION: YODA 0x00424360
void World::OnUpdateToggleMusic(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(nMusicEnabled);
}

// FUNCTION: YODA 0x00424380
// Seed the RNG from cursor position + wall clock, then pack rand() bytes into the world seed
// (one of 3 rotating byte layouts).
unsigned int World::Randomize()
{
    POINT pt;
    GetCursorPos(&pt);
    time_t t = time(NULL);
    clock_t c = clock();
    srand(c + t);
    unsigned int b0 = rand();
    unsigned int b1 = rand();
    unsigned int b2 = rand();
    unsigned int b3 = rand();
    switch (rand() % 3)
    {
    case 0:
        b1 <<= 8;
        b1 &= 0xff00;
        b2 <<= 0x10;
        b2 &= 0xff0000;
        b3 <<= 0x18;
        b3 &= 0xff000000;
        break;
    case 1:
        b2 <<= 8;
        b2 &= 0xff00;
        b3 <<= 0x10;
        b3 &= 0xff0000;
        b1 <<= 0x18;
        b1 &= 0xff000000;
        break;
    case 2:
        b3 <<= 8;
        b3 &= 0xff00;
        b1 <<= 0x10;
        b1 &= 0xff0000;
        b2 <<= 0x18;
        b2 &= 0xff000000;
        break;
    }
    return b3 | b2 | b1 | b0;
}
