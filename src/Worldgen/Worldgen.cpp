// Worldgen TU (0x41c340–0x429000): worldgen + .wld save/load + .dta load (doc class source file).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
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
