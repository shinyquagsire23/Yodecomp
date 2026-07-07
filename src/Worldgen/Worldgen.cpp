// Worldgen TU (0x41c340–0x429000): worldgen + .wld save/load + .dta load (doc class source file).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Worldgen.h"

// FUNCTION: YODA 0x0041bfa0
// [EFFECTIVE: align=12 — clean ESI/EDI rename + one arg-marshal slot at the recursion call
// (orig loads visible into CX and sel into EAX; ours AX/ECX). Twin 0x41c0b0 scores align=0
// with the identical source shape → TU-phase tie-break, joint pass.]
// Recursive: does zoneId (or a DOOR_IN-linked child zone) list itemId in cobArray4 (sel==0)
// or cobArray5? Boolean twin of ZoneFindInIzxList.
int World::ZoneHasIzxItemMaybe(short zoneId, short itemId, int sel)
{
    int found = 0;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    if (sel != 0)
    {
        int nCount = pZone->cobArray5.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                if ((short)pZone->cobArray5.GetAt(i) == itemId)
                {
                    found = 1;
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
                if ((short)pZone->cobArray4.GetAt(i) == itemId)
                {
                    found = 1;
                    break;
                }
                i++;
            } while (i < nCount);
        }
    }
    int j = 0;
    if (found == 0)
    {
        int nObjs = pZone->objects.GetSize();
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
                {
                    if (pObj->visible >= 0)
                        found = ZoneHasIzxItemMaybe(pObj->visible, itemId, sel);
                    if (found == 1)
                        break;
                }
                j++;
            } while (nObjs > j);
        }
    }
    return found;
}

// FUNCTION: YODA 0x0041c0b0
// [EFFECTIVE: align=0, pure reg tie-break (8-reg bijection slots) — may flip exact as the
// TU fills; do not grind.]
// Recursive: does zoneId (or a DOOR_IN-linked child zone) list itemId in genCandidateA (IZAX)?
int World::ZoneRequiresItemMaybe(short zoneId, short itemId)
{
    int found = 0;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    int nCount = pZone->genCandidateA.GetSize();
    int i = 0;
    if (nCount > 0)
    {
        do
        {
            if ((short)pZone->genCandidateA.GetAt(i) == itemId)
            {
                found = 1;
                break;
            }
            i++;
        } while (i < nCount);
    }
    int j = 0;
    if (found == 0)
    {
        int nObjs = pZone->objects.GetSize();
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
                {
                    if (pObj->visible >= 0)
                        found = ZoneRequiresItemMaybe(pObj->visible, itemId);
                    if (found == 1)
                        break;
                }
                j++;
            } while (nObjs > j);
        }
    }
    return found;
}

// FUNCTION: YODA 0x0041c200
// [EFFECTIVE: align=12 — one mov/xor scheduling transposition at the recursion-loop head +
// ESI/EDI/EBX 3-cycle role rotation. Structure proven: `int v` (hoisted xor zero-extend),
// `nAvail <= 0` (TEST/JG), j declared inside the guard. Tie-break family, joint pass.]
// Pick a random item from zoneId's genCandidateB (IZX3) that IsItemPlaced hasn't seen yet;
// falls back to DOOR_IN-linked child zones. Returns the item id or -1.
int World::PickUnplacedItemMaybe(short zoneId)
{
    int nResult = -1;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    int i = 0;
    int nCount = pZone->genCandidateB.GetSize();
    if (nCount > 0)
    {
        CWordArray paItems;
        if (nCount > 0)
        {
            do
            {
                int v = pZone->genCandidateB.GetAt(i);
                if (IsItemPlaced(v) == 0)
                    paItems.SetAtGrow(paItems.GetSize(), v);
                i++;
                nCount--;
            } while (nCount != 0);
        }
        int nAvail = paItems.GetSize();
        if (nAvail <= 0)
            nResult = -1;
        else
            nResult = paItems.GetAt(rand() % nAvail);
    }
    if (nResult < 0)
    {
        int j = 0;
        int nObjs = pZone->objects.GetSize();
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
                {
                    if (pObj->visible >= 0)
                        nResult = PickUnplacedItemMaybe(pObj->visible);
                    if (nResult >= 0)
                        break;
                }
                j++;
            } while (nObjs > j);
        }
    }
    return nResult;
}

// FUNCTION: YODA 0x0041c3b0
// [PARKED reg-alloc: original keeps `found` in EDI and spills the objects-loop index; ours
// allocates the reverse. Structure/insn-count converged; joint TU pass territory.]
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
                    if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
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
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
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

// FUNCTION: YODA 0x0041c580
// [EFFECTIVE: 20B — j/count reg-role swap (EBX/EDI) in the spots loop + three backedge
// cmp-operand mirrors + one layout jmp; insns 129/128. Tie-break family, joint pass.]
// Place a required item into a random OBJ_QUEST_ITEM_SPOT (type 0) of the zone whose
// genCandidateA (IZAX) list names itemId; marks the spot visible=itemId/state=1. Falls back
// to recursing into DOOR_IN-linked child zones. Returns the placed item id or -1.
int World::WorldgenFillQuestItemSpot(short zoneId, short itemId)
{
    int nResult = -1;
    CWordArray paSpots;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    int i = 0;
    int nCount = pZone->genCandidateA.GetSize();
    if (nCount > 0)
    {
        do
        {
            short v = (short)pZone->genCandidateA.GetAt(i);
            if (itemId == v)
            {
                int nObjs = pZone->objects.GetSize();
                int j = 0;
                paSpots.SetSize(0, -1);
                if (nObjs > 0)
                {
                    do
                    {
                        if (((ZoneObj *)pZone->objects.GetAt(j))->type == OBJ_QUEST_ITEM_SPOT)
                            paSpots.SetAtGrow(paSpots.GetSize(), (unsigned short)j);
                        j++;
                    } while (j < nObjs);
                }
                if (paSpots.GetSize() > 0)
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(
                        paSpots.GetAt(rand() % paSpots.GetSize()));
                    if (pObj != NULL && pObj->type == OBJ_QUEST_ITEM_SPOT)
                    {
                        nResult = v;
                        pObj->visible = v;
                        pObj->state = 1;
                    }
                }
                if (nResult >= 0)
                    break;
            }
            i++;
        } while (i < nCount);
    }
    if (nResult == -1)
    {
        int nObjs = pZone->objects.GetSize();
        int j = 0;
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
                {
                    if (pObj->visible >= 0)
                        nResult = WorldgenFillQuestItemSpot(pObj->visible, itemId);
                    if (nResult >= 0)
                        break;
                }
                j++;
            } while (j < nObjs);
        }
    }
    return nResult;
}

// FUNCTION: YODA 0x0041c730
// Clone of WorldgenFillQuestItemSpot for OBJ_SPAWN (type 1) via genCandidateB (IZX3);
// also records the placed id in genCellQuestSlot6Scratch.
int World::WorldgenFillSpawn(short zoneId, short itemId)
{
    int nResult = -1;
    CWordArray paSpots;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    int i = 0;
    int nCount = pZone->genCandidateB.GetSize();
    if (nCount > 0)
    {
        do
        {
            short v = (short)pZone->genCandidateB.GetAt(i);
            if (itemId == v)
            {
                int nObjs = pZone->objects.GetSize();
                int j = 0;
                paSpots.SetSize(0, -1);
                if (nObjs > 0)
                {
                    do
                    {
                        if (((ZoneObj *)pZone->objects.GetAt(j))->type == OBJ_SPAWN)
                            paSpots.SetAtGrow(paSpots.GetSize(), (unsigned short)j);
                        j++;
                    } while (j < nObjs);
                }
                if (paSpots.GetSize() > 0)
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(
                        paSpots.GetAt(rand() % paSpots.GetSize()));
                    if (pObj != NULL && pObj->type == OBJ_SPAWN)
                    {
                        nResult = v;
                        pObj->visible = v;
                        pObj->state = 1;
                        genCellQuestSlot6Scratch = nResult;
                    }
                }
                if (nResult >= 0)
                    break;
            }
            i++;
        } while (i < nCount);
    }
    if (nResult == -1)
    {
        int nObjs = pZone->objects.GetSize();
        int j = 0;
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj != NULL && pObj->type == OBJ_DOOR_IN)
                {
                    if (pObj->visible >= 0)
                        nResult = WorldgenFillSpawn(pObj->visible, itemId);
                    if (nResult >= 0)
                        break;
                }
                j++;
            } while (j < nObjs);
        }
    }
    return nResult;
}

// FUNCTION: YODA 0x0041c8f0
// [EFFECTIVE-WIP: align=92, insns 254/254, all call/store/branch structure converged. Residual
// = ONE scheduling family: orig evaluates {iB movsx, qB[iB]} before {qA[iA+1], pPuzA2 deref},
// ours the reverse — statement-order swap and an `int nB` mid-decl both proven INERT (identical
// score, canonicalized back), so the interleave is scheduler-internal; plus the this=ESI-vs-EDI
// callee-save cascade (reg_pen=50). Both phase-coupled — joint TU pass. `int nA/nB` locals
// removed WAS load-bearing (108→92): params CSE-spill on their own.]
// Populate the FINAL-ITEM goal zone (Zone.type==10): the goal cell consumes puzzle-pair items
// from BOTH quest chains — questItemsA[iA]/[iA+1] and questItemsB[iB] give the three puzzles
// whose items must meet here. Two paths: if the zone still provides an unplaced IZX3 item,
// spawn it; otherwise wire the A/B chain items into quest-item spots. Returns 1 on success.
int World::WorldgenPopulateGoalZone(short zoneId, short iA, short iB, short nOrder, int a5)
{
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    if (pZone->type != ZONE_TYPE_FINAL_ITEM)
        return 0;
    short itemA1 = ((Puzzle *)puzzles.GetAt((short)questItemsA.GetAt(iA)))->itemA;
    Puzzle *pPuzB = (Puzzle *)puzzles.GetAt((short)questItemsB.GetAt(iB));
    Puzzle *pPuzA2 = (Puzzle *)puzzles.GetAt((short)questItemsA.GetAt(iA + 1));
    short itemB1 = pPuzB->itemA;
    short itemA2a = pPuzA2->itemA;
    short itemA2b = pPuzA2->itemB;
    if (CheckZoneItemsAvailable(zoneId) == 0)
        return 0;
    int bProvides = 0;
    int nPick = PickUnplacedItemMaybe(zoneId);
    if (nPick >= 0)
        bProvides = ZoneProvidesItem(zoneId, nPick);
    int b1 = ZoneHasIzxItemMaybe(zoneId, itemA1, 0);
    ZoneHasIzxItemMaybe(zoneId, itemB1, 1);          // sic: result discarded
    int b2 = ZoneRequiresItemMaybe(zoneId, itemA2a);
    ZoneRequiresItemMaybe(zoneId, itemA2b);          // sic: result discarded
    if (b1 == 0 || b2 == 0)
        return 0;
    if (bProvides != 0)
    {
        if (WorldgenFillSpawn(zoneId, nPick) >= 0)
        {
            genCellQuestSlot6Scratch = nPick;
            genCellItemCScratch = itemA2a;
            genCellQuestSlot5Scratch = itemA2b;
            genCellItemAScratch = itemA1;
            genCellQuestSlot0Scratch = iA;
            genCellQuestSlot1Scratch = iB;
            genCellItemBScratch = itemB1;
            WorldgenCollectZoneRefs(zoneId);
            return 1;
        }
        return 0;
    }
    int r1 = ZoneFindInIzxList(zoneId, itemA1, 0);
    int r2 = WorldgenFillQuestItemSpot(zoneId, itemA2a);
    if (r1 >= 0 && r2 >= 0)
    {
        WorldgenAddZoneEntry(itemA1, nOrder);
        WorldgenAddZoneEntry(itemA2a, nOrder);
    }
    int r3 = ZoneFindInIzxList(zoneId, itemB1, 1);
    int r4 = WorldgenFillQuestItemSpot(zoneId, itemA2b);
    if (r3 >= 0 && r4 >= 0)
    {
        WorldgenAddZoneEntry(itemB1, nOrder);
        WorldgenAddZoneEntry(itemA2b, nOrder);
    }
    if (r1 >= 0 && r2 >= 0 && r3 >= 0 && r4 >= 0)
    {
        genCellQuestSlot6Scratch = -1;
        genCellItemCScratch = itemA2a;
        genCellItemAScratch = itemA1;
        genCellQuestSlot0Scratch = iA;
        genCellQuestSlot5Scratch = itemA2b;
        genCellItemBScratch = itemB1;
        genCellQuestSlot1Scratch = iB;
        WorldgenCollectZoneRefs(zoneId);
        return 1;
    }
    RemoveZoneEntry2(itemA1);
    RemoveZoneEntry2(itemA2a);
    RemoveZoneEntry2(itemB1);
    RemoveZoneEntry2(itemA2b);
    return 0;
}

// FUNCTION: YODA 0x0041cbe0
// [PHASE-DISPLACED: byte-EXACT at the 57-marker dial state (verified 2026-07-06); adding the
// UsefulObject/AssignTransit decls rotated it out. Source proven -- do not touch. Both
// `if (sel)` dispatches are written `!= 0`-first (A-arm fall-through): that was the last crack.]
// Build one USEFUL-DROP chain node (Zone.type==0x10): resolve the quest item (and the next
// chain item, if any) from the questItemsA/B list picked by sel, spawn a fresh IZX3 item,
// record the scratch block + AddZoneEntry. Returns 1 on success.
int World::WorldgenPlaceUsefulDropChainMaybe(short zoneId, short idx, short nOrder, short sel)
{
    if (zoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    if (pZone->type != ZONE_TYPE_FIND_USEFUL_DROP)
        return 0;
    if (CheckZoneItemsAvailable(zoneId) == 0)
        return 0;
    unsigned short item1;
    if (sel != 0)
        item1 = questItemsA.GetAt(idx);
    else
        item1 = questItemsB.GetAt(idx);
    unsigned short next;
    if (sel != 0)
    {
        if (idx < questItemsA.GetSize() - 1)
            next = questItemsA.GetAt(idx + 1);
        else
            next = 0xffff;
    }
    else
    {
        if (idx < questItemsB.GetSize() - 1)
            next = questItemsB.GetAt(idx + 1);
        else
            next = 0xffff;
    }
    Puzzle *pPuz = (Puzzle *)puzzles.GetAt((short)item1);
    if (pPuz == NULL)
        return 0;
    Puzzle *pPuz2;
    if ((short)next >= 0)
    {
        pPuz2 = (Puzzle *)puzzles.GetAt((short)next);
        if (pPuz2 == NULL)
            return 0;
    }
    short item1a = pPuz->itemA;
    short nextItem = -1;
    if ((short)next >= 0)
        nextItem = pPuz2->itemA;
    int nPick = PickUnplacedItemMaybe(zoneId);
    if (nPick < 0)
        return 0;
    int ok2 = 1;
    int ok1 = ZoneHasIzxItemMaybe(zoneId, item1a, 0);
    if (nextItem >= 0)
        ok2 = ZoneRequiresItemMaybe(zoneId, nextItem);
    if (ok1 != 0 && ok2 != 0)
    {
        if (WorldgenFillSpawn(zoneId, nPick) >= 0)
        {
            genCellQuestSlot6Scratch = nPick;
            genCellItemCScratch = nextItem;
            genCellItemAScratch = item1a;
            genCellQuestSlot0Scratch = idx;
            WorldgenAddZoneEntry(nPick, nOrder);
            WorldgenCollectZoneRefs(zoneId);
            return 1;
        }
        return 0;
    }
    return 0;
}

// FUNCTION: YODA 0x0041cdc0
// Place itemId onto the first OBJ_LOCK (type 0xc) of zoneId if the zone's cobArray4 (sel==0)
// or cobArray5 lists it; records genCellItemA/BScratch + WorldgenAddZoneEntry; recurses into
// DOOR_IN children. nResult lives in EAX end-to-end (0 / 1 / last recursion result).
int World::WorldgenPlaceItemOnLock(short zoneId, int a2, int nVal, short itemId, int sel)
{
    int nResult = 0;
    int bFound = 0;
    if (zoneId >= 0)
    {
        Zone *pZone = (Zone *)zones.GetAt(zoneId);
        if (sel != 0)
        {
            int nCount = pZone->cobArray5.GetSize();
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    if ((short)pZone->cobArray5.GetAt(i) == itemId)
                    {
                        bFound = 1;
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
                    if ((short)pZone->cobArray4.GetAt(i) == itemId)
                    {
                        bFound = 1;
                        break;
                    }
                    i++;
                } while (i < nCount);
            }
        }
        if (bFound)
        {
            int nObjs = pZone->objects.GetSize();
            int i = 0;
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
                    if (pObj->type == OBJ_LOCK)
                    {
                        WorldgenAddZoneEntry(itemId, (short)nVal);
                        if (sel == 0)
                            genCellItemAScratch = itemId;
                        else
                            genCellItemBScratch = itemId;
                        pObj->visible = itemId;
                        nResult = 1;
                        pObj->state = 1;
                        break;
                    }
                    i++;
                } while (i < nObjs);
            }
        }
        if (nResult == 0)
        {
            int nObjs = pZone->objects.GetSize();
            int j = 0;
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj->type == OBJ_DOOR_IN &&
                        (nResult = WorldgenPlaceItemOnLock(pObj->visible, a2, nVal, itemId, sel)) == 1)
                        break;
                    j++;
                } while (j < nObjs);
            }
        }
    }
    return nResult;
}

// FUNCTION: YODA 0x0041cf10
// Variant of FillQuestItemSpot: place itemId into a random OBJ_QUEST_ITEM_SPOT (type 0) of
// zoneId if its genCandidateA lists it; registers the item via WorldgenAddZoneEntry and
// genCellItemCScratch. Recurses into DOOR_IN children (no null/negative-id guard — sic).
// Returns 1 on success.
int World::WorldgenFillQuestItemSpot2Maybe(short zoneId, short a2, short nVal, unsigned short itemId)
{
    int bFound = 0;
    int bSpot = 0;
    int nResult = 0;
    if (zoneId < 0)
        return 0;
    {
        Zone *pZone = (Zone *)zones.GetAt(zoneId);
        int i = 0;
        int nCount = pZone->genCandidateA.GetSize();
        if (nCount > 0)
        {
            do
            {
                if (pZone->genCandidateA.GetAt(i) == itemId)
                {
                    bFound = 1;
                    break;
                }
                i++;
            } while (i < nCount);
        }
        if (bFound)
        {
            int nObjs = pZone->objects.GetSize();
            CWordArray paSpots;
            int j = 0;
            paSpots.SetSize(0, -1);
            if (nObjs > 0)
            {
                do
                {
                    if (((ZoneObj *)pZone->objects.GetAt(j))->type == OBJ_QUEST_ITEM_SPOT)
                    {
                        bSpot = 1;
                        paSpots.SetAtGrow(paSpots.GetSize(), (unsigned short)j);
                    }
                    j++;
                } while (j < nObjs);
            }
            if (bSpot)
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(
                    paSpots.GetAt(rand() % paSpots.GetSize()));
                WorldgenAddZoneEntry(itemId, nVal);
                genCellItemCScratch = (short)itemId;
                pObj->visible = itemId;
                pObj->state = 1;
                nResult = 1;
            }
        }
        if (nResult == 0)
        {
            int j = 0;
            int nObjs = pZone->objects.GetSize();
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj->type == OBJ_DOOR_IN &&
                        (nResult = WorldgenFillQuestItemSpot2Maybe(pObj->visible, a2, nVal, itemId)) == 1)
                        break;
                    j++;
                } while (j < nObjs);
            }
        }
    }
    return nResult;
}

// FUNCTION: YODA 0x0041d0c0
// [EFFECTIVE-WIP: align=56, insns 156/155 — all branches/calls/arms converged (`sel==0`-first
// arm order was load-bearing). Residual: nOk allocated to EBX in orig vs a stack slot here
// (the one extra insn = our spill; drives the sbb-into-eax + cmp-mem forms), entangled with
// the this=EDI-vs-ESI callee-save cascade. Tie-break family — joint pass.]
// Build a MAP-TO-ITEM-FOR-LOCK node (Zone.type==0xf, must have an EMPTY IZX3 list): resolve
// the lock item (and follow-up item) from the questItemsA/B chain, register both, place the
// lock item on the zone's OBJ_LOCK, then the follow-up into a quest-item spot.
// sic: on CheckZoneItemsAvailable failure it removes item1a TWICE and never removes item2
// (docs/engine-bugs.md #11); the two `>= 0` guards on zero-extended WORDs are always true.
int World::WorldgenPlaceItemForLockChainMaybe(short zoneId, short idx, int nOrder, short sel)
{
    short item1a = -1;
    short item2 = -1;
    if (zoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    if (pZone->type != ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK)
        return 0;
    if (pZone->genCandidateB.GetSize() > 0)
        return 0;
    int item1;
    if (sel == 0)
    {
        item1 = questItemsB.GetAt(idx);
        if (item1 >= 0)   // sic: always true (zero-extended WORD)
            item1a = ((Puzzle *)puzzles.GetAt(item1))->itemA;
        if (questItemsB.GetSize() - 1 >= idx)
        {
            int next = questItemsB.GetAt(idx + 1);
            if (next >= 0)   // sic: always true (zero-extended WORD)
                item2 = ((Puzzle *)puzzles.GetAt(next))->itemA;
        }
    }
    else
    {
        item1 = questItemsA.GetAt(idx);
        if (item1 >= 0)   // sic: always true (zero-extended WORD)
            item1a = ((Puzzle *)puzzles.GetAt(item1))->itemA;
        if (questItemsA.GetSize() - 1 >= idx)
        {
            int next = questItemsA.GetAt(idx + 1);
            if (next >= 0)   // sic: always true (zero-extended WORD)
                item2 = ((Puzzle *)puzzles.GetAt(next))->itemA;
        }
    }
    WorldgenAddZoneEntry(item1a, nOrder);
    WorldgenAddZoneEntry(item2, nOrder);
    if (CheckZoneItemsAvailable(zoneId) == 0)
    {
        RemoveZoneEntry2(item1a);
        RemoveZoneEntry2(item1a);   // sic: should be item2 (docs/engine-bugs.md #11)
        return 0;
    }
    int nOk = 0;
    if (WorldgenPlaceItemOnLock(zoneId, item1, nOrder, item1a, 0) != 0)
        nOk = WorldgenFillQuestItemSpot2Maybe(zoneId, item1, nOrder, item2) != 0;
    if (nOk != 0)
    {
        genCellQuestSlot0Scratch = idx;
        WorldgenCollectZoneRefs(zoneId);
    }
    return nOk;
}

// FUNCTION: YODA 0x0041d260
// [EFFECTIVE: align=80, insns 164/162 -- guard ||-chain into ONE shared `return 0` was the
// big crack (170->80). Residual: three backedge cmp-operand mirrors (orig cmp count,i;JG),
// zero-reg-vs-imm guard compares, and the return-0 epilogue not folding into the common
// tail (2 extra insns). Tie-break/layout family, joint pass.]
// Place itemId as a useful object: if the zone's genCandidateA (IZAX) lists it (and the zone
// has EMPTY cobArray4/genCandidateB lists), pick the target object type from the item tile's
// flags (TILE_WEAPON -> OBJ_THE_FORCE, TILE_LOCATOR -> OBJ_LOCATOR, TILE_ITEM ->
// OBJ_QUEST_ITEM_SPOT) and drop it on a random object of that type; else recurse into DOOR_IN
// children. Returns 1 on success (nPlaced).
int World::WorldgenPlaceUsefulObjectMaybe(short zoneId, short itemId, short nOrder)
{
    Zone *pZone;
    int bFound = 0;
    int nPlaced = 0;
    if (zoneId < 0 || CheckZoneItemsAvailable(zoneId) == 0 ||
        (pZone = (Zone *)zones.GetAt(zoneId)) == NULL ||
        pZone->cobArray4.GetSize() > 0 || pZone->genCandidateB.GetSize() > 0)
        return 0;
    int nCount = pZone->genCandidateA.GetSize();
    int i = 0;
    if (nCount > 0)
    {
        do
        {
            if ((short)pZone->genCandidateA.GetAt(i) == itemId)
            {
                bFound = 1;
                break;
            }
            i++;
        } while (nCount > i);
    }
    unsigned int objType = 0;
    if (bFound)
    {
        CWordArray paSpots;
        paSpots.SetSize(0, -1);
        int nTile = itemId;
        unsigned int flags = ((Tile *)tiles.GetAt(nTile))->flags;
        if (flags & TILE_WEAPON)
            objType = OBJ_THE_FORCE;
        else if (flags & TILE_LOCATOR)
            objType = OBJ_LOCATOR;
        else if (flags & TILE_ITEM)
            objType = OBJ_QUEST_ITEM_SPOT;   // sic: dead store, objType is already 0
        int nObjs = pZone->objects.GetSize();
        int j = 0;
        if (nObjs > 0)
        {
            do
            {
                if (((ZoneObj *)pZone->objects.GetAt(j))->type == objType)
                {
                    paSpots.SetAtGrow(paSpots.GetSize(), (unsigned short)j);
                    nPlaced = 1;
                }
                j++;
            } while (nObjs > j);
        }
        if (nPlaced == 1)
        {
            int nAvail = paSpots.GetSize();
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(
                paSpots.GetAt(rand() % nAvail));
            pObj->visible = itemId;
            pObj->state = 1;
            WorldgenAddZoneEntry(itemId, nOrder);
            genCellItemCScratch = nTile;
        }
    }
    else
    {
        int nObjs = pZone->objects.GetSize();
        int j = 0;
        if (nObjs > 0)
        {
            do
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                if (pObj->type == OBJ_DOOR_IN)
                {
                    int vis = pObj->visible;
                    if (vis >= 0)
                        nPlaced = WorldgenPlaceUsefulObjectMaybe(vis, itemId, nOrder);
                    if (nPlaced == 1)
                        break;
                }
                j++;
            } while (nObjs > j);
        }
    }
    if (nPlaced == 1)
        WorldgenCollectZoneRefs(zoneId);
    return nPlaced;
}

// FUNCTION: YODA 0x0041d480
// [EFFECTIVE: align=12 -- EARLY-RETURN restructure was the crack (176->12): the first
// `if (zoneId < 0) return 0;` emits the shared dtor+return-0 block as its FALL-THROUGH and
// every later `return 0` cross-jumps back to it (write guards as separate early returns,
// NOT nested ifs, in EH functions). Residual: bIn-zero placement + a reg double-swap in the
// uniqueRequiredItems dedup block (decl-order probe made it worse -- reverted). Joint pass.]
// Transit-zone item assignment (quest-path cases 2-7): pick a random not-yet-placed item from
// the zone's cobArray4 (sel==0) or cobArray5, push it onto the worklist (priority 5 for
// Zone.type==6, else nOrder) and register it; if the zone's IZAX lists exactly ONE required
// item, that item is deduped through uniqueRequiredItemsMaybe @0x234 — a repeat aborts with 0.
int World::WorldgenAssignTransitItemMaybe(short zoneId, short nOrder, int sel)
{
    CWordArray paItems;
    if (zoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    int nCount;
    if (sel != 0)
        nCount = pZone->cobArray5.GetSize();
    else
        nCount = pZone->cobArray4.GetSize();
    if (nCount == 0)
        return 0;
    if (CheckZoneItemsAvailable(zoneId) == 0)
        return 0;
    if (sel != 0)
    {
        if (nCount > 0)
        {
            int i = 0;
            do
            {
                unsigned short v = pZone->cobArray5.GetAt(i);
                if (IsItemPlaced(v) == 0)
                    paItems.SetAtGrow(paItems.GetSize(), v);
                i++;
                nCount--;
            } while (nCount != 0);
        }
    }
    else
    {
        if (nCount > 0)
        {
            int i = 0;
            do
            {
                unsigned short v = pZone->cobArray4.GetAt(i);
                if (IsItemPlaced(v) == 0)
                    paItems.SetAtGrow(paItems.GetSize(), v);
                i++;
                nCount--;
            } while (nCount != 0);
        }
    }
    if (paItems.GetSize() == 0)
        return 0;
    int nAvail = paItems.GetSize();
    int item = paItems.GetAt(rand() % nAvail);
    if (pZone->genCandidateA.GetSize() == 1)
    {
        unsigned short first = pZone->genCandidateA.GetAt(0);
        int bIn = 0;
        int nUsed = uniqueRequiredItemsMaybe.GetSize();
        int k = nUsed;
        if (k > 0)
        {
            int i = 0;
            do
            {
                if (uniqueRequiredItemsMaybe.GetAt(i) == first)
                    bIn = 1;
                i++;
                k--;
            } while (k != 0);
        }
        if (bIn != 0)
            return 0;
        uniqueRequiredItemsMaybe.SetAtGrow(nUsed, first);
    }
    if (pZone->type == ZONE_TYPE_FROM_ANOTHER_MAP)
        WorldgenPushZoneEntry(item, 5);
    else
        WorldgenPushZoneEntry(item, nOrder);
    WorldgenAddZoneEntry(item, nOrder);
    genCellItemAScratch = item;
    WorldgenCollectZoneRefs(zoneId);
    return 1;
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
                if (pObj->type == OBJ_DOOR_IN)
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
// [PARKED reg-alloc: the {bAnyEmpty, nMoved, k*2-offset} register contest + the unrotated
// init loop; structure fully converged (122/122 insns). Joint TU pass territory.]
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

// FUNCTION: YODA 0x00421620
// [EFFECTIVE: 39B, insns 255/255 exact, structure fully aligned. Residuals are pure
// reg-role tie-breaks: the entry movsx-vs-pRow-load scheduling (both statement orders
// probed; nMax-first is closer) and delete-loops 2/3 swap n/walker between ESI/EDI (loop 1
// matches; identical clone source, allocator enters each loop with different live state).
// STRUCTURE LESSONS (hard-won): (a) hoisting `int nIso = paIsolated.GetSize()` re-keyed the
// three arrays' frame slots to the original AND produced the mov+test / idiv-reg forms —
// array slot order follows USE COUNTS; (b) the pick is sequential-if + else, NOT an
// else-if chain, with the far arm ending in an explicit `goto cleanup` — that is the only
// shape where the far store cross-jumps into the B-arm's tail while the log arm falls
// through the (dead) re-tests; (c) the delete loops are `int i = 0; int n = GetSize();
// do { delete GetAt(i); i++; n--; } while (n != 0);` under a separate GetSize()>0 guard —
// this yields the DEC/JNE countdown the plain guard+do-while i<n form never produces.]
// Find-puzzle cell picker: scan the 10x10 plan grid; cells whose grid order <= nOrderMax and
// hold code 1/300 go to the isolated list (no 306/placed-puzzle neighbor) or the adjacent
// list; cells past the cutoff holding 1 go to the far list. Pick randomly (isolated first,
// then adjacent; far only when both are empty), return the cell in *pX/*pY.
int World::PlacePuzzle(short nOrderMax, short *paPlanGrid, int *pX, int *pY)
{
    CObArray paIsolated;
    CObArray paAdjacent;
    CObArray paFar;
    int nResult = 0;
    paIsolated.SetSize(0, -1);
    paAdjacent.SetSize(0, -1);
    paFar.SetSize(0, -1);
    int nMax = nOrderMax;
    short *pRow = paPlanGrid;
    for (int y = 0; y < 10; y++)
    {
        short *pCell = pRow;
        for (int x = 0; x < 10; x++)
        {
            int bLeft = 0;
            int bRight = 0;
            int bUp = 0;
            int bDown = 0;
            if (GetZoneGridOrder(x, y) <= nMax)
            {
                if (*pCell == 1 || *pCell == 300)
                {
                    CPoint *pPt = new CPoint(x, y);
                    if (x < 1 || pCell[-1] != 306)
                        bLeft = 1;
                    if (x > 8 || pCell[1] != 306)
                        bRight = 1;
                    if (y < 1 || pCell[-10] != 306)
                        bUp = 1;
                    if (y > 8 || pCell[10] != 306)
                        bDown = 1;
                    if (bLeft == 0 || bRight == 0 || bUp == 0 || bDown == 0)
                        paAdjacent.SetAtGrow(paAdjacent.GetSize(), (CObject *)pPt);
                    else
                        paIsolated.SetAtGrow(paIsolated.GetSize(), (CObject *)pPt);
                }
            }
            else
            {
                if (*pCell == 1)
                {
                    CPoint *pPt = new CPoint(x, y);
                    paFar.SetAtGrow(paFar.GetSize(), (CObject *)pPt);
                }
            }
            pCell++;
        }
        pRow += 10;
    }
    int nIso = paIsolated.GetSize();
    if (nIso == 0 && paAdjacent.GetSize() == 0)
    {
        if (paFar.GetSize() == 0)
        {
            ((CTheApp *)AfxGetApp())->LogWrite("!!!!No Place to put Find Puzzle!!!\n");
            nResult = 0;
        }
        else
        {
            int nFar = paFar.GetSize();
            CPoint *pPt = (CPoint *)paFar.GetAt(rand() % nFar);
            nResult = 1;
            *pX = pPt->x;
            *pY = pPt->y;
            goto cleanup;
        }
    }
    if (nIso > 0)
    {
        CPoint *pPt = (CPoint *)paIsolated.GetAt(rand() % nIso);
        nResult = 1;
        *pX = pPt->x;
        *pY = pPt->y;
    }
    else
    {
        int nAdj = paAdjacent.GetSize();
        if (nAdj > 0)
        {
            CPoint *pPt = (CPoint *)paAdjacent.GetAt(rand() % nAdj);
            nResult = 1;
            *pX = pPt->x;
            *pY = pPt->y;
        }
    }
cleanup:
    if (paIsolated.GetSize() > 0)
    {
        int i = 0;
        int nDelIso = paIsolated.GetSize();
        do
        {
            delete (CPoint *)paIsolated.GetAt(i);
            i++;
            nDelIso--;
        } while (nDelIso != 0);
    }
    paIsolated.SetSize(0, -1);
    if (paAdjacent.GetSize() > 0)
    {
        int i = 0;
        int nDelAdj = paAdjacent.GetSize();
        do
        {
            delete (CPoint *)paAdjacent.GetAt(i);
            i++;
            nDelAdj--;
        } while (nDelAdj != 0);
    }
    paAdjacent.SetSize(0, -1);
    if (paFar.GetSize() > 0)
    {
        int i = 0;
        int nDelFar = paFar.GetSize();
        do
        {
            delete (CPoint *)paFar.GetAt(i);
            i++;
            nDelFar--;
        } while (nDelFar != 0);
    }
    paFar.SetSize(0, -1);
    return nResult;
}

// FUNCTION: YODA 0x00421930
// [EFFECTIVE-WIP: structurally converged (insns 368/364). Residuals: (1) the OPEN
// block-sinking family — the orig sinks all four accept-block copies (first-tele + one per
// worldSize case), the shared retry block and the return-0 epilogue PAST the switch to
// 0x421c4b-0x421cd9; ours emits the copies inline at their case arms (condition-polarity
// probes inert — same mechanism as the dispatcher cleanup blocks / GetLocatorIcon).
// (2) {nBanned,nLastX,nLastY} local-slot rotation (+0x20/24/28) + reg cascade in the
// phase-1 cell math. Structure proven: `int nVal = pEntry->val` (one movsx, two pushes),
// flag-if with the direct-call arm first and `if (bRetry == 0) call else nZoneId=nBanned`
// inner shape, i++/n-- countdown recipes for the pending-free and teleporter scans,
// n++ before the lastX/lastY stores in all four accept copies, dead x/y stores in the
// final forced-partner block.]
// Puzzle/teleporter placement pass: seed the pending list with the two quest anchors
// (0x1ff order 2, 0x1a5 order 1), place each pending puzzle via PlacePuzzle+PlaceQuestNode
// (cell code 306 marks it in the plan grid), free the pending list, then sweep the plan grid
// placing a zone on every 1/300/0x68 cell. Zones containing a teleporter (obj type 13) must
// be at least 2 (worldSize 1/2) or 3 (worldSize 3) cells from the previous teleporter — a
// rejected zone id is banned (bRetry/nBanned) and re-rolled. If only ONE teleporter got
// placed, force a partner zone at its recorded cell with the distance check disabled.
int World::WorldgenPlacePuzzles(short *paPlanGrid)
{
    int x, y;
    WorldgenPushZoneEntry(0x1ff, 2);
    WorldgenPushZoneEntry(0x1a5, 1);
    int n = worldgenPendingZones.GetSize();
    int i = 0;
    if (n > 0)
    {
        do
        {
            WorldgenZoneEntry *pEntry = (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            int nVal = pEntry->val;
            if (PlacePuzzle((short)nVal, paPlanGrid, &x, &y) != 1)
                return 0;
            int nZoneId = (short)PlaceQuestNode(0x11, -1, -1, pEntry->zoneId, -1, (short)nVal, 0);
            if (nZoneId < 0)
                return 0;
            int nCell = y * 10 + x;
            mapGrid[nCell].zoneType = 0x11;
            mapGrid[nCell].cellItemC = (short)genCellItemCScratch;
            apZoneGrid[nCell] = (Zone *)zones.GetAt(nZoneId);
            mapGrid[nCell].id = (short)nZoneId;
            paPlanGrid[nCell] = 306;
            AddPlacedZoneId(nZoneId);
            i++;
        } while (i < n);
    }
    if (n > 0)
    {
        int j = 0;
        int nLeft = n;
        do
        {
            WorldgenZoneEntry *pEntry = (WorldgenZoneEntry *)worldgenPendingZones.GetAt(j);
            delete pEntry;
            j++;
            nLeft--;
        } while (nLeft != 0);
    }
    worldgenPendingZones.SetSize(0, -1);
    uniqueRequiredItemsMaybe.SetSize(0, -1);

    int bRetry;
    int nBanned;
    int nLastX, nLastY;
    n = 0;
    bRetry = 0;
    for (y = 0; y < 10; y++)
    {
        for (x = 0; x < 10; x++)
        {
            int nCode = paPlanGrid[y * 10 + x];
            if (nCode == 1 || nCode == 300 || nCode == 0x68)
            {
                int nOrder = GetZoneGridOrder(x, y);
                if (nCode == 0x68 || nOrder < 2)
                    genSkipTeleCheckMaybe = 1;
                else
                    genSkipTeleCheckMaybe = 0;
                int nZoneId;
                if (genSkipTeleCheckMaybe != 0)
                {
                    nZoneId = (short)PlaceQuestNode(1, -1, -1, -1, -1, nOrder, 0);
                }
                else
                {
                    if (bRetry == 0)
                        nZoneId = (short)PlaceQuestNode(1, -1, -1, -1, -1, nOrder, 0);
                    else
                        nZoneId = nBanned;
                }
                Zone *pZone;
                for (;;)
                {
                    if (nZoneId < 0)
                        return 0;
                    pZone = (Zone *)zones.GetAt(nZoneId);
                    if (genSkipTeleCheckMaybe != 0)
                        goto place;
                    i = 0;
                    {
                        int nObjs = pZone->objects.GetSize();
                        if (nObjs > 0)
                        {
                            int k = 0;
                            do
                            {
                                if (((ZoneObj *)pZone->objects.GetAt(k))->type == 13)
                                    i = 1;
                                k++;
                                nObjs--;
                            } while (nObjs != 0);
                        }
                    }
                    if (i == 0)
                        goto place;
                    if (n == 0)
                    {
                        n++;
                        nLastX = x;
                        nLastY = y;
                        if (bRetry != 0)
                        {
                            bRetry = 0;
                            nBanned = -1;
                        }
                        goto place;
                    }
                    switch (worldSize)
                    {
                    case 1:
                        if (abs(nLastX - x) > 1 || abs(nLastY - y) > 1)
                        {
                            n++;
                            nLastX = x;
                            nLastY = y;
                            if (bRetry != 0)
                            {
                                bRetry = 0;
                                nBanned = -1;
                            }
                            goto place;
                        }
                        break;
                    case 2:
                        if (abs(nLastX - x) > 1 || abs(nLastY - y) > 1)
                        {
                            n++;
                            nLastX = x;
                            nLastY = y;
                            if (bRetry != 0)
                            {
                                bRetry = 0;
                                nBanned = -1;
                            }
                            goto place;
                        }
                        break;
                    case 3:
                        if (abs(nLastX - x) > 2 || abs(nLastY - y) > 2)
                        {
                            n++;
                            nLastX = x;
                            nLastY = y;
                            if (bRetry != 0)
                            {
                                bRetry = 0;
                                nBanned = -1;
                            }
                            goto place;
                        }
                        break;
                    default:
                        goto place;
                    }
                    nBanned = nZoneId;
                    bRetry = 1;
                    nZoneId = (short)PlaceQuestNode(1, -1, -1, -1, -1, nOrder, 0);
                }
place:
                {
                    int nCell = y * 10 + x;
                    mapGrid[nCell].zoneType = 1;
                    apZoneGrid[nCell] = pZone;
                    mapGrid[nCell].id = (short)nZoneId;
                    mapGrid[nCell].cellItemA = -1;
                    mapGrid[nCell].cellQuestSlot0 = -1;
                    mapGrid[nCell].cellItemC = -1;
                    mapGrid[nCell].cellQuestSlot6 = -1;
                    AddPlacedZoneId(nZoneId);
                    if (nZoneId == nBanned)
                    {
                        nBanned = -1;
                        bRetry = 0;
                    }
                }
            }
        }
    }
    if (n == 1)
    {
        genSkipTeleCheckMaybe = 1;
        int nZoneId = (short)PlaceQuestNode(1, -1, -1, -1, -1,
                                            (short)GetZoneGridOrder(nLastX, nLastY), 0);
        if (nZoneId >= 0)
        {
            x = nLastX;      // sic: dead stores the original keeps
            y = nLastY;
            Zone *pZone = (Zone *)zones.GetAt(nZoneId);
            int nCell = nLastY * 10 + nLastX;
            mapGrid[nCell].zoneType = 1;
            apZoneGrid[nCell] = pZone;
            mapGrid[nCell].id = (short)nZoneId;
            mapGrid[nCell].cellItemA = -1;
            mapGrid[nCell].cellQuestSlot0 = -1;
            mapGrid[nCell].cellItemC = -1;
            mapGrid[nCell].cellQuestSlot6 = -1;
            AddPlacedZoneId(nZoneId);
        }
    }
    return 1;
}

// FUNCTION: YODA 0x00421e50
// Worldgen grid-cell traversal/placement priority (static 10x10 dword table). `this` is unused.
int World::GetZoneGridOrder(int x, int y)
{
    return gWorldgenGridOrderTable[x + y * 10];
}

// FUNCTION: YODA 0x00421e70
// [EFFECTIVE: 17B — SetAtGrow call-site this-reg {EAX,ECX} swap (lea vs add) + one NOP.]
// CHUNK chunk: allocate + read Character records, -1-terminated id list.
int World::ParseChar(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id < 0)
        {
            nDone++;
        }
        else
        {
            Character *pNew;
            TRY {
                pNew = new Character;
            }
            }              // closes the try block the TRY macro opened
            catch (CException *e) {                // hand-expanded CATCH_ALL(e)
                _afxExceptionLink.m_pException = e;
                THROW_LAST();
                AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable OOM dialog
                AfxAbort();                            //      (docs/engine-bugs.md #7)
            }
            }              // closes the TRY macro's outer (link-scope) brace
            if (pNew == NULL)
                return 0;
            characters.SetAtGrow(characters.GetSize(), pNew);
            pNew->Read(pFile);
        }
    } while (nDone == 0);
    if (characters.GetAt(0) != NULL)
        pPlayerChar = (Character *)characters.GetAt(0);
    return 1;
}

// FUNCTION: YODA 0x00421fd0
// [EFFECTIVE-WIP: ~97% insn-identical. Residuals: (1) the loop-exit cleanup block (delete
// pFile + dtors + return) sits mid-ladder after the ZONE arm in the orig, at the end here —
// the OPEN block-layout family (WorldDoc GetLocatorIcon); nesting the ladder under
// if(nDone==0) proven IL-equivalent (identical bytes). (2) a reg-pool cascade seeded by
// nRet-init 1 landing in ESI (orig) vs EDI (ours): zero-pool xor, delete-loop countdown
// (dec/jne) vs up-count+spill, few cmp forms. Dial/joint-pass territory. Structure proven:
// CPoint-pair CRect ctor (r,b computed before l,t), x/y locals, DoWaitCursor(1) direct,
// pApp spilled, delete-loop guard+do-while.]
// New-game world loader (StartGame): re-pick the planet (demo then FORCES Hoth), free the old
// zone list, open the .dta (theApp.m_str) and dispatch its FourCC chunk stream until ENDF.
// VERS must be 0x200.
int World::LoadWorld()
{
    CTheApp *pApp = (CTheApp *)AfxGetApp();
    if (bStartingGameMaybe == 0)
    {
        currentPlanet = pApp->GetProfileInt("OPTIONS", "Terrain", 1);
        if (completionCount == 5 || completionCount == 10 || completionCount == 15)
        {
            switch (currentPlanet)
            {
            case 1:
                currentPlanet = 3;
                break;
            case 2:
                if (rand() % 2 == 0)
                    currentPlanet = 3;
                else
                    currentPlanet = 1;
                break;
            case 3:
                currentPlanet = 1;
                break;
            }
        }
        else
        {
            switch (currentPlanet)
            {
            case 1:
                if (rand() % 2 == 0)
                    currentPlanet = 3;
                else
                    currentPlanet = 2;
                break;
            case 2:
                if (rand() % 2 == 0)
                    currentPlanet = 3;
                else
                    currentPlanet = 1;
                break;
            case 3:
                if (rand() % 2)
                    currentPlanet = 2;
                else
                    currentPlanet = 1;
                break;
            }
        }
    }
    currentPlanet = 2;               // sic: demo hardcode — the whole pick above is overridden
    if (pApp != NULL)
        pApp->WriteProfileInt("OPTIONS", "Terrain", 2);

    int nRet = 1;
    CProgressCtrl progress;
    int x = nViewLeft;
    int y = nViewTop;
    progress.Create(WS_CHILD | WS_VISIBLE,
                    CRect(CPoint(x + 0x11, y + 0x110), CPoint(x + 0x11e, y + 0x11d)),
                    AfxGetMainWnd(), 0x3e9);
    progress.SetRange(0, 4);
    progress.SetStep(1);

    int nZones = zones.GetSize();
    if (nZones > 0)
    {
        int i = 0;
        do
        {
            Zone *pZone = (Zone *)zones.GetAt(i);
            if (pZone != (Zone *)-1)
                delete pZone;
            i++;
        } while (i < nZones);
    }
    zones.SetSize(0, -1);

    CString strPath;
    strPath = pApp->m_str;
    CFile *pFile = new CFile;
    if (!pFile->Open(strPath, CFile::modeRead | CFile::typeBinary, NULL))
    {
        if (pFile != NULL)
            delete pFile;
        nFrameMode = 12;
        return 0;
    }
    AfxGetApp()->DoWaitCursor(1);
    pFile->SeekToBegin();
    int nDone = 0;
    do
    {
        progress.StepIt();
        char tag[5];
        int nLen;
        TRY {
            pFile->Read(tag, 4);
            tag[4] = 0;
            if (strcmp(tag, "ZONE") != 0)
                pFile->Read(&nLen, 4);
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e) {                // hand-expanded CATCH_ALL(e)
            _afxExceptionLink.m_pException = e;
            nRet = 0;
            nDone++;
        }
        }              // closes the TRY macro's outer (link-scope) brace
        if (nDone != 0)
            break;
        if (strcmp(tag, "VERS") == 0)
        {
            if (nLen != 0x200)
            {
                AfxMessageBox(0xe003, 0, (UINT)-1);
                nDone++;
            }
        }
        else if (strcmp(tag, "ZONE") == 0)
        {
            nRet = ParseZone(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAUX") == 0)
        {
            nRet = ParseZaux(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAX2") == 0)
        {
            nRet = ParseZax2(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAX3") == 0)
        {
            nRet = ParseZax3(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "HTSP") == 0)
        {
            nRet = ParseHtsp(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ACTN") == 0)
        {
            nRet = ParseActn(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ENDF") == 0)
        {
            nDone++;
        }
        else
        {
            pFile->Seek(nLen, CFile::current);
        }

    } while (nDone == 0);
    if (pFile != NULL)
        delete pFile;
    return nRet;
}

// FUNCTION: YODA 0x00422670
// [EFFECTIVE-WIP: ~95% insn-identical. Residuals: (1) OPEN — the m_cause switch: orig emits a
// DIRECT 13-entry jump table whose entries point at only 3 merged arm blocks; VC4.2 probes
// (sw*.cpp battery 2026-07-06) show grouped 3-arm switches ALWAYS lower to the byte-map
// two-level form, ≥6 written-out arms give a direct table but NEVER merge the arm blocks
// (cdecl or stdcall callee, leaf or shared-continuation; 13-arm form measured WORSE in-tree).
// The direct-table+3-merged-blocks combo is unreachable from every tried source shape —
// possibly TU-context-dependent lowering. (2) the same loop-exit-block placement family as
// LoadWorld (orig glues it after the FIRST parse arm (TILE); ours after the tail). (3) one
// frame-slot shift (our EH/catch temp region one dword tighter) + the reg cascade.
// Structure proven: bDtaLoadedMaybe++ (inc form), grouped switch with per-arm AfxMessageBox
// calls, dead code after AfxAbort, success block (Generate loop) sunk to the end.]
// The main .dta asset loader: open theApp.m_str (CFileException box + AfxAbort on failure),
// dispatch the FourCC chunk stream (VERS/TILE/TNAM/ZONE/ZAUX/ZAX2/ZAX3/CHAR/CHWP/CAUX/HTSP/
// SNDS/PUZ2/ACTN/ENDF), then drive worldgen: Randomize+Generate until success, Populate.
// Declaring the local CFileException also emits this TU's ~CFileException COMDAT — the
// original's copy is the TU-opening function at 0x41c340.
int World::Load()
{
    int nRet = 1;
    CFile *pFile = new CFile;
    CString strPath;
    strPath = ((CTheApp *)AfxGetApp())->m_str;
    CFileException e;
    if (!pFile->Open(strPath, CFile::modeRead | CFile::typeBinary | CFile::shareDenyNone, &e))
    {
        switch (e.m_cause)
        {
        case CFileException::fileNotFound:
        case CFileException::badPath:
        case CFileException::tooManyOpenFiles:
        case CFileException::invalidFile:
        case CFileException::badSeek:
        case CFileException::hardIO:
        case CFileException::endOfFile:
            AfxMessageBox(5, 0, (UINT)-1);
            break;
        case CFileException::accessDenied:
        case CFileException::directoryFull:
        case CFileException::sharingViolation:
        case CFileException::lockViolation:
        case CFileException::diskFull:
            AfxMessageBox(6, 0, (UINT)-1);
            break;
        default:
            AfxMessageBox(0xe01e, 0, (UINT)-1);
            break;
        }
        AfxAbort();
        nFrameMode = 12;    // sic: unreachable after AfxAbort, still emitted (engine-bugs.md #7)
        return 0;           // sic
    }
    if (bDtaLoadedMaybe == 0)
        bDtaLoadedMaybe++;
    CProgressCtrl progress;
    int x = nViewLeft;
    int y = nViewTop;
    progress.Create(WS_CHILD | WS_VISIBLE,
                    CRect(CPoint(x + 0x11, y + 0x110), CPoint(x + 0x11e, y + 0x11d)),
                    AfxGetMainWnd(), 0x3e9);
    progress.SetRange(0, 11);
    progress.SetStep(1);
    AfxGetApp()->DoWaitCursor(1);
    pFile->SeekToBegin();
    int nDone = 0;
    do
    {
        progress.StepIt();
        char tag[5];
        int nLen;
        TRY {
            pFile->Read(tag, 4);
            tag[4] = 0;
            if (strcmp(tag, "ZONE") != 0)
                pFile->Read(&nLen, 4);
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e2) {               // hand-expanded CATCH_ALL(e2)
            _afxExceptionLink.m_pException = e2;
            nDone++;
        }
        }              // closes the TRY macro's outer (link-scope) brace
        if (nDone != 0)
            break;
        if (strcmp(tag, "VERS") == 0)
        {
            if (nLen != 0x200)
                nDone++;
        }
        else if (strcmp(tag, "TILE") == 0)
        {
            nRet = ParseTilesMaybe(pFile, nLen);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "TNAM") == 0)
        {
            nRet = ParseTnam(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZONE") == 0)
        {
            nRet = ParseZone(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAUX") == 0)
        {
            nRet = ParseZaux(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAX2") == 0)
        {
            nRet = ParseZax2(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ZAX3") == 0)
        {
            nRet = ParseZax3(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "CHAR") == 0)
        {
            nRet = ParseChar(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "CHWP") == 0)
        {
            nRet = ParseChwp(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "CAUX") == 0)
        {
            nRet = ParseCaux(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "HTSP") == 0)
        {
            nRet = ParseHtsp(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "SNDS") == 0)
        {
            nRet = ParseSnds(pFile);
            if (nRet == 0)
                break;
            UpdateAllViews(NULL, 399, NULL);
        }
        else if (strcmp(tag, "PUZ2") == 0)
        {
            nRet = ParsePuz2(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ACTN") == 0)
        {
            nRet = ParseActn(pFile);
            if (nRet == 0)
                break;
        }
        else if (strcmp(tag, "ENDF") == 0)
        {
            nDone++;
        }
        else
        {
            pFile->Seek(nLen, CFile::current);
        }
    } while (nDone == 0);
    if (pFile != NULL)
        delete pFile;
    if (nRet == 0)
    {
        AfxGetApp()->DoWaitCursor(-1);
        nFrameMode = 12;
    }
    else
    {
        int nGenerated = 0;
        zoneCountLoadedMaybe = zones.GetSize();
        CacheUiTilePtrsMaybe();
        unsigned int nSeed = Randomize();
        do
        {
            if (Generate(nSeed) != 0)
                nGenerated++;
            else
                nSeed = Randomize();
        } while (nGenerated == 0);
        AfxGetApp()->DoWaitCursor(-1);
        Populate();
        return 1;
    }
    return nRet;
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

// FUNCTION: YODA 0x00422f60
// ZONE chunk: count + N zone records.
int World::ParseZone(CFile *pFile)
{
    short nZones;
    pFile->Read(&nZones, 2);
    zones.SetSize(nZones, -1);
    for (int i = 0; i < nZones; i++)
    {
        Zone *pZone = ReadZone(pFile, i);
        if (pZone == NULL)
            return 0;
        zones.SetAt(i, pZone);
    }
    return 1;
}

// FUNCTION: YODA 0x00422fd0
// [WIP: nDone++-arm layout family + SetAtGrow this-reg (lea vs add) + a stray NOP from the
// TRY expansion; structure converged.]
// CHUNK chunk: allocate + read Puzzle records, -1-terminated id list.
int World::ParsePuz2(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id < 0)
        {
            nDone++;
        }
        else
        {
            Puzzle *pNew;
            TRY {
                pNew = new Puzzle;
            }
            }              // closes the try block the TRY macro opened
            catch (CException *e) {                // hand-expanded CATCH_ALL(e)
                _afxExceptionLink.m_pException = e;
                THROW_LAST();
                AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable OOM dialog
                AfxAbort();                            //      (docs/engine-bugs.md #7)
            }
            }              // closes the TRY macro's outer (link-scope) brace
            if (pNew == NULL)
                return 0;
            puzzles.SetAtGrow(puzzles.GetSize(), pNew);
            pNew->Read(pFile);
        }
        if (nDone != 0)
            return 1;
    } while (1);
}

// FUNCTION: YODA 0x00423110
// ZAUX chunk: per-zone 8-byte header + IZAX payload.
int World::ParseZaux(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    char buf[8];
    for (int i = 0; i < nCount; i++)
    {
        pFile->Read(buf, 8);
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadZaux(pFile);
    }
    return 1;
}

// FUNCTION: YODA 0x00423190
// ZAX3 chunk.
int World::ParseZax3(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    char buf[8];
    for (int i = 0; i < nCount; i++)
    {
        pFile->Read(buf, 8);
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadZax3(pFile);
    }
    return 1;
}

// FUNCTION: YODA 0x00423210
// ZAX2 chunk.
int World::ParseZax2(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    char buf[8];
    for (int i = 0; i < nCount; i++)
    {
        pFile->Read(buf, 8);
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadZax2(pFile);
    }
    return 1;
}

// FUNCTION: YODA 0x00423290
// [EFFECTIVE: block-layout — original parks the nDone++ arm at function end; ours inlines
// it (arm-order/continue knobs proven inert). Plus one reg rotation.]
// CAUX chunk: per-character damage words, -1-terminated id list.
int World::ParseCaux(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id >= 0)
        {
            Character *pChar = (Character *)characters.GetAt(id);
            if (pChar == NULL)
                return 0;
            pFile->Read(&pChar->damage, 2);
        }
        else
        {
            nDone++;
        }
    } while (nDone == 0);
    return 1;
}

// FUNCTION: YODA 0x00423300
// [EFFECTIVE: same nDone++-arm layout family as ParseCaux; registers exact.]
// CHWP chunk: per-character weapon id + health, -1-terminated id list.
int World::ParseChwp(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id >= 0)
        {
            Character *pChar = (Character *)characters.GetAt(id);
            if (pChar == NULL)
                return 0;
            pFile->Read(&pChar->weaponCharId, 2);
            pFile->Read(&pChar->health, 2);
        }
        else
        {
            nDone++;
        }
    } while (nDone == 0);
    return 1;
}

// FUNCTION: YODA 0x00423380
// TNAM chunk: short tile id + 24-byte name, -1-terminated.
int World::ParseTnam(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id < 0)
        {
            nDone++;
        }
        else
        {
            Tile *pTile = (Tile *)tiles.GetAt(id);
            char buf[24];
            pFile->Read(buf, 0x18);
            pTile->name = buf;
        }
    } while (nDone == 0);
    return 1;
}

// FUNCTION: YODA 0x004233f0
// [EFFECTIVE: 5B — frame-slot order of the char buffers: orig ascending {ext,fname,name,path}
// (size-sorted), ours {ext,name,fname,path}. Probes ALL inert (layout byte-stable): decl order
// x2, nested strcat(strcpy(),) vs sequential, if-scope vs loop-scope vs split scopes. The array
// slot key is compiler-internal, not source-steerable from inside the function; insns/regs 100%
// (align=0 reg_pen=0). Park for the joint TU pass.]
// SNDS chunk: NEGATED sound count, then per-sound a length-prefixed source path; only the
// bare "fname.ext" is kept in soundNames[i].
int World::ParseSnds(CFile *pFile)
{
    short nCount;
    pFile->Read(&nCount, 2);
    nCount = -nCount;
    for (int i = 0; i < nCount; i++)
    {
        short nLen;
        pFile->Read(&nLen, 2);
        if (nLen > 0)
        {
            char path[128];
            char fname[12];
            char ext[8];
            char name[16];
            pFile->Read(path, nLen);
            _splitpath(path, NULL, NULL, fname, ext);
            strcpy(name, fname);
            strcat(name, ext);
            soundNames[i] = name;
        }
    }
    return 1;
}

// FUNCTION: YODA 0x00423510
// ACTN chunk: records of (zone id, script count, scripts...); -1 zone id terminates.
// Each zone's IACT scripts (conditions + commands) land in zone->iactScripts.
int World::ParseActn(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id == -1)
        {
            nDone++;
        }
        else
        {
            Zone *pZone = (Zone *)zones.GetAt(id);
            short nCount;
            pFile->Read(&nCount, 2);
            pZone->iactScripts.SetSize(nCount, -1);
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    IactScript *pNew;
                    TRY {
                        pNew = new IactScript;
                    }
                    }              // closes the try block the TRY macro opened
                    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
                        _afxExceptionLink.m_pException = e;
                        THROW_LAST();
                        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable OOM dialog
                        AfxAbort();                            //      (docs/engine-bugs.md #7)
                    }
                    }              // closes the TRY macro's outer (link-scope) brace
                    if (pNew == NULL)
                        return 0;
                    pZone->iactScripts.SetAt(i, pNew);
                    pNew->Read(pFile);
                    i++;
                } while (nCount > i);
            }
        }
        if (nDone != 0)
            return 1;
    } while (1);
}

// FUNCTION: YODA 0x004236b0
// HTSP chunk: records of (zone id, hotspot count, hotspots...); negative zone id terminates.
// Replaces each zone's object list, then re-derives its quest-object flags.
int World::ParseHtsp(CFile *pFile)
{
    int nDone = 0;
    do
    {
        short id;
        pFile->Read(&id, 2);
        if (id < 0)
        {
            nDone++;
        }
        else
        {
            short nCount;
            pFile->Read(&nCount, 2);
            Zone *pZone = (Zone *)zones.GetAt(id);
            pZone->objects.SetSize(nCount, -1);
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    ZoneObj *pNew;
                    TRY {
                        pNew = new ZoneObj;
                    }
                    }              // closes the try block the TRY macro opened
                    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
                        _afxExceptionLink.m_pException = e;
                        THROW_LAST();
                        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable OOM dialog
                        AfxAbort();                            //      (docs/engine-bugs.md #7)
                    }
                    }              // closes the TRY macro's outer (link-scope) brace
                    if (pNew == NULL)
                        return 0;
                    pNew->Read(pFile);
                    pZone->objects.SetAt(i, pNew);
                    i++;
                } while (nCount > i);
            }
            pZone->FlagQuestObjects();
        }
        if (nDone != 0)
            return 1;
    } while (1);
}

// FUNCTION: YODA 0x00423d20
// Find the INTRO zone (map_flags 9), make it current and refresh (StartGame).
void World::SetCurrentToIntroZone()
{
    int nCount = zones.GetSize();
    for (int i = 0; i < nCount; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone->type == ZONE_TYPE_INTRO)
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

// FUNCTION: YODA 0x00425e30
// Lay the (demo-hardcoded) quest into the 10x10 grid: pick one of the shipped goal zones by
// rand()%4 (or forced by the goal item), place its content, tag the center 2x2 cells.
int World::Populate()
{
    GameView *pView = NULL;
    POSITION pos = GetFirstViewPosition();
    if (pos != NULL)
        pView = (GameView *)GetNextView(pos);
    pView->bBusy = 1;
    BackupZoneGrid();
    SetupGrid();
    short r = (short)(rand() % 4);
    if (goalItemTileId == 0x84)
        r = 3;
    if (goalItemTileId == 0xbd)
        r = 4;
    switch (r)
    {
    case 0:
        PlaceZone(0x5e, 0x30c);
        mapGrid[44].zoneType = 0x10;
        mapGrid[44].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
        mapGrid[44].id = 0x5e;
        mapGrid[44].flagA = 0;
        mapGrid[44].flagSolved = 0;
        mapGrid[44].cellItemC = ((Puzzle *)puzzles.GetAt(questItemsB.GetAt(0)))->itemA;
        PlaceZoneObjectTiles(0x5e);
        break;
    case 1:
        PlaceZone(0x217, 0x30c);
        mapGrid[45].zoneType = 0x10;
        mapGrid[45].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
        mapGrid[45].id = 0x217;
        mapGrid[45].flagA = 0;
        mapGrid[45].flagSolved = 0;
        mapGrid[45].cellItemC = ((Puzzle *)puzzles.GetAt(questItemsB.GetAt(0)))->itemA;
        PlaceZoneObjectTiles(0x217);
        break;
    case 2:
        PlaceZone(0x60, 0x30c);
        mapGrid[55].zoneType = 0x10;
        mapGrid[55].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
        mapGrid[55].id = 0x60;
        mapGrid[55].flagA = 0;
        mapGrid[55].flagSolved = 0;
        mapGrid[55].cellItemC = ((Puzzle *)puzzles.GetAt(questItemsB.GetAt(0)))->itemA;
        PlaceZoneObjectTiles(0x60);
        break;
    case 3:
        PlaceZone(0x5d, 0x30c);
        mapGrid[54].zoneType = 0x10;
        mapGrid[54].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
        mapGrid[54].id = 0x5d;
        mapGrid[54].flagA = 0;
        mapGrid[54].flagSolved = 0;
        mapGrid[54].cellItemC = ((Puzzle *)puzzles.GetAt(questItemsB.GetAt(0)))->itemA;
        PlaceZoneObjectTiles(0x5d);
        break;
    case 4:
        PlaceZone(0x217, 0x7f2);
        mapGrid[45].zoneType = 0x10;
        mapGrid[45].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
        mapGrid[45].id = 0x217;
        mapGrid[45].flagA = 0;
        mapGrid[45].flagSolved = 0;
        mapGrid[45].cellItemC = ((Puzzle *)puzzles.GetAt(questItemsB.GetAt(0)))->itemA;
        PlaceZoneObjectTiles(0x217);
        break;
    }
    mapGrid[44].id = 0x5e;
    mapGrid[45].id = 0x5f;
    mapGrid[54].id = 0x5d;
    mapGrid[55].id = 0x60;
    mapGrid[54].flagSolved = 0;
    pView->nTargetZoneId = 0;
    unk2e34 = 0;
    unk50 = 0;
    cameraX = 0x140;
    cameraY = 0x140;
    playerX = 4;
    playerY = 5;
    nFrameMode = 0xb;
    unk33b8 = 1;
    BackupRecords();
    pView->bBusy = 0;
    return 1;
}

// FUNCTION: YODA 0x004260e0
// [WIP: reg 2-cycle {EDX,ECX} in the find loops + EH-state(-1) placement in the 0x217
// found path; lengths/structure converged.]
// Place a zone's quest content: find tileId in its IZX3 list and stamp it on a spawn object
// (zone 0x217 hardcodes the object at (3,3)).
int World::PlaceZone(short zoneId, unsigned short tileId)
{
    unsigned short v;
    int found = -1;
    CWordArray spawns;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    int nCand = pZone->genCandidateB.GetSize();
    if (zoneId == 0x217)
    {
        if (tileId == 0x30c)
        {
            int i = 0;
            if (nCand > 0)
            {
                do
                {
                    v = (unsigned short)pZone->genCandidateB.GetAt(i);
                    if (v == tileId)
                        break;
                    i++;
                } while (i < nCand);
            }
            int nObjs = pZone->objects.GetSize();
            int j = 0;
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj->x == 3 && pObj->y == 3)
                    {
                        pObj->visible = v;
                        pObj->state = 1;
                        genCellQuestSlot6Scratch = (short)v;
                        return 1;
                    }
                    j++;
                } while (j < nObjs);
            }
        }
        else
        if (tileId == 0x7f2)
        {
            int i = 0;
            if (nCand > 0)
            {
                do
                {
                    v = (unsigned short)pZone->genCandidateB.GetAt(i);
                    if (v == tileId)
                        break;
                    i++;
                } while (i < nCand);
            }
            int nObjs = pZone->objects.GetSize();
            int j = 0;
            if (nObjs > 0)
            {
                do
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(j);
                    if (pObj->x == 3 && pObj->y == 3)
                    {
                        pObj->visible = v;
                        pObj->state = 1;
                        genCellQuestSlot6Scratch = (short)v;
                        return 1;
                    }
                    j++;
                } while (j < nObjs);
            }
        }
    }
    else
    {
        int i = 0;
        if (nCand > 0)
        {
            do
            {
                v = (unsigned short)pZone->genCandidateB.GetAt(i);
                if (tileId == v)
                {
                    int nObjs = pZone->objects.GetSize();
                    int j = 0;
                    spawns.SetSize(0, -1);
                    if (nObjs > 0)
                    {
                        do
                        {
                            if (((ZoneObj *)pZone->objects.GetAt(j))->type == OBJ_SPAWN)
                                spawns.SetAtGrow(spawns.GetSize(), (unsigned short)j);
                            j++;
                        } while (j < nObjs);
                    }
                    if (spawns.GetSize() > 0)
                    {
                        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(spawns.GetAt(rand() % spawns.GetSize()));
                        if (pObj != NULL && pObj->type == OBJ_SPAWN)
                        {
                            found = (short)v;
                            pObj->visible = v;
                            pObj->state = 1;
                            genCellQuestSlot6Scratch = found;
                        }
                    }
                    if (found >= 0)
                        break;
                }
                i++;
            } while (i < nCand);
        }
    }
    return 1;
}

// FUNCTION: YODA 0x00426380
// Restore the center 2x2 quest cells (44,45,54,55) from mapScratch, re-tagging their ids.
void World::RestoreRecords()
{
    mapGrid[44].id = 0x5e;
    mapGrid[44].cellQuestSlot0 = mapScratch[0].cellQuestSlot0;
    mapGrid[44].cellQuestSlot1 = mapScratch[0].cellQuestSlot1;
    mapGrid[44].zoneType = mapScratch[0].zoneType;
    mapGrid[44].cellItemA = mapScratch[0].cellItemA;
    mapGrid[44].cellItemB = mapScratch[0].cellItemB;
    mapGrid[44].cellItemC = mapScratch[0].cellItemC;
    mapGrid[44].cellQuestSlot5 = mapScratch[0].cellQuestSlot5;
    mapGrid[44].cellQuestSlot6 = mapScratch[0].cellQuestSlot6;
    mapGrid[44].flagSolved = mapScratch[0].flagSolved;
    mapGrid[44].flagA = mapScratch[0].flagA;
    mapGrid[44].flagB = mapScratch[0].flagB;
    mapGrid[44].flagC = mapScratch[0].flagC;
    mapGrid[44].flagD = mapScratch[0].flagD;
    mapGrid[44].field30 = mapScratch[0].field30;
    mapGrid[45].id = 0x5f;
    mapGrid[45].cellQuestSlot0 = mapScratch[1].cellQuestSlot0;
    mapGrid[45].cellQuestSlot1 = mapScratch[1].cellQuestSlot1;
    mapGrid[45].zoneType = mapScratch[1].zoneType;
    mapGrid[45].cellItemA = mapScratch[1].cellItemA;
    mapGrid[45].cellItemB = mapScratch[1].cellItemB;
    mapGrid[45].cellItemC = mapScratch[1].cellItemC;
    mapGrid[45].cellQuestSlot5 = mapScratch[1].cellQuestSlot5;
    mapGrid[45].cellQuestSlot6 = mapScratch[1].cellQuestSlot6;
    mapGrid[45].flagSolved = mapScratch[1].flagSolved;
    mapGrid[45].flagA = mapScratch[1].flagA;
    mapGrid[45].flagB = mapScratch[1].flagB;
    mapGrid[45].flagC = mapScratch[1].flagC;
    mapGrid[45].flagD = mapScratch[1].flagD;
    mapGrid[45].field30 = mapScratch[1].field30;
    mapGrid[54].id = 0x5d;
    mapGrid[54].cellQuestSlot0 = mapScratch[2].cellQuestSlot0;
    mapGrid[54].cellQuestSlot1 = mapScratch[2].cellQuestSlot1;
    mapGrid[54].zoneType = mapScratch[2].zoneType;
    mapGrid[54].cellItemA = mapScratch[2].cellItemA;
    mapGrid[54].cellItemB = mapScratch[2].cellItemB;
    mapGrid[54].cellItemC = mapScratch[2].cellItemC;
    mapGrid[54].cellQuestSlot5 = mapScratch[2].cellQuestSlot5;
    mapGrid[54].cellQuestSlot6 = mapScratch[2].cellQuestSlot6;
    mapGrid[54].flagSolved = mapScratch[2].flagSolved;
    mapGrid[54].flagA = mapScratch[2].flagA;
    mapGrid[54].flagB = mapScratch[2].flagB;
    mapGrid[54].flagC = mapScratch[2].flagC;
    mapGrid[54].flagD = mapScratch[2].flagD;
    mapGrid[54].field30 = mapScratch[2].field30;
    mapGrid[55].id = 0x60;
    mapGrid[55].cellQuestSlot0 = mapScratch[3].cellQuestSlot0;
    mapGrid[55].cellQuestSlot1 = mapScratch[3].cellQuestSlot1;
    mapGrid[55].zoneType = mapScratch[3].zoneType;
    mapGrid[55].cellItemA = mapScratch[3].cellItemA;
    mapGrid[55].cellItemB = mapScratch[3].cellItemB;
    mapGrid[55].cellItemC = mapScratch[3].cellItemC;
    mapGrid[55].cellQuestSlot5 = mapScratch[3].cellQuestSlot5;
    mapGrid[55].cellQuestSlot6 = mapScratch[3].cellQuestSlot6;
    mapGrid[55].flagSolved = mapScratch[3].flagSolved;
    mapGrid[55].flagA = mapScratch[3].flagA;
    mapGrid[55].flagB = mapScratch[3].flagB;
    mapGrid[55].flagC = mapScratch[3].flagC;
    mapGrid[55].flagD = mapScratch[3].flagD;
    mapGrid[55].field30 = mapScratch[3].field30;
}

// FUNCTION: YODA 0x00426690
// Snapshot the center 2x2 quest cells (44,45,54,55) into mapScratch, tagging the scratch ids.
void World::BackupRecords()
{
    mapScratch[0].id = 0x5e;
    mapScratch[0].cellQuestSlot0 = mapGrid[44].cellQuestSlot0;
    mapScratch[0].cellQuestSlot1 = mapGrid[44].cellQuestSlot1;
    mapScratch[0].zoneType = mapGrid[44].zoneType;
    mapScratch[0].cellItemA = mapGrid[44].cellItemA;
    mapScratch[0].cellItemB = mapGrid[44].cellItemB;
    mapScratch[0].cellItemC = mapGrid[44].cellItemC;
    mapScratch[0].cellQuestSlot5 = mapGrid[44].cellQuestSlot5;
    mapScratch[0].cellQuestSlot6 = mapGrid[44].cellQuestSlot6;
    mapScratch[0].flagSolved = mapGrid[44].flagSolved;
    mapScratch[0].flagA = mapGrid[44].flagA;
    mapScratch[0].flagB = mapGrid[44].flagB;
    mapScratch[0].flagC = mapGrid[44].flagC;
    mapScratch[0].flagD = mapGrid[44].flagD;
    mapScratch[0].field30 = mapGrid[44].field30;
    mapScratch[1].id = 0x5f;
    mapScratch[1].cellQuestSlot0 = mapGrid[45].cellQuestSlot0;
    mapScratch[1].cellQuestSlot1 = mapGrid[45].cellQuestSlot1;
    mapScratch[1].zoneType = mapGrid[45].zoneType;
    mapScratch[1].cellItemA = mapGrid[45].cellItemA;
    mapScratch[1].cellItemB = mapGrid[45].cellItemB;
    mapScratch[1].cellItemC = mapGrid[45].cellItemC;
    mapScratch[1].cellQuestSlot5 = mapGrid[45].cellQuestSlot5;
    mapScratch[1].cellQuestSlot6 = mapGrid[45].cellQuestSlot6;
    mapScratch[1].flagSolved = mapGrid[45].flagSolved;
    mapScratch[1].flagA = mapGrid[45].flagA;
    mapScratch[1].flagB = mapGrid[45].flagB;
    mapScratch[1].flagC = mapGrid[45].flagC;
    mapScratch[1].flagD = mapGrid[45].flagD;
    mapScratch[1].field30 = mapGrid[45].field30;
    mapScratch[2].id = 0x5d;
    mapScratch[2].cellQuestSlot0 = mapGrid[54].cellQuestSlot0;
    mapScratch[2].cellQuestSlot1 = mapGrid[54].cellQuestSlot1;
    mapScratch[2].zoneType = mapGrid[54].zoneType;
    mapScratch[2].cellItemA = mapGrid[54].cellItemA;
    mapScratch[2].cellItemB = mapGrid[54].cellItemB;
    mapScratch[2].cellItemC = mapGrid[54].cellItemC;
    mapScratch[2].cellQuestSlot5 = mapGrid[54].cellQuestSlot5;
    mapScratch[2].cellQuestSlot6 = mapGrid[54].cellQuestSlot6;
    mapScratch[2].flagSolved = mapGrid[54].flagSolved;
    mapScratch[2].flagA = mapGrid[54].flagA;
    mapScratch[2].flagB = mapGrid[54].flagB;
    mapScratch[2].flagC = mapGrid[54].flagC;
    mapScratch[2].flagD = mapGrid[54].flagD;
    mapScratch[2].field30 = mapGrid[54].field30;
    mapScratch[3].id = 0x60;
    mapScratch[3].cellQuestSlot0 = mapGrid[55].cellQuestSlot0;
    mapScratch[3].cellQuestSlot1 = mapGrid[55].cellQuestSlot1;
    mapScratch[3].zoneType = mapGrid[55].zoneType;
    mapScratch[3].cellItemA = mapGrid[55].cellItemA;
    mapScratch[3].cellItemB = mapGrid[55].cellItemB;
    mapScratch[3].cellItemC = mapGrid[55].cellItemC;
    mapScratch[3].cellQuestSlot5 = mapGrid[55].cellQuestSlot5;
    mapScratch[3].cellQuestSlot6 = mapGrid[55].cellQuestSlot6;
    mapScratch[3].flagSolved = mapGrid[55].flagSolved;
    mapScratch[3].flagA = mapGrid[55].flagA;
    mapScratch[3].flagB = mapGrid[55].flagB;
    mapScratch[3].flagC = mapGrid[55].flagC;
    mapScratch[3].flagD = mapGrid[55].flagD;
    mapScratch[3].field30 = mapGrid[55].field30;
}

// FUNCTION: YODA 0x004269a0
// Reset the active 10x10 grid to empty cells.
void World::SetupGrid()
{
    int n = 0;
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 10; j++)
        {
            mapGrid[n].id = -1;
            mapGrid[n].cellQuestSlot0 = -1;
            mapGrid[n].cellItemC = -1;
            mapGrid[n].cellItemA = -1;
            mapGrid[n].cellQuestSlot1 = -1;
            mapGrid[n].cellQuestSlot5 = -1;
            mapGrid[n].cellItemB = -1;
            mapGrid[n].cellQuestSlot6 = -1;
            mapGrid[n].flagSolved = 0;
            mapGrid[n].flagA = 0;
            mapGrid[n].flagB = 0;
            mapGrid[n].flagC = 0;
            mapGrid[n].flagD = 0;
            mapGrid[n].field30 = -1;
            n++;
        }
    }
}

// FUNCTION: YODA 0x00426a00
// Read one ZONE record: skip zones for other planets (demo whitelist forces a fixed id set);
// returns the Zone, (Zone *)-1 for skipped, NULL on failure.
Zone *World::ReadZone(CFile *pFile, int idx)
{
    BOOL bForce = FALSE;
    switch (idx)
    {
    case 0:
    case 0x4c:
    case 0x4d:
    case 0x5d:
    case 0x5e:
    case 0x5f:
    case 0x60:
    case 0x10a:
    case 0x10b:
    case 0x10f:
    case 0x1d7:
    case 0x217:
    case 0x282:
        bForce = TRUE;
    }
    short nPlanet;
    int nLen;
    pFile->Read(&nPlanet, 2);
    pFile->Read(&nLen, 4);
    Zone *pZone;
    if (currentPlanet == nPlanet || bForce)
    {
        pZone = new Zone(0x12, 0x12);
        short nWidth;
        pFile->Read(&nWidth, 2);
        pZone->ReadIzon(pFile);
        short nObjs;
        pFile->Read(&nObjs, 2);
        for (int i = 0; i < nObjs; i++)
        {
            ZoneObj *pObj = new ZoneObj;
            pObj->Read(pFile);
            pZone->objects.SetAtGrow(pZone->objects.GetSize(), pObj);
        }
        pZone->ReadZaux(pFile);
        pZone->ReadZax2(pFile);
        pZone->ReadZax3(pFile);
        pZone->ReadZax4(pFile);
        short nScripts;
        pFile->Read(&nScripts, 2);
        for (int j = 0; j < nScripts; j++)
        {
            IactScript *pScript = new IactScript;
            pZone->iactScripts.SetAtGrow(pZone->iactScripts.GetSize(), pScript);
            pScript->Read(pFile);
        }
    }
    else
    {
        pFile->Seek(nLen, CFile::current);
        return (Zone *)-1;
    }
    return pZone;
}
