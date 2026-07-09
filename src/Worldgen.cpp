// Worldgen TU (0x41c340–0x429000): worldgen + .wld save/load + .dta load (doc class source file).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Worldgen.h"

// ---- .data lookup tables (extracted from the original binary) ----
// 10x10 worldgen grid-order priority ring (0x00456630): outer ring 5 -> center 1.
int gWorldgenGridOrderTable[100] = {
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
     5, 4, 4, 4, 4, 4, 4, 4, 4, 5,
     5, 4, 3, 3, 3, 3, 3, 3, 4, 5,
     5, 4, 3, 2, 2, 2, 2, 3, 4, 5,
     5, 4, 3, 2, 1, 1, 2, 3, 4, 5,
     5, 4, 3, 2, 1, 1, 2, 3, 4, 5,
     5, 4, 3, 2, 2, 2, 2, 3, 4, 5,
     5, 4, 3, 3, 3, 3, 3, 3, 4, 5,
     5, 4, 4, 4, 4, 4, 4, 4, 4, 5,
     5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

// Quarter-circle needle-offset table (radius 16), 0x00456938 (25 ints in the binary;
// the health-dial needle indexes [50-nLo] which reaches [25] at exactly nLo=25 -- an
// original one-past-the-end read of the adjacent chunk tag. We provide a 26th element
// (=16, the geometric continuation) so our build is well-defined at 25%% health.
int gNeedleTable[26] = {
     0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10,10,11,11,12,12,13,13,14,14,
    15,15,15,16,16,
    16,
};


// FUNCTION: YODA 0x0041bfa0
// [EFFECTIVE: align=12 — clean ESI/EDI rename + one arg-marshal slot at the recursion call
// (orig loads visible into CX and sel into EAX; ours AX/ECX). Twin 0x41c0b0 scores align=0
// with the identical source shape → TU-phase tie-break, joint pass.]
// Recursive: does zoneId (or a DOOR_IN-linked child zone) list itemId in providedItemsA (sel==0)
// or providedItemsB? Boolean twin of ZoneFindInIzxList.
int CDeskcppDoc::ZoneHasIzxItemMaybe(short zoneId, short itemId, int sel)
{
    int found = 0;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    if (sel != 0)
    {
        int nCount = pZone->providedItemsB.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                if ((short)pZone->providedItemsB.GetAt(i) == itemId)
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
        int nCount = pZone->providedItemsA.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                if ((short)pZone->providedItemsA.GetAt(i) == itemId)
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
                    if (pObj->arg >= 0)
                        found = ZoneHasIzxItemMaybe(pObj->arg, itemId, sel);
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
int CDeskcppDoc::ZoneRequiresItemMaybe(short zoneId, short itemId)
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
                    if (pObj->arg >= 0)
                        found = ZoneRequiresItemMaybe(pObj->arg, itemId);
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
int CDeskcppDoc::PickUnplacedItemMaybe(short zoneId)
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
                    if (pObj->arg >= 0)
                        nResult = PickUnplacedItemMaybe(pObj->arg);
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
int CDeskcppDoc::ZoneProvidesItem(short zoneId, short itemId)
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
                        if (pObj->arg >= 0)
                            found = ZoneProvidesItem(pObj->arg, itemId);
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
// Recursive lookup of itemId in providedItemsA (sel==0) or providedItemsB (sel!=0); returns the stored
// value or -1, descending DOOR_IN child zones.
int CDeskcppDoc::ZoneFindInIzxList(short zoneId, short itemId, int sel)
{
    short v;
    int result = -1;
    Zone *pZone = GetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    if (sel != 0)
    {
        int nCount = pZone->providedItemsB.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                v = (short)pZone->providedItemsB.GetAt(i);
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
        int nCount = pZone->providedItemsA.GetSize();
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                v = (short)pZone->providedItemsA.GetAt(i);
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
                    if (pObj->arg >= 0)
                        result = ZoneFindInIzxList(pObj->arg, itemId, sel);
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
int CDeskcppDoc::WorldgenFillQuestItemSpot(short zoneId, short itemId)
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
                        pObj->arg = v;
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
                    if (pObj->arg >= 0)
                        nResult = WorldgenFillQuestItemSpot(pObj->arg, itemId);
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
int CDeskcppDoc::WorldgenFillSpawn(short zoneId, short itemId)
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
                        pObj->arg = v;
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
                    if (pObj->arg >= 0)
                        nResult = WorldgenFillSpawn(pObj->arg, itemId);
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
int CDeskcppDoc::WorldgenPopulateGoalZone(short zoneId, short iA, short iB, short nOrder, short a5)
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
int CDeskcppDoc::WorldgenPlaceUsefulDropChainMaybe(short zoneId, short idx, short nOrder, short sel)
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
// Place itemId onto the first OBJ_LOCK (type 0xc) of zoneId if the zone's providedItemsA (sel==0)
// or providedItemsB lists it; records genCellItemA/BScratch + WorldgenAddZoneEntry; recurses into
// DOOR_IN children. nResult lives in EAX end-to-end (0 / 1 / last recursion result).
int CDeskcppDoc::WorldgenPlaceItemOnLock(short zoneId, int a2, int nVal, short itemId, int sel)
{
    int nResult = 0;
    int bFound = 0;
    if (zoneId >= 0)
    {
        Zone *pZone = (Zone *)zones.GetAt(zoneId);
        if (sel != 0)
        {
            int nCount = pZone->providedItemsB.GetSize();
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    if ((short)pZone->providedItemsB.GetAt(i) == itemId)
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
            int nCount = pZone->providedItemsA.GetSize();
            int i = 0;
            if (nCount > 0)
            {
                do
                {
                    if ((short)pZone->providedItemsA.GetAt(i) == itemId)
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
                        pObj->arg = itemId;
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
                        (nResult = WorldgenPlaceItemOnLock(pObj->arg, a2, nVal, itemId, sel)) == 1)
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
int CDeskcppDoc::WorldgenFillQuestItemSpot2Maybe(short zoneId, short a2, short nVal, unsigned short itemId)
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
                pObj->arg = itemId;
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
                        (nResult = WorldgenFillQuestItemSpot2Maybe(pObj->arg, a2, nVal, itemId)) == 1)
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
int CDeskcppDoc::WorldgenPlaceItemForLockChainMaybe(short zoneId, short idx, short nOrder, short sel)
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
// has EMPTY providedItemsA/genCandidateB lists), pick the target object type from the item tile's
// flags (TILE_WEAPON -> OBJ_THE_FORCE, TILE_LOCATOR -> OBJ_LOCATOR, TILE_ITEM ->
// OBJ_QUEST_ITEM_SPOT) and drop it on a random object of that type; else recurse into DOOR_IN
// children. Returns 1 on success (nPlaced).
int CDeskcppDoc::WorldgenPlaceUsefulObjectMaybe(short zoneId, short itemId, short nOrder)
{
    Zone *pZone;
    int bFound = 0;
    int nPlaced = 0;
    if (zoneId < 0 || CheckZoneItemsAvailable(zoneId) == 0 ||
        (pZone = (Zone *)zones.GetAt(zoneId)) == NULL ||
        pZone->providedItemsA.GetSize() > 0 || pZone->genCandidateB.GetSize() > 0)
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
            pObj->arg = itemId;
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
                    int vis = pObj->arg;
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
// the zone's providedItemsA (sel==0) or providedItemsB, push it onto the worklist (priority 5 for
// Zone.type==6, else nOrder) and register it; if the zone's IZAX lists exactly ONE required
// item, that item is deduped through uniqueRequiredItemsMaybe @0x234 — a repeat aborts with 0.
int CDeskcppDoc::WorldgenAssignTransitItemMaybe(short zoneId, short nOrder, int sel)
{
    CWordArray paItems;
    if (zoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    if (pZone == NULL)
        return 0;
    int nCount;
    if (sel != 0)
        nCount = pZone->providedItemsB.GetSize();
    else
        nCount = pZone->providedItemsA.GetSize();
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
                unsigned short v = pZone->providedItemsB.GetAt(i);
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
                unsigned short v = pZone->providedItemsA.GetAt(i);
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
int CDeskcppDoc::IsItemPlaced(short itemId)
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
void CDeskcppDoc::WorldgenPushZoneEntry(short zoneId, short val)
{
    WorldgenZoneEntry *pEntry = new WorldgenZoneEntry(zoneId, val);
    if (pEntry != NULL)
        worldgenPendingZones.InsertAt(0, pEntry, 1);
}

// FUNCTION: YODA 0x0041d740
// Linear-search the pending-zone worklist by zoneId; RemoveAt + delete the entry.
void CDeskcppDoc::RemoveZoneEntry(short zoneId)
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
void CDeskcppDoc::RemoveZoneEntry2(short zoneId)
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
void CDeskcppDoc::WorldgenAddZoneEntry(short zoneId, short val)
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

// FUNCTION: YODA 0x0041d8d0  [dial-breather: exact -> out at the MapZone.h de-dup -> back
//   EXACT at the Canvas.h de-dup (both 2026-07-07). Pure provenance/decl-set tie-break.]
// Zone unavailable/already placed: no such zone, or its id is in placedZoneIds.
int CDeskcppDoc::IsZoneUsed(short zoneId)
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
void CDeskcppDoc::AddPlacedZoneId(short zoneId)
{
    placedZoneIds.SetAtGrow(placedZoneIds.GetSize(), zoneId);
}

// FUNCTION: YODA 0x0041d940
// [EFFECTIVE-WIP, structurally 100% converged (every aligned row a pure register rename):
// residual = one callee-saved rotation (nBudget EBX<->EDI, y ESI<->EBX, pCell EDI<->ESI) and
// the up-arm's scratch pick (orig AX -> short-form cmp ax,imm16 saves 1 byte; ours CX) which
// shifts the four switch byte-tables by 4 via alignment — hence the scary raw byte diff.
// Cracked on the way: exit test is literally `nBudget <= 0` (test/jg), and the band picks are
// TERNARIES `rand() % 2 != 0 ? base : opp` (load/cond-overwrite/store — the two-statement form
// emits store-store). Joint pass.]
// Random-walk one difficulty ring of the quest path onto the plan grid. Per attempt: pick a
// random cell on the tier's border band (a vertical or horizontal edge, coin-flipped), and if
// it is empty and touches exactly-reachable path on one side, either extend a corridor fork
// (budgeted by rand()%forkDen<forkNum and the neighbor's grid order), continue a corridor
// through the cell, or stamp a plain path room; freshly placed rooms may be promoted to goal
// rooms. Runs until the room budget is spent or 145 attempts.
void CDeskcppDoc::WorldgenCarveQuestPath(int nTier, int nBudget, short *paPlanGrid, int maxGoals,
                                   int *pnGoals, int maxSplits, int *pnSplits, int *pnPlaced)
{
    int xBase, xOpp, yBase, yOpp, span, forkDen, forkNum, goalNum;
    short v;
    int nGoals = *pnGoals;
    int nSplits = *pnSplits;
    int nPlaced = *pnPlaced;
    switch (nTier)
    {
    case 2:
        xBase = 3;
        xOpp = 6;
        yBase = 3;
        yOpp = 6;
        span = 4;
        forkDen = 9;
        forkNum = 2;
        goalNum = 1;
        break;
    case 3:
        xBase = 2;
        xOpp = 7;
        yBase = 2;
        yOpp = 7;
        span = 6;
        forkDen = 4;
        goalNum = 3;
        forkNum = 2;
        break;
    case 4:
        xBase = 1;
        xOpp = 8;
        yBase = 1;
        yOpp = 8;
        span = 8;
        forkNum = 1;
        forkDen = 5;
        goalNum = 6;
        break;
    }
    int bDone = 0;
    int nAttempts = 0;
    do
    {
        int x, y;
        if (rand() % 2 != 0)
        {
            x = rand() % 2 != 0 ? xBase : xOpp;
            y = yBase + rand() % span;
        }
        else
        {
            y = rand() % 2 != 0 ? yBase : yOpp;
            x = xBase + rand() % span;
        }
        nAttempts++;
        short *pCell = &paPlanGrid[x + y * 10];
        if (*pCell == 0)
        {
            if (pCell[-1] != 0 && pCell[-1] != PLAN_BLOCKED)
            {
                v = pCell[-1];
                switch (v)
                {
                case PLAN_PATH:
                case PLAN_GOAL:
                case PLAN_START:
                case PLAN_ORDERED:
                    if (maxSplits > nSplits && rand() % forkDen < forkNum)
                    {
                        if (GetZoneGridOrder(x - 1, y) < nTier)
                        {
                            if ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                                (pCell[10] == 0 || pCell[10] == PLAN_BLOCKED))
                            {
                                *pCell = PLAN_FORK_E;
                                nPlaced++;
                                nBudget--;
                                nPlaced++;
                                pCell[-10] = PLAN_BLOCKED;
                                pCell[10] = PLAN_BLOCKED;
                                pCell[1] = PLAN_CORRIDOR;
                                pCell[-9] = PLAN_BLOCKED;
                                pCell[11] = PLAN_BLOCKED;
                                nSplits++;
                            }
                            else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                                     pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                            {
                                *pCell = PLAN_PATH;
                                nPlaced++;
                                nBudget--;
                            }
                        }
                    }
                    else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                             pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                    {
                        *pCell = PLAN_PATH;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                case PLAN_CORRIDOR:
                case PLAN_FORK_E:
                    if ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                        (pCell[10] == 0 || pCell[10] == PLAN_BLOCKED))
                    {
                        *pCell = PLAN_CORRIDOR;
                        pCell[-10] = PLAN_BLOCKED;
                        pCell[10] = PLAN_BLOCKED;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                }
            }
            else if (pCell[1] != 0 && pCell[1] != PLAN_BLOCKED)
            {
                v = pCell[1];
                switch (v)
                {
                case PLAN_PATH:
                case PLAN_GOAL:
                case PLAN_START:
                case PLAN_ORDERED:
                    if (maxSplits > nSplits && rand() % forkDen < forkNum)
                    {
                        if (GetZoneGridOrder(x + 1, y) < nTier)
                        {
                            if ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                                (pCell[10] == 0 || pCell[10] == PLAN_BLOCKED))
                            {
                                *pCell = PLAN_FORK_W;
                                nPlaced++;
                                nBudget--;
                                nPlaced++;
                                pCell[-10] = PLAN_BLOCKED;
                                pCell[10] = PLAN_BLOCKED;
                                pCell[-1] = PLAN_CORRIDOR;
                                pCell[-11] = PLAN_BLOCKED;
                                pCell[9] = PLAN_BLOCKED;
                                nSplits++;
                            }
                            else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                                     pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                            {
                                *pCell = PLAN_PATH;
                                nPlaced++;
                                nBudget--;
                            }
                        }
                    }
                    else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                             pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                    {
                        *pCell = PLAN_PATH;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                case PLAN_CORRIDOR:
                case PLAN_FORK_W:
                    if ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                        (pCell[10] == 0 || pCell[10] == PLAN_BLOCKED))
                    {
                        *pCell = PLAN_CORRIDOR;
                        pCell[-10] = PLAN_BLOCKED;
                        pCell[10] = PLAN_BLOCKED;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                }
            }
            else if (pCell[-10] != 0 && pCell[-10] != PLAN_BLOCKED)
            {
                v = pCell[-10];
                switch (v)
                {
                case PLAN_PATH:
                case PLAN_GOAL:
                case PLAN_START:
                case PLAN_ORDERED:
                    if (maxSplits > nSplits && rand() % forkDen < forkNum)
                    {
                        if (GetZoneGridOrder(x, y - 1) < nTier)
                        {
                            if ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                                (pCell[1] == 0 || pCell[1] == PLAN_BLOCKED))
                            {
                                *pCell = PLAN_FORK_S;
                                nPlaced++;
                                nBudget--;
                                nPlaced++;
                                pCell[-1] = PLAN_BLOCKED;
                                pCell[1] = PLAN_BLOCKED;
                                pCell[10] = PLAN_CORRIDOR;
                                pCell[9] = PLAN_BLOCKED;
                                pCell[11] = PLAN_BLOCKED;
                                nSplits++;
                            }
                            else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                                     pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                            {
                                *pCell = PLAN_PATH;
                                nPlaced++;
                                nBudget--;
                            }
                        }
                    }
                    else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                             pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                    {
                        *pCell = PLAN_PATH;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                case PLAN_CORRIDOR:
                case PLAN_FORK_S:
                    if ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                        (pCell[1] == 0 || pCell[1] == PLAN_BLOCKED))
                    {
                        *pCell = PLAN_CORRIDOR;
                        pCell[-1] = PLAN_BLOCKED;
                        pCell[1] = PLAN_BLOCKED;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                }
            }
            else if (pCell[10] != 0 && pCell[10] != PLAN_BLOCKED)
            {
                v = pCell[10];
                switch (v)
                {
                case PLAN_PATH:
                case PLAN_GOAL:
                case PLAN_START:
                case PLAN_ORDERED:
                    if (maxSplits > nSplits && rand() % forkDen < forkNum)
                    {
                        if (GetZoneGridOrder(x, y + 1) < nTier)
                        {
                            if ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                                (pCell[1] == 0 || pCell[1] == PLAN_BLOCKED))
                            {
                                *pCell = PLAN_FORK_N;
                                nPlaced++;
                                nBudget--;
                                nPlaced++;
                                pCell[-1] = PLAN_BLOCKED;
                                pCell[1] = PLAN_BLOCKED;
                                pCell[-10] = PLAN_CORRIDOR;
                                pCell[-11] = PLAN_BLOCKED;
                                pCell[-9] = PLAN_BLOCKED;
                                nSplits++;
                            }
                            else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                                     pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                            {
                                *pCell = PLAN_PATH;
                                nPlaced++;
                                nBudget--;
                            }
                        }
                    }
                    else if (pCell[-1] < PLAN_CORRIDOR && pCell[1] < PLAN_CORRIDOR &&
                             pCell[-10] < PLAN_CORRIDOR && pCell[10] < PLAN_CORRIDOR)
                    {
                        *pCell = PLAN_PATH;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                case PLAN_CORRIDOR:
                case PLAN_FORK_N:
                    if ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                        (pCell[1] == 0 || pCell[1] == PLAN_BLOCKED))
                    {
                        *pCell = PLAN_CORRIDOR;
                        pCell[-1] = PLAN_BLOCKED;
                        pCell[1] = PLAN_BLOCKED;
                        nPlaced++;
                        nBudget--;
                    }
                    break;
                }
            }
            if (*pCell == PLAN_PATH && maxGoals > nGoals && rand() % 8 < goalNum &&
                v != PLAN_GOAL && nTier > 2)
            {
                *pCell = PLAN_GOAL;
                nGoals++;
            }
        }
        if (nBudget <= 0)
            bDone++;
        if (nAttempts > 0x90)
            bDone++;
    } while (bDone == 0);
    *pnGoals = nGoals;
    *pnSplits = nSplits;
    *pnPlaced = nPlaced;
}

// FUNCTION: YODA 0x0041e350
// [EFFECTIVE-WIP, structurally converged (all four REP-STOS wall fills reproduced, unmerged):
// residual = per-case scan-register rotation (the original itself drifts roles across its four
// clone arms), two mirrored fill guards (jge/jle — orig drifts too), the bPlaced=0 store slot,
// and case-0's count reassociation staging. KEY CRACKS: horizontal wall fills must be indexed
// for-loops over a FRESH variable (a reused/live counter or explicit-count do-while defeats
// VC's rep-stos idiom); far arms are written EXPRESSION-style (nWid + nStart - 1 folds the
// trip count to nWid-1); bPlaced = 1 sits INSIDE each near/far arm (the after-if/else form
// lets the compiler cross-jump the two rep tails into one, which the original does not).]
// Stamp nCount ITEM_TO_PASS blockades onto the plan grid: per blockade, roll one of the four
// map edges, find the longest run of cells where the edge lane is empty and the lane beside it
// is empty-or-blocked, then write one PLAN_LOCK at a random end of the (clamped) run and
// PLAN_WALLs across the rest. Edge cases: 3-runs only qualify at the grid corners (and the
// south/east arms require >= 4, leaving their 3-run corner logic dead - sic, kept verbatim);
// up to 201 rolls per blockade.
void CDeskcppDoc::WorldgenPlaceBlockades(int nCount, short *paPlanGrid)
{
    if (nCount > 0)
    {
        int nLeft = nCount;
        do
        {
            int bDone = 0;
            int nTries = 0;
            do
            {
                int bPlaced = 0;
                int r, j;
                switch (rand() % 4)
                {
                case 0:
                {
                    int nRun = 0, nBest = 0, nStart = 0, i = 0;
                    int nWid;
                    short *p = &paPlanGrid[1];
                    do
                    {
                        if (p[-1] == 0 && (*p == 0 || *p == PLAN_BLOCKED))
                            nRun++;
                        else
                        {
                            if (nBest < nRun)
                            {
                                nBest = nRun;
                                nStart = i - nRun;
                            }
                            nRun = 0;
                        }
                        p += 10;
                        i++;
                    } while (i < 10);
                    if (nBest < nRun)
                    {
                        nBest = nRun;
                        nStart = 10 - nRun;
                    }
                    if (nBest < 3)
                        break;
                    if (nBest == 3 && (nStart == 0 || nStart == 7))
                        nWid = 2;
                    if (nBest == 3 && nStart > 0 && nStart < 7)
                        break;
                    if (nBest >= 4)
                        nWid = nBest - 2;
                    if (nWid > 4)
                        nWid = 4;
                    r = rand() % 2;
                    if (nStart > 0 && nWid + nStart <= 9)
                        nStart++;
                    if (r != 0)
                    {
                        paPlanGrid[nStart * 10] = PLAN_LOCK;
                        for (j = nStart + 1; j < nWid + nStart; j++)
                            paPlanGrid[j * 10] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    else
                    {
                        paPlanGrid[(nWid + nStart - 1) * 10] = PLAN_LOCK;
                        for (j = nStart; j < nWid + nStart - 1; j++)
                            paPlanGrid[j * 10] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    break;
                }
                case 1:
                {
                    int nRun = 0, nBest = 0, nStart = 0, i = 0;
                    int nWid;
                    short *p = &paPlanGrid[10];
                    do
                    {
                        if (p[-10] == 0 && (*p == 0 || *p == PLAN_BLOCKED))
                            nRun++;
                        else
                        {
                            if (nBest < nRun)
                            {
                                nBest = nRun;
                                nStart = i - nRun;
                            }
                            nRun = 0;
                        }
                        p++;
                        i++;
                    } while (i < 10);
                    if (nBest < nRun)
                    {
                        nBest = nRun;
                        nStart = 10 - nRun;
                    }
                    if (nBest < 3)
                        break;
                    if (nBest == 3 && (nStart == 0 || nStart == 7))
                        nWid = 2;
                    if (nBest == 3 && nStart > 0 && nStart < 7)
                        break;
                    if (nBest >= 4)
                        nWid = nBest - 2;
                    if (nWid > 4)
                        nWid = 4;
                    r = rand() % 2;
                    if (nStart > 0 && nWid + nStart <= 9)
                        nStart++;
                    if (r != 0)
                    {
                        paPlanGrid[nStart] = PLAN_LOCK;
                        for (j = nStart + 1; j < nWid + nStart; j++)
                            paPlanGrid[j] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    else
                    {
                        paPlanGrid[nWid + nStart - 1] = PLAN_LOCK;
                        for (j = nStart; j < nWid + nStart - 1; j++)
                            paPlanGrid[j] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    break;
                }
                case 2:
                {
                    int nRun = 0, nBest = 0, nStart = 0, i = 0;
                    int nWid;
                    short *p = &paPlanGrid[80];
                    do
                    {
                        if (p[10] == 0 && (*p == 0 || *p == PLAN_BLOCKED))
                            nRun++;
                        else
                        {
                            if (nBest < nRun)
                            {
                                nBest = nRun;
                                nStart = i - nRun;
                            }
                            nRun = 0;
                        }
                        p++;
                        i++;
                    } while (i < 10);
                    if (nBest < nRun)
                    {
                        nBest = nRun;
                        nStart = 10 - nRun;
                    }
                    if (nBest < 4)
                        break;
                    if (nBest == 3 && (nStart == 0 || nStart == 7))
                        nWid = 2;
                    if (nBest == 3 && nStart > 0 && nStart < 7)
                        break;
                    if (nBest >= 4)
                        nWid = nBest - 2;
                    if (nWid > 4)
                        nWid = 4;
                    r = rand() % 2;
                    if (nStart > 0 && nWid + nStart <= 9)
                        nStart++;
                    if (r != 0)
                    {
                        paPlanGrid[nStart + 90] = PLAN_LOCK;
                        for (j = nStart + 1; j < nWid + nStart; j++)
                            paPlanGrid[j + 90] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    else
                    {
                        paPlanGrid[nWid + nStart - 1 + 90] = PLAN_LOCK;
                        for (j = nStart; j < nWid + nStart - 1; j++)
                            paPlanGrid[j + 90] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    break;
                }
                case 3:
                {
                    int nRun = 0, nBest = 0, nStart = 0, i = 0;
                    int nWid;
                    short *p = &paPlanGrid[8];
                    do
                    {
                        if (p[1] == 0 && (*p == 0 || *p == PLAN_BLOCKED))
                            nRun++;
                        else
                        {
                            if (nBest < nRun)
                            {
                                nBest = nRun;
                                nStart = i - nRun;
                            }
                            nRun = 0;
                        }
                        p += 10;
                        i++;
                    } while (i < 10);
                    if (nBest < nRun)
                    {
                        nBest = nRun;
                        nStart = 10 - nRun;
                    }
                    if (nBest < 4)
                        break;
                    if (nBest == 3 && (nStart == 0 || nStart == 7))
                        nWid = 2;
                    if (nBest == 3 && nStart > 0 && nStart < 7)
                        break;
                    if (nBest >= 4)
                        nWid = nBest - 2;
                    if (nWid > 4)
                        nWid = 4;
                    r = rand() % 2;
                    if (nStart > 0 && nWid + nStart <= 9)
                        nStart++;
                    if (r != 0)
                    {
                        paPlanGrid[nStart * 10 + 9] = PLAN_LOCK;
                        for (j = nStart + 1; j < nWid + nStart; j++)
                            paPlanGrid[j * 10 + 9] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    else
                    {
                        paPlanGrid[(nWid + nStart - 1) * 10 + 9] = PLAN_LOCK;
                        for (j = nStart; j < nWid + nStart - 1; j++)
                            paPlanGrid[j * 10 + 9] = PLAN_WALL;
                        bPlaced = 1;
                    }
                    break;
                }
                default:
                    nTries++;
                }
                if (bPlaced != 0)
                    bDone++;
                else
                {
                    nTries++;
                    if (nTries > 200)
                        bDone++;
                }
            } while (bDone == 0);
            nLeft--;
        } while (nLeft != 0);
    }
}

// FUNCTION: YODA 0x0041e920
// Collect the zone's IZX2/IZX3 item ids not already placed, random-pick one; if none, recurse
// into DOOR_IN child zones.
int CDeskcppDoc::WorldgenPickItemFromZone(short zoneId, short a2, int sel)
{
    int result = -1;
    unsigned int n = 0;
    if (a2 == 0)   // sic: n is dead until reused below (kept by the original compiler)
        n = 1;
    Zone *pZone = (Zone *)zones.GetAt(zoneId);
    CWordArray candidates;
    if (sel != 0)
    {
        short nItems = (short)pZone->providedItemsB.GetSize();
        if (nItems > 0)
        {
            for (int i = 0; i < nItems; i++)
            {
                int v = pZone->providedItemsB.GetAt(i);
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
        short nItems = (short)pZone->providedItemsA.GetSize();
        if (nItems > 0)
        {
            for (int i = 0; i < nItems; i++)
            {
                int v = pZone->providedItemsA.GetAt(i);
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
                    result = WorldgenPickItemFromZone(pObj->arg, a2, sel);
                    if (result >= 0)
                        break;
                }
            }
        }
    }
    return result;
}

// FUNCTION: YODA 0x0041eab0
// [EFFECTIVE-WIP ~97% insn-identical: residual = the i-vs-nPlanet homing contest (orig memory-
// homes the short i and keeps nPlanet in EDX with compares-up-front arm layout; ours registers
// i in DI and memory-homes nPlanet). switch(nPlanet) probed: it spills `this` — chaotic, worse.
// The scan-local pair (n,p) x3 vs i for the callee-saved regs is the same usage-count contest
// as the Iact slot cycles. Raw scores inflated by the two whitelist byte tables. Joint pass.]
// Pick a puzzle for a quest node: phase 1 collects every puzzle whose PuzzleType matches the
// requested worldgen zone type (10->GOAL_PRIZE, 15->TRADE, 16->TRANSACTION, 9999->WORLD_MISSION)
// and is not already in the goal-tile list; the 9999 arm additionally screens against the
// current planet's story-replay history and a hardcoded per-planet goal-id whitelist. Phase 2
// shuffles the candidates and returns the first whose itemA matches (any WORLD_MISSION for
// 9999), or -1. nItem2/bFirst are accepted but never read (demo leftovers?).
int CDeskcppDoc::WorldgenSelectPuzzle(short nItem, short nItem2, short nType, int bFirst)
{
    short bDone = 0;
    short nCount = (short)puzzles.GetSize();
    CWordArray list;
    list.SetSize(0, -1);
    short i = 0;
    if (nCount > 0)
    {
        int nT = nType;
        do
        {
            Puzzle *pPuz = (Puzzle *)puzzles.GetAt(i);
            switch (nT)
            {
            case ZONE_TYPE_FINAL_ITEM:
                if (pPuz->nType == PUZZLE_TYPE_GOAL_PRIZE && IsTileInGoalList(i) == 0)
                    list.SetAtGrow(list.GetSize(), i);
                break;
            case ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK:
                if (pPuz->nType == PUZZLE_TYPE_TRADE && IsTileInGoalList(i) == 0)
                    list.SetAtGrow(list.GetSize(), i);
                break;
            case ZONE_TYPE_FIND_USEFUL_DROP:
                if (pPuz->nType == PUZZLE_TYPE_TRANSACTION && IsTileInGoalList(i) == 0)
                    list.SetAtGrow(list.GetSize(), i);
                break;
            case 9999:
                if (pPuz->nType == PUZZLE_TYPE_WORLD_MISSION)
                {
#ifdef GAME_INDY
                    // Indy has no planets and no per-planet goal-id whitelist, so every
                    // WORLD_MISSION puzzle is an eligible goal (phase 2 still screens it
                    // against the goal-tile list). Verified from DESKTOP.DAW: 15 type-3
                    // (WORLD_MISSION) puzzles exist; the Yoda planet-2 whitelist matched only 1.
                    list.SetAtGrow(list.GetSize(), i);
#else
                    int bFound = 0;
                    int nPlanet = currentPlanet;
                    if (nPlanet == 1)
                    {
                        int n = storyHistoryNevada.GetSize();
                        if (n > 0)
                        {
                            unsigned short *p = storyHistoryNevada.GetData();
                            do
                            {
                                if (*p == i)
                                    bFound = 1;
                                p++;
                                n--;
                            } while (n != 0);
                        }
                    }
                    else if (nPlanet == 2)
                    {
                        int n = storyHistoryAlaska.GetSize();
                        if (n > 0)
                        {
                            unsigned short *p = storyHistoryAlaska.GetData();
                            do
                            {
                                if (*p == i)
                                    bFound = 1;
                                p++;
                                n--;
                            } while (n != 0);
                        }
                    }
                    else if (nPlanet == 3)
                    {
                        int n = storyHistoryOregon.GetSize();
                        if (n > 0)
                        {
                            unsigned short *p = storyHistoryOregon.GetData();
                            do
                            {
                                if (*p == i)
                                    bFound = 1;
                                p++;
                                n--;
                            } while (n != 0);
                        }
                    }
                    if (bFound == 0 || nRequestedGoalItem >= 0)
                    {
                        // Per-planet goal-puzzle id whitelist (Nevada/Hoth/Endor). NOT a demo
                        // restriction — retail Yodesk (FUN_00421360, 9999 arm) has the IDENTICAL
                        // switch, so these are the real full-game mission-puzzle ids per planet.
                        // (The old "demo whitelist" comment here was a misread; verified 2026-07-08.)
                        if (nPlanet == 1)
                        {
                            switch (i)
                            {
                            case 0x55:
                            case 0x73:
                            case 0xb9:
                            case 0xc7:
                            case 0xc9:
                                list.SetAtGrow(list.GetSize(), i);
                            }
                        }
                        else if (nPlanet == 2)
                        {
                            switch (i)
                            {
                            case 0x67:
                            case 0x6c:
                            case 0x87:
                            case 0xbd:
                            case 0xc5:
                                list.SetAtGrow(list.GetSize(), i);
                            }
                        }
                        else if (nPlanet == 3)
                        {
                            if (i >= 0x83 && (i <= 0x86 || i == 0xc6))
                                list.SetAtGrow(list.GetSize(), i);
                        }
                    }
#endif
                }
                break;
            }
            i++;
        } while (nCount > i);
    }
    if (list.GetSize() <= 0)
        return -1;
    WorldgenShuffleList(&list);
    i = 0;
    int nSize = list.GetSize();
    do
    {
        unsigned short id = list.GetAt(i);
        Puzzle *pPuz = (Puzzle *)puzzles.GetAt(id);
        if (IsTileInGoalList(id) == 0)
        {
            switch (nType)
            {
            case ZONE_TYPE_FINAL_ITEM:
                if (pPuz->nType == PUZZLE_TYPE_GOAL_PRIZE && pPuz->itemA == nItem)
                    return list.GetAt(i);
                break;
            case ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK:
                if (pPuz->nType == PUZZLE_TYPE_TRADE && pPuz->itemA == nItem)
                    return list.GetAt(i);
                break;
            case ZONE_TYPE_FIND_USEFUL_DROP:
                if (pPuz->nType == PUZZLE_TYPE_TRANSACTION && pPuz->itemA == nItem)
                    return list.GetAt(i);
                break;
            case 9999:
                if (pPuz->nType == PUZZLE_TYPE_WORLD_MISSION)
                    return list.GetAt(i);
                break;
            }
        }
        if (bDone != 0)     // sic: dead — bDone is only ever set below, and the while()
            break;          // already exits; kept verbatim from the original control flow
        i++;
        if (i >= nSize)
            bDone++;
    } while (bDone == 0);
    return -1;
}

// FUNCTION: YODA 0x0041ef90
// [PARKED reg-alloc: the {bAnyEmpty, nMoved, k*2-offset} register contest + the unrotated
// init loop; structure fully converged (122/122 insns). Joint TU pass territory.]
// Fisher-Yates-style shuffle of a CWordArray: scatter each element into a random empty slot of a
// temp array (0xffff = empty sentinel), then copy back.
void CDeskcppDoc::WorldgenShuffleList(CWordArray *pList)
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

// FUNCTION: YODA 0x0041f120
// [EFFECTIVE-WIP ~97% insn-identical: residual = the this/i callee-saved 2-cycle (EDI/EBX
// swapped; proven source-SENSITIVE — a SetAt-arms probe flipped it while breaking the single
// store, so it is dial-coupled, not a fixed source shape) + the prologue push/store interleave
// and the movsx(a2)-above-the-branch schedule in the 0xf/0x10 cases, all downstream of that
// cycle. Raw byte/align scores are inflated by the two switch jump tables at +0x6a0. Structure
// fully mirrored: both switches, the shared EH fail-return, sunk accept blocks, pDst join.
// Joint-TU pass territory.]
// The per-quest-step placement hub: collect all unloaded-slot-free zones of the requested worldgen
// type on the current planet into a local list, shuffle it, then try each candidate with the
// type-specific placer until one accepts. Returns the chosen zone id, or 0xffff if none accepts.
unsigned short CDeskcppDoc::PlaceQuestNode(short nType, short a2, short a3, short a4, short a5,
                                     short nOrder, short a7)
{
    int bDone = 0;
    int nZones = zones.GetSize();
    int bFirst = 0;
    if (a2 == 0)
        bFirst = 1;
    short i = 0;
    CWordArray list;
    list.SetSize(0, -1);
    if (nZones > 0)
    {
        do
        {
            Zone *pZone = (Zone *)zones.GetAt(i);
            if (pZone != (Zone *)-1 && pZone->planet == currentPlanet)
            {
                switch (nType)
                {
                case ZONE_TYPE_ENEMY_TERRITORY:
                case ZONE_TYPE_FINAL_DESTINATION:
                case ZONE_TYPE_ITEM_FOR_ITEM:
                case ZONE_TYPE_FIND_USEFUL_NPC:
                case ZONE_TYPE_ITEM_TO_PASS:
                case ZONE_TYPE_FROM_ANOTHER_MAP:
                case ZONE_TYPE_TO_ANOTHER_MAP:
                case ZONE_TYPE_FINAL_ITEM:
                case ZONE_TYPE_MAP_START:
                case ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK:
                case ZONE_TYPE_FIND_USEFUL_DROP:
                    if ((short)pZone->type == nType)
                        list.SetAtGrow(list.GetSize(), i);
                    break;
                case ZONE_TYPE_FIND_USEFUL_BUILDING:
                    if (pZone->type == ZONE_TYPE_FIND_USEFUL_BUILDING ||
                        pZone->type == ZONE_TYPE_FIND_THE_FORCE)
                        list.SetAtGrow(list.GetSize(), i);
                    break;
                }
            }
            i++;
        } while (i < nZones);
    }
    if (list.GetSize() == 0)
        return 0xffff;
    WorldgenShuffleList(&list);
    short nPos = 0;
    short nCount = (short)list.GetSize();
    do
    {
        short nZoneId = list.GetAt(nPos);
        Zone *pZone = (Zone *)zones.GetAt(nZoneId);
        if (IsZoneUsed(nZoneId) == 0 ||
            (nType == ZONE_TYPE_FINAL_ITEM && nRequestedGoalItem > 0))
        {
            switch (nType)
            {
            case ZONE_TYPE_ENEMY_TERRITORY:
                if (genSkipTeleCheck == 0)
                {
                    if (pZone->type == ZONE_TYPE_ENEMY_TERRITORY)
                        return nZoneId;
                }
                else
                {
                    int nObjs = pZone->objects.GetSize();
                    if (nObjs <= 0)
                        return nZoneId;
                    int j = 0;
                    do
                    {
                        if (((ZoneObj *)pZone->objects.GetAt(j))->type == OBJ_TELEPORTER)
                            break;
                        j++;
                    } while (j < nObjs);
                    // sic: the scan result is unused — with the teleporter check on, only
                    // object-FREE zones are ever accepted (docs/engine-bugs.md #12)
                }
                break;
            case ZONE_TYPE_FINAL_DESTINATION:
                if (pZone->type == ZONE_TYPE_FINAL_DESTINATION &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_ITEM_FOR_ITEM:
                if (pZone->type == ZONE_TYPE_ITEM_FOR_ITEM &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_FIND_USEFUL_NPC:
                if (pZone->type == ZONE_TYPE_FIND_USEFUL_NPC &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_ITEM_TO_PASS:
                if (pZone->type == ZONE_TYPE_ITEM_TO_PASS &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_FROM_ANOTHER_MAP:
                if (pZone->type == ZONE_TYPE_FROM_ANOTHER_MAP &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_TO_ANOTHER_MAP:
                if (pZone->type == ZONE_TYPE_TO_ANOTHER_MAP &&
                    WorldgenAssignTransitItemMaybe(nZoneId, nOrder, 0) == 1)
                    return nZoneId;
                break;
            case ZONE_TYPE_FINAL_ITEM:
                if (pZone->type == ZONE_TYPE_FINAL_ITEM &&
                    ZoneRequiresItemMaybe(nZoneId, a4) == 1 &&
                    ZoneRequiresItemMaybe(nZoneId, a5) == 1)
                {
                    int nItemA = WorldgenPickItemFromZone(nZoneId, a2, 0);
                    int nItemB = WorldgenPickItemFromZone(nZoneId, a3, 1);
                    if (nItemA >= 0 && nItemB >= 0)
                    {
                        int nPuzA = WorldgenSelectPuzzle(nItemA, a4, ZONE_TYPE_FINAL_ITEM, bFirst);
                        if (nPuzA >= 0)
                            goalTileList.SetAtGrow(goalTileList.GetSize(), nPuzA);
                        int nPuzB = WorldgenSelectPuzzle(nItemB, a5, ZONE_TYPE_FINAL_ITEM, bFirst);
                        if (nPuzB >= 0)
                            goalTileList.SetAtGrow(goalTileList.GetSize(), nPuzB);
                        if (nPuzA >= 0 && nPuzB >= 0)
                        {
                            questItemsA.SetAt(a2, nPuzA);
                            questItemsB.SetAt(a3, nPuzB);
                            if (WorldgenPopulateGoalZone(nZoneId, a2, a3, nOrder, a7) == 1)
                            {
                                WorldgenAddZoneEntry(nItemA, nOrder);
                                WorldgenAddZoneEntry(nItemB, nOrder);
                                return nZoneId;
                            }
                            questItemsA.SetAt(a2, 0xffff);
                            questItemsB.SetAt(a3, 0xffff);
                        }
                    }
                }
                break;
            case ZONE_TYPE_MAP_START:
                if (pZone->type == ZONE_TYPE_MAP_START)
                    return nZoneId;
                break;
            case ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK:
                if (pZone->type == ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK &&
                    ZoneRequiresItemMaybe(nZoneId, a4) == 1)
                {
                    int nItem = WorldgenPickItemFromZone(nZoneId, a2, 0);
                    if (nItem >= 0)
                    {
                        int nPuz = WorldgenSelectPuzzle(nItem, a4,
                                                        ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK, bFirst);
                        if (nPuz >= 0)
                        {
                            unsigned short *pDst;
                            if (a7 != 0)
                                pDst = questItemsA.GetData();
                            else
                                pDst = questItemsB.GetData();
                            pDst[a2] = nPuz;
                            if (WorldgenPlaceItemForLockChainMaybe(nZoneId, a2, nOrder, a7) == 1)
                            {
                                goalTileList.SetAtGrow(goalTileList.GetSize(), nPuz);
                                WorldgenAddZoneEntry(nItem, nOrder);
                                return nZoneId;
                            }
                        }
                    }
                }
                break;
            case ZONE_TYPE_FIND_USEFUL_DROP:
                if (pZone->type == ZONE_TYPE_FIND_USEFUL_DROP &&
                    ZoneRequiresItemMaybe(nZoneId, a4) == 1)
                {
                    int nItem = WorldgenPickItemFromZone(nZoneId, a2, 0);
                    if (nItem >= 0)
                    {
                        int nPuz = WorldgenSelectPuzzle(nItem, a4,
                                                        ZONE_TYPE_FIND_USEFUL_DROP, bFirst);
                        if (nPuz >= 0)
                        {
                            unsigned short *pDst;
                            if (a7 != 0)
                                pDst = questItemsA.GetData();
                            else
                                pDst = questItemsB.GetData();
                            pDst[a2] = nPuz;
                            if (WorldgenPlaceUsefulDropChainMaybe(nZoneId, a2, nOrder, a7) == 1)
                            {
                                goalTileList.SetAtGrow(goalTileList.GetSize(), nPuz);
                                WorldgenAddZoneEntry(nItem, nOrder);
                                return nZoneId;
                            }
                        }
                    }
                }
                break;
            case ZONE_TYPE_FIND_USEFUL_BUILDING:
                if ((pZone->type == ZONE_TYPE_FIND_USEFUL_BUILDING ||
                     pZone->type == ZONE_TYPE_FIND_THE_FORCE) &&
                    WorldgenPlaceUsefulObjectMaybe(nZoneId, a4, nOrder) == 1)
                    return nZoneId;
                break;
            }
        }
        nPos++;
        if (nPos >= nCount)
            bDone++;
    } while (bDone == 0);
    return 0xffff;
}

// FUNCTION: YODA 0x0041f830
// Recursively verify a quest sub-tree is satisfiable: object types 6-8 must reference items not
// already placed; DOOR_IN (9) recurses into the child zone.
int CDeskcppDoc::CheckZoneItemsAvailable(short zoneId)
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
                if (pObj->arg >= 0 && IsItemPlaced(pObj->arg) != 0)
                    ok = 0;
                break;
            case 9:
                if (pObj->arg >= 0)
                    ok = CheckZoneItemsAvailable(pObj->arg);
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
void CDeskcppDoc::WorldgenCollectZoneRefs(short zoneId)
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
                    if (pObj->arg >= 0)
                        WorldgenAddZoneEntry(pObj->arg, -1);
                    break;
                case 9:
                    if (pObj->arg >= 0)
                        WorldgenCollectZoneRefs(pObj->arg);
                    break;
                }
            }
        }
    }
}

// FUNCTION: YODA 0x0041f960
// [TRANSCRIBED-WIP ~90% row-aligned (2018/2032 insns): semantically complete, all phases
// mirrored from the CFG. Confirmed source shapes: ALL planet dispatches are switch()es
// (compares-up-front); worldSize params via switch with per-arm store order; budget exprs are
// `rand() % (hi - lo + 1) + lo` (rand first); the stray-pass pick keeps TWO flags
// (bHoriz fall-arm on rand()%2!=0, bVert on else) with branchless ?9:0 edge ternaries; grid
// cells are addressed by the FULL `x + y * 10` expression inlined per use (CSE pre-doubles
// the offset — a named nCell or pointer defeats it, idxprobe-proven); the vehicle-pass unwind
// is the else-arm of if(bFound). Residuals: global this=EBX(orig)/EBP(ours) rotation and the
// dx/dy homing (orig EDI/ESI regs), the completionCount arm-placement flip, mapGrid walker
// anchored at .id (orig hoists &mapGrid[0].id), A/B-loop scratch-store interleave, and the
// CarveQuestPath arg-push scheduling. The A-retry do-while (nRetry < 0xc9) and the B-chain
// goal arm (nStepsB == nStepB, never true) are dead-ish original structures kept verbatim;
// the !bFoundStep fallbacks read the STALE stray-pass `x` (function-scoped on purpose).]
// The worldgen driver. Seeds the RNG, resets all quest state, carves the three difficulty
// rings of the quest path onto a local plan grid (budgets scaled by worldSize), attaches a
// few stray border rooms, stamps the blockades, numbers the path (BuildQuestPath), then walks
// the two quest chains backwards placing a zone per step (goal node first, then alternating
// TRADE/DROP nodes), converts the goal cells to vehicle pairs, consumes the remaining plan
// tokens (START->MAP_START, forks->transit types 2..5, else ENEMY_TERRITORY), and finishes
// with the puzzle pass. Any placement failure resets everything and returns 0.
int CDeskcppDoc::Generate(unsigned int nSeed)
{
    short aOrder[100];
    short aPlan[100];
    int x, y;
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *pView = NULL;
    if (pos != NULL)
        pView = (CDeskcppView *)GetNextView(pos);
    pView->bBusy = 1;
    worldSeed = nSeed;
    gameState = 0;
    unk248.SetSize(0, -1);
    short bFirst = 1;
    totalZones = 0;
    worldSeed = nSeed;
    srand(abs((int)nSeed));
    int maxGoals = rand() % 3;
    int maxSplits = rand() % 4;
    rand();
    int nGoals = 0;
    int nSplits = 0;
    int nPlaced = 0;
    int nSeedX = rand() % 2 + 4;
    int nSeedY = rand() % 2 + 4;
    for (y = 0; y < 10; y++)
    {
        memset(&aOrder[y * 10], 0xff, 20);
        memset(&aPlan[y * 10], 0, 20);
        for (x = 0; x < 10; x++)
        {
            mapGrid[y * 10 + x].id = -1;
            mapGrid[y * 10 + x].cellQuestSlot0 = -1;
            mapGrid[y * 10 + x].cellItemC = -1;
            mapGrid[y * 10 + x].cellItemA = -1;
            mapGrid[y * 10 + x].cellQuestSlot1 = -1;
            mapGrid[y * 10 + x].cellQuestSlot5 = -1;
            mapGrid[y * 10 + x].cellItemB = -1;
            mapGrid[y * 10 + x].cellQuestSlot6 = -1;
            mapGrid[y * 10 + x].flagSolved = 0;
            mapGrid[y * 10 + x].flagA = 0;
            mapGrid[y * 10 + x].flagB = 0;
            mapGrid[y * 10 + x].flagC = 0;
            mapGrid[y * 10 + x].flagD = 0;
            mapGrid[y * 10 + x].field30 = -1;
        }
    }
    questItemsA.SetSize(0, -1);
    questItemsB.SetSize(0, -1);
    uniqueRequiredItemsMaybe.SetSize(0, -1);
    storyHistoryNevada.SetSize(0, -1);
    storyHistoryAlaska.SetSize(0, -1);
    storyHistoryOregon.SetSize(0, -1);
    int n = worldgenPendingZones.GetSize();
    int i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenPendingZones.SetSize(0, -1);
    n = worldgenRefZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenRefZones.SetSize(0, -1);
    goalTileList.SetSize(0, -1);
    placedZoneIds.SetSize(0, -1);
    if (nRequestedGoalItem < 0)
    {
        Nop1();
        switch (currentPlanet)
        {
        case 1:
            LoadStoryHistoryNevada();
            break;
        case 2:
            LoadStoryHistoryAlaska();
            break;
        case 3:
            LoadStoryHistoryOregon();
            break;
        }
    }
    int nOldPlaced = placedZoneIds.GetSize();
    switch (currentPlanet)
    {
    case 1:
        if (storyHistoryNevada.GetSize() > 3)
            storyHistoryNevada.RemoveAt(0, 1);
        break;
    case 2:
        if (storyHistoryAlaska.GetSize() > 3)
            storyHistoryAlaska.RemoveAt(0, 1);
        break;
    case 3:
        if (storyHistoryOregon.GetSize() > 3)
            storyHistoryOregon.RemoveAt(0, 1);
        break;
    }
    aPlan[44] = PLAN_PATH;
    aPlan[45] = PLAN_PATH;
    aPlan[54] = PLAN_PATH;
    aPlan[55] = PLAN_PATH;
    int nT2Hi, nT2Lo, nT3Hi, nT3Lo, nT4Hi, nT4Lo, nXtraHi, nXtraLo;
    switch (worldSize)
    {
    case 1:
        nT2Hi = 8;
        nT2Lo = 5;
        nT3Hi = 6;
        nT3Lo = 4;
        nT4Hi = 1;
        nT4Lo = 1;
        nXtraHi = 1;
        nXtraLo = 1;
        break;
    case 3:
    {
        nT2Hi = 0xc;
        nT2Lo = 6;
        nT4Hi = 0xb;
        nT3Hi = 0xc;
        nT3Lo = 6;
        nT4Lo = 6;
        nXtraLo = 4;
        nXtraHi = 0xb;
        break;
    }
    default:
        nT2Hi = 9;
        nT2Lo = 5;
        nT4Hi = 8;
        nT3Hi = 9;
        nT3Lo = 5;
        nXtraHi = 8;
        nT4Lo = 4;
        nXtraLo = 3;
    }
    nPlaced = 4;
    aPlan[nSeedX + nSeedY * 10] = PLAN_START;
    int nBudget = rand() % (nT2Hi - nT2Lo + 1) + nT2Lo + maxSplits + maxGoals;
    if (nBudget > 0xc)
        nBudget = 0xc;
    WorldgenCarveQuestPath(2, nBudget, aPlan, maxGoals, &nGoals, maxSplits, &nSplits, &nPlaced);
    WorldgenCarveQuestPath(3, rand() % (nT3Hi - nT3Lo + 1) + nT3Lo, aPlan, maxGoals, &nGoals,
                           maxSplits, &nSplits, &nPlaced);
    WorldgenCarveQuestPath(4, rand() % (nT4Hi - nT4Lo + 1) + nT4Lo, aPlan, maxGoals, &nGoals,
                           maxSplits, &nSplits, &nPlaced);
    int nXtra = nXtraLo + rand() % (nXtraHi - nXtraLo + 1);
    int bDone = 0;
    int nTries = 0;
    do
    {
        int dy = 0;
        int dx = 0;
        int bHoriz = 0;
        int bVert = 0;
        if (rand() % 2 != 0)
        {
            bHoriz = 1;
            x = rand() % 2 == 0 ? 9 : 0;
            y = rand() % 10;
        }
        else
        {
            bVert = 1;
            y = rand() % 2 == 0 ? 9 : 0;
            x = rand() % 10;
        }
        nTries++;
        if (aPlan[x + y * 10] == 0)
        {
            if (bHoriz != 0)
            {
                if (x == 0)
                {
                    if (aPlan[x + y * 10 + 1] != 0 && aPlan[x + y * 10 + 1] != PLAN_BLOCKED)
                    {
                        dx = -1;
                        dy = 0;
                    }
                }
                else if (x == 9 && aPlan[x + y * 10 - 1] != 0 && aPlan[x + y * 10 - 1] != PLAN_BLOCKED)
                {
                    dx = 1;
                    dy = 0;
                }
            }
            else if (bVert != 0)
            {
                if (y == 0)
                {
                    if (aPlan[x + y * 10 + 10] != 0 && aPlan[x + y * 10 + 10] != PLAN_BLOCKED)
                    {
                        dx = 0;
                        dy = -1;
                    }
                }
                else if (y == 9 && aPlan[x + y * 10 - 10] != 0 && aPlan[x + y * 10 - 10] != PLAN_BLOCKED)
                {
                    dx = 0;
                    dy = 1;
                }
            }
            if (dx != 0 || dy != 0)
            {
                int v = aPlan[(y - dy) * 10 + (x - dx)];
                switch (v)
                {
                case PLAN_PATH:
                case PLAN_GOAL:
                case PLAN_START:
                case PLAN_ORDERED:
                    aPlan[x + y * 10] = PLAN_PATH;
                    nPlaced++;
                    nXtra--;
                    break;
                case PLAN_CORRIDOR:
                    aPlan[x + y * 10] = PLAN_CORRIDOR;
                    if (dx == 0)
                    {
                        if (x > 0)
                            aPlan[x + y * 10 - 1] = PLAN_BLOCKED;
                        if (x < 9)
                            aPlan[x + y * 10 + 1] = PLAN_BLOCKED;
                    }
                    else if (dy == 0)
                    {
                        if (y > 0)
                            aPlan[x + y * 10 - 10] = PLAN_BLOCKED;
                        if (y < 9)
                            aPlan[x + y * 10 + 10] = PLAN_BLOCKED;
                    }
                    break;
                case PLAN_FORK_W:
                    if (dx == -1 &&
                        (y == 0 || aPlan[x + y * 10 - 10] == 0 || aPlan[x + y * 10 - 10] > PLAN_FORK_N) &&
                        (y == 9 || aPlan[x + y * 10 + 10] == 0 || aPlan[x + y * 10 + 10] > PLAN_FORK_N))
                    {
                        aPlan[x + y * 10] = PLAN_CORRIDOR;
                        if (y > 0)
                            aPlan[x + y * 10 - 10] = PLAN_BLOCKED;
                        if (y < 9)
                            aPlan[x + y * 10 + 10] = PLAN_BLOCKED;
                        nPlaced++;
                        nXtra--;
                    }
                    break;
                case PLAN_FORK_E:
                    if (dx == 1 &&
                        (y == 0 || aPlan[x + y * 10 - 10] == 0 || aPlan[x + y * 10 - 10] > PLAN_FORK_N) &&
                        (y == 9 || aPlan[x + y * 10 + 10] == 0 || aPlan[x + y * 10 + 10] > PLAN_FORK_N))
                    {
                        aPlan[x + y * 10] = PLAN_CORRIDOR;
                        if (y > 0)
                            aPlan[x + y * 10 - 10] = PLAN_BLOCKED;
                        if (y < 9)
                            aPlan[x + y * 10 + 10] = PLAN_BLOCKED;
                        nPlaced++;
                        nXtra--;
                    }
                    break;
                case PLAN_FORK_N:
                    if (dy == -1 &&
                        (x == 0 || aPlan[x + y * 10 - 1] == 0 || aPlan[x + y * 10 - 1] > PLAN_FORK_N) &&
                        (x == 9 || aPlan[x + y * 10 + 1] == 0 || aPlan[x + y * 10 + 1] > PLAN_FORK_N))
                    {
                        aPlan[x + y * 10] = PLAN_CORRIDOR;
                        if (x > 0)
                            aPlan[x + y * 10 - 1] = PLAN_BLOCKED;
                        if (x < 9)
                            aPlan[x + y * 10 + 1] = PLAN_BLOCKED;
                        nPlaced++;
                        nXtra--;
                    }
                    break;
                case PLAN_FORK_S:
                    if (dy == 1 &&
                        (x == 0 || aPlan[x + y * 10 - 1] == 0 || aPlan[x + y * 10 - 1] > PLAN_FORK_N) &&
                        (x == 9 || aPlan[x + y * 10 + 1] == 0 || aPlan[x + y * 10 + 1] > PLAN_FORK_N))
                    {
                        aPlan[x + y * 10] = PLAN_CORRIDOR;
                        if (x > 0)
                            aPlan[x + y * 10 - 1] = PLAN_BLOCKED;
                        if (x < 9)
                            aPlan[x + y * 10 + 1] = PLAN_BLOCKED;
                        nPlaced++;
                        nXtra--;
                    }
                    break;
                }
            }
        }
        if (nXtra <= 0)
            bDone++;
        if (nTries > 400)
            bDone++;
    } while (bDone == 0);
    WorldgenPlaceBlockades(nGoals, aPlan);
    n = zones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            Zone *pZone = (Zone *)zones.GetAt(i);
            if (pZone != (Zone *)-1)
                pZone->activatedFlag = 0;
            i++;
            n--;
        } while (n != 0);
    }
    int nTotalOrder = BuildQuestPathMaybe(aPlan, aOrder);
    if (questItemsA.GetSize() > 0)
        questItemsA.SetSize(0, -1);
    if (questItemsB.GetSize() > 0)
        questItemsB.SetSize(0, -1);
    int nStepsA, nStepsB;
    if (nTotalOrder % 2 == 0)
    {
        nStepsB = nTotalOrder / 2;
        nStepsA = nStepsB + 1;
    }
    else
    {
        nStepsA = (nTotalOrder + 1) / 2;
        nStepsB = nStepsA;
    }
    questItemsA.SetSize(nStepsA + 1, -1);
    questItemsB.SetSize(nStepsB + 1, -1);
    if (completionCount < 1)
    {
        storyHistoryAlaska.SetAtGrow(storyHistoryAlaska.GetSize(), 0xbd);
        storyHistoryAlaska.SetAtGrow(storyHistoryAlaska.GetSize(), 0xc5);
    }
    else if (completionCount < 10)
    {
        storyHistoryAlaska.SetAtGrow(storyHistoryAlaska.GetSize(), 0xc5);
    }
#if defined(YODA_FULL) || defined(GAME_INDY)
    // Full game / Indy: pick the goal puzzle dynamically instead of the demo's fixed Hoth goal.
    // On a replay nRequestedGoalItem already holds the requested goal; otherwise select a
    // WORLD_MISSION puzzle (WorldgenSelectPuzzle, nType 9999 — Yoda screens it against the current
    // planet's story-replay history + goal-id whitelist; Indy accepts any WORLD_MISSION). A -1
    // result = no eligible goal for this seed, so fail and let the retry loop try a fresh seed.
    // Retail Yodesk Generate (0x00422210, goal region) did exactly this; the demo replaced the
    // whole selection with the constant 0x6c (puzzle 108).
    int goal = nRequestedGoalItem;
    if (goal < 0)
    {
        goal = WorldgenSelectPuzzle(-1, -1, 9999, 0);
        if (goal < 0)
            return 0;
    }
#else
    int goal = 0x6c;                      // demo hardcode: Hoth's fixed goal (puzzle 108)
#endif
    questItemsA.SetAt(nStepsA, goal);
    questItemsB.SetAt(nStepsB, goal);
    goalItemTileId = goal;
    Puzzle *pPuz = (Puzzle *)puzzles.GetAt(goal);
    startItem = pPuz->itemA;
    startItem2Maybe = pPuz->itemB;
    goalTileList.SetAtGrow(goalTileList.GetSize(), goal);
    nCurrentGoalItem = goal;
    switch (currentPlanet)
    {
    case 1:
        storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), goal);
        break;
    case 2:
        storyHistoryAlaska.SetAtGrow(storyHistoryAlaska.GetSize(), goal);
        break;
    case 3:
        storyHistoryOregon.SetAtGrow(storyHistoryOregon.GetSize(), goal);
        break;
    }
    int j;
    for (i = 0; i < 10; i++)
        for (j = 0; j < 10; j++)
            apZoneGrid[i * 10 + j] = NULL;
    // vehicle pass: turn each unclaimed GOAL cell into a FROM/TO_ANOTHER_MAP pair, the TO
    // side landing on a border LOCK cell
    {
        int vy, vx;
        for (vy = 0; vy < 10; vy++)
        {
            for (vx = 0; vx < 10; vx++)
            {
                genCellQuestSlot6Scratch = -1;
                genCellItemAScratch = -1;
                genCellItemCScratch = -1;
                if (aPlan[vy * 10 + vx] == PLAN_GOAL && apZoneGrid[vy * 10 + vx] == NULL)
                {
                    int nZone = (short)PlaceQuestNode(6, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(vx, vy), 1);
                    int nDest = -1;
                    if (nZone >= 0)
                    {
                        mapGrid[vy * 10 + vx].id = (unsigned short)nZone;
                        mapGrid[vy * 10 + vx].zoneType = ZONE_TYPE_FROM_ANOTHER_MAP;
                        mapGrid[vy * 10 + vx].cellItemA = (short)genCellItemAScratch;
                        Zone *pZone = (Zone *)zones.GetAt((short)nZone);
                        int k = 0;
                        int nObjs = pZone->objects.GetSize();
                        if (nObjs > 0)
                        {
                            do
                            {
                                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(k);
                                if (pObj->type == OBJ_VEHICLE_TO)
                                {
                                    nDest = (short)pObj->arg;
                                    break;
                                }
                                k++;
                            } while (k < nObjs);
                        }
                        int bFound = 0;
                        if (nDest >= 0)
                        {
                            int xLock = 0;
                            int yLock = 0;
                            Zone **pp = apZoneGrid;
                            short *p = aPlan;
                            k = 0;
                            do
                            {
                                if (*p == PLAN_LOCK && *pp == NULL)
                                {
                                    bFound = 1;
                                    yLock = k;
                                    break;
                                }
                                pp += 10;
                                p += 10;
                                k++;
                            } while (p < &aPlan[100]);
                            if (!bFound)
                            {
                                pp = apZoneGrid + yLock * 10;
                                p = aPlan + yLock * 10;
                                k = 0;
                                do
                                {
                                    if (*p == PLAN_LOCK && *pp == NULL)
                                    {
                                        bFound = 1;
                                        xLock = k;
                                        break;
                                    }
                                    pp++;
                                    p++;
                                    k++;
                                } while (k < 10);
                                if (!bFound)
                                {
                                    pp = apZoneGrid + 9;
                                    p = &aPlan[9];
                                    xLock = 9;
                                    k = 0;
                                    do
                                    {
                                        if (*p == PLAN_LOCK && *pp == NULL)
                                        {
                                            bFound = 1;
                                            yLock = k;
                                            break;
                                        }
                                        pp += 10;
                                        p += 10;
                                        k++;
                                    } while (p < &aPlan[109]);
                                }
                            }
                            if (!bFound)
                            {
                                pp = apZoneGrid + 90;
                                p = &aPlan[90];
                                yLock = 9;
                                xLock = 0;
                                k = 0;
                                do
                                {
                                    if (*p == PLAN_LOCK && *pp == NULL)
                                    {
                                        bFound = 1;
                                        xLock = k;
                                        break;
                                    }
                                    pp++;
                                    p++;
                                    k++;
                                } while (p < &aPlan[100]);
                            }
                            if (bFound)
                            {
                                if (IsZoneUsed((short)nDest) == 0)
                                {
                                    int nCell2 = xLock + yLock * 10;
                                    apZoneGrid[nCell2] = (Zone *)zones.GetAt(nDest);
                                    mapGrid[nCell2].id = (short)nDest;
                                    mapGrid[nCell2].zoneType = ZONE_TYPE_TO_ANOTHER_MAP;
                                    mapGrid[nCell2].cellItemA = (short)genCellItemAScratch;
                                    apZoneGrid[vy * 10 + vx] = pZone;
                                    AddPlacedZoneId(nZone);
                                    AddPlacedZoneId((short)nDest);
                                }
                            }
                            else
                            {
                                RemoveZoneEntry((short)genCellItemAScratch);
                                aPlan[vy * 10 + vx] = PLAN_PATH;
                                apZoneGrid[vy * 10 + vx] = NULL;
                                mapGrid[vy * 10 + vx].id = -1;
                                mapGrid[vy * 10 + vx].zoneType = -1;
                                mapGrid[vy * 10 + vx].cellItemA = -1;
                            }
                        }
                    }
                }
            }
        }
    }
    // quest chain A (bFirst = 1): steps nStepsA..1, walking the order grid backwards
    int nStepA = nStepsA;
    int nStepB = nStepsB;
    int nOrderVal = nTotalOrder;
    int bFoundStep = 0;
    while (nStepA > 0)
    {
        genZoneTypeScratch = -1;
        genCellItemCScratch = -1;
        genCellQuestSlot5Scratch = -1;
        genCellItemAScratch = -1;
        genCellItemBScratch = -1;
        genCellQuestSlot6Scratch = -1;
        genCellQuestSlot0Scratch = -1;
        genCellQuestSlot1Scratch = -1;
        int nItemIdx = questItemsA.GetAt(nStepA);
        bFoundStep = 0;
        int i2 = 0, j2 = 0;
        int nFound = 0;
        short *pRow = aOrder;
        do
        {
            i2 = 0;
            short *p2 = pRow;
            do
            {
                if (*p2 - nOrderVal == -1)
                {
                    nFound++;
                    break;
                }
                p2++;
                i2++;
            } while (i2 < 10);
            if (nFound != 0)
                break;
            pRow += 10;
            j2++;
        } while (pRow < &aOrder[100]);
        if (nFound != 0)
        {
            int nZone = -1;
            int nRetry = 0;
            Puzzle *pP = (Puzzle *)puzzles.GetAt(nItemIdx);
            short nItemA = pP->itemA;
            short nItemB = pP->itemB;
            do
            {
                if (nZone >= 0)
                    break;
                if (nOrderVal == nTotalOrder)
                {
                    nZone = (short)PlaceQuestNode(10, nStepA - 1, nStepB - 1, nItemA, nItemB,
                                                  (short)GetZoneGridOrder(i2, j2), 1);
                    if (nZone < 0)
                        goto fail_a;
                    genZoneTypeScratch = 10;
                    genCellQuestSlot0Scratch = nStepA - 1;
                    genCellQuestSlot1Scratch = nStepB - 1;
                }
                else
                {
                    int nType = rand() % 2 == 0 ? 0x10 : 0xf;
                    short nStep16 = (short)(nStepA - 1);
                    nZone = (short)PlaceQuestNode((short)nType, nStep16, -1, nItemA, -1,
                                                  (short)GetZoneGridOrder(i2, j2), 1);
                    if (nZone >= 0)
                    {
                        genZoneTypeScratch = nType;
                        genCellQuestSlot0Scratch = nStepA - 1;
                    }
                    else if (nType == 0x10)
                    {
                        nZone = (short)PlaceQuestNode(0xf, nStep16, -1, nItemA, -1,
                                                      (short)GetZoneGridOrder(i2, j2), 1);
                        if (nZone < 0)
                            goto fail_a;
                        genZoneTypeScratch = 0xf;
                        genCellQuestSlot0Scratch = nStepA - 1;
                    }
                    else
                    {
                        nZone = (short)PlaceQuestNode(0x10, nStep16, -1, nItemA, -1,
                                                      (short)GetZoneGridOrder(i2, j2), 1);
                        if (nZone < 0)
                            goto fail_a;
                        genZoneTypeScratch = 0x10;
                        genCellQuestSlot0Scratch = nStepA - 1;
                    }
                }
                AddPlacedZoneId(nZone);
                if (nZone < 0)
                    goto fail_a;
                int nCell = i2 + j2 * 10;
                bFoundStep = 1;
                mapGrid[nCell].zoneType = genZoneTypeScratch;
                mapGrid[nCell].cellItemC = (short)genCellItemCScratch;
                mapGrid[nCell].cellQuestSlot5 = (short)genCellQuestSlot5Scratch;
                mapGrid[nCell].cellItemA = (short)genCellItemAScratch;
                mapGrid[nCell].cellItemB = (short)genCellItemBScratch;
                mapGrid[nCell].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
                mapGrid[nCell].cellQuestSlot0 = (short)genCellQuestSlot0Scratch;
                mapGrid[nCell].cellQuestSlot1 = (short)genCellQuestSlot1Scratch;
                mapGrid[nCell].field30 = 1;
                apZoneGrid[nCell] = (Zone *)zones.GetAt(nZone);
                mapGrid[nCell].id = (unsigned short)nZone;
                if (nStepA == 1)
                    WorldgenPushZoneEntry((short)genCellItemAScratch,
                                          (short)GetZoneGridOrder(i2, j2));
                nRetry++;
            } while (nRetry < 0xc9);
        }
        if (bFoundStep == 0)
        {
            short nZ = PlaceQuestNode(1, -1, -1, -1, -1,
                                      (short)GetZoneGridOrder(x, nOrderVal), 1);
            if (nZ >= 0)
            {
                int nCell = i2 + j2 * 10;
                mapGrid[nCell].zoneType = genZoneTypeScratch;
                apZoneGrid[nCell] = (Zone *)zones.GetAt(nZ);
                mapGrid[nCell].id = nZ;
                AddPlacedZoneId(nZ);
            }
        }
        nOrderVal--;
        nStepB--;
        nStepA--;
    }
    // quest chain B (bFirst = 0): steps nStepsB-1..1
    nStepB = nStepsB - 1;
    if (nStepB > 0)
    {
        bFirst = 0;
        do
        {
            genZoneTypeScratch = -1;
            genCellItemCScratch = -1;
            genCellQuestSlot5Scratch = -1;
            genCellItemAScratch = -1;
            genCellItemBScratch = -1;
            genCellQuestSlot6Scratch = -1;
            genCellQuestSlot0Scratch = -1;
            genCellQuestSlot1Scratch = -1;
            int nItemIdx = questItemsB.GetAt(nStepB);
            bFoundStep = 0;
            int i2 = 0, j2 = 0;
            int nFound = 0;
            short *pRow = aOrder;
            do
            {
                i2 = 0;
                short *p2 = pRow;
                do
                {
                    if (*p2 - nOrderVal == -1)
                    {
                        nFound++;
                        break;
                    }
                    p2++;
                    i2++;
                } while (i2 < 10);
                if (nFound != 0)
                    break;
                pRow += 10;
                j2++;
            } while (pRow < &aOrder[100]);
            if (nFound != 0)
            {
                int nZone = -1;
                int nRetry = 0;
                short nItemA = ((Puzzle *)puzzles.GetAt(nItemIdx))->itemA;
                do
                {
                    if (nZone >= 0)
                        break;
                    if (nStepsB == nStepB)
                    {
                        nZone = (short)PlaceQuestNode(10, nOrderVal - 1, -1, nItemA, -1,
                                                      (short)GetZoneGridOrder(i2, j2), 0);
                        if (nZone < 0)
                            goto fail_b;
                        genZoneTypeScratch = 10;
                        genCellQuestSlot0Scratch = nOrderVal - 1;
                    }
                    else
                    {
                        int nType = rand() % 2 == 0 ? 0xf : 0x10;
                        short nStep16 = (short)(nStepB - 1);
                        nZone = (short)PlaceQuestNode((short)nType, nStep16, -1, nItemA, -1,
                                                      (short)GetZoneGridOrder(i2, j2), 0);
                        if (nZone >= 0)
                        {
                            genZoneTypeScratch = nType;
                            genCellQuestSlot0Scratch = nStepB - 1;
                        }
                        else if (nType == 0x10)
                        {
                            nZone = (short)PlaceQuestNode(0xf, nStep16, -1, nItemA, -1,
                                                          (short)GetZoneGridOrder(i2, j2), 0);
                            if (nZone < 0)
                                goto fail_b;
                            genZoneTypeScratch = 0xf;
                            genCellQuestSlot0Scratch = nStepB - 1;
                        }
                        else
                        {
                            nZone = (short)PlaceQuestNode(0x10, nStep16, -1, nItemA, -1,
                                                          (short)GetZoneGridOrder(i2, j2), 0);
                            if (nZone < 0)
                                goto fail_b;
                            genZoneTypeScratch = 0x10;
                            genCellQuestSlot0Scratch = nStepB - 1;
                        }
                    }
                    AddPlacedZoneId(nZone);
                    if (nZone < 0)
                        goto fail_b;
                    nRetry++;
                    int nCell = i2 + j2 * 10;
                    bFoundStep = 1;
                    mapGrid[nCell].zoneType = genZoneTypeScratch;
                    mapGrid[nCell].cellItemC = (short)genCellItemCScratch;
                    mapGrid[nCell].cellItemA = (short)genCellItemAScratch;
                    mapGrid[nCell].cellQuestSlot6 = (short)genCellQuestSlot6Scratch;
                    mapGrid[nCell].cellQuestSlot0 = (short)genCellQuestSlot0Scratch;
                    mapGrid[nCell].field30 = 0;
                    apZoneGrid[nCell] = (Zone *)zones.GetAt(nZone);
                    mapGrid[nCell].id = (unsigned short)nZone;
                } while (nRetry < 0xc9);
            }
            if (bFoundStep == 0)
            {
                short nZ = PlaceQuestNode(1, -1, -1, -1, -1,
                                          (short)GetZoneGridOrder(x, nOrderVal), 0);
                if (nZ >= 0)
                {
                    int nCell = i2 + j2 * 10;
                    mapGrid[nCell].zoneType = genZoneTypeScratch;
                    apZoneGrid[nCell] = (Zone *)zones.GetAt(nZ);
                    mapGrid[nCell].id = nZ;
                    AddPlacedZoneId(nZ);
                }
            }
            nOrderVal--;
            nStepB--;
        } while (nStepB > 0);
    }
    // consume the remaining plan tokens
    {
        int ty, tx;
        for (ty = 0; ty < 10; ty++)
        {
            for (tx = 0; tx < 10; tx++)
            {
                int bMatched = 0;
                genCellQuestSlot0Scratch = -1;
                genCellQuestSlot6Scratch = -1;
                genCellItemAScratch = -1;
                genCellItemCScratch = -1;
                short v = aPlan[ty * 10 + tx];
                if (v != 0 && v != PLAN_BLOCKED && apZoneGrid[ty * 10 + tx] == NULL)
                {
                    int nZone = -1;
                    switch (v)
                    {
                    case PLAN_START:
                        nZone = (short)PlaceQuestNode(0xb, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(tx, ty), bFirst);
                        if (nZone >= 0)
                            mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_MAP_START;
                        bMatched = 1;
                        break;
                    case PLAN_FORK_W:
                        nZone = (short)PlaceQuestNode(5, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(tx, ty), bFirst);
                        if (nZone >= 0)
                        {
                            mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_ITEM_TO_PASS;
                            mapGrid[ty * 10 + tx].cellItemA = (short)genCellItemAScratch;
                        }
                        bMatched = 1;
                        break;
                    case PLAN_FORK_E:
                        nZone = (short)PlaceQuestNode(4, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(tx, ty), bFirst);
                        if (nZone >= 0)
                        {
                            mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_FIND_USEFUL_NPC;
                            mapGrid[ty * 10 + tx].cellItemA = (short)genCellItemAScratch;
                        }
                        bMatched = 1;
                        break;
                    case PLAN_FORK_N:
                        nZone = (short)PlaceQuestNode(2, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(tx, ty), bFirst);
                        if (nZone >= 0)
                        {
                            mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_FINAL_DESTINATION;
                            mapGrid[ty * 10 + tx].cellItemA = (short)genCellItemAScratch;
                        }
                        bMatched = 1;
                        break;
                    case PLAN_FORK_S:
                        nZone = (short)PlaceQuestNode(3, -1, -1, -1, -1,
                                                      (short)GetZoneGridOrder(tx, ty), bFirst);
                        if (nZone >= 0)
                        {
                            mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_ITEM_FOR_ITEM;
                            mapGrid[ty * 10 + tx].cellItemA = (short)genCellItemAScratch;
                        }
                        bMatched = 1;
                        break;
                    }
                    if (nZone < 0)
                    {
                        if (v != PLAN_PATH && v != PLAN_WALL && v != PLAN_CORRIDOR && bMatched)
                        {
                            nZone = (short)PlaceQuestNode(1, -1, -1, -1, -1,
                                                          (short)GetZoneGridOrder(tx, ty), bFirst);
                            if (nZone >= 0)
                            {
                                apZoneGrid[ty * 10 + tx] = (Zone *)zones.GetAt(nZone);
                                mapGrid[ty * 10 + tx].id = (unsigned short)nZone;
                                mapGrid[ty * 10 + tx].zoneType = ZONE_TYPE_ENEMY_TERRITORY;
                                mapGrid[ty * 10 + tx].cellItemC = -1;
                                mapGrid[ty * 10 + tx].cellItemA = -1;
                                mapGrid[ty * 10 + tx].cellQuestSlot6 = -1;
                                AddPlacedZoneId(nZone);
                            }
                        }
                    }
                    else
                    {
                        apZoneGrid[ty * 10 + tx] = (Zone *)zones.GetAt(nZone);
                        mapGrid[ty * 10 + tx].id = (short)nZone;
                        mapGrid[ty * 10 + tx].cellItemC = -1;
                        if (mapGrid[ty * 10 + tx].zoneType != ZONE_TYPE_MAP_START)
                            AddPlacedZoneId(nZone);
                    }
                }
            }
        }
    }
    if (WorldgenPlacePuzzles(aPlan) != 0)
    {
        MapZone *pMapCell = mapGrid;
        i = 10;
        do
        {
            j = 10;
            do
            {
                if (pMapCell->id >= 0)
                    PlaceZoneObjectTiles(pMapCell->id);
                pMapCell++;
                j--;
            } while (j != 0);
            i--;
        } while (i != 0);
        placedZoneIds.RemoveAt(0, nOldPlaced);
        RemoveEmptyZonesFromPlacedList();
        Nop2();
        switch (currentPlanet)
        {
        case 1:
            SaveStoryHistoryNevada();
            break;
        case 2:
            SaveStoryHistoryAlaska();
            break;
        case 3:
            SaveStoryHistoryOregon();
            break;
        }
        nRequestedGoalItem = -1;
        totalZones = (nSplits + nGoals) * 2 + questItemsA.GetSize() + questItemsB.GetSize();
        timeBase = (int)time(NULL);
        timeOffset = 0;
        return 1;
    }
    questItemsA.SetSize(0, -1);
    questItemsB.SetSize(0, -1);
    uniqueRequiredItemsMaybe.SetSize(0, -1);
    switch (currentPlanet)
    {
    case 1:
        storyHistoryNevada.SetSize(0, -1);
        break;
    case 2:
        storyHistoryAlaska.SetSize(0, -1);
        break;
    case 3:
        storyHistoryOregon.SetSize(0, -1);
        break;
    }
    n = worldgenPendingZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenPendingZones.SetSize(0, -1);
    n = worldgenRefZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenRefZones.SetSize(0, -1);
    goalTileList.SetSize(0, -1);
    placedZoneIds.SetSize(0, -1);
    return 0;
fail_a:
    questItemsA.SetSize(0, -1);
    questItemsB.SetSize(0, -1);
    uniqueRequiredItemsMaybe.SetSize(0, -1);
    switch (currentPlanet)
    {
    case 1:
        storyHistoryNevada.SetSize(0, -1);
        break;
    case 2:
        storyHistoryAlaska.SetSize(0, -1);
        break;
    case 3:
        storyHistoryOregon.SetSize(0, -1);
        break;
    }
    n = worldgenPendingZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenPendingZones.SetSize(0, -1);
    n = worldgenRefZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenRefZones.SetSize(0, -1);
    goalTileList.SetSize(0, -1);
    placedZoneIds.SetSize(0, -1);
    return 0;
fail_b:
    questItemsA.SetSize(0, -1);
    questItemsB.SetSize(0, -1);
    uniqueRequiredItemsMaybe.SetSize(0, -1);
    switch (currentPlanet)
    {
    case 1:
        storyHistoryNevada.SetSize(0, -1);
        break;
    case 2:
        storyHistoryAlaska.SetSize(0, -1);
        break;
    case 3:
        storyHistoryOregon.SetSize(0, -1);
        break;
    }
    n = worldgenPendingZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenPendingZones.SetSize(0, -1);
    n = worldgenRefZones.GetSize();
    i = 0;
    if (n > 0)
    {
        do
        {
            delete (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
            i++;
            n--;
        } while (n != 0);
    }
    worldgenRefZones.SetSize(0, -1);
    goalTileList.SetSize(0, -1);
    placedZoneIds.SetSize(0, -1);
    return 0;
}

// FUNCTION: YODA 0x00421460
// Snapshot the active 10x10 MapZone grid into the backup grid (sparse-save baseline).
void CDeskcppDoc::BackupZoneGrid()
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
void CDeskcppDoc::RestoreGridFromBackup()
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
int CDeskcppDoc::IsTileInGoalList(unsigned int tileId)
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
int CDeskcppDoc::PlacePuzzle(short nOrderMax, short *paPlanGrid, int *pX, int *pY)
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
            ((CDeskcppApp *)AfxGetApp())->LogWrite("!!!!No Place to put Find Puzzle!!!\n");
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
int CDeskcppDoc::WorldgenPlacePuzzles(short *paPlanGrid)
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
                    genSkipTeleCheck = 1;
                else
                    genSkipTeleCheck = 0;
                int nZoneId;
                if (genSkipTeleCheck != 0)
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
                    if (genSkipTeleCheck != 0)
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
        genSkipTeleCheck = 1;
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
int CDeskcppDoc::GetZoneGridOrder(int x, int y)
{
    return gWorldgenGridOrderTable[x + y * 10];
}

// FUNCTION: YODA 0x00421e70
// [EFFECTIVE: 17B — SetAtGrow call-site this-reg {EAX,ECX} swap (lea vs add) + one NOP.]
// CHUNK chunk: allocate + read Character records, -1-terminated id list.
int CDeskcppDoc::ParseChar(CFile *pFile)
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
int CDeskcppDoc::LoadWorld()
{
    CDeskcppApp *pApp = (CDeskcppApp *)AfxGetApp();
    if (bStartingGame == 0)
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
#if defined(GAME_INDY)
    currentPlanet = -1;              // Indy has no planets: -1 matches every zone's planet=-1
                                     // (Zone ctor default; Indy's IZON carries no planet field),
                                     // so PlaceQuestNode's `planet == currentPlanet` accepts all
                                     // zones and the Yoda per-planet story switches all no-op.
#elif !defined(YODA_FULL)
    currentPlanet = 2;               // sic: demo hardcode — the whole pick above is overridden
    if (pApp != NULL)
        pApp->WriteProfileInt("OPTIONS", "Terrain", 2);
#else
    // Full game: keep the rotated planet from the pick above and persist it (retail Yodesk
    // FUN_004248a0 writes the computed Terrain, with no forced =2). This is the operative
    // planet selector for new-game worldgen — the CDeskcppDoc ctor override is separate.
    if (pApp != NULL)
        pApp->WriteProfileInt("OPTIONS", "Terrain", currentPlanet);
#endif

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
#ifdef GAME_INDY
        // Indy DAW: aux/scripts are GLOBAL parallel-array chunks, not inline per zone (same as the
        // Load() dispatcher — LoadWorld is a second copy of the loader, re-run on New World to reset
        // zone state, and it must apply the identical Indy handling or it misparses the DAW: the
        // Yoda ParseZaux on Indy's global ZAUX walks off the rails and the chunk loop never reaches
        // ENDF -> the New World "progress bar jumping / infinite load". ZONE/HTSP/ACTN are shared
        // (fall through below); ZAUX/ZAX2/ZAX3 use the Indy distributors; ZAX4/IZAX/PNAM/ANAM skip.
        if (strcmp(tag, "ZAUX") == 0)
        {
            nRet = ParseZauxIndy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX2") == 0)
        {
            nRet = ParseZax2Indy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX3") == 0)
        {
            nRet = ParseZax3Indy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX4") == 0 || strcmp(tag, "IZAX") == 0 ||
                 strcmp(tag, "PNAM") == 0 || strcmp(tag, "ANAM") == 0)
        {
            pFile->Seek(nLen, CFile::current);
            continue;
        }
#endif
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
// Structure proven: bDtaLoaded++ (inc form), grouped switch with per-arm AfxMessageBox
// calls, dead code after AfxAbort, success block (Generate loop) sunk to the end.]
// The main .dta asset loader: open theApp.m_str (CFileException box + AfxAbort on failure),
// dispatch the FourCC chunk stream (VERS/TILE/TNAM/ZONE/ZAUX/ZAX2/ZAX3/CHAR/CHWP/CAUX/HTSP/
// SNDS/PUZ2/ACTN/ENDF), then drive worldgen: Randomize+Generate until success, Populate.
// Declaring the local CFileException also emits this TU's ~CFileException COMDAT — the
// original's copy is the TU-opening function at 0x41c340.
int CDeskcppDoc::Load()
{
    int nRet = 1;
    CFile *pFile = new CFile;
    CString strPath;
    strPath = ((CDeskcppApp *)AfxGetApp())->m_str;
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
    if (bDtaLoaded == 0)
        bDtaLoaded++;
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
#ifdef GAME_INDY
        // Indy stores each zone's aux/objects/scripts in GLOBAL chunks (parallel-array layout),
        // not inline per zone. Distribute the AUX chunks back to zones here: ZAUX (IZAX per zone),
        // ZAX2 (IZX2 -> genCandidateA), ZAX3 (IZX3 -> genCandidateB). HTSP (objects) is a keyed
        // (id,count,12-byte-objects,0xFFFF-terminator) chunk byte-identical to Yoda's, so it falls
        // through to the shared ParseHtsp dispatch below — objects are REQUIRED by Indy worldgen
        // (the quest item pools live in DOOR_IN child zones reached via object type 9). ACTN is
        // ALSO a keyed [zone_id(2), count(2), scripts...] block byte-identical to Yoda's per-zone
        // IACT format (verified by raw-byte simulation of DESKTOP.DAW: 319 zones / 2825 scripts,
        // consumes the chunk exactly; DESKADV.EXE IndyParseActn 1010:b5d4 is structurally identical
        // to our ParseActn) — it only DIFFERS by living in one global chunk instead of inline per
        // zone, so it falls through to the shared ParseActn dispatch below and distributes to
        // zone->iactScripts. Still length-skipped (H3 milestone 2b+): ZAX4 (IZX4 static-map flag,
        // Yoda discards it), and Indy-only PNAM/ANAM (puzzle/actor names).
        if (strcmp(tag, "ZAUX") == 0)
        {
            nRet = ParseZauxIndy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX2") == 0)
        {
            nRet = ParseZax2Indy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX3") == 0)
        {
            nRet = ParseZax3Indy(pFile);
            if (nRet == 0)
                break;
            continue;
        }
        else if (strcmp(tag, "ZAX4") == 0 || strcmp(tag, "IZAX") == 0 ||
                 strcmp(tag, "PNAM") == 0 || strcmp(tag, "ANAM") == 0)
        {
            pFile->Seek(nLen, CFile::current);
            continue;
        }
        // ACTN falls through to the shared ParseActn (same keyed format) below.
#endif
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
        nZonesLoaded = zones.GetSize();
        CacheUiTilePtrsMaybe();
        unsigned int nSeed = Randomize();
        do
        {
#ifdef GAME_INDY
            // Indy: IndyGenerate does the whole job (plan + placement passes + materialize +
            // play-state), matching DESKADV.EXE which loops IndyGenerate alone with no separate
            // Populate. The Yoda #else path is byte-identical to the original (anchor-safe).
            if (IndyGenerate(nSeed) != 0)
                nGenerated++;
            else
                nSeed = Randomize();
#else
            if (Generate(nSeed) != 0)
                nGenerated++;
            else
                nSeed = Randomize();
#endif
        } while (nGenerated == 0);
        AfxGetApp()->DoWaitCursor(-1);
#ifndef GAME_INDY
        Populate();
#endif
        return 1;
    }
    return nRet;
}

// FUNCTION: YODA 0x00422f40
BOOL CDeskcppDoc::IsModified()
{
    return m_bModified;
}

// FUNCTION: YODA 0x00422f50
void CDeskcppDoc::SetModifiedFlag(BOOL bModified)
{
    m_bModified = bModified;
}

// FUNCTION: YODA 0x00422f60
// ZONE chunk: count + N zone records.
int CDeskcppDoc::ParseZone(CFile *pFile)
{
    short nZones;
#ifdef GAME_INDY
    int nChunkLen;               // Indy's ZONE chunk carries a 4-byte length prefix Yoda lacks
    pFile->Read(&nChunkLen, 4);  // (verified in DESKTOP.DAW: "ZONE" + len(4) + nZones(2))
#endif
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
int CDeskcppDoc::ParsePuz2(CFile *pFile)
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
int CDeskcppDoc::ParseZaux(CFile *pFile)
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
int CDeskcppDoc::ParseZax3(CFile *pFile)
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
int CDeskcppDoc::ParseZax2(CFile *pFile)
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

#ifdef GAME_INDY
// Indy aux distributors (NOT byte-match functions — Indy-only). The Indy ZAUX/ZAX2/ZAX3 chunks are
// parallel arrays: after the chunk tag+len, N back-to-back sub-records (one per zone, IN ORDER) with
// NO per-zone 8-byte header (unlike Yoda's Parse* which read `buf[8]` before each sub-record).
// IZX2/IZX3 records are byte-identical to Yoda's, so ReadZax2/ReadZax3 are reused verbatim; only
// IZAX differs (ReadIzaxIndy). Verified against DESKTOP.DAW: each walks exactly 366 records.
int CDeskcppDoc::ParseZauxIndy(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    for (int i = 0; i < nCount; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadIzaxIndy(pFile);
    }
    return 1;
}

int CDeskcppDoc::ParseZax2Indy(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    for (int i = 0; i < nCount; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadZax2(pFile);      // IZX2 record format identical to Yoda -> genCandidateA
    }
    return 1;
}

int CDeskcppDoc::ParseZax3Indy(CFile *pFile)
{
    short nCount = (short)zones.GetSize();
    for (int i = 0; i < nCount; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        if (pZone == NULL)
            return 0;
        pZone->ReadZax3(pFile);      // IZX3 record format identical to Yoda -> genCandidateB
    }
    return 1;
}
#endif

// FUNCTION: YODA 0x00423290
// [EFFECTIVE: block-layout — original parks the nDone++ arm at function end; ours inlines
// it (arm-order/continue knobs proven inert). Plus one reg rotation.]
// CAUX chunk: per-character damage words, -1-terminated id list.
int CDeskcppDoc::ParseCaux(CFile *pFile)
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
int CDeskcppDoc::ParseChwp(CFile *pFile)
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
int CDeskcppDoc::ParseTnam(CFile *pFile)
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
#ifdef GAME_INDY
            buf[0x10] = 0;            // Indy tile names are 16 bytes (vs Yoda 24) — verified:
            pFile->Read(buf, 0x10);   // 143 names align the DAW tail to ENDF
#else
            pFile->Read(buf, 0x18);
#endif
            pTile->name = buf;
        }
    } while (nDone == 0);
    return 1;
}

// FUNCTION: YODA 0x004233f0
// [EFFECTIVE: 5B — frame-slot order of the char buffers: orig ascending {ext,fname,name,path}
// (size-sorted), ours {ext,name,fname,path} (name(16) lands before fname(12)). Probes ALL inert
// (layout byte-stable): ⭐ EXHAUSTIVE v36 — ALL 24 permutations of the four buffer decls compiled
// (tryorder.py) => every one gives byte_diff=5, align=0. Also nested strcat(strcpy(),) vs
// sequential, if-scope vs loop-scope vs split scopes: inert. The array slot key is compiler-
// internal (NOT decl order, NOT scope) — likely first-linearized-use or an internal symtab hash;
// insns/regs 100% (align=0 reg_pen=0). Definitive proof for the frame-layout park class:
// address-taken char[] slots are decl-order-INVARIANT in cl 10.20. Joint-pass/whole-image only
// (and that build uses the SAME cl, so this 5B may be irreducible from the source side).]
// SNDS chunk: NEGATED sound count, then per-sound a length-prefixed source path; only the
// bare "fname.ext" is kept in soundNames[i].
int CDeskcppDoc::ParseSnds(CFile *pFile)
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
int CDeskcppDoc::ParseActn(CFile *pFile)
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
int CDeskcppDoc::ParseHtsp(CFile *pFile)
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

// FUNCTION: YODA 0x00423850
// [EFFECTIVE MATCH: DIFF(2), insns 230/230. Sole residual: the STUP arm's nDone++ — orig
// emits add [nDone],ecx reusing the CSE'd ECX=1; ours emits inc. ++/+=1/n=n+1 all inert
// (probed 2026-07-06) — inc-vs-add-reg is instruction selection, same family as the
// cmp-direction tie-breaks. Serialize (0x423b30) carries the identical residual.]
// Standalone .wld-state reader (invoked from OnDraw): open the doc path read/binary, read the
// chunk stream VERS(==0x200)/STUP(->ReadStupCanvas)/ENDF, then enter the world-view state
// (bWorldReady/bHidePlayer/nMapChangeReason=1/nFrameMode=7) and UpdateAllViews.
void CDeskcppDoc::LoadWorldStateFile()
{
    CFile *pFile = new CFile;
    CString strPath;
    strPath = ((CDeskcppApp *)AfxGetApp())->m_str;
    CFileException e;
    pFile->Open(strPath, CFile::modeRead | CFile::typeBinary | CFile::shareDenyNone, &e);
    int nDone = 0;
    do
    {
        char tag[5];
        int nLen;
        TRY {
            pFile->Read(tag, 4);
            tag[4] = 0;
            pFile->Read(&nLen, 4);
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e2) {               // hand-expanded CATCH_ALL(e2)
            _afxExceptionLink.m_pException = e2;
            nFrameMode = 12;
            nDone++;
        }
        }              // closes the TRY macro's outer (link-scope) brace
        if (nDone != 0)
            break;
        if (strcmp(tag, "VERS") == 0)
        {
            if (nLen != 0x200)
            {
                AfxMessageBox(5, 0, (UINT)-1);
                nDone++;
            }
        }
        else if (strcmp(tag, "STUP") == 0)
        {
            if (bDtaLoaded == 0)
            {
                ReadStupCanvas(pFile);
                nDone++;
                unk50 = 0;
                bHidePlayer = 1;
                bWorldReadyMaybe = 1;
                unk3378 = 0;
                nMapChangeReason = 1;
                nFrameMode = 7;
            }
            else
                nDone++;
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
    UpdateAllViews(NULL);
    pFile->Close();
    delete pFile;
}

// FUNCTION: YODA 0x00423b30
// [EFFECTIVE MATCH: DIFF(2) — same single inc-vs-add tie-break as LoadWorldStateFile.]
// CDocument::Serialize override: on load, the same VERS/STUP/ENDF chunk loop as
// LoadWorldStateFile over ar.m_pFile (copy-paste source). Store path: empty (demo save
// disabled).
void CDeskcppDoc::Serialize(CArchive &ar)
{
    if (ar.IsLoading())
    {
        int nDone = 0;
        CFile *pFile = ar.GetFile();
        do
        {
            char tag[5];
            int nLen;
            TRY {
                pFile->Read(tag, 4);
                tag[4] = 0;
                pFile->Read(&nLen, 4);
            }
            }              // closes the try block the TRY macro opened
            catch (CException *e2) {               // hand-expanded CATCH_ALL(e2)
                _afxExceptionLink.m_pException = e2;
                nFrameMode = 12;
                nDone++;
            }
            }              // closes the TRY macro's outer (link-scope) brace
            if (nDone != 0)
                break;
            if (strcmp(tag, "VERS") == 0)
            {
                if (nLen != 0x200)
                {
                    AfxMessageBox(5, 0, (UINT)-1);
                    nDone++;
                }
            }
            else if (strcmp(tag, "STUP") == 0)
            {
                if (bDtaLoaded == 0)
                {
                    ReadStupCanvas(pFile);
                    nDone++;
                    unk50 = 0;
                    bHidePlayer = 1;
                    bWorldReadyMaybe = 1;
                    unk3378 = 0;
                    nMapChangeReason = 1;
                    nFrameMode = 7;
                }
                else
                    nDone++;
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
        UpdateAllViews(NULL);
    }
}

// FUNCTION: YODA 0x00423d20
// Find the INTRO zone (map_flags 9), make it current and refresh (StartGame).
void CDeskcppDoc::SetCurrentToIntroZone()
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
void CDeskcppDoc::ReadStupCanvas(CFile *pFile)
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
int CDeskcppDoc::GetZoneIndex(Zone *pZone)
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

// FUNCTION: YODA 0x00423df0
// [EFFECTIVE MATCH: DIFF(12), insns 101/101, reg_pen=0. Sole residual: orig emits a redundant
// promotion MOVSX EDI,DI / MOVSX EBX,BX before the word-adds destX/destY += 0x1c (its peephole
// kept the int-promotion; ours deletes it). Probed inert: +=, expression assign, (short) cast.
// Instruction-selection family — not source-steerable.]
// Draw the 10x10 locator map into pCanvas (28px cells at +4,+4; background tile 0x344 under
// each cell; per-cell icon from GetLocatorIconMaybe; the 32x32 player marker apUiTiles[15]
// over the player's cell when bDrawPlayer), then blit the 288x288 result to the DC at
// rectUnk3274's origin. UI tile slots 1..15 fall back to slot 0 when unset.
void CDeskcppDoc::DrawLocatorMap(CDC *pDC, int bDrawPlayer, int bAlt)
{
    pCanvas->Fill(0);
    for (short i = 1; i < 16; i++)
    {
        if (apUiTiles[i] == NULL)
            apUiTiles[i] = apUiTiles[0];
    }
    short y = 0;
    short destY = 4;
    do
    {
        short x = 0;
        int nY = y;
        short destX = 4;
        do
        {
            Tile *pTile = GetTileData(0x344);
            pCanvas->BlitFast(pTile->pixels, 0x1c, 0x1c, 0x20, destX, destY);
            short nIcon = (short)GetLocatorIconMaybe(x, nY, bAlt);
            if (nIcon >= 0)
            {
                pCanvas->BlitFast(apUiTiles[nIcon]->pixels, 0x1c, 0x1c, 0x20, destX, destY);
                if (playerX == x && playerY == nY && bDrawPlayer != 0)
                    pCanvas->BlitMasked((char *)apUiTiles[15]->pixels, 0x20, 0x20, destX, destY, 0);
            }
            destX += 0x1c;
            x++;
        } while (x < 10);
        destY += 0x1c;
        y++;
    } while (y < 10);
    pCanvas->BitBlt(pDC, rectUnk3274.left, rectUnk3274.top, 0x120, 0x120, 0, 0);
}

// FUNCTION: YODA 0x00423f50
// Clamp the visible 288x288 window to the camera position (small zones pin to full window).
void CDeskcppDoc::UpdateCamera()
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

// FUNCTION: YODA 0x00424010
// [EFFECTIVE MATCH: insns 243/243, align residual = one frame-slot pair (orig homes pPenTL at
// ebp-0x14 = pen A's raw-new temp slot; ours picks ebp-0x10 — usage-count slot ranking, the
// ParseSnds family) + three clean 2-cycle reg rotations (EBX/EDI edge2, ESI/EDI edges 3-4).
// Edge-4 decl order y1-LAST is load-bearing (aligns the pRect reload after EAX is clobbered
// by the bottom load): x1, x2, nBottom, y1 — align 26 -> 8.]
// World member (thiscall; Ghidra: Render::DrawRect): draw an nThickness-pixel 3D bevel border
// just inside pRect. bRaised==1 = raised (top/left highlight, bottom/right shadow), else
// sunken. Top/left edges use the second pen, bottom/right the first; corners mitred by
// pulling each line end in per ring.
void CDeskcppDoc::DrawRect(CDC *pDC, RECT *pRect, int bRaised, int nThickness)
{
    DWORD dwShadow = GetSysColor(COLOR_BTNSHADOW);
    DWORD dwHilite = GetSysColor(COLOR_BTNHIGHLIGHT);
    CPen *pPenBR, *pPenTL;
    if (bRaised == 1)
    {
        pPenBR = new CPen(PS_SOLID, 1, dwShadow);
        pPenTL = new CPen(PS_SOLID, 1, dwHilite);
    }
    else
    {
        pPenBR = new CPen(PS_SOLID, 1, dwHilite);
        pPenTL = new CPen(PS_SOLID, 1, dwShadow);
    }
    CPen *pOld;
    {
        int x1 = pRect->left;
        int y1 = pRect->top;
        int y2 = y1;
        int x2 = pRect->right;
        pOld = pDC->SelectObject(pPenTL);
        if (nThickness > 0)
        {
            int n = nThickness;
            do
            {
                pDC->MoveTo(x1, y2);
                pDC->LineTo(x2, y1);
                y2++;
                y1++;
                x2--;
                n--;
            } while (n != 0);
        }
    }
    {
        int x1 = pRect->left;
        int nTop = pRect->top;
        int x2 = x1;
        int y2 = pRect->bottom - 1;
        if (nThickness > 0)
        {
            int n = nThickness;
            do
            {
                pDC->MoveTo(x2, nTop);
                pDC->LineTo(x1, y2);
                x2++;
                y2--;
                x1++;
                n--;
            } while (n != 0);
        }
    }
    pDC->SelectObject(pPenBR);
    {
        int x1 = pRect->left;
        int y1 = pRect->bottom - 1;
        int nRight = pRect->right;
        int y2 = y1;
        if (nThickness > 0)
        {
            int n = nThickness;
            do
            {
                pDC->MoveTo(x1, y2);
                pDC->LineTo(nRight, y1);
                x1++;
                y2--;
                y1--;
                n--;
            } while (n != 0);
        }
    }
    {
        int x1 = pRect->right - 1;
        int x2 = x1;
        int nBottom = pRect->bottom - 1;
        int y1 = pRect->top;
        if (nThickness > 0)
        {
            int n = nThickness;
            do
            {
                pDC->MoveTo(x2, nBottom);
                pDC->LineTo(x1, y1);
                x2--;
                y1++;
                x1--;
                n--;
            } while (n != 0);
        }
    }
    pDC->SelectObject(pOld);
    delete pPenBR;
    delete pPenTL;
}

// FUNCTION: YODA 0x004242a0
// Audio menu: toggle sound; opening the first sound session lazily.
void CDeskcppDoc::OnToggleSound()
{
    nSoundEnabled = (nSoundEnabled == 0);
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *pView = (CDeskcppView *)GetNextView(pos);
    if (pView != NULL && nSoundEnabled != 0 && pView->soundSession == 0)
        pView->SoundInit();
}

// FUNCTION: YODA 0x004242f0
void CDeskcppDoc::OnUpdateToggleSound(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(nSoundEnabled);
}

// FUNCTION: YODA 0x00424310
// Audio menu: toggle music.
void CDeskcppDoc::OnToggleMusic()
{
    nMusicEnabled = (nMusicEnabled == 0);
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *pView = (CDeskcppView *)GetNextView(pos);
    if (pView != NULL && nMusicEnabled != 0 && pView->soundSession == 0)
        pView->SoundInit();
}

// FUNCTION: YODA 0x00424360
void CDeskcppDoc::OnUpdateToggleMusic(CCmdUI *pCmdUI)
{
    pCmdUI->SetCheck(nMusicEnabled);
}

// FUNCTION: YODA 0x00424380
// Seed the RNG from cursor position + wall clock, then pack rand() bytes into the world seed
// (one of 3 rotating byte layouts).
unsigned int CDeskcppDoc::Randomize()
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

// FUNCTION: YODA 0x00424450
// ON_COMMAND(0x8008 File>New World) [msgmap @0x44c330]: confirm via AfxMessageBox(0xE001
// "...Build a New World anyway?") unless a game is over/not started, then
// StartGame(Randomize(), 0) under a wait cursor; on failure show the 0xE01E fatal string
// and FatalAppExit.
void CDeskcppDoc::OnNewWorld()
{
    int nAnswer;
    if (bSkipNewWorldConfirm == 0 && gameState == 0)
        nAnswer = AfxMessageBox(0xe001, 4, 0);
    else
        nAnswer = 6;
    if (nAnswer == 6)
    {
        unsigned int nSeed = Randomize();
        AfxGetApp()->DoWaitCursor(1);
        int nRet = StartGame(nSeed, 0);
        AfxGetApp()->DoWaitCursor(-1);
        if (nRet == 0)
        {
            CString str;
            str.LoadString(0xe01e);
            nFrameMode = 12;
            OnCloseDocument();
            FatalAppExit(0, str);
        }
    }
}

// FUNCTION: YODA 0x00424540
// [EFFECTIVE-WIP: insns 892/900, align=352 mostly echo; len 2670/2672. Residual autopsy:
// (1) this-reload reg color (orig ECX, ours EAX) — single consistent bijection, cascades
// into the pDlg test-eax + FindTile/SaveZoneRecursive push colors; (2) the open-fail
// switch's arm tails each carry a duplicated `cmp pDlg,0/je` join head — the OPEN
// tail-duplication family (same as Load's m_cause switch / dispatcher exit-block); (3)
// GetPathName chain: ours pushes GetBuffer's 0xc8 before the GetPathName call (arg-push
// scheduling family); (4) EH-state-store placement in the cancel tail (PlaceZone family);
// (5) state-constant CSE: orig keeps 1 in EBX, ours 3; (6) the shared short nCount slot
// rank (-0xe vs -0x26, ParseSnds family). Cracked here: bQuestCellsResident selectors are `!= 0`
// grid/backup-arm-FIRST at all four sites; the 2x2 recursive block materializes
// MapZone *pCell (base-folded [ebx+4] id reads); pView=NULL declared AFTER the
// GetFirstViewPosition call.]
// ON_COMMAND(ID_FILE_SAVE 0xE103 File>Save World) [msgmap @0x44c348]: no-op in cutscene/world
// frame modes, after a win, or at full health/lives; save dialog (filter 0xE006,
// "savegame.wld") then write the "YODASAV44" .wld state: seed/planet + quest-item lists +
// the center 2x2 quest cells (mapScratch or live grid per bQuestCellsResident) + the 10x10 grid dump +
// recursive zone records + inventory/player/weapon/camera/health/elapsed-time tail.
void CDeskcppDoc::OnSaveWorld()
{
    CString strPath;
    CString strFilter;
    strFilter.LoadString(0xe006);
    if (nFrameMode == 1 || nFrameMode == 7 || nFrameMode == 6 || nFrameMode == 0xb
        || gameState != 0)
        return;
    int *pHealth = &healthLo;
    if (*pHealth > 99 && healthHi >= 3)
        return;
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *pView = NULL;
    if (pos != NULL)
        pView = (CDeskcppView *)GetNextView(pos);
    int nSavedMode = nFrameMode;
    nFrameMode = 0;
    CFileDialog *pDlg = new CFileDialog(0, "wld", "savegame", 0x80006, strFilter, (CWnd *)pView);
    CString strTitle;
    strTitle.LoadString(0xe032);
    pDlg->m_ofn.lpstrInitialDir = lpszSaveDir;  // sic: dereferences pDlg BEFORE the null
                                                     // check below (engine-bugs.md #13)
    if (pDlg == NULL)
    {
        AfxMessageBox(9, 0, (UINT)-1);
        return;
    }
    if (pDlg->DoModal() == 1)
    {
        strPath = pDlg->GetPathName().GetBuffer(200);
        CFile *pFile = new CFile;
        CFileException e;
        if (pFile->Open(strPath, CFile::modeCreate | CFile::modeReadWrite, &e) == 0)
        {
            switch (e.m_cause)
            {
            case CFileException::generic:
            case CFileException::fileNotFound:
            case CFileException::badPath:
            case CFileException::tooManyOpenFiles:
            case CFileException::invalidFile:
            case CFileException::removeCurrentDir:
            case CFileException::badSeek:
            case CFileException::hardIO:
            case CFileException::sharingViolation:
            case CFileException::lockViolation:
            case CFileException::endOfFile:
                AfxMessageBox(9, 0, (UINT)-1);
                break;
            case CFileException::accessDenied:
            case CFileException::directoryFull:
            case CFileException::diskFull:
                AfxMessageBox(7, 0, (UINT)-1);
                break;
            default:
                AfxMessageBox(9, 0, (UINT)-1);
                break;
            }
            if (pDlg != NULL)
                delete pDlg;
            nFrameMode = nSavedMode;
            pView->bBusy = 0;
            return;
        }
        pFile->Write("YODASAV44", 9);
        pFile->Write(&worldSeed, 4);
        pFile->Write(&currentPlanet, 4);
        pFile->Write(&bQuestCellsResident, 4);
        short nCount = (short)questItemsA.GetSize();
        pFile->Write(&nCount, 2);
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                short v = questItemsA.GetAt(i);
                pFile->Write(&v, 2);
                i++;
            } while (i < nCount);
        }
        nCount = (short)questItemsB.GetSize();
        pFile->Write(&nCount, 2);
        i = 0;
        if (nCount > 0)
        {
            do
            {
                short v = questItemsB.GetAt(i);
                pFile->Write(&v, 2);
                i++;
            } while (i < nCount);
        }
        int j;
        i = 0;
        do
        {
            j = 0;
            do
            {
                MapZone *pCell;
                if (bQuestCellsResident != 0)
                    pCell = &mapGrid[i * 10 + j + 0x2c];
                else
                    pCell = &mapScratch[i * 2 + j];
                pFile->Write(&pCell->flagSolved, 4);
                pFile->Write(&pCell->flagA, 4);
                pFile->Write(&pCell->flagC, 4);
                pFile->Write(&pCell->flagB, 4);
                pFile->Write(&pCell->flagD, 4);
                pFile->Write(&pCell->id, 2);
                pFile->Write(&pCell->cellQuestSlot0, 2);
                pFile->Write(&pCell->cellItemA, 2);
                pFile->Write(&pCell->cellItemC, 2);
                pFile->Write(&pCell->cellQuestSlot1, 2);
                pFile->Write(&pCell->cellItemB, 2);
                pFile->Write(&pCell->cellQuestSlot5, 2);
                pFile->Write(&pCell->cellQuestSlot6, 2);
                pFile->Write(&pCell->zoneType, 4);
                pFile->Write(&pCell->field30, 2);
                j++;
            } while (j < 2);
            i++;
        } while (i < 2);
        if (bQuestCellsResident != 0)
        {
            i = 4;
            do
            {
                j = 4;
                do
                {
                    MapZone *pCell = &mapGrid[i * 10 + j];
                    if (pCell->id >= 0)
                    {
                        pFile->Write(&i, 4);
                        pFile->Write(&j, 4);
                        SaveZoneRecursive(pFile, pCell->id, pCell->flagSolved);
                    }
                    j++;
                } while (j < 6);
                i++;
            } while (i < 6);
        }
        else
        {
            i = 0;
            do
            {
                j = 0;
                do
                {
                    MapZone *pCell = &mapScratch[i * 2 + j];
                    if (pCell->id >= 0)
                    {
                        pFile->Write(&i, 4);
                        pFile->Write(&j, 4);
                        SaveZoneRecursive(pFile, pCell->id, pCell->flagSolved);
                    }
                    j++;
                } while (j < 2);
                i++;
            } while (i < 2);
        }
        int nEnd = -1;
        pFile->Write(&nEnd, 4);
        pFile->Write(&nEnd, 4);
        i = 0;
        do
        {
            j = 0;
            do
            {
                MapZone *pCell;
                if (bQuestCellsResident != 0)
                    pCell = &mapGridBackup[i * 10 + j];
                else
                    pCell = &mapGrid[i * 10 + j];
                pFile->Write(&pCell->flagSolved, 4);
                pFile->Write(&pCell->flagA, 4);
                pFile->Write(&pCell->flagC, 4);
                pFile->Write(&pCell->flagB, 4);
                pFile->Write(&pCell->flagD, 4);
                pFile->Write(&pCell->id, 2);
                pFile->Write(&pCell->cellQuestSlot0, 2);
                pFile->Write(&pCell->cellItemA, 2);
                pFile->Write(&pCell->cellItemC, 2);
                pFile->Write(&pCell->cellQuestSlot1, 2);
                pFile->Write(&pCell->cellItemB, 2);
                pFile->Write(&pCell->cellQuestSlot5, 2);
                pFile->Write(&pCell->cellQuestSlot6, 2);
                pFile->Write(&pCell->zoneType, 4);
                pFile->Write(&pCell->field30, 2);
                j++;
            } while (j < 10);
            i++;
        } while (i < 10);
        i = 0;
        do
        {
            j = 0;
            do
            {
                MapZone *pCell;
                if (bQuestCellsResident != 0)
                    pCell = &mapGridBackup[i * 10 + j];
                else
                    pCell = &mapGrid[i * 10 + j];
                if (pCell->id >= 0)
                {
                    pFile->Write(&i, 4);
                    pFile->Write(&j, 4);
                    SaveZoneRecursive(pFile, pCell->id, pCell->flagSolved);
                }
                j++;
            } while (j < 10);
            i++;
        } while (i < 10);
        nEnd = -1;
        pFile->Write(&nEnd, 4);
        pFile->Write(&nEnd, 4);
        int nInv = inventory.GetSize();
        pFile->Write(&nInv, 4);
        i = 0;
        if (nInv > 0)
        {
            do
            {
                short v = (short)FindTile(((InvItem *)inventory.GetAt(i))->pTile);
                pFile->Write(&v, 2);
                i++;
            } while (i < nInv);
        }
        short nZone = (short)GetZoneIndex(currentZone);
        pFile->Write(&nZone, 2);
        pFile->Write(&playerX, 4);
        pFile->Write(&playerY, 4);
        if (currentWeapon == NULL)
        {
            short v = -1;
            pFile->Write(&v, 2);
        }
        else
        {
            i = 0;
            nCount = (short)characters.GetSize();
            if (nCount > 0)
            {
                do
                {
                    if ((Character *)characters.GetAt(i) == currentWeapon)
                    {
                        short vi = (short)i;
                        pFile->Write(&vi, 2);
                        short va = currentWeapon->unk48;
                        pFile->Write(&va, 2);
                        break;
                    }
                    i++;
                } while (i < nCount);
            }
        }
        pFile->Write(&weaponState[0], 2);
        pFile->Write(&weaponState[1], 2);
        pFile->Write(&weaponState[2], 2);
        pFile->Write(&cameraX, 4);
        pFile->Write(&cameraY, 4);
        pFile->Write(pHealth, 4);
        pFile->Write(&healthHi, 4);
        pFile->Write(&difficulty, 4);
        int nElapsed = (int)difftime(timeBase, time(NULL));
        pFile->Write(&nElapsed, 4);
        pFile->Write(&totalZones, 4);
        short nCnt2 = (short)unk248.GetSize();
        pFile->Write(&nCnt2, 2);
        short nSum = 0;
        if (nCnt2 > 0)
        {
            unsigned short *p = unk248.GetData();
            int n = nCnt2;
            do
            {
                nSum += *p;
                p++;
                n--;
            } while (n != 0);
        }
        pFile->Write(&nSum, 2);
        pFile->Write(&nCurrentGoalItem, 4);
        pFile->Write(&goalItemTileId, 4);
        pFile->Close();
        if (pFile != NULL)
            delete pFile;
        if (pDlg != NULL)
            delete pDlg;
        nFrameMode = nSavedMode;
        return;
    }
    if (pDlg != NULL)
        delete pDlg;
    nFrameMode = nSavedMode;
    pView->bBusy = 0;
}

// NOTE: 0x424fb0 (16 bytes) is a bare `jmp 0x424fc0` thunk that GameView::OnTimer (0x40e0ec)
// calls to kick a replay load — not producible from C++ source (likely an incremental-link
// ILT remnant); reproduce at the Phase-G whole-image link, not here.

// FUNCTION: YODA 0x00424fc0
// [EFFECTIVE-WIP: insns 1131/1136, align=496 mostly echo; len 3606 vs ~3696 extent (orig
// includes more EH-funclet bytes). Cracks that landed (session 2026-07-06): early-return
// guard shape (the shared dtor+epilogue is the FIRST guard's fall-through; later exits
// cross-jump back — v10 EH lesson); DoModal success arm as if-body fall-through (cancel arm
// out-of-line after it); SEPARATE x/y locals for the -1,-1 sentinel reads (reusing the loop
// counters for Read(&i,4) address-homes them and wrecks reg-alloc TU-wide); story-history
// planet dispatch is a SWITCH (compares up front) with per-arm vGoal/pArr temps feeding one
// cross-jumped SetAtGrow tail; countdown-init-first preheaders on the 2x2/10x10 grid loops.
// Parked residuals: the Open-fail AfxMessageBox switch arms each carry a duplicated
// `cmp pDlg,0/je` join head (OPEN tail-dup family, same as Load/OnSaveWorld); inventory-loop
// backedge cmp direction (reg-vs-mem operand order, lesson #6 — `nInv > i` inert); the
// imm-vs-reg batching in the tail state stores (WorldDoc-ctor OPEN family); preheader
// slot/reg shuffles riding the this-reload color cascade.]
// ON_COMMAND(0x800A File>Load World) [msgmap @0x44c360] — also entered via the 0x424fb0 jmp
// thunk (GameView::OnTimer's replay kick; Ghidra's old name "Serialize"). Confirm box unless
// replaying; open dialog (filter 0xE007, *.wld) or take g_strReplayPath; read the
// "YODASAV44" state back: seed/planet/bQuestCellsResident + StartGame(0,1) + quest-item lists (tail
// element seeds nCurrentGoalItem/startItem from the puzzle records) + the 2x2 quest cells +
// zone records (LoadZoneRecursive until the -1,-1 sentinel) + the 10x10 grid (rebuilding
// apZoneGrid) + inventory (freeing and re-newing InvItems from tile ids) + player/weapon/
// camera/health/time tail; then PlaceZoneObjectTiles over unsolved grid cells and append the
// goal to the planet's story history.
void CDeskcppDoc::OnLoadWorld()
{
    CString strPath;
    CFileDialog *pDlg = NULL;
    int nAnswer;
    if (gameState == 0 && g_bReplayMode == 0)
        nAnswer = AfxMessageBox(3, 4, 0);
    else
        nAnswer = 6;
    if (nAnswer != 6)
        return;
    {
        CString strFilter;
        strFilter.LoadString(0xe007);
        if (g_bReplayMode == 0)
        {
            pDlg = new CFileDialog(1, "wld", "*.wld", 0x1006, strFilter, NULL);
            pDlg->m_ofn.lpstrInitialDir = lpszSaveDir;  // sic: dereferences pDlg BEFORE
            if (pDlg == NULL)                                //      the null check
                return;
            pDlg->m_ofn.Flags &= ~0x10;
            if (pDlg->DoModal() == 1)
            {
                strPath = pDlg->GetPathName().GetBuffer(200);
            }
            else
            {
                delete pDlg;
                return;
            }
        }
        else
        {
            strPath = g_strReplayPath;
        }
        CDeskcppView *pView = NULL;
        POSITION pos = GetFirstViewPosition();
        if (pos != NULL)
            pView = (CDeskcppView *)GetNextView(pos);
        int nSavedMode = nFrameMode;
        if (nSavedMode == 1)
        {
            if (pDlg != NULL)
                delete pDlg;
            return;
        }
        nFrameMode = 0;
        pView->bBusy = 1;
        CFile *pFile = new CFile;
        CFileException e;
        if (pFile->Open(strPath, CFile::modeRead, &e) == 0)
        {
            switch (e.m_cause)
            {
            case CFileException::generic:
            case CFileException::fileNotFound:
            case CFileException::badPath:
            case CFileException::tooManyOpenFiles:
            case CFileException::removeCurrentDir:
            case CFileException::badSeek:
            case CFileException::hardIO:
            case CFileException::sharingViolation:
            case CFileException::lockViolation:
            case CFileException::endOfFile:
                AfxMessageBox(8, 0, (UINT)-1);
                break;
            case CFileException::accessDenied:
            case CFileException::invalidFile:
            case CFileException::directoryFull:
            case CFileException::diskFull:
                AfxMessageBox(8, 0, (UINT)-1);
                break;
            default:
                AfxMessageBox(8, 0, (UINT)-1);
                break;
            }
            if (pDlg != NULL)
                delete pDlg;
            nFrameMode = nSavedMode;
            pView->bBusy = 0;
            g_bReplayMode = 0;
            return;
        }
        char buf[10];
        pFile->Read(buf, 9);
        buf[9] = 0;
        if (strcmp(buf, "YODASAV44") != 0)
        {
            if (pDlg != NULL)
                delete pDlg;
            AfxMessageBox(0xe008, 0, (UINT)-1);
            nFrameMode = nSavedMode;
            pView->bBusy = 0;
            g_bReplayMode = 0;
            return;
        }
        pFile->Read(&worldSeed, 4);
        pFile->Read(&currentPlanet, 4);
        pFile->Read(&bQuestCellsResident, 4);
        bStartingGame = 1;
        StartGame(0, 1);
        gameState = 0;
        abortFrame = 0;
        short nCount;
        pFile->Read(&nCount, 2);
        questItemsA.SetSize(0, -1);
        questItemsA.SetSize(nCount, -1);
        int i = 0;
        if (nCount > 0)
        {
            do
            {
                unsigned short v;
                pFile->Read(&v, 2);
                questItemsA.SetAt(i, v);
                if (nCount - i == 1)
                {
                    nCurrentGoalItem = (short)v;
                    startItem = ((Puzzle *)puzzles.GetAt((short)v))->itemA;
                }
                i++;
            } while (i < nCount);
        }
        pFile->Read(&nCount, 2);
        questItemsB.SetSize(0, -1);
        questItemsB.SetSize(nCount, -1);
        i = 0;
        if (nCount > 0)
        {
            do
            {
                unsigned short v;
                pFile->Read(&v, 2);
                questItemsB.SetAt(i, v);
                if (nCount - i == 1)
                {
                    nCurrentGoalItem = (short)v;
                    startItem2Maybe = ((Puzzle *)puzzles.GetAt((short)v))->itemB;
                }
                i++;
            } while (i < nCount);
        }
        i = 2;
        MapZone *pGridQuest = mapGrid + 0x2c;
        MapZone *pScratch = mapScratch;
        do
        {
            int j = 2;
            MapZone *pG = pGridQuest;
            do
            {
                MapZone *pCell = pG;
                if (bQuestCellsResident == 0)
                    pCell = pScratch;
                pFile->Read(&pCell->flagSolved, 4);
                pFile->Read(&pCell->flagA, 4);
                pFile->Read(&pCell->flagC, 4);
                pFile->Read(&pCell->flagB, 4);
                pFile->Read(&pCell->flagD, 4);
                pFile->Read(&pCell->id, 2);
                pFile->Read(&pCell->cellQuestSlot0, 2);
                pFile->Read(&pCell->cellItemA, 2);
                pFile->Read(&pCell->cellItemC, 2);
                pFile->Read(&pCell->cellQuestSlot1, 2);
                pFile->Read(&pCell->cellItemB, 2);
                pFile->Read(&pCell->cellQuestSlot5, 2);
                pFile->Read(&pCell->cellQuestSlot6, 2);
                pFile->Read(&pCell->zoneType, 4);
                pFile->Read(&pCell->field30, 2);
                pG++;
                pScratch++;
                j--;
            } while (j != 0);
            pGridQuest += 10;
            i--;
        } while (i != 0);
        int nDone = 0;
        int x = 0;
        int y = 0;
        do
        {
            pFile->Read(&x, 4);
            pFile->Read(&y, 4);
            if (y == -1 && x == -1)
            {
                nDone++;
            }
            else
            {
                short id;
                int nArg;
                pFile->Read(&id, 2);
                pFile->Read(&nArg, 4);
                LoadZoneRecursive(pFile, id, nArg);
            }
        } while (nDone == 0);
        int j;
        i = 10;
        MapZone *pBackup = mapGridBackup;
        Zone **ppZone = apZoneGrid;
        do
        {
            j = 10;
            do
            {
                MapZone *pCell = pBackup;
                if (bQuestCellsResident == 0)
                    pCell = pBackup - 100;
                pFile->Read(&pCell->flagSolved, 4);
                pFile->Read(&pCell->flagA, 4);
                pFile->Read(&pCell->flagC, 4);
                pFile->Read(&pCell->flagB, 4);
                pFile->Read(&pCell->flagD, 4);
                pFile->Read(&pCell->id, 2);
                pFile->Read(&pCell->cellQuestSlot0, 2);
                pFile->Read(&pCell->cellItemA, 2);
                pFile->Read(&pCell->cellItemC, 2);
                pFile->Read(&pCell->cellQuestSlot1, 2);
                pFile->Read(&pCell->cellItemB, 2);
                pFile->Read(&pCell->cellQuestSlot5, 2);
                pFile->Read(&pCell->cellQuestSlot6, 2);
                pFile->Read(&pCell->zoneType, 4);
                pFile->Read(&pCell->field30, 2);
                if (pCell->id >= 0)
                    *ppZone = (Zone *)zones.GetAt(pCell->id);
                ppZone++;
                pBackup++;
                j--;
            } while (j != 0);
            i--;
        } while (i != 0);
        nDone = 0;
        int x2 = 0;
        int y2 = 0;
        do
        {
            pFile->Read(&x2, 4);
            pFile->Read(&y2, 4);
            if (y2 == -1 && x2 == -1)
            {
                nDone++;
            }
            else
            {
                short id;
                int nArg;
                pFile->Read(&id, 2);
                pFile->Read(&nArg, 4);
                LoadZoneRecursive(pFile, id, nArg);
            }
        } while (nDone == 0);
        int nInv = inventory.GetSize();
        i = 0;
        if (nInv > 0)
        {
            do
            {
                InvItem *pItem = (InvItem *)inventory.GetAt(i);
                if (pItem != NULL)
                    delete pItem;
                i++;
            } while (nInv > i);
        }
        inventory.SetSize(0, -1);
        pFile->Read(&nInv, 4);
        i = 0;
        if (nInv > 0)
        {
            do
            {
                short nTile;
                pFile->Read(&nTile, 2);
                Tile *pTile = (Tile *)tiles.GetAt(nTile);
                InvItem *pNew;
                TRY {
                    pNew = new InvItem;
                }
                }              // closes the try block the TRY macro opened
                catch (CException *e2) {               // hand-expanded CATCH_ALL(e2)
                    _afxExceptionLink.m_pException = e2;
                    THROW_LAST();
                    AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable OOM dialog
                    AfxAbort();                            //      (docs/engine-bugs.md #7)
                }
                }              // closes the TRY macro's outer (link-scope) brace
                pNew->pTile = pTile;
                pNew->name = pTile->name;
                inventory.SetAtGrow(inventory.GetSize(), pNew);
                i++;
            } while (nInv > i);
        }
        short nZone;
        pFile->Read(&nZone, 2);
        pView->nTargetZoneId = nZone;
        pView->nTransitionStep = 0;
        pFile->Read(&playerX, 4);
        pFile->Read(&playerY, 4);
        short nWeapon;
        pFile->Read(&nWeapon, 2);
        if (nWeapon < 0)
        {
            currentWeapon = NULL;
        }
        else
        {
            currentWeapon = (Character *)characters.GetAt(nWeapon);
            short nAmmo;
            pFile->Read(&nAmmo, 2);
            currentWeapon->unk48 = nAmmo;
        }
        pFile->Read(&weaponState[0], 2);
        pFile->Read(&weaponState[1], 2);
        pFile->Read(&weaponState[2], 2);
        pFile->Read(&cameraX, 4);
        pFile->Read(&cameraY, 4);
        pFile->Read(&healthLo, 4);
        pFile->Read(&healthHi, 4);
        pFile->Read(&difficulty, 4);
        i = 0;
        int nElapsed;
        pFile->Read(&nElapsed, 4);
        timeBase = time(NULL) - nElapsed;
        pFile->Read(&totalZones, 4);
        unk248.SetSize(0, -1);
        short nCnt2;
        short nSum;
        pFile->Read(&nCnt2, 2);
        pFile->Read(&nSum, 2);
        if (nCnt2 > 0 && nSum > 0)
        {
            short nAvg = nSum / nCnt2;
            unk248.SetSize(nCnt2, -1);
            if (nCnt2 > 0)
            {
                do
                {
                    unk248.SetAt(i, nAvg);
                    i++;
                } while (i < nCnt2);
            }
        }
        pFile->Read(&nCurrentGoalItem, 4);
        pFile->Read(&goalItemTileId, 4);
        pFile->Close();
        if (pFile != NULL)
            delete pFile;
        if (pDlg != NULL)
            delete pDlg;
        MapZone *pCell = mapGrid;
        i = 10;
        do
        {
            j = 10;
            do
            {
                if (pCell->id >= 0 && pCell->flagSolved == 0)
                    PlaceZoneObjectTiles(pCell->id);
                pCell++;
                j--;
            } while (j != 0);
            i--;
        } while (i != 0);
        unsigned short vGoal;
        CWordArray *pArr;
        switch (currentPlanet)
        {
        case 1:
        {
            int bFound = 0;
            i = 0;
            int nSize = storyHistoryNevada.GetSize();
            nCount = (short)nSize;
            if (nCount > 0)
            {
                unsigned short *p = storyHistoryNevada.GetData();
                do
                {
                    if (*p == (unsigned int)nCurrentGoalItem)
                    {
                        bFound = 1;
                        break;
                    }
                    p++;
                    i++;
                } while (i < nCount);
            }
            if (bFound == 0)
            {
                vGoal = (unsigned short)nCurrentGoalItem;
                pArr = &storyHistoryNevada;
                pArr->SetAtGrow(nSize, vGoal);
            }
            break;
        }
        case 2:
        {
            int bFound = 0;
            i = 0;
            int nSize = storyHistoryAlaska.GetSize();
            nCount = (short)nSize;
            if (nCount > 0)
            {
                unsigned short *p = storyHistoryAlaska.GetData();
                do
                {
                    if (*p == (unsigned int)nCurrentGoalItem)
                    {
                        bFound = 1;
                        break;
                    }
                    p++;
                    i++;
                } while (i < nCount);
            }
            if (bFound == 0)
            {
                vGoal = (unsigned short)nCurrentGoalItem;
                pArr = &storyHistoryAlaska;
                pArr->SetAtGrow(nSize, vGoal);
            }
            break;
        }
        case 3:
        {
            int bFound = 0;
            i = 0;
            int nSize = storyHistoryOregon.GetSize();
            nCount = (short)nSize;
            if (nCount > 0)
            {
                unsigned short *p = storyHistoryOregon.GetData();
                do
                {
                    if (*p == (unsigned int)nCurrentGoalItem)
                    {
                        bFound = 1;
                        break;
                    }
                    p++;
                    i++;
                } while (i < nCount);
            }
            if (bFound == 0)
            {
                vGoal = (unsigned short)nCurrentGoalItem;
                pArr = &storyHistoryOregon;
                pArr->SetAtGrow(nSize, vGoal);
            }
            break;
        }
        }
        pView->bBusy = 0;
        abortFrame = 0;
        bWorldInvalid = 1;
        bWorldReadyMaybe = 1;
        nFrameMode = 0xb;
        bStartingGame = 0;
        nMapChangeReason = 0;
        g_bReplayMode = 0;
    }
}

// FUNCTION: YODA 0x00425e10  (compiler-generated scalar-deleting destructor ??_GCProgressCtrl —
// emitted into this TU by Load's local CProgressCtrl; body calls the lib ~CProgressCtrl)

// FUNCTION: YODA 0x00425e30
// Lay the (demo-hardcoded) quest into the 10x10 grid: pick one of the shipped goal zones by
// rand()%4 (or forced by the goal item), place its content, tag the center 2x2 cells.
int CDeskcppDoc::Populate()
{
    CDeskcppView *pView = NULL;
    POSITION pos = GetFirstViewPosition();
    if (pos != NULL)
        pView = (CDeskcppView *)GetNextView(pos);
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
    bQuestCellsResident = 1;
    BackupRecords();
    pView->bBusy = 0;
    return 1;
}

// FUNCTION: YODA 0x004260e0
// [WIP: reg 2-cycle {EDX,ECX} in the find loops + EH-state(-1) placement in the 0x217
// found path; lengths/structure converged.]
// Place a zone's quest content: find tileId in its IZX3 list and stamp it on a spawn object
// (zone 0x217 hardcodes the object at (3,3)).
int CDeskcppDoc::PlaceZone(short zoneId, unsigned short tileId)
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
                        pObj->arg = v;
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
                        pObj->arg = v;
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
                            pObj->arg = v;
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
void CDeskcppDoc::RestoreRecords()
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
void CDeskcppDoc::BackupRecords()
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
void CDeskcppDoc::SetupGrid()
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
Zone *CDeskcppDoc::ReadZone(CFile *pFile, int idx)
{
#ifdef GAME_INDY
    // Indy zones are parallel-array (verified in DESKTOP.DAW): the ZONE section is back-to-back
    // IZON tile records with NO per-zone planet/len prefix and NO planet filter (Indy has no
    // planets). Each zone's objects (HTSP), aux (IZAX/ZAX2/ZAX4/ZAX3) and scripts (ACTN) are
    // separate GLOBAL chunks parsed after all zones and distributed back to zones — TODO H3.
    (void)idx;
    Zone *pZone = new Zone(0x12, 0x12);
    pZone->ReadIzon(pFile);      // ReadIzon is shared: same 8 header bytes + tiles for both games
    return pZone;
#else
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
#endif
}

// ============================== GameView methods (TU tail) ==============================
// The doc TU ends with a block of GameView (CDeskcppView) methods, 0x426c40-0x429150.

// FUNCTION: YODA 0x00426c40
// [EFFECTIVE: DIFF~21, insns 351/351, align residual = two 1-insn scheduling rotations
// (the &nGameSpeed/&bInitialized cache loads vs neighboring pushes — arg-push scheduling
// family; SetTimer temp form inert) + one small reg pair. First compile was already
// structurally exact; the &field pointer caches emerge from plain field accesses here.
// v34 FIX: the 11 cursor loads use AfxGetResourceHandle() (NOT AfxGetInstanceHandle) — the
// orig reads AfxGetModuleState()->m_hCurrentResourceHandle @+0xc, we had InstanceHandle @+0x8
// (10 field bytes; DIFF 31->21). Both inline through AfxGetModuleState; see lesson #25.]
// CView::OnInitialUpdate override (vft +0xe8), one-shot via bInitialized: load the 11 game
// cursors, wire pWorld/nGameSpeed, create the inventory scrollbar + 0x1d1d game timer +
// 32x32 drag-tile canvas, and create (hidden) the 3 dialog balloon CBitmapButtons and the
// MS-Sans-Serif-8 dialog text CEdit.
void CDeskcppView::OnInitialUpdate()
{
    if (bInitialized == 0)
    {
        AfxGetResourceHandle();
        hCursor3 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x6a));
        hCursor9 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x6b));
        hCursor = LoadCursor(NULL, IDC_ARROW);
        hCursor2 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x71));
        hCursor4 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x73));
        hCursor5 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x6c));
        hCursor7 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x72));
        hCursor8 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x74));
        hCursor6 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x6d));
        hCursor10 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0x76));
        hCursor11 = LoadCursor(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc2));
        unkB8_always1 = 1;
        nMoveDY = 0;
        nMoveDX = 0;
        bShiftHeld = 0;
        bMapViewOpen = 0;
        bRearmHotspotsMaybe = 0;
        bFireKeyLatchMaybe = 0;
        draggedTile = NULL;
        bDragActive = 0;
        nDragLastScreenY = -1;
        nDragLastScreenX = -1;
        unk154 = 0;
        bBlinkState = 0;
        pWorld = (CDeskcppDoc *)m_pDocument;
        nGameSpeed = pWorld->gameSpeed;
        TRY {
            pInvScrollBar = new InvScrollBar(this, &pWorld->rectInvScroll);
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e) {                // hand-expanded CATCH_ALL(e)
            _afxExceptionLink.m_pException = e;
            AfxMessageBox(0xe01e, 0, (UINT)-1);
            AfxAbort();
        }
        }              // closes the TRY macro's outer (link-scope) brace
        nTimerId = ::SetTimer(m_hWnd, 0x1d1d, nGameSpeed, NULL);
        bInputLocked = 0;
        TRY {
            pDragTileCanvas = new Canvas(0x20, 0x20);
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e2) {               // hand-expanded CATCH_ALL(e2)
            _afxExceptionLink.m_pException = e2;
            AfxMessageBox(0xe01e, 0, (UINT)-1);
            AfxAbort();
        }
        }              // closes the TRY macro's outer (link-scope) brace
        if (pDragTileCanvas != NULL)
            pDragTileCanvas->SetPalette(0, 0x100, (RGBQUAD *)pWorld->pSysColorTable);
        WPARAM wFont = 0;
        nWalkFramePhase = 0;
        bMouseCaptured = 0;
        nMovePending = 0;
        bInitialized++;
        bSkipEntryIactMaybe = 0;
        CRect rc(0, 0, 0, 0);
        btnDialogClose.Create("", 0x5000000b, rc, this, 0x1389);
        btnDialogDown.Create("", 0x5000000b, rc, this, 0x138a);
        btnDialogUp.Create("", 0x5000000b, rc, this, 0x138b);
        btnDialogClose.LoadBitmaps("CLOSEU", "CLOSED", "CLOSEF", "CLOSEX");
        btnDialogDown.LoadBitmaps("DNAU", "DNAD", "DNAF", "DNAX");
        btnDialogUp.LoadBitmaps("UPAU", "UPAD", "UPAF", "UPAX");
        btnDialogClose.ShowWindow(0);
        btnDialogDown.ShowWindow(0);
        btnDialogUp.ShowWindow(0);
        CRect rcText(0, 0, 0x82, 0xd);
        wndDialogText.Create(0x50000504, rcText, this, 0x138c);
        HFONT hFont = CreateFont(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
        CFont *pFont = CFont::FromHandle(hFont);
        if (pFont != NULL)
            wFont = (WPARAM)pFont->m_hObject;
        ::SendMessage(wndDialogText.m_hWnd, WM_SETFONT, wFont, 1);
        wndDialogText.ShowWindow(0);
    }
}

// FUNCTION: YODA 0x004270f0
// [EFFECTIVE: insns 168/168, align=12. Residual = ECX/EDX 2-cycle on the pWorld reload +
// x/y temps in arrow blocks 2-3 (1 and 4 land exact) + the pDC param-load slot. Cracks:
// each arrow is a DUPLICATED full LoadIcon call per arm (cross-jumped tails), `== 0`
// disabled-icon-first arm order (VC4.2 jumps TO the then-arm here), per-call x/y int
// locals ahead of DrawIcon (block 1 x-first, blocks 2-4 y-first), rectArrowBox is a
// struct-copied RECT, dirs mask held as a char local.
// v34 FIX: the 8 icon loads use AfxGetResourceHandle() (NOT AfxGetInstanceHandle) — orig reads
// AfxGetModuleState()->m_hCurrentResourceHandle @+0xc, we had InstanceHandle @+0x8 (4 field
// bytes closed). Both inline through AfxGetModuleState; see lesson #25.]
// Fill the arrow panel (rectArrowBox widened 4px left/bottom, COLOR_3DFACE) and draw the
// four zone-exit arrows from pWorld->GetExitDirections()'s bitmask (bits 1/2/4/8 =
// N/S/E/W), enabled/disabled icon pairs 0xcb/0xca, 0xc5/0xc4, 0xc9/0xc8, 0xc7/0xc6. A NULL
// pDC means draw to our own window DC under the world palette (released at the end).
void CDeskcppView::DrawDirectionArrows(CDC *pDC)
{
    int bReleaseDC = 0;
    if (pWorld == NULL)
        pWorld = (CDeskcppDoc *)m_pDocument;
    CPalette *pOldPal;
    if (pDC == NULL)
    {
        pDC = CDC::FromHandle(::GetDC(m_hWnd));
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        bReleaseDC = 1;
    }
    RECT rc = pWorld->rectArrowBox;
    rc.left -= 4;
    rc.bottom += 4;
    char nDirs = (char)pWorld->GetExitDirections();
    AfxGetResourceHandle();
    CBrush br(GetSysColor(COLOR_3DFACE));
    ::FillRect(pDC->m_hDC, &rc, (HBRUSH)br.m_hObject);
    HICON hIcon;
    if ((nDirs & 1) == 0)
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xca));
    else
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xcb));
    {
        int x = pWorld->rectArrowBox.left + 0xb;
        int y = pWorld->rectArrowBox.top;
        ::DrawIcon(pDC->m_hDC, x, y, hIcon);
    }
    if ((nDirs & 8) == 0)
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc6));
    else
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc7));
    {
        int y = pWorld->rectArrowBox.top + 0xe;
        int x = pWorld->rectArrowBox.left - 3;
        ::DrawIcon(pDC->m_hDC, x, y, hIcon);
    }
    if ((nDirs & 2) == 0)
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc4));
    else
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc5));
    {
        int y = pWorld->rectArrowBox.top + 0x1d;
        int x = pWorld->rectArrowBox.left + 0xb;
        ::DrawIcon(pDC->m_hDC, x, y, hIcon);
    }
    if ((nDirs & 4) == 0)
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc8));
    else
        hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(0xc9));
    {
        int y = pWorld->rectArrowBox.top + 0xe;
        int x = pWorld->rectArrowBox.left + 0x1a;
        ::DrawIcon(pDC->m_hDC, x, y, hIcon);
    }
    if (bReleaseDC != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00427310
// Run the modal in-game text dialog: mark shown/busy, build a stack TextDialog carrying the
// text + 3 args + the sound session, park the world in frame-mode 5 (mode 3 on failure),
// and on success reset the drag/move interaction state and restore the saved frame mode.
int CDeskcppView::ShowTextDialog(CString &strText, int a, int b, int c)
{
    bTextDialogShown = 1;
    bBusy = 1;
    TextDialog dlg(this);
    pTextDialog = &dlg;
    dlg.strText = strText;
    dlg.nArgX = a;
    dlg.nArgY = b;
    dlg.soundSession = soundSession;
    dlg.nMode = c;
    int nSavedMode = pWorld->nFrameMode;
    ReleaseCapture();
    pWorld->nFrameMode = 5;
    int nRet = dlg.Run();
    if (nRet == 0)
    {
        pWorld->nFrameMode = 3;
        bBusy = 0;
        return 0;
    }
    pTextDialog = NULL;
    bMouseCaptured = 0;
    bShiftHeld = 0;
    nMovePending = 0;
    nMoveCommand = -1;
    bKeyboardMoveActive = 1;
    pWorld->nFrameMode = nSavedMode;
    bBusy = 0;
    if (nRet != 0)      // sic: dead re-test (nRet != 0 always here), still emitted (no DCE)
        return 1;
    return 0;
}

// FUNCTION: YODA 0x00427440  (compiler-generated implicit destructor ??1TextDialog — destroys
// only strText; emitted into this TU by ShowTextDialog's stack TextDialog)

// FUNCTION: YODA 0x00427490
// [EFFECTIVE-WIP: insns 167/165; residual = the this=ESI-vs-spill rotation (Generate
// family) — orig keeps this in ESI and the four coord ints in frame slots, ours spills
// this and registers the coords; everything else aligns. Not source-steerable piecemeal;
// joint pass.]
// Draw the circular health dial's 3D rim: two Chord halves over rectHealthDial inflated by
// 2px — highlight pen/brush for the lower-left half, shadow for the upper-right. NULL pDC
// means our own window DC under the world palette.
void CDeskcppView::DrawHealthDial(CDC *pDC)
{
    int bReleaseDC = 0;
    CPalette *pOldPal;
    if (pDC == NULL)
    {
        pDC = CDC::FromHandle(::GetDC(m_hWnd));
        if (pDC == NULL)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        bReleaseDC = 1;
    }
    CPen penShadow(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    CPen penHilite(PS_SOLID, 1, GetSysColor(COLOR_BTNHIGHLIGHT));
    CBrush brShadow(GetSysColor(COLOR_BTNSHADOW));
    CBrush brHilite(GetSysColor(COLOR_BTNHIGHLIGHT));
    int x1 = pWorld->rectHealthDial.left - 2;
    int x2 = pWorld->rectHealthDial.right + 2;
    int y1 = pWorld->rectHealthDial.top - 2;
    int y2 = pWorld->rectHealthDial.bottom + 2;
    CPen *pOldPen = pDC->SelectObject(&penHilite);
    CBrush *pOldBrush = pDC->SelectObject(&brHilite);
    ::Chord(pDC->m_hDC, x1, y1, x2, y2, x1, y2, x2, y1);
    pDC->SelectObject(&penShadow);
    pDC->SelectObject(&brShadow);
    ::Chord(pDC->m_hDC, x1, y1, x2, y2, x2, y1, x1, y2);
    pDC->SelectObject(pOldPen);
    pDC->SelectObject(pOldBrush);
    if (bReleaseDC != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00427690
// [EFFECTIVE: insns 158/158; residual = one consistent nLo/nHi ESI<->EBP bijection + the
// pTile-vs-m_nSize load order in the death tail + a cross-jumped-vs-duplicated epilogue.
// Cracks: heal tiers are FLAT >=300/>=200/>=100/else arms with the 1/1 clamp DUPLICATED
// per tier (compiler cross-jumps them back together — Ghidra re-merges and fakes a goto);
// damage clamp is `> -1`; death threshold mixes forms (`> 99 && >= 3`); the life-force
// tail is if/else-if (single exit).]
// Apply a health change (IACT AddHealth; negative = damage, scaled by 100/difficulty).
// healthLo (0..100) and healthHi (0..3 dial segments) ACCUMULATE damage; 100/3 = dead.
// Damage tiers -300/-200/-100 bump whole segments; heals reverse them. At the death
// threshold: consume the life-force item (tile 0x598) if carried and survive at 1/1, else
// (game not won) request the game-over frame via abortFrame = -1.
void CDeskcppView::AddHealth(int nDelta)
{
    int nLo = pWorld->healthLo;
    int nHi = pWorld->healthHi;
    if (bInvincibleCheat != 0)
        return;
    if (nDelta < 0)
    {
        int nScaled = nDelta / (100 / pWorld->difficulty);
        if (nScaled > -1)
            nScaled = -1;
        if (nScaled <= -300)
        {
            nHi += 3;
            nLo = nScaled / -3 - 1 + nLo;
            if (nHi > 3)
            {
                nHi = 3;
                nLo = 100;
            }
        }
        else if (nScaled <= -200)
        {
            nHi += 2;
            nLo = nScaled / -3 - 1 + nLo;
            if (nHi > 3)
            {
                nHi = 3;
                nLo = 100;
            }
        }
        else if (nScaled <= -100)
        {
            nHi += 1;
            nLo = nScaled / -3 - 1 + nLo;
            if (nHi > 3)
            {
                nHi = 3;
                nLo = 100;
            }
        }
        else
        {
            nLo -= nScaled;
            if (nLo > 100)
            {
                nLo -= 100;
                nHi += 1;
                if (nHi > 3)
                {
                    nHi = 3;
                    nLo = 100;
                }
            }
        }
    }
    else
    {
        if (nDelta < 1)
            nDelta = 1;
        if (nDelta >= 300)
        {
            nLo = 1;
            nHi = 1;
        }
        else if (nDelta >= 200)
        {
            nHi -= 2;
            nLo = nDelta / -3 + 1 + nLo;
            if (nHi < 1)
            {
                nHi = 1;
                nLo = 1;
            }
        }
        else if (nDelta >= 100)
        {
            nHi -= 1;
            if (nHi < 1)
            {
                nHi = 1;
                nLo = 1;
            }
        }
        else
        {
            nLo -= nDelta;
            if (nLo < 1)
            {
                nLo += 100;
                nHi -= 1;
                if (nHi < 1)
                {
                    nHi = 1;
                    nLo = 1;
                }
            }
        }
    }
    pWorld->healthLo = nLo;
    pWorld->healthHi = nHi;
    DrawHealthNeedle(NULL);
    if (nLo > 99 && nHi >= 3)
    {
        CDeskcppDoc *pW = pWorld;
        int bFound = 0;
        int i = 0;
        Tile *pTile = (Tile *)pW->tiles.GetAt(0x598);
        if (pW->inventory.GetSize() > 0)
        {
            InvItem **pp = (InvItem **)pW->inventory.GetData();
            do
            {
                if ((*pp)->pTile == pTile)
                {
                    bFound = 1;
                    break;
                }
                pp++;
                i++;
            } while (i < pW->inventory.GetSize());
        }
        if (bFound != 0)
        {
            pW->healthLo = 1;
            pW->healthHi = 1;
            PlaySound(0x3a);
            RemoveItem(pTile);
        }
        else if (pW->gameState != 1)
            pW->abortFrame = -1;
    }
}

// FUNCTION: YODA 0x004278a0
// [EFFECTIVE-WIP: insns 342/346, structure converged (early-return guard, range-pair
// else-if quadrant chain with jump threading, shared table). Residual = the pDC/this
// reg-vs-param-slot rotation (orig homes pDC in ESI, xe/ye in slots; ours the inverse —
// same family as DrawHealthDial; local-copy probe inert) + default-ctor zero-reg choice.]
// Draw the health-dial needle + pie fill. Colors by healthHi segment (1 yellow/green,
// 2 red/yellow, 3 black/red; 0 = default pens). The full disc is drawn in color A, then
// the remaining-health slice from 12 o'clock to the needle in color B; the needle end
// comes from the shared quarter-circle table, one quadrant per 25 points of healthLo.
// NULL pDC = own window DC under the world palette (leaked when healthLo == 0 — sic).
void CDeskcppView::DrawHealthNeedle(CDC *pDC)
{
    int bReleaseDC = 0;
    CPalette *pOldPal;
    if (pDC == NULL)
    {
        pDC = CDC::FromHandle(::GetDC(m_hWnd));
        if (pDC == NULL)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        bReleaseDC = 1;
    }
    CPen penA;
    CBrush brA;
    CPen penB;
    CBrush brB;
    int cx = pWorld->rectHealthDial.left + 0x10;
    int cy = pWorld->rectHealthDial.top - 1;
    if (pWorld->healthHi == 1)
    {
        penA.Attach(::CreatePen(PS_SOLID, 1, 0xffff));
        penB.Attach(::CreatePen(PS_SOLID, 1, 0xff00));
        brA.Attach(::CreateSolidBrush(0xffff));
        brB.Attach(::CreateSolidBrush(0xff00));
    }
    else if (pWorld->healthHi == 2)
    {
        penA.Attach(::CreatePen(PS_SOLID, 1, 0xff));
        penB.Attach(::CreatePen(PS_SOLID, 1, 0xffff));
        brA.Attach(::CreateSolidBrush(0xff));
        brB.Attach(::CreateSolidBrush(0xffff));
    }
    else if (pWorld->healthHi == 3)
    {
        penA.Attach(::CreatePen(PS_SOLID, 1, 0));
        penB.Attach(::CreatePen(PS_SOLID, 1, 0xff));
        brA.Attach(::CreateSolidBrush(0));
        brB.Attach(::CreateSolidBrush(0xff));
    }
    int nLo = pWorld->healthLo;
    int cx2 = pWorld->rectHealthDial.left + 0x10;
    int t = pWorld->rectHealthDial.top;
    int cy2 = t + 0x10;
    int l = pWorld->rectHealthDial.left;
    int r = pWorld->rectHealthDial.right;
    int b = pWorld->rectHealthDial.bottom;
    if (nLo == 0)
        return;
    {
        int xe, ye;
        if (nLo == 100)
        {
            xe = cx;
            ye = cy;
        }
        else if (nLo > 0 && nLo < 0x19)
        {
            xe = cx2 + gNeedleTable[nLo];
            ye = cy2 - gNeedleTable[25 - nLo];
        }
        else if (nLo >= 0x19 && nLo < 0x32)
        {
            ye = cy2 + gNeedleTable[nLo - 25];
            xe = cx2 + gNeedleTable[50 - nLo];
        }
        else if (nLo >= 0x32 && nLo < 0x4b)
        {
            xe = cx2 - gNeedleTable[nLo - 50];
            ye = cy2 + gNeedleTable[75 - nLo];
        }
        else if (nLo >= 0x4b && nLo < 0x64)
        {
            xe = cx2 - gNeedleTable[100 - nLo];
            ye = cy2 - gNeedleTable[nLo - 75];
        }
        CPen *pOldPen = pDC->SelectObject(&penA);
        CBrush *pOldBrush = pDC->SelectObject(&brA);
        ::Pie(pDC->m_hDC, l, t, r, b, cx, cy, cx, cy);
        if (nLo != 100)
        {
            pDC->SelectObject(&penB);
            pDC->SelectObject(&brB);
            ::Pie(pDC->m_hDC, l, t, r, b, cx, cy, xe, ye);
        }
        pDC->SelectObject(pOldPen);
        pDC->SelectObject(pOldBrush);
        if (bReleaseDC != 0)
        {
            pDC->SelectPalette(pOldPal, 0);
            ::ReleaseDC(m_hWnd, pDC->m_hDC);
        }
    }
}

// FUNCTION: YODA 0x00427d20
// [EFFECTIVE-STRUCTURAL (2026-07-06 v15 dedicated pass): insns 772/774, structure fully
// aligned — the residual is ONE allocator binding flip + its instruction-form echoes.
// Cracks that landed this pass (each verified against the disasm):
//  - NO coordinate locals AT ALL: the original writes `x + dx`, `y + dy`, `x + dx*2`,
//    `x + dx*3` as full expressions at EVERY site; the compiler CSEs x+dx/y+dy into
//    slots 0x18/0x14 itself (our explicit tx/ty locals put them in REGISTERS instead —
//    provably wrong slot/reg residency; fresh tx2/ty3 locals likewise). Same lesson as
//    GameData's "params used at 2+ sites CSE-spill on their own".
//  - NO short locals: sdx/sdy/sDmg word-slot stores don't exist in the original. The
//    16-bit DrawZoneCell arithmetic (`add ax,bx` / `imul bx,bx,3` / `add ax,word[esp+x]`)
//    falls out of DrawZoneCell's SHORT PARAMS applied to plain int expressions.
//    Operand order is source-mirrored per site: `x + nAX, nAY + y` (step1 saber),
//    `nAX + x, y + nAY` (step2/3 saber), `x + dx, y + dy` (step1 blaster),
//    `x + dx*2, y + dy*2` (step2 blaster: 32-bit LEAs, mem-first),
//    `dx*3 + x, dy*3 + y` (step3 blaster: 16-bit IMUL, mul-first).
//  - decl order `nType, bRifle, bSaber` puts nType in ESI (matches orig; probed all 6
//    orders + 3 placements — the rest of the head is order-insensitive).
// RESIDUAL: orig promotes dy->EBX, dx->EDI, this->EBP (x/y stay memory); ours promotes
// y->EBX, x->EBP, this->EDI (dx/dy stay memory). Root: the SECOND flag zero picks EDI
// (orig) vs EBX (ours) and the whole assignment cascades. Proven NOT steerable by decl
// order/placement (9 probes) and NOT TU-position (identical score in a minimal
// one-function TU) => it is the GameView class-decl DIAL (Worldgen.h carries ~19 of the
// ~60+ real methods; Phase E completes it). All align hits are this flip's echoes:
// dx-tests as cmp-mem vs test-reg, x+nAX as lea [ebp+esi] vs add-from-mem, 16-bit adds
// reg-vs-mem mirrored, CSE slot ranks rotated. JOINT-pass candidate with the rest of
// the GameView-block rotation family (DrawHealthDial/Needle, WeaponBox/Icon,
// AddItemToInv).]
// Player attack step nStep (1..3) in direction (dx,dy) from (x,y). Weapon class from the
// icon tile id: 0x1ff = rifle (HitEntityAt at range nStep), saber ids 0x12/0x1fe = adjacent
// swing (sideways cell per the threaded direction pick), else blaster (DamageEntityAt +
// tile clear + redraw); no projectile tile degrades the class to the saved equipped item
// (sic). Damage = weapon damage scaled by difficulty. equippedItem is swapped to the
// weapon's tile around the swing, and IACT event 3 runs at the struck cell.
void CDeskcppView::UseWeapon(int x, int y, int dx, int dy, int nStep)
{
    int nType = 0;
    int bRifle = 0;
    int bSaber = 0;
    CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    CDeskcppDoc *pW = pWorld;
    Tile *pSavedItem = pW->equippedItem;
    Character *pWeapon = pW->currentWeapon;
    pW->equippedItem = (Tile *)pW->tiles.GetAt(pWeapon->frames[7]);
    int nDmg = pWeapon->damage;
    int nDiff = pWorld->difficulty;
    if (nDiff < 0x32)
        nDmg = (nDiff / -5 + 10) * nDmg;
    if (nDmg < 1)
        nDmg = 1;
    int nTileId = pWeapon->frames[7];
    if (nTileId == 0x1ff)
        bRifle = 1;
    if (nTileId == 0x1fe || nTileId == 0x12)
        bSaber = 1;
    if (pWeapon->GetProjectileTile(1, -1, 0, 0, &pWorld->tiles) != NULL)
        nType = 1;
    if (bRifle)
        nType = 1;
    else if (bSaber)
        nType = 2;
    else
        nType = nType != 0 ? 3 : (int)pSavedItem;  // sic: degrades to the saved equipped-item POINTER value
    short nTile = pWorld->currentZone->GetTile(x + dx, y + dy, 1);
    switch (nStep)
    {
    case 1:
        switch (nType)
        {
        case 1:
        {
            nTile = pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                pW2->currentZone->HitEntityAt(x + dx, y + dy, &pW2->characters, nDmg, pW2, this);
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx, y + dy, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 2:
        {
            int nAX = 0;
            int nAY = 0;
            if (dx != 0 && dy == 0)
            {
                nTile = pWorld->currentZone->GetTile(x + dx, y - 1, 1);
                nAY = -1;
                nAX = dx;
            }
            else if (dy != 0 && dx == 0)
            {
                nAX = -1;
                nTile = pWorld->currentZone->GetTile(x - 1, y + dy, 1);
                nAY = dy;
            }
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(x + nAX, nAY + y, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + nAX, nAY + y, 1, (short)0xffff);
                    DrawZoneCell(x + nAX, nAY + y);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + nAX, nAY + y, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 3:
        {
            nTile = pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(x + dx, y + dy, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + dx, y + dy, 1, (short)0xffff);
                    DrawZoneCell(x + dx, y + dy);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx, y + dy, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        default:
            goto done;
        }
        break;
    case 2:
        switch (nType)
        {
        case 1:
        {
            pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            nTile = pWorld->currentZone->GetTile(x + dx * 2, y + dy * 2, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                pW2->currentZone->HitEntityAt(x + dx * 2, y + dy * 2, &pW2->characters, nDmg, pW2, this);
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx * 2, y + dy * 2, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 2:
        {
            int nAY = 0;
            int nAX = 0;
            if (dx != 0 && dy == 0)
            {
                nTile = pWorld->currentZone->GetTile(x + dx, y, 1);
                nAX = dx;
            }
            else if (dy != 0 && dx == 0)
            {
                nTile = pWorld->currentZone->GetTile(x, y + dy, 1);
                nAX = 0;
                nAY = dy;
            }
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(nAX + x, y + nAY, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(nAX + x, y + nAY, 1, (short)0xffff);
                    DrawZoneCell(nAX + x, y + nAY);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, nAX + x, y + nAY, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 3:
        {
            pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            nTile = pWorld->currentZone->GetTile(x + dx * 2, y + dy * 2, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(x + dx * 2, y + dy * 2, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + dx * 2, y + dy * 2, 1, (short)0xffff);
                    DrawZoneCell(x + dx * 2, y + dy * 2);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx * 2, y + dy * 2, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        default:
            goto done;
        }
        break;
    case 3:
        switch (nType)
        {
        case 1:
        {
            pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            pWorld->currentZone->GetTile(x + dx * 2, y + dy * 2, 1);
            nTile = pWorld->currentZone->GetTile(x + dx * 3, y + dy * 3, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                pW2->currentZone->HitEntityAt(x + dx * 3, y + dy * 3, &pW2->characters, nDmg, pW2, this);
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx * 3, y + dy * 3, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 2:
        {
            int nAY = 0;
            int nAX = 0;
            if (dx != 0 && dy == 0)
            {
                nTile = pWorld->currentZone->GetTile(x + dx, y + 1, 1);
                nAX = dx;
                nAY = 1;
            }
            else if (dy != 0 && dx == 0)
            {
                nTile = pWorld->currentZone->GetTile(x + 1, y + dy, 1);
                nAX = 1;
                nAY = dy;
            }
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(nAX + x, y + nAY, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(nAX + x, y + nAY, 1, (short)0xffff);
                    DrawZoneCell(nAX + x, y + nAY);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, nAX + x, y + nAY, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        case 3:
        {
            pWorld->currentZone->GetTile(x + dx, y + dy, 1);
            pWorld->currentZone->GetTile(x + dx * 2, y + dy * 2, 1);
            nTile = pWorld->currentZone->GetTile(x + dx * 3, y + dy * 3, 1);
            if (nTile >= 0 && (pWorld->GetTileData(nTile)->flags & 0x20000) != 0)
            {
                CDeskcppDoc *pW2 = pWorld;
                if (pW2->currentZone->DamageEntityAt(x + dx * 3, y + dy * 3, &pW2->characters,
                                                     nDmg, pW2, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + dx * 3, y + dy * 3, 1, (short)0xffff);
                    DrawZoneCell(dx * 3 + x, dy * 3 + y);
                }
            }
            bWeaponIactActiveMaybe = 1;
            {
                CDeskcppDoc *pW3 = pWorld;
                pW3->currentZone->IactRun(3, x + dx * 3, y + dy * 3, 0, 0, 0, pDC, pW3, this);
            }
            bWeaponIactActiveMaybe = 0;
            break;
        }
        default:
            goto done;
        }
        break;
    default:
        goto done;
    }
done:
    pWorld->equippedItem = pSavedItem;
    pDC->SelectPalette(pOldPal, 0);
    ::ReleaseDC(m_hWnd, pDC->m_hDC);
}

// FUNCTION: YODA 0x00428680
// [EFFECTIVE: align=0, insns 377/377 — pure ESI<->EDI role swap (60 identity bytes). Cracks:
// nTile is the movsx-IMMEDIATELY int form; DrawZoneCell args are plain int expressions
// (no short casts/locals at the call sites).
// ⭐ v39 PROOF-OF-MECHANISM: the entire 60B residual is ONE ESI<->EDI transposition of the two
// params (orig y->ESI x->EDI via a push-ebp-interleaved prologue that loads both at [esp+0x14];
// ours x->ESI y->EDI). Swapping the SIGNATURE to (int y,int x)+caller args collapses it to
// byte_diff=2 (reg_pen=0 identity_miss=0 — registers then MATCH), the 2 residual bytes being
// only the two prologue load displacements (y lands in param1's 0x10 slot vs orig's 0x14). BUT
// the swap is NOT faithful: the caller (StepDetonatorEffect 0x40e4bf) pushes [+0x15c]=nDetonatorX
// as arg1, [+0x160]=nDetonatorY as arg2 => the TRUE original sig is (int x,int y) as written. So
// the residual is the genuine intrinsic allocator choice: with the faithful (x,y) sig our cl
// enregisters param1(x)->ESI, orig enregisters param2(y)->ESI. NOT source-steerable without an
// ABI-breaking param reorder. PROVEN INTRINSIC (v39): identical asmscore solo vs full-TU, and
// byte-stable across a COMDAT-set probe (+3 COMDATs inserted immediately before it) and a
// reg-pressure predecessor — the emitted-set/position axis is DEAD for it (G2 note in CLAUDE.md).]
// Bomb blast: nine copy-pasted blocks, one per 3x3 cell around (x,y). Each reads the
// layer-1 tile; if its data is destructible (flags & 0x20000), DamageEntityAt (6 corners,
// 8 edges, 10 center) and on a hit clear the tile and redraw the cell.
void CDeskcppView::DetonateAdjacentTiles(int x, int y)
{
    {
        int nTile = (short)pWorld->currentZone->GetTile(x - 1, y - 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x - 1, y - 1, &pW->characters, 6, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x - 1, y - 1, 1, (short)0xffff);
                    DrawZoneCell(x - 1, y - 1);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x, y - 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x, y - 1, &pW->characters, 8, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x, y - 1, 1, (short)0xffff);
                    DrawZoneCell(x, y - 1);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x + 1, y - 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x + 1, y - 1, &pW->characters, 6, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + 1, y - 1, 1, (short)0xffff);
                    DrawZoneCell(x + 1, y - 1);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x - 1, y, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x - 1, y, &pW->characters, 8, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x - 1, y, 1, (short)0xffff);
                    DrawZoneCell(x - 1, y);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x, y, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x, y, &pW->characters, 10, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x, y, 1, (short)0xffff);
                    DrawZoneCell(x, y);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x + 1, y, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x + 1, y, &pW->characters, 8, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + 1, y, 1, (short)0xffff);
                    DrawZoneCell(x + 1, y);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x - 1, y + 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x - 1, y + 1, &pW->characters, 6, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x - 1, y + 1, 1, (short)0xffff);
                    DrawZoneCell(x - 1, y + 1);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x, y + 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x, y + 1, &pW->characters, 8, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x, y + 1, 1, (short)0xffff);
                    DrawZoneCell(x, y + 1);
                }
            }
        }
    }
    {
        int nTile = (short)pWorld->currentZone->GetTile(x + 1, y + 1, 1);
        if (nTile >= 0)
        {
            Tile *pT = pWorld->GetTileData(nTile);
            if (pT->flags & 0x20000)
            {
                CDeskcppDoc *pW = pWorld;
                if (pW->currentZone->DamageEntityAt(x + 1, y + 1, &pW->characters, 6, pW, this) != 0)
                {
                    pWorld->currentZone->SetTile(x + 1, y + 1, 1, (short)0xffff);
                    DrawZoneCell(x + 1, y + 1);
                }
            }
        }
    }
}

// FUNCTION: YODA 0x00428aa0
// WM_COMMAND 0x8001: minimize the main frame.
void CDeskcppView::OnCmdMinimize()
{
    CFrameWnd *pFrame = GetParentFrame();
    ::PostMessage(pFrame->m_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

// FUNCTION: YODA 0x00428ac0
// [EFFECTIVE-WIP: insns 123/120; residual = arm LAYOUT (orig falls through into the
// armed arm, JE to the out-of-line unarmed fill; ours inverts — both if-spellings compile
// identically here, so the placement is compiler-internal: open family) + bReleaseDC
// slot-vs-EBX + the GetNearestPaletteIndex import-cache reg.]
// Draw the current-weapon box: sunken 2px bevel around rectWeaponBox inflated by 2, the
// weapon's icon tile (frames[7]) masked onto the drag canvas over a COLOR_3DFACE fill
// (plain fill when unarmed), blitted at +3,+3, then a raised 1px bevel on the tight rect.
void CDeskcppView::DrawWeaponBox(CDC *pDC)
{
    int bReleaseDC = 0;
    CPalette *pOldPal;
    if (pDC == NULL)
    {
        pDC = CDC::FromHandle(::GetDC(m_hWnd));
        if (pDC == NULL)
            return;
        bReleaseDC = 1;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    }
    RECT rc;
    rc.left = pWorld->rectWeaponBox.left - 2;
    rc.right = pWorld->rectWeaponBox.right + 2;
    rc.top = pWorld->rectWeaponBox.top - 2;
    rc.bottom = pWorld->rectWeaponBox.bottom + 2;
    pWorld->DrawRect(pDC, &rc, 0, 2);
    if (pWorld->currentWeapon != NULL)
    {
        Tile *pTile = (Tile *)pWorld->tiles.GetAt(pWorld->currentWeapon->frames[7]);
        pDragTileCanvas->Fill((unsigned char)GetNearestPaletteIndex(
            (HPALETTE)pWorld->pPalette->m_hObject, GetSysColor(COLOR_3DFACE)));
        pDragTileCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, 0, 0, 0);
    }
    else
    {
        pDragTileCanvas->Fill((unsigned char)GetNearestPaletteIndex(
            (HPALETTE)pWorld->pPalette->m_hObject, GetSysColor(COLOR_3DFACE)));
    }
    pDragTileCanvas->BitBlt(pDC, rc.left + 3, rc.top + 3, 0x1e, 0x1e, 1, 1);
    pWorld->DrawRect(pDC, (RECT *)&pWorld->rectWeaponBox, 1, 1);
    if (bReleaseDC != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00428c40
// [EFFECTIVE-WIP: same arm-layout + reg families as DrawWeaponBox; armed-arm-first form
// kept (dropped DIFF 400->213). The per-shot-height dispatch is a sparse switch.]
// Draw the ammo bar: sunken bevel around rectAmmoBar inflated by 2; when armed, a 0x91
// (green) full-height column then black covering the spent part (0x1e - ammo * per-shot
// height from the weapon's icon tile id: blaster family 1, rifle 2, thermal 3), else a
// COLOR_3DFACE fill; raised 1px bevel on the tight rect.
void CDeskcppView::DrawWeaponIcon(CDC *pDC)
{
    int bReleaseDC = 0;
    CPalette *pOldPal;
    if (pDC == NULL)
    {
        pDC = CDC::FromHandle(::GetDC(m_hWnd));
        if (pDC == NULL)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        bReleaseDC = 1;
    }
    int nAmmo, nMult;
    if (pWorld->currentWeapon != NULL)
    {
        nAmmo = pWorld->currentWeapon->unk48;
        switch (pWorld->currentWeapon->frames[7])
        {
        case 0x12:
        case 0x1fe:
            nMult = 1;
            break;
        case 0x1ff:
            nMult = 2;
            break;
        case 0x200:
            nMult = 1;
            break;
        case 0x201:
            nMult = 3;
            break;
        default:
            nMult = 0;
            break;
        }
    }
    RECT rc;
    rc.left = pWorld->rectAmmoBar.left - 2;
    rc.right = pWorld->rectAmmoBar.right + 2;
    rc.top = pWorld->rectAmmoBar.top - 2;
    rc.bottom = pWorld->rectAmmoBar.bottom + 2;
    pWorld->DrawRect(pDC, &rc, 0, 2);
    int nHeight;
    if (pWorld->currentWeapon != NULL)
    {
        pDragTileCanvas->Fill(0x91);
        pDragTileCanvas->BitBlt(pDC, rc.left + 3, rc.top + 3, 7, 0x1e, 1, 1);
        pDragTileCanvas->Fill(0);
        nHeight = 0x1e - nAmmo * nMult;
    }
    else
    {
        pDragTileCanvas->Fill((unsigned char)GetNearestPaletteIndex(
            (HPALETTE)pWorld->pPalette->m_hObject, GetSysColor(COLOR_3DFACE)));
        nHeight = 0x1e;
    }
    pDragTileCanvas->BitBlt(pDC, rc.left + 3, rc.top + 3, 7, nHeight, 1, 1);
    pWorld->DrawRect(pDC, (RECT *)&pWorld->rectAmmoBar, 1, 1);
    if (bReleaseDC != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00428e30
// [EFFECTIVE: insns 80/78; residual = IV zero-init placement + a reg rotation in the
// BitBlt arg block (pWorld-reload color). Offset-form outer loop (while nOff < 0x51000)
// is decompile-literal; the x*y%2 signed-mod dance comes from the prod-accumulator.]
// Dim the 576x576 canvas with a multiplicative checkerboard (zero where x*y is even), blit
// the visible 288x288 window to the screen at (8,7), then restore the palette.
void CDeskcppView::BlitViewportDither()
{
    CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
    if (pDC != NULL)
    {
        CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
        unsigned char *pData = (unsigned char *)pWorld->pCanvas->GetData();
        int nOff = 0;
        int y = 0;
        do
        {
            int x = 0;
            int prod = 0;
            do
            {
                if (prod % 2 == 0)
                    pData[nOff + x] = 0;
                prod += y;
                x++;
            } while (x < 0x240);
            nOff += 0x240;
            y++;
        } while (nOff < 0x51000);
        pWorld->pCanvas->BitBlt(pDC, 8, 7, 0x120, 0x120, pWorld->nViewLeft, pWorld->nViewTop);
        pDC->SelectPalette(pOldPal, 0);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00428f30
// CWnd::PreCreateWindow override: run the base without WS_BORDER, then force it on.
BOOL CDeskcppView::PreCreateWindow(CREATESTRUCT &cs)
{
    cs.style &= ~WS_BORDER;
    BOOL bRet = CView::PreCreateWindow(cs);
    cs.style |= WS_BORDER;
    return bRet;                     // rides EAX across the |= for free
}

// FUNCTION: YODA 0x00428f50
// [EFFECTIVE-WIP: insns 160/162; residual = the same arm-LAYOUT normalization as the
// weapon-box pair (orig falls into the >7 scrollbar arm / jle out-of-line; ours inverts
// under both spellings) + the &pWorld lea-vs-reload and scan-backedge cmp direction.]
// Add a tile to the inventory unless it is a duplicate of a unique (flags & 0x100000)
// item: new InvItem named after the tile, the locator (flags == 0x100081) inserts at the
// front, others at slot 2 (or append while fewer than 2 items); then rescale the inventory
// scrollbar (visible rows: 7) and repaint via OnUpdate(NULL, 1, NULL).
void CDeskcppView::AddItemToInv(Tile *pTile)
{
    int bFound = 0;
    if (pTile != NULL)
    {
        int i = 0;
        int nInv = pWorld->inventory.GetSize();
        if (nInv > 0)
        {
            InvItem **pp = (InvItem **)pWorld->inventory.GetData();
            do
            {
                if ((*pp)->pTile == pTile)
                {
                    bFound = 1;
                    break;
                }
                pp++;
                i++;
            } while (i < nInv);
        }
        if (bFound == 0 || (pTile->flags & 0x100000) == 0)
        {
            InvItem *pNew;
            TRY {
                pNew = new InvItem(pTile, "");
            }
            }              // closes the try block the TRY macro opened
            catch (CException *e) {                // hand-expanded CATCH_ALL(e)
                _afxExceptionLink.m_pException = e;
                AfxMessageBox(0xe01e, 0, (UINT)-1);
                AfxAbort();
            }
            }              // closes the TRY macro's outer (link-scope) brace
            PlaySound(3);
            pNew->name = pTile->name;
            if (pTile->flags == 0x100081)
            {
                pWorld->inventory.InsertAt(0, pNew, 1);
            }
            else
            {
                if (pWorld->inventory.GetSize() <= 1)
                    pWorld->inventory.SetAtGrow(pWorld->inventory.GetSize(), pNew);
                else
                    pWorld->inventory.InsertAt(2, pNew, 1);
            }
            if (pWorld->inventory.GetSize() > 7)
            {
                int n = pWorld->inventory.GetSize() - 7;
                ::SetScrollRange(pInvScrollBar->m_hWnd, SB_CTL, 0, n, 1);
                pInvScrollBar->scrollMax = n;
            }
            else
            {
                ::SetScrollRange(pInvScrollBar->m_hWnd, SB_CTL, 0, 1, 1);
                pInvScrollBar->scrollMax = 0;
            }
            OnUpdate(NULL, 1, NULL);
        }
    }
}

// FUNCTION: YODA 0x00429150
// Remove an inventory item, matched by its Tile* definition. If it was the equipped weapon
// (tiles[0x12]) clear currentWeapon and repaint the weapon icon + box; then scan the inventory,
// RemoveAt + delete the matching InvItem, repaint the item text, and resize the inventory
// scrollbar (max 0 while <8 items, else count-7). The TU's last function (was missed by the
// "zero FUN_*" sweep — it sits just past the recorded 0x429150 end; found via the G0 link audit).
void CDeskcppView::RemoveItem(Tile *pItem)
{
    if (pWorld->tiles.GetAt(0x12) == (CObject *)pItem)
    {
        pWorld->currentWeapon = NULL;
        DrawWeaponIcon(NULL);
        DrawWeaponBox(NULL);
    }
    CDeskcppDoc *pW = pWorld;
    int i = 0;
    int n = pW->inventory.GetSize();
    if (n > 0)
    {
        InvItem **pp = (InvItem **)pW->inventory.GetData();
        do
        {
            InvItem *pEntry = *pp;
            if (pEntry->pTile == pItem)
            {
                pW->inventory.RemoveAt(i, 1);
                delete pEntry;
                break;
            }
            pp++;
            i++;
        } while (n > i);
    }
    DrawText(NULL);
    int cnt = pWorld->inventory.GetSize();
    if (cnt > 7)
    {
        cnt -= 7;
        pInvScrollBar->SetScrollRange(0, cnt, TRUE);
        pInvScrollBar->scrollMax = cnt;
    }
    else
    {
        pInvScrollBar->SetScrollRange(0, 1, TRUE);
        pInvScrollBar->scrollMax = 0;
    }
}

// ============================================================================
// Indiana Jones' Desktop Adventures worldgen (GAME_INDY) — transcribed from
// DESKADV.EXE (16-bit). Each function carries its DESKADV seg:off marker.
// Not byte-matched (Indy-only); anchor-safe (whole block is GAME_INDY-guarded
// and end-of-file). See docs/phase-h3-indy.md milestone 4.
// ============================================================================
#ifdef GAME_INDY

// --- Integration shim for the transcribed Indy worldgen ---
// VC++ 4.2 predates C++ bool; map the draft's bool/true/false to int.
#define bool  int
#define true  1
#define false 0
// Aliased helpers: the Indy engine reuses these Yoda functions verbatim (same
// engine). Wire the Indy* call names to the existing methods. Arg order verified
// against the Yoda signatures in Worldgen.h.
#define IndyIsItemPlaced(i)                   IsItemPlaced(i)
#define IndyMarkZoneUsed(z)                   AddPlacedZoneId(z)
#define IndyShuffleList(l)                    WorldgenShuffleList(l)
#define IndyZoneRequiresItem(item, zone)      ZoneRequiresItemMaybe(zone, item)
#define IndyQueueItemForPlacement(tag, item)  WorldgenPushZoneEntry(item, tag)
#define IndyAddPlacedItemEntry(tag, item)     WorldgenAddZoneEntry(item, tag)
#define IndyFilterEnemyZonesFromPlacedList()  RemoveEmptyZonesFromPlacedList()
// Intentionally-skipped INI persistence + DOS-time seed helper (seed comes from
// the existing Randomize()). No-ops for now (docs/phase-h3-indy.md TODOs).
#define IndyLoadPlacedZoneList()              ((void)0)
#define IndyLoadStoryHistory()                ((void)0)
#define IndySavePlacedZoneList()              ((void)0)
#define IndySaveStoryHistory()                ((void)0)
#define IndyTime()                            0

// ===========================================================================
// Tiny accessors
// ===========================================================================

// Indy IndyGetZoneById — DESKADV 1020:11b8
// Bounds-checked zones.GetAt (returns NULL if id<0 or id>=count). Yoda inlines this.
Zone *CDeskcppDoc::IndyGetZoneById(int nZoneId)
{
    if (nZoneId >= 0 && nZoneId < zones.GetSize())
        return (Zone *)zones.GetAt(nZoneId);
    return NULL;
}

// Indy IndyIsPuzzleUsed — DESKADV 1010:9b4e
// Linear membership in the used-puzzle list (== our storyHistoryNevada, doc+0x140).
int CDeskcppDoc::IndyIsPuzzleUsed(short puzzleId)
{
    int n = storyHistoryNevada.GetSize();
    for (int i = 0; i < n; i++)
        if ((short)storyHistoryNevada.GetAt(i) == puzzleId)
            return 1;
    return 0;
}

// ===========================================================================
// Zone item-pool membership tests (recurse through DOOR_IN children, obj type 9)
// ===========================================================================

// Indy IndyZoneProvidesItem — DESKADV 1010:5566
// True if zoneId (or any DOOR_IN child) offers `itemId` in its providedItemsA (IZAX) pool.
// (Structural twin of IndyZoneRequiresItem but reads providedItemsA instead of genCandidateA.)
int CDeskcppDoc::IndyZoneProvidesItem(short itemId, short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    int found = 0;
    int nA = pZone->providedItemsA.GetSize();
    for (int i = 0; i < nA; i++)
    {
        if ((short)pZone->providedItemsA.GetAt(i) == itemId)
        {
            found = 1;
            break;
        }
    }
    if (found == 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9)          // DOOR_IN
            {
                if (pObj->arg >= 0)
                    found = IndyZoneProvidesItem(itemId, pObj->arg);
                if (found == 1)
                    return 1;
            }
        }
    }
    return found;
}

// Indy IndyZoneSpawnPoolHasItem — DESKADV 1010:5842
// itemId==-1: does the zone have ANY spawn-pool (genCandidateB / IZX3) entries?
// else: does genCandidateB contain itemId (recursing DOOR_IN children)?
unsigned int CDeskcppDoc::IndyZoneSpawnPoolHasItem(short itemId, short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    if (pZone == NULL)
        return 0;
    int nB = pZone->genCandidateB.GetSize();
    if (itemId == -1)
        return (unsigned int)(nB > 0);
    unsigned int found = 0;
    for (int i = 0; i < nB; i++)
    {
        if ((short)pZone->genCandidateB.GetAt(i) == itemId)
        {
            found = 1;
            break;
        }
    }
    if (found == 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9)          // DOOR_IN
            {
                if (pObj->arg >= 0)
                    found = IndyZoneSpawnPoolHasItem(itemId, pObj->arg);
                if (found == 1)
                    return 1;
            }
        }
    }
    return found;
}

// ===========================================================================
// Item pickers
// ===========================================================================

// Indy IndyPickUnplacedProvidedItem — DESKADV 1010:7a28
// Build a temp list of this zone's providedItemsA (IZAX) items that IsItemPlaced hasn't
// seen, random-pick one; recurse DOOR_IN if none. When bAvoidRange (Indy param_3==0) is
// set, items in the id range [0x21d..0x21f] are EXCLUDED (special/reserved items).
// (Indy: local_12 top word 0=avoid-range OFF for param_3!=0, so the range filter runs only
//  when the caller passes bFirst-like 0; here modeled as bAvoidRange.)
int CDeskcppDoc::IndyPickUnplacedProvidedItem(int bAvoidRange, int nZoneId)
{
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    int result = -1;
    CWordArray list;
    int nA = pZone->providedItemsA.GetSize();
    for (int i = 0; i < nA; i++)
    {
        short item = (short)pZone->providedItemsA.GetAt(i);
        if (IsItemPlaced(item) == 0)
        {
            if (!bAvoidRange)
                list.SetAtGrow(list.GetSize(), item);
            // bAvoidRange: keep unless item in [0x21d..0x21f] and != 0x21c handling;
            // faithful form of the DESKADV SBORROW test:
            else if (item != 0x21c && !(item >= 0x21d && item <= 0x21f))
                list.SetAtGrow(list.GetSize(), item);
        }
    }
    if (list.GetSize() > 0)
        result = (short)list.GetAt(rand() % list.GetSize());
    if (result < 0)                       // none here — recurse DOOR_IN children
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9)
            {
                result = IndyPickUnplacedProvidedItem(bAvoidRange, pObj->arg);
                if (result >= 0)
                    break;
            }
        }
    }
    return result;
}

// Indy IndyPickUnplacedSpawnItem — DESKADV 1010:571e
// Random unplaced item from genCandidateB (IZX3 spawn pool); recurse DOOR_IN if none.
int CDeskcppDoc::IndyPickUnplacedSpawnItem(short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    int result = -1;
    if (pZone == NULL)
        return -1;
    int nB = pZone->genCandidateB.GetSize();
    if (nB > 0)
    {
        CWordArray list;
        for (int i = 0; i < nB; i++)
        {
            short item = (short)pZone->genCandidateB.GetAt(i);
            if (IsItemPlaced(item) == 0)
                list.SetAtGrow(list.GetSize(), item);
        }
        if (list.GetSize() < 1)
            result = -1;
        else
            result = (short)list.GetAt(rand() % list.GetSize());
    }
    if (result < 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9 && pObj->arg >= 0)
            {
                result = IndyPickUnplacedSpawnItem(pObj->arg);
                if (result >= 0)
                    return result;
            }
        }
    }
    return result;
}

// Indy IndyFindItemInProvidedPool — DESKADV 1010:593e
// Return itemId if it is in this zone's providedItemsA (IZAX) pool (recurse DOOR_IN),
// else -1. NOTE the DESKADV quirk: on a hit it returns the matched value (== itemId);
// if the pool is scanned without a match it returns the LAST scanned value only when the
// loop body set iVar1 — but the faithful net behaviour is "return itemId if present else -1".
int CDeskcppDoc::IndyFindItemInProvidedPool(short itemId, short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    if (pZone == NULL)
        return -1;
    int result = -1;
    int nA = pZone->providedItemsA.GetSize();
    for (int i = 0; i < nA; i++)
    {
        if ((short)pZone->providedItemsA.GetAt(i) == itemId)
        {
            result = itemId;
            break;
        }
    }
    if (result < 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9)
            {
                if (pObj->arg >= 0)
                    result = IndyFindItemInProvidedPool(itemId, pObj->arg);
                if (result >= 0)
                    return result;
            }
        }
    }
    return result;
}

// ===========================================================================
// Item placement into host objects
// ===========================================================================

// Indy IndyFillQuestItemSpot — DESKADV 1010:5a14
// In a zone whose genCandidateA (IZX2) requires `itemId`, pick a random ITEM host
// (obj type 0), set obj->arg = itemId, obj->state = 1. Recurse DOOR_IN. Returns itemId
// on success else -1. (Yoda WorldgenFillQuestItemSpot analog, single-pool.)
int CDeskcppDoc::IndyFillQuestItemSpot(short itemId, short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    int result = -1;
    if (pZone == NULL)
        return -1;
    int nReq = pZone->genCandidateA.GetSize();
    for (int r = 0; r < nReq; r++)
    {
        if ((short)pZone->genCandidateA.GetAt(r) == itemId)
        {
            CWordArray hosts;                 // indices of type-0 objects
            int nObj = pZone->objects.GetSize();
            for (int i = 0; i < nObj; i++)
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
                if (pObj->type == 0)          // ITEM host
                    hosts.SetAtGrow(hosts.GetSize(), (short)i);
            }
            if (hosts.GetSize() > 0)
            {
                int idx = (short)hosts.GetAt(rand() % hosts.GetSize());
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(idx);
                if (pObj != NULL && pObj->type == 0)
                {
                    result = itemId;
                    pObj->arg = itemId;
                    pObj->state = 1;
                }
            }
            if (result >= 0)
                break;
        }
    }
    if (result == -1)                         // recurse DOOR_IN
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9 && pObj->arg >= 0)
            {
                result = IndyFillQuestItemSpot(itemId, pObj->arg);
                if (result >= 0)
                    break;
            }
        }
    }
    return result;
}

// Indy IndyFillSpawn — DESKADV 1010:5bdc
// Identical to IndyFillQuestItemSpot but the required pool is genCandidateB (IZX3, spawn)
// and the host object type is 1 (SPAWN). On success also records the spawn item into the
// per-cell scratch (genCellQuestSlot5Scratch, DESKADV doc+0x21). Returns itemId or -1.
int CDeskcppDoc::IndyFillSpawn(short itemId, short zoneId)
{
    Zone *pZone = IndyGetZoneById(zoneId);
    int result = -1;
    if (pZone == NULL)
        return -1;
    int nSpawn = pZone->genCandidateB.GetSize();
    for (int r = 0; r < nSpawn; r++)
    {
        if ((short)pZone->genCandidateB.GetAt(r) == itemId)
        {
            CWordArray hosts;                 // indices of type-1 objects
            int nObj = pZone->objects.GetSize();
            for (int i = 0; i < nObj; i++)
            {
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
                if (pObj->type == 1)          // SPAWN host
                    hosts.SetAtGrow(hosts.GetSize(), (short)i);
            }
            if (hosts.GetSize() > 0)
            {
                int idx = (short)hosts.GetAt(rand() % hosts.GetSize());
                ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(idx);
                if (pObj != NULL && pObj->type == 1)
                {
                    result = itemId;
                    pObj->arg = itemId;
                    pObj->state = 1;
                    genCellQuestSlot5Scratch = itemId;   // DESKADV doc+0x21
                }
            }
            if (result >= 0)
                break;
        }
    }
    if (result == -1)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9 && pObj->arg >= 0)
            {
                result = IndyFillSpawn(itemId, pObj->arg);
                if (result >= 0)
                    break;
            }
        }
    }
    return result;
}

// Indy IndyPlaceItemOnLock — DESKADV 1010:610a
// If the zone PROVIDES `itemId` (providedItemsA), find a LOCK host (obj type 0xc), set its
// arg = itemId, state = 1, record AddPlacedItemEntry + genCellItemAScratch (doc+0x1f).
// Recurse DOOR_IN. Returns 1 on success.
int CDeskcppDoc::IndyPlaceItemOnLock(short itemId, short nQueueTag, int nZoneId)
{
    if (nZoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    int result = 0;
    bool bProvides = false;
    int nA = pZone->providedItemsA.GetSize();
    for (int i = 0; i < nA; i++)
        if ((short)pZone->providedItemsA.GetAt(i) == itemId) { bProvides = true; break; }
    if (bProvides)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj->type == 0xc)            // LOCK
            {
                WorldgenAddZoneEntry(itemId, nQueueTag);   // IndyAddPlacedItemEntry
                genCellItemAScratch = itemId;              // doc+0x1f
                pObj->arg = itemId;
                pObj->state = 1;
                result = 1;
                break;
            }
        }
    }
    if (result == 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj->type == 9 &&
                (result = IndyPlaceItemOnLock(itemId, nQueueTag, pObj->arg)) == 1)
                return 1;
        }
    }
    return result;
}

// Indy IndyPlaceRewardInItemSpot — DESKADV 1010:6260
// If the zone REQUIRES `itemId` (genCandidateA), place it into a random ITEM host (type 0),
// record AddPlacedItemEntry + genCellItemBScratch (doc+0x1d). Recurse DOOR_IN. Returns 1.
int CDeskcppDoc::IndyPlaceRewardInItemSpot(short itemId, short nQueueTag, int nZoneId)
{
    if (nZoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    int result = 0;
    bool bRequires = false;
    int nReq = pZone->genCandidateA.GetSize();
    for (int i = 0; i < nReq; i++)
        if ((short)pZone->genCandidateA.GetAt(i) == itemId) { bRequires = true; break; }
    if (bRequires)
    {
        CWordArray hosts;
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj->type == 0)
                hosts.SetAtGrow(hosts.GetSize(), (short)i);
        }
        if (hosts.GetSize() > 0)
        {
            int idx = (short)hosts.GetAt(rand() % hosts.GetSize());
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(idx);
            WorldgenAddZoneEntry(itemId, nQueueTag);     // IndyAddPlacedItemEntry
            genCellItemAScratch = itemId;                // doc+0x1d (reward -> itemB scratch role)
            pObj->arg = itemId;
            pObj->state = 1;
            result = 1;
        }
    }
    if (result == 0)
    {
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj->type == 9 &&
                (result = IndyPlaceRewardInItemSpot(itemId, nQueueTag, pObj->arg)) == 1)
                return 1;
        }
    }
    return result;
}

// ===========================================================================
// Zone item availability (item-carry objects: types 6/7/8 must be placed; 9 recurses)
// ===========================================================================

// Indy IndyCheckZoneItemsAvailable — DESKADV 1010:83bc
// Every carried-item object (type 6,7,8 with arg>=0) in the zone must ALREADY be placed
// (IsItemPlaced) for the zone to be usable; return 0 if any such item is NOT yet placed
// (i.e. still available). Recurse DOOR_IN (type 9). (obj+6==0 sub-test dropped, see header.)
int CDeskcppDoc::IndyCheckZoneItemsAvailable(int nZoneId)
{
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL)
        return 0;
    int ok = 1;
    int nObj = pZone->objects.GetSize();
    for (int i = 0; i < nObj; i++)
    {
        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
        unsigned int t = pObj->type;
        if (t > 5)
        {
            if (t == 6 || t == 7 || t == 8)          // carried item host
            {
                if (pObj->arg >= 0 && IsItemPlaced(pObj->arg) != 0)
                    ok = 0;                          // that item already placed -> unavailable
            }
            else if (t == 9)                         // DOOR_IN
            {
                if (pObj->arg >= 0)
                    ok = IndyCheckZoneItemsAvailable(pObj->arg);
            }
        }
        if (ok == 0)
            return 0;
    }
    return ok;
}

// Indy IndyCollectZoneItems — DESKADV 1010:8482
// Mark all of a zone's carried items (obj type 6/7/8) as placed (AddPlacedItemEntry),
// recursing DOOR_IN (type 9). Called after a zone is populated to consume its items.
void CDeskcppDoc::IndyCollectZoneItems(int nZoneId)
{
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL)
        return;
    int nObj = pZone->objects.GetSize();
    for (int i = 0; i < nObj; i++)
    {
        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
        unsigned int t = pObj->type;
        if (t > 5)
        {
            if (t == 6 || t == 7 || t == 8)
            {
                if (pObj->arg >= 0)
                    WorldgenAddZoneEntry(pObj->arg, -1);   // IndyAddPlacedItemEntry(tag=0xffff)
            }
            else if (t == 9)
            {
                if (pObj->arg >= 0)
                    IndyCollectZoneItems(pObj->arg);
            }
        }
    }
}

// ===========================================================================
// Populate a placed quest zone (fills its objects with the quest's items)
// ===========================================================================

// Indy IndyPopulateGoalZone — DESKADV 1010:5dac  (node type 10, FINAL_ITEM)
// The goal zone: the puzzle at step nStepSlot supplies two item ids (puzzle.itemA of the
// slot's two puzzle entries). It needs the "provided" item present + the "required" item
// placeable; either via the spawn pool (IndyFillSpawn) or the provided/required item
// spots (IndyFindItemInProvidedPool + IndyFillQuestItemSpot). Returns 1 on success.
// NOTE: goalTileList holds the per-step puzzle id (doc+0x136); slot entries [nStepSlot]
//   and [nStepSlot+1] are the two puzzle ids. Their Puzzle.itemA give iProvide/iRequire.
int CDeskcppDoc::IndyPopulateGoalZone(short nQueueTag, int nStepSlot, int nZoneId)
{
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL || pZone->type != 10)
        return 0;
    // Two puzzle ids at goalTileList[nStepSlot], [nStepSlot+1]:
    short puzA = (short)goalTileList.GetAt(nStepSlot);
    short puzB = (short)goalTileList.GetAt(nStepSlot + 1);   // DESKADV reads slot pair
    short iProvide = ((Puzzle *)puzzles.GetAt(puzA))->itemA;  // uVar7 (doc+0x1f role)
    short iRequire = ((Puzzle *)puzzles.GetAt(puzB))->itemA;  // uVar8 (doc+0x1d role)

    if (IndyCheckZoneItemsAvailable(nZoneId) == 0)
        return 0;

    int spawnItem = IndyPickUnplacedSpawnItem(nZoneId);
    int spawnOk = 0;
    if (spawnItem >= 0)
        spawnOk = IndyZoneSpawnPoolHasItem((short)spawnItem, nZoneId);
    int prov = IndyZoneProvidesItem(iProvide, nZoneId);
    int req  = IndyZoneRequiresItem(iRequire, nZoneId);   // == Yoda ZoneRequiresItemMaybe
    if (prov != 0 && req != 0)
    {
        if (spawnOk == 0)
        {
            int r1 = IndyFindItemInProvidedPool(iProvide, nZoneId);
            int r2 = IndyFillQuestItemSpot(iRequire, nZoneId);
            if (r1 < 0 || r2 < 0)
                return 0;
            // record per-cell scratch (DESKADV doc+0x1d/0x1f/0x21/0x23):
            genCellQuestSlot5Scratch = -1;               // doc+0x21 = -1
            genCellItemAScratch = iProvide;              // doc+0x1f
            genCellItemBScratch = iRequire;              // doc+0x1d
            genCellQuestSlot0Scratch = nStepSlot;        // doc+0x23
            IndyCollectZoneItems(nZoneId);
            WorldgenAddZoneEntry(iProvide, nQueueTag);   // IndyAddPlacedItemEntry
            WorldgenAddZoneEntry(iRequire, nQueueTag);
        }
        else
        {
            int r = IndyFillSpawn((short)spawnItem, nZoneId);
            if (r < 0)
                return 0;
            genCellQuestSlot5Scratch = spawnItem;        // doc+0x21
            genCellItemAScratch = iProvide;              // doc+0x1f
            genCellItemBScratch = iRequire;              // doc+0x1d
            genCellQuestSlot0Scratch = nStepSlot;        // doc+0x23
            IndyCollectZoneItems(nZoneId);
        }
        return 1;
    }
    return 0;
}

// Indy IndyPopulateTradeZone — DESKADV 1010:6422  (node type 0xf, TRADE)
// Zone must be type 0xf with an empty spawn pool (genCandidateB size < 1). The step's
// puzzle (goalTileList[nStepSlot]) gives itemA (the lock item); if there is a next step
// (nStepSlot < nQuestSteps-1) the +1 puzzle gives the reward item. Place lock item on a
// LOCK, reward in an ITEM spot. Returns true on success.
int CDeskcppDoc::IndyPopulateTradeZone(short nQueueTag, int nStepSlot, int nZoneId)
{
    if (nZoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL || pZone->type != 0xf || pZone->genCandidateB.GetSize() >= 1)
        return 0;
    short lockItem   = -1;
    short rewardItem = -1;
    short puz = (short)goalTileList.GetAt(nStepSlot);
    if (puz >= 0)
        lockItem = ((Puzzle *)puzzles.GetAt(puz))->itemA;
    if (nStepSlot <= goalTileList.GetSize() - 1)          // has a following step
    {
        short puz2 = (short)goalTileList.GetAt(nStepSlot + 1);
        if (puz2 >= 0)
            rewardItem = ((Puzzle *)puzzles.GetAt(puz2))->itemA;
    }
    WorldgenAddZoneEntry(lockItem, nQueueTag);            // provisional placement...
    WorldgenAddZoneEntry(rewardItem, nQueueTag);
    if (IndyCheckZoneItemsAvailable(nZoneId) == 0)
    {
        RemoveZoneEntry2(lockItem);
        RemoveZoneEntry2(lockItem);   // sic: DESKADV removes lockItem twice (mirror engine-bug)
        return 0;
    }
    bool ok;
    if (IndyPlaceItemOnLock(lockItem, nQueueTag, nZoneId) == 0)
        ok = false;
    else
        ok = IndyPlaceRewardInItemSpot(rewardItem, nQueueTag, nZoneId) != 0;
    if (ok)
    {
        genCellQuestSlot0Scratch = nStepSlot;             // doc+0x23
        IndyCollectZoneItems(nZoneId);
    }
    return ok;
}

// Indy IndyPopulateTransactionZone — DESKADV 1010:5f66  (node type 0x10, TRANSACTION)
// Zone type 0x10; the step puzzle's itemA is the provided item; if a next step exists its
// puzzle itemA is the required item. Needs the zone to PROVIDE the provided item and (if
// present) REQUIRE the required item, then FillSpawn a picked spawn item. Returns 1.
int CDeskcppDoc::IndyPopulateTransactionZone(short nQueueTag, int nStepSlot, int nZoneId)
{
    if (nZoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL || pZone->type != 0x10)
        return 0;
    if (IndyCheckZoneItemsAvailable(nZoneId) == 0)
        return 0;
    short puz  = (short)goalTileList.GetAt(nStepSlot);
    short nextPuz;
    if (nStepSlot < goalTileList.GetSize() - 1)
        nextPuz = (short)goalTileList.GetAt(nStepSlot + 1);
    else
        nextPuz = -1;
    Puzzle *pPuz = (Puzzle *)puzzles.GetAt(puz);
    if (pPuz == NULL)
        return 0;
    if (nextPuz >= 0 && puzzles.GetAt(nextPuz) == NULL)
        return 0;
    short iProvide = pPuz->itemA;
    short iRequire = -1;
    if (nextPuz >= 0)
        iRequire = ((Puzzle *)puzzles.GetAt(nextPuz))->itemA;
    int spawnItem = IndyPickUnplacedSpawnItem(nZoneId);
    if (spawnItem >= 0)
    {
        int reqOk = 1;
        int provOk = IndyZoneProvidesItem(iProvide, nZoneId);
        if (iRequire >= 0)
            reqOk = IndyZoneRequiresItem(iRequire, nZoneId);
        if (provOk != 0 && reqOk != 0 &&
            IndyFillSpawn((short)spawnItem, nZoneId) >= 0)
        {
            genCellQuestSlot5Scratch = spawnItem;         // doc+0x21
            genCellItemBScratch = iRequire;               // doc+0x1d
            genCellItemAScratch = iProvide;               // doc+0x1f
            genCellQuestSlot0Scratch = nStepSlot;         // doc+0x23
            WorldgenAddZoneEntry((short)spawnItem, nQueueTag);
            IndyCollectZoneItems(nZoneId);
            return 1;
        }
    }
    return 0;
}

// Indy IndyPopulateSimpleZone — DESKADV 1010:67ec  (node types 2..7)
// Pick a random UNPLACED item from the zone's genCandidateB (IZX3 spawn pool, doc+0x7d8/
// providedItems? see NOTE), then queue it for placement. If the zone requires exactly one
// item (genCandidateA size==1) that isn't already recorded, dedup-append it to
// uniqueRequiredItemsMaybe. Queue tag becomes 5 for zone type 6, else the passed tag.
// Returns 1 on success.
// NOTE: DESKADV reads the pool at zone+0x7d8 count with items at +0x7d4 -> that is
//   providedItemsA in our layout. (The +0x7e6/+0x7e2 read for the single-required check is
//   genCandidateA.) I map accordingly.
int CDeskcppDoc::IndyPopulateSimpleZone(short nQueueTag, int nZoneId)
{
    if (nZoneId < 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL)
        return 0;
    int nPool = pZone->providedItemsA.GetSize();          // doc+0x7d8
    if (nPool == 0 || IndyCheckZoneItemsAvailable(nZoneId) == 0)
        return 0;
    CWordArray cands;
    for (int i = 0; i < nPool; i++)
    {
        short item = (short)pZone->providedItemsA.GetAt(i);
        if (IsItemPlaced(item) == 0)                      // Indy IndyIsItemPlaced == Yoda IsItemPlaced
            cands.SetAtGrow(cands.GetSize(), item);
    }
    if (cands.GetSize() == 0)
        return 0;
    short chosen = (short)cands.GetAt(rand() % cands.GetSize());
    // Single-required-item dedup (genCandidateA size == 1):
    if (pZone->genCandidateA.GetSize() == 1)
    {
        int reqItem = (short)pZone->genCandidateA.GetAt(0);
        int used = 0;
        int nUsed = uniqueRequiredItemsMaybe.GetSize();
        for (int i = 0; i < nUsed; i++)
            if ((short)uniqueRequiredItemsMaybe.GetAt(i) == reqItem) { used = 1; break; }
        if (used != 0)
            return 0;                                     // required item already consumed
        uniqueRequiredItemsMaybe.SetAtGrow(uniqueRequiredItemsMaybe.GetSize(), (short)reqItem);
    }
    short tag = nQueueTag;
    if (pZone->type == 6)                                 // island/companion => tag 5
        tag = 5;
    IndyQueueItemForPlacement(tag, chosen);               // == WorldgenPushZoneEntry(chosen, tag)
    WorldgenAddZoneEntry(chosen, nQueueTag);              // IndyAddPlacedItemEntry
    genCellItemAScratch = chosen;                         // doc+0x1f
    IndyCollectZoneItems(nZoneId);
    return 1;
}

// Indy IndyPopulateUsefulObjectZone — DESKADV 1010:6580  (node types 0x11 / 0x12)
// If the zone REQUIRES the puzzle item `a4` (genCandidateA), place a "useful object" of a
// category derived from the queued item's flags (worldgenPendingZones entry a4): a random
// object whose type matches the category is activated (state=1, arg=a4). Recurse DOOR_IN.
// Returns 1 on success. Category select (from the queued item's flag bits, DESKADV +0x404/
// +0x406): flag 0x40 -> category 2, flag 0x10 -> category 5, flag 0x80 -> category 0.
// NOTE: a4 is an INDEX into worldgenPendingZones here (the queued item), not an item id.
int CDeskcppDoc::IndyPopulateUsefulObjectZone(short nQueueTag, short a4, int nZoneId)
{
    if (nZoneId < 0 || IndyCheckZoneItemsAvailable(nZoneId) == 0)
        return 0;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL)
        return 0;
    if (pZone->providedItemsA.GetSize() > 0)   // doc+0x7d8 must be empty
        return 0;
    if (pZone->genCandidateB.GetSize() > 0)    // doc+0x7f4 must be empty
        return 0;
    // Require the puzzle item a4 in genCandidateA (doc+0x7e6/0x7e2):
    int requires = 0;
    int nReq = pZone->genCandidateA.GetSize();
    for (int i = 0; i < nReq; i++)
        if ((short)pZone->genCandidateA.GetAt(i) == a4) { requires = 1; break; }
    int result = 0;
    if (requires == 0)
    {
        // recurse DOOR_IN
        int nObj = pZone->objects.GetSize();
        for (int i = 0; i < nObj; i++)
        {
            ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
            if (pObj != NULL && pObj->type == 9 && pObj->arg >= 0)
            {
                result = IndyPopulateUsefulObjectZone(nQueueTag, a4, pObj->arg);
                if (result == 1)
                    break;
            }
        }
        if (result == 1)
            IndyCollectZoneItems(nZoneId);
        return result;
    }
    // a4 indexes worldgenPendingZones -> the queued item object; derive its category from
    // its flag bits.  RESOLVED: DESKADV reads doc+0x60 [= our `tiles` CObArray] indexed by a4
    // (a4 IS a tile/item id), then the item TILE's flags at +0x404/+0x406 — which is exactly our
    // Tile.flags (unsigned int @ +0x404; src/GameObjectClasses.h line 185).  Byte +0x404 = flags
    // bits 0..7, word +0x406 = flags bits 16..31.  So:
    //   (flags & 0x40)       -> category 2   (DESKADV *(byte*)(t+0x404)&0x40)
    //   (flags>>16) & 0x10   -> category 5   (DESKADV *(uint*)(t+0x406)&0x10)
    //   (flags & 0x80)       -> category 0   (DESKADV *(byte*)(t+0x404)&0x80)  [default 0]
    unsigned int category = 0;
    Tile *pTile = (Tile *)tiles.GetAt(a4);
    unsigned int tflags = pTile->flags;
    if (tflags & 0x40)              category = 2;
    else if ((tflags >> 16) & 0x10) category = 5;
    else if (tflags & 0x80)         category = 0;   // else stays 0
    CWordArray hosts;
    int nObj = pZone->objects.GetSize();
    for (i = 0; i < nObj; i++)
    {
        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
        if (pObj->type == category)
        {
            hosts.SetAtGrow(hosts.GetSize(), (short)i);
            result = 1;
        }
    }
    if (result == 1)
    {
        int idx = (short)hosts.GetAt(rand() % hosts.GetSize());
        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(idx);
        pObj->arg = a4;
        pObj->state = 1;
        WorldgenAddZoneEntry(a4, nQueueTag);              // IndyAddPlacedItemEntry
        genCellItemBScratch = a4;                         // doc+0x1d
    }
    if (result == 1)
        IndyCollectZoneItems(nZoneId);
    return result;
}

// ===========================================================================
// Puzzle selection
// ===========================================================================

// Indy IndySelectPuzzle — DESKADV 1010:7b58
// Build a shuffled list of unused puzzles matching the mode, return the first that fits:
//   nMode 10   -> puzzle.nType == 2 (GOAL_PRIZE),   reqItemA must match puzzle.itemA
//   nMode 0xf  -> puzzle.nType == 1 (TRADE),        reqItemA must match puzzle.itemA
//   nMode 0x10 -> puzzle.nType == 0 (TRANSACTION),  reqItemA must match puzzle.itemA
//   nMode 9999 -> puzzle.nType == 3 (WORLD_MISSION); accepted if in story-history (or if
//                 requestedGoal>=0 skip the history filter); the passed key must match a
//                 puzzle field (+0x18 == nWorldMissionKey).  [GAME_INDY: any WORLD_MISSION.]
// bFirst is Indy param_3 (==0 when reqItemA is the "provided" branch). Returns puzzle id or -1.
short CDeskcppDoc::IndySelectPuzzle(int bFirst, short nMode, unsigned short reqItemA,
                                    unsigned short nWorldMissionKey)
{
    CWordArray list;
    int nPuz = puzzles.GetSize();
    for (int i = 0; i < nPuz; i++)
    {
        Puzzle *pPuz = (Puzzle *)puzzles.GetAt(i);
        bool match = false;
        if (nMode == 10)
            match = (pPuz->nType == 2);
        else if (nMode == 0xf)
            match = (pPuz->nType == 1);
        else if (nMode == 0x10)
            match = (pPuz->nType == 0);             // +6==0 sub-test dropped
        else if (nMode == 9999)
        {
            if (pPuz->nType == 3)                   // WORLD_MISSION
            {
#ifdef GAME_INDY
                match = true;                       // Indy: no per-planet whitelist / story screen
#else
                // (Yoda path: story-history + requestedGoal gating; not used for Indy)
                match = false;
#endif
            }
        }
        if (match && IndyIsPuzzleUsed((short)i) == 0)
            list.SetAtGrow(list.GetSize(), (short)i);
    }
    if (list.GetSize() > 0)
    {
        WorldgenShuffleList(&list);                 // == IndyShuffleList
        int n = list.GetSize();
        for (int k = 0; k < n; k++)
        {
            short pid = (short)list.GetAt(k);
            Puzzle *pPuz = (Puzzle *)puzzles.GetAt(pid);
            if (IndyIsPuzzleUsed(pid) != 0)
                continue;
            unsigned short need;
            if (nMode == 10)      { if (pPuz->nType != 2) continue; need = (unsigned short)pPuz->itemA; }
            else if (nMode == 0xf){ if (pPuz->nType != 1) continue; need = (unsigned short)pPuz->itemA; }
            else if (nMode == 0x10) { need = (unsigned short)pPuz->itemA; /* nType|... == 0 */ }
            else if (nMode == 9999)
            {
                if (pPuz->nType == 3)
                    return pid;                     // first shuffled WORLD_MISSION
                continue;
            }
            else continue;
            // For 10/0xf/0x10: DESKADV matches the puzzle's itemA against param_5 (the PICKED
            // item = nWorldMissionKey), NOT reqItemA (param_4, which is unused in the match).
            if (need == nWorldMissionKey)
                return pid;
        }
    }
    return -1;
}

// ===========================================================================
// Quest-node placement — the core "find a zone of node type T and populate it"
// ===========================================================================

// Indy IndyPlaceQuestNode — DESKADV 1010:7f0c
// Collect all zones whose Zone.type matches nNodeType (0x11 also accepts type 0x12 with the
// obj+6==0 sub-test dropped), shuffle, and try each: skip if used (except goal type 10 with
// requestedGoal set). Then per node type, gate + populate the zone. On success returns the
// zone id and records the puzzle into goalTileList[nOrder]. Returns -1 if none fit.
// Params (mapped from DESKADV): nOrder = quest step slot (param_5), a4reqItem = required item
// (param_4), a5reqItem2 unused for single-item Indy, nNodeType = param_6.
int CDeskcppDoc::IndyPlaceQuestNode(short nOrder, short a4reqItem, short a5reqItem2, short nNodeType)
{
    CWordArray list;
    int nZones = zones.GetSize();
    for (int i = 0; i < nZones; i++)
    {
        Zone *pZone = (Zone *)zones.GetAt(i);
        bool match;
        switch (nNodeType)
        {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7:
        case 10: case 0xb: case 0xf: case 0x10:
            match = (pZone->type == nNodeType);
            break;
        case 0x11:
            match = (pZone->type == 0x11) || (pZone->type == 0x12);
            break;
        default:
            match = false;
        }
        if (match)
            list.SetAtGrow(list.GetSize(), (short)i);
    }
    if (list.GetSize() == 0)
        return -1;

    WorldgenShuffleList(&list);
    int nCand = list.GetSize();
    for (int k = 0; k < nCand; k++)
    {
        int nZoneId = (short)list.GetAt(k);
        Zone *pZone = (Zone *)zones.GetAt(nZoneId);
        // used-zone gate (goal type 10 with a requested goal is allowed to reuse):
        if (IsZoneUsed((short)nZoneId) != 0 && (nNodeType != 10 || nRequestedGoalItem < 1))
            continue;

        switch (nNodeType)
        {
        case 1:
            // Generic/start zone. If NOT on the edge (indyPlaceOnEdge==0) require Zone.type==1 and
            // the zone's claimed-flag clear; else it is a teleporter placement.
            // RESOLVED: the DESKADV `*(int*)(zone+6)==0` guard is our Zone.activatedFlag (the
            // per-zone "claimed" flag; 16-bit zone+6 == our zone+8; IndyGenerate clears it for all
            // zones before assembly). NOTE the edge arm: DESKADV returns the zone only when it has
            // NO objects, and (with objects) searches for a TELEPORTER (obj type 0xd) then breaks
            // without an explicit return in the decomp — but IndyPlacePuzzlesPass sets
            // indyPlaceOnEdge=1 specifically to place teleporter zones, so "teleporter found ->
            // return" is the intended behaviour. VERIFY the object-less short-circuit.
            if (indyPlaceOnEdge == 0)
            {
                if (pZone->type == 1 && pZone->activatedFlag == 0)
                    return nZoneId;
            }
            else
            {
                int nObj = pZone->objects.GetSize();
                if (nObj < 1)
                    return nZoneId;             // DESKADV: object-less edge zone -> accept
                bool hasTele = false;
                for (int i = 0; i < nObj; i++)
                    if (((ZoneObj *)pZone->objects.GetAt(i))->type == 0xd) { hasTele = true; break; }
                if (hasTele)
                    return nZoneId;             // teleporter zone -> accept (intended; see NOTE)
            }
            break;
        case 2: case 3: case 4: case 5: case 6: case 7:
            // DESKADV: zone.type==N && zone.activatedFlag==0 (the +6 claimed flag) then populate.
            if (pZone->type == nNodeType && pZone->activatedFlag == 0)
            {
                if (IndyPopulateSimpleZone(nOrder, nZoneId) == 1)
                    return nZoneId;
            }
            break;
        case 10:                                // FINAL_ITEM goal
            if (pZone->type == 10 &&
                IndyZoneRequiresItem(a4reqItem, (short)nZoneId) == 1)
            {
                int item = IndyPickUnplacedProvidedItem(a5reqItem2, nZoneId);
                if (item >= 0)
                {
                    short puz = IndySelectPuzzle(a5reqItem2 == 0, 10, (unsigned short)a4reqItem,
                                                 (unsigned short)item);
                    if (puz >= 0)
                    {
                        goalTileList.SetAt(a5reqItem2, puz);   // doc+0x136[orderSlot] = puzzle id
                        if (IndyPopulateGoalZone(nOrder, a5reqItem2, nZoneId) == 1)
                        {
                            // record puzzle used (storyHistoryNevada), placed item:
                            storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), puz);
                            WorldgenAddZoneEntry((short)item, 0);
                            return nZoneId;
                        }
                        goalTileList.SetAt(a5reqItem2, (short)-1);
                    }
                }
            }
            break;
        case 0xb:                               // start-terminal zone
            // RESOLVED: DESKADV checks zone.type==0xb then `iVar1 = *(int*)(zone+6); if(iVar1==0)
            // return zone`.  zone+6 is the ZONE's claimed-flag (== our Zone.activatedFlag), NOT an
            // object field.  So: accept an unclaimed type-0xb zone (no populate — the start zone is
            // populated implicitly by the engine).
            if (pZone->type == 0xb && pZone->activatedFlag == 0)
                return nZoneId;
            break;
        case 0xf:                               // TRADE
            if (pZone->type == 0xf &&
                IndyZoneRequiresItem(a4reqItem, (short)nZoneId) == 1)
            {
                int item = IndyPickUnplacedProvidedItem(a5reqItem2, nZoneId);
                if (item >= 0)
                {
                    short puz = IndySelectPuzzle(a5reqItem2 == 0, 0xf, (unsigned short)a4reqItem,
                                                 (unsigned short)item);
                    if (puz >= 0)
                    {
                        goalTileList.SetAt(a5reqItem2, puz);
                        if (IndyPopulateTradeZone(nOrder, a5reqItem2, nZoneId) == 1)
                        {
                            storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), puz);
                            WorldgenAddZoneEntry((short)item, 0);
                            return nZoneId;
                        }
                    }
                }
            }
            break;
        case 0x10:                              // TRANSACTION
            if (pZone->type == 0x10 &&
                IndyZoneRequiresItem(a4reqItem, (short)nZoneId) == 1)
            {
                int item = IndyPickUnplacedProvidedItem(a5reqItem2, nZoneId);
                if (item >= 0)
                {
                    short puz = IndySelectPuzzle(a5reqItem2 == 0, 0x10, (unsigned short)a4reqItem,
                                                 (unsigned short)item);
                    if (puz >= 0)
                    {
                        goalTileList.SetAt(a5reqItem2, puz);
                        if (IndyPopulateTransactionZone(nOrder, a5reqItem2, nZoneId) == 1)
                        {
                            storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), puz);
                            WorldgenAddZoneEntry((short)item, 0);
                            return nZoneId;
                        }
                    }
                }
            }
            break;
        case 0x11:                              // useful-object (0x11/0x12)
            if ((pZone->type == 0x11 || pZone->type == 0x12) &&
                IndyPopulateUsefulObjectZone(0, a4reqItem, nZoneId) == 1)
                return nZoneId;
            break;
        }
    }
    return -1;
}

// ===========================================================================
// Plan-grid geometry helpers
// ===========================================================================

// Indy IndyGetIslandOrientation — DESKADV 1010:3dc4
// Given a PLAN_LOCK (island seed, 0x66) cell at (row,col), return which side has an island
// body (PLAN_WALL, 0x68) adjacent: 1=west, 3=east, 2=north, 4=south, 0=none.
int CDeskcppDoc::IndyGetIslandOrientation(short *paPlan, int nRow, int nCol)
{
    int base = nRow * 10 + nCol;
    if (nCol > 0 && paPlan[base - 1] == PLAN_WALL)  return 1;   // 0x68 to west
    if (nCol < 9 && paPlan[base + 1] == PLAN_WALL)  return 3;   // east
    if (nRow > 0 && paPlan[base - 10] == PLAN_WALL) return 2;   // north
    if (nRow < 9 && paPlan[base + 10] == PLAN_WALL) return 4;   // south
    return 0;
}

// ===========================================================================
// IndyCarveQuestPath — DESKADV 1010:6c5c
// ===========================================================================
// Random-walk one difficulty ring of the quest corridor onto the plan grid, exactly like
// Yoda's WorldgenCarveQuestPath but with the Indy token set. Per attempt: pick a random cell
// on the tier's border band, and if empty and touching reachable path/corridor on one side,
// either extend a corridor fork (budgeted by rand()%forkDen<forkNum and the neighbor's grid
// order < tier), continue a corridor, or stamp a plain path room; fresh rooms may be promoted
// to goal rooms (PLAN_GOAL). Runs until room budget spent or 144 attempts (0x90).
// nTier -> band table (param_10): 2/3/4. paPlan is the 10x10 plan grid (short[100]).
// pnPlaced/pnSplits/pnGoals are running counters (in/out); maxSplits/maxGoals are caps.
//
// RESOLVED (re-decompiled 1010:6c5c): each attempt finds the FIRST occupied neighbor in the
// order W,E,N,S (DESKADV nests: west-clear? east-clear? north-clear? south-clear?), then
// extends a corridor/fork AWAY from it. The four directional writes are transcribed verbatim
// (6 cells each). Counters (DESKADV): fork -> nSplits++, nPlaced+=2, budget--; corridor/path
// -> nPlaced++, budget--. Fork gate: nSplits<maxSplits && rand()%forkDen<forkNum && the
// OCCUPIED neighbor's grid order < tier && the perpendicular pair is clear (0 or PLAN_BLOCKED).
// Fork-budget-not-taken -> plain path; occOrder>=tier -> abort (no placement); perpendicular
// blocked -> plain path. Plain path only if all four neighbors < 300. Goal-promote a fresh
// PATH cell (rand()%8<goalNum, tier>2, captured token != PLAN_GOAL).
// The 6-cell fork stamps (verbatim): FORK_E (west occ)=cell FORK_E, N/S=WALL, E=CORRIDOR,
// NE(-9)/SE(+11)=WALL; FORK_W (east occ)=cell FORK_W, N/S=WALL, W=CORRIDOR, NW(-11)/SW(+9)=WALL;
// FORK_S (north occ)=cell FORK_S, W/E=WALL, S=CORRIDOR, SW(+9)/SE(+11)=WALL; FORK_N (south
// occ)=cell FORK_N, W/E=WALL, N=CORRIDOR, NW(-11)/NE(-9)=WALL.
void CDeskcppDoc::IndyCarveQuestPath(int *pnPlaced, int *pnSplits, int maxSplits,
                                     int *pnGoals, int maxGoals, short *paPlan,
                                     int nBudget, int nTier)
{
    int xBase, xOpp, yBase, yOpp, span, forkDen, forkNum, goalNum;
    switch (nTier)
    {
    case 2: xBase=3; xOpp=6; yBase=3; yOpp=6; span=4; forkDen=9; forkNum=2; goalNum=1; break;
    case 3: xBase=2; xOpp=7; yBase=2; yOpp=7; span=6; forkDen=4; forkNum=2; goalNum=3; break;
    case 4: xBase=1; xOpp=8; yBase=1; yOpp=8; span=8; forkDen=5; forkNum=1; goalNum=6; break;
    }
    int nGoals  = *pnGoals;
    int nPlaced = *pnPlaced;
    int nSplits = *pnSplits;
    int nBudgetLeft = nBudget;
    int nAttempts = 0;
    int bDone = 0;
    do
    {
        int r, c;   // r=row (stride 10), c=col — DESKADV iVar2=r, iVar3=c
        if (rand() & 1)
        {
            r = (rand() & 1) ? xBase : xOpp;
            c = rand() % span + yBase;
        }
        else
        {
            c = (rand() & 1) ? yBase : yOpp;
            r = rand() % span + xBase;
        }
        nAttempts++;
        short *pCell = &paPlan[r * 10 + c];
        short v = 0;                        // captured occupied-neighbor token (DESKADV local_24)
        if (*pCell == 0)
        {
            // find first OCCUPIED neighbor in W,E,N,S order (occupied = not 0 and not PLAN_BLOCKED)
            int dir = 0;                    // 1=W,2=E,3=N,4=S occupied
            if      (pCell[-1]  != 0 && pCell[-1]  != PLAN_BLOCKED) { dir = 1; v = pCell[-1];  }
            else if (pCell[1]   != 0 && pCell[1]   != PLAN_BLOCKED) { dir = 2; v = pCell[1];   }
            else if (pCell[-10] != 0 && pCell[-10] != PLAN_BLOCKED) { dir = 3; v = pCell[-10]; }
            else if (pCell[10]  != 0 && pCell[10]  != PLAN_BLOCKED) { dir = 4; v = pCell[10];  }

            if (dir != 0)
            {
                bool horiz = (dir == 1 || dir == 2);   // W/E occupied -> extend along columns
                // the "aligned" directional fork token that means "a corridor already points at us":
                short aligned = (dir == 1) ? (short)PLAN_FORK_E :
                                (dir == 2) ? (short)PLAN_FORK_W :
                                (dir == 3) ? (short)PLAN_FORK_S : (short)PLAN_FORK_N;

                if (v == PLAN_CORRIDOR || v == aligned)
                {
                    // continue a corridor through this cell, blocking the perpendicular pair
                    if (horiz)
                    {
                        if ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                            (pCell[10]  == 0 || pCell[10]  == PLAN_BLOCKED))
                        { *pCell = PLAN_CORRIDOR; pCell[-10] = PLAN_BLOCKED; pCell[10] = PLAN_BLOCKED;
                          nPlaced++; nBudgetLeft--; }
                    }
                    else
                    {
                        if ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                            (pCell[1]  == 0 || pCell[1]  == PLAN_BLOCKED))
                        { *pCell = PLAN_CORRIDOR; pCell[-1] = PLAN_BLOCKED; pCell[1] = PLAN_BLOCKED;
                          nPlaced++; nBudgetLeft--; }
                    }
                }
                else if (v == PLAN_PATH || v == PLAN_GOAL || v == PLAN_START || v == PLAN_ORDERED)
                {
                    bool forked = false, abort = false, tryPath = false;
                    if (nSplits < maxSplits && rand() % forkDen < forkNum)
                    {
                        int occOrder = (dir == 1) ? GetZoneGridOrder(c - 1, r) :
                                       (dir == 2) ? GetZoneGridOrder(c + 1, r) :
                                       (dir == 3) ? GetZoneGridOrder(c, r - 1) :
                                                    GetZoneGridOrder(c, r + 1);
                        if (occOrder >= nTier)
                            abort = true;                          // DESKADV goto 73fe
                        else
                        {
                            bool perpClear = horiz
                                ? ((pCell[-10] == 0 || pCell[-10] == PLAN_BLOCKED) &&
                                   (pCell[10]  == 0 || pCell[10]  == PLAN_BLOCKED))
                                : ((pCell[-1] == 0 || pCell[-1] == PLAN_BLOCKED) &&
                                   (pCell[1]  == 0 || pCell[1]  == PLAN_BLOCKED));
                            if (!perpClear)
                                tryPath = true;                    // DESKADV goto LAB_6e85
                            else
                            {
                                // stamp the directional fork (verbatim 6-cell writes):
                                if (dir == 1)        // west occ -> FORK_E, corridor east
                                { *pCell = PLAN_FORK_E; pCell[-10] = PLAN_BLOCKED; pCell[10] = PLAN_BLOCKED;
                                  pCell[1] = PLAN_CORRIDOR; pCell[-9] = PLAN_BLOCKED; pCell[11] = PLAN_BLOCKED; }
                                else if (dir == 2)   // east occ -> FORK_W, corridor west
                                { *pCell = PLAN_FORK_W; pCell[-10] = PLAN_BLOCKED; pCell[10] = PLAN_BLOCKED;
                                  pCell[-1] = PLAN_CORRIDOR; pCell[-11] = PLAN_BLOCKED; pCell[9] = PLAN_BLOCKED; }
                                else if (dir == 3)   // north occ -> FORK_S, corridor south
                                { *pCell = PLAN_FORK_S; pCell[-1] = PLAN_BLOCKED; pCell[1] = PLAN_BLOCKED;
                                  pCell[10] = PLAN_CORRIDOR; pCell[9] = PLAN_BLOCKED; pCell[11] = PLAN_BLOCKED; }
                                else                 // south occ -> FORK_N, corridor north
                                { *pCell = PLAN_FORK_N; pCell[-1] = PLAN_BLOCKED; pCell[1] = PLAN_BLOCKED;
                                  pCell[-10] = PLAN_CORRIDOR; pCell[-11] = PLAN_BLOCKED; pCell[-9] = PLAN_BLOCKED; }
                                nSplits++; nPlaced += 2; nBudgetLeft--;
                                forked = true;
                            }
                        }
                    }
                    else
                        tryPath = true;                            // fork budget not taken -> path
                    if (tryPath && !abort && !forked)
                    {
                        // plain path only if no neighbor is a wall/corridor/fork/ordered (>= 300)
                        if (pCell[-1] < 300 && pCell[1] < 300 && pCell[-10] < 300 && pCell[10] < 300)
                        { *pCell = PLAN_PATH; nPlaced++; nBudgetLeft--; }
                    }
                }
                // (a non-extendable occupied neighbor -> DESKADV goto 73fe: no placement)

                // goal promotion of a freshly stamped plain-path cell:
                if (*pCell == PLAN_PATH && nGoals < maxGoals &&
                    rand() % 8 < goalNum && v != PLAN_GOAL && nTier > 2)
                {
                    *pCell = PLAN_GOAL;
                    nGoals++;
                }
            }
        }
        if (nBudgetLeft < 1) bDone++;
        if (nAttempts > 0x90) bDone++;
    } while (bDone == 0);

    *pnGoals  = nGoals;
    *pnPlaced = nPlaced;
    *pnSplits = nSplits;
}

// ===========================================================================
// IndyPlaceIslandStrips — DESKADV 1010:7490
// ===========================================================================
// Place `nIslands` decorative island strips against the plan-grid edges. For each island:
// spin a rand()%4 to choose an edge (0=top col-scan / 1=left row-scan / 2=bottom / 3=right),
// find the LONGEST run of empty-or-blocked cells adjacent to the border, and if it is >=3
// (>=4 for the bottom/right edges), stamp a strip: one PLAN_LOCK (0x66, island seed) at one
// end and PLAN_WALL (0x68, island body) along the rest. Retries up to 200 attempts per island.
//
// NOTE: transcribed faithfully from the 4 near-identical edge-scan arms; the run-finding and
//   the seed-vs-body end selection (coin flip iVar%2) are reproduced. VERIFY offsets.
void CDeskcppDoc::IndyPlaceIslandStrips(short *paPlan, int nIslands)
{
    if (nIslands <= 0)
        return;
    for (int island = nIslands; island != 0; island--)
    {
        int done = 0;
        int attempts = 0;
        do
        {
            bool placed = false;
            int edge = rand() % 4;
            int runStart = 0, runLen = 0, len = 0;
            // Scan the chosen edge for the longest empty run adjacent to the border.
            if (edge == 0)          // top edge, column scan (stride 10, border row 0)
            {
                int cur = 0, best = 0, bestStart = 0;
                for (int i = 0; i < 10; i++)
                {
                    // cell paPlan[i] (row0) empty AND paPlan[i+10] (row1) empty/blocked
                    if (paPlan[i] == 0 && (paPlan[i + 10] == 0 || paPlan[i + 10] == PLAN_BLOCKED))
                        cur++;
                    else { if (best < cur) { best = cur; bestStart = i - cur; } cur = 0; }
                }
                if (best < cur) { best = cur; bestStart = 10 - cur; }
                runLen = best; runStart = bestStart;
                if (runLen > 2)
                {
                    if (runLen == 3 && (runStart == 0 || runStart == 7)) len = 2;
                    if (!(runLen == 3 && runStart >= 1 && runStart <= 6))
                    {
                        if (runLen > 3) len = runLen - 2;
                        if (len > 4) len = 4;
                        if (runStart > 0 && len + runStart < 10) runStart++;
                        // seed at one end, body along the strip (coin flip picks which end):
                        if (rand() % 2 == 0)
                        {
                            paPlan[(len + runStart) * 10 - 10] = PLAN_LOCK;   // seed 0x66
                            for (int j = runStart; j < len + runStart - 1; j++)
                                paPlan[j * 10] = PLAN_WALL;                    // body 0x68
                        }
                        else
                        {
                            paPlan[runStart * 10] = PLAN_LOCK;
                            for (int j = runStart + 1; j < len + runStart; j++)
                                paPlan[j * 10] = PLAN_WALL;
                        }
                        placed = true;
                    }
                }
            }
            else if (edge == 1)     // left edge, row scan (border col 0)
            {
                int cur = 0, best = 0, bestStart = 0;
                for (int i = 0; i < 10; i++)
                {
                    if (paPlan[i * 10] == 0 && (paPlan[i * 10 + 1] == 0 || paPlan[i * 10 + 1] == PLAN_BLOCKED))
                        cur++;
                    else { if (best < cur) { best = cur; bestStart = i - cur; } cur = 0; }
                }
                if (best < cur) { best = cur; bestStart = 10 - cur; }
                runLen = best; runStart = bestStart;
                if (runLen > 2 && !(runLen == 3 && runStart >= 1 && runStart <= 6))
                {
                    if (runLen == 3 && (runStart == 0 || runStart == 7)) len = 2;
                    if (runLen > 3) len = runLen - 2;
                    if (len > 4) len = 4;
                    if (runStart > 0 && runStart + len < 10) runStart++;
                    if (rand() % 2 == 0)
                    {
                        paPlan[runStart + len - 1] = PLAN_LOCK;
                        for (int j = runStart; j < runStart + len - 1; j++) paPlan[j] = PLAN_WALL;
                    }
                    else
                    {
                        paPlan[runStart] = PLAN_LOCK;
                        for (int j = runStart + 1; j < runStart + len; j++) paPlan[j] = PLAN_WALL;
                    }
                    placed = true;
                }
            }
            else if (edge == 2)     // bottom edge (border row 9); requires run > 3
            {
                int cur = 0, best = 0, bestStart = 0;
                for (int i = 0; i < 10; i++)
                {
                    // paPlan[0xa0/2 + i] == paPlan[80+i] row8; border row9 = +10
                    if (paPlan[80 + i] == 0 && (paPlan[90 + i] == 0 || paPlan[90 + i] == PLAN_BLOCKED))
                        cur++;
                    else { if (best < cur) { best = cur; bestStart = i - cur; } cur = 0; }
                }
                if (best < cur) { best = cur; bestStart = 10 - cur; }
                runLen = best; runStart = bestStart;
                if (runLen > 3 && !(runLen == 3 && runStart >= 1 && runStart <= 6))
                {
                    if (runLen == 3 && (runStart == 0 || runStart == 7)) len = 2;
                    if (runLen > 3) len = runLen - 2;
                    if (len > 4) len = 4;
                    if (runStart > 0 && runStart + len < 10) runStart++;
                    if (rand() % 2 == 0)
                    {
                        paPlan[90 + runStart + len - 1] = PLAN_LOCK;    // DESKADV +0xb2 base
                        for (int j = runStart; j < runStart + len - 1; j++) paPlan[90 + j + 1] = PLAN_WALL;
                    }
                    else
                    {
                        paPlan[90 + runStart + 1] = PLAN_LOCK;
                        for (int j = runStart + 1; j < runStart + len; j++) paPlan[90 + j + 1] = PLAN_WALL;
                    }
                    placed = true;
                }
            }
            else                    // edge == 3, right edge (border col 9); requires run > 3
            {
                int cur = 0, best = 0, bestStart = 0;
                for (int i = 0; i < 10; i++)
                {
                    if (paPlan[i * 10 + 8] == 0 && (paPlan[i * 10 + 9] == 0 || paPlan[i * 10 + 9] == PLAN_BLOCKED))
                        cur++;
                    else { if (best < cur) { best = cur; bestStart = i - cur; } cur = 0; }
                }
                if (best < cur) { best = cur; bestStart = 10 - cur; }
                runLen = best; runStart = bestStart;
                if (runLen > 3 && !(runLen == 3 && runStart >= 1 && runStart <= 6))
                {
                    if (runLen == 3 && (runStart == 0 || runStart == 7)) len = 2;
                    if (runLen > 3) len = runLen - 2;
                    if (len > 4) len = 4;
                    if (runStart > 0 && runStart + len < 10) runStart++;
                    if (rand() % 2 == 0)
                    {
                        paPlan[(len + runStart) * 10 - 20 + 9] = PLAN_LOCK;  // DESKADV +0x12 offsets
                        for (int j = runStart; j < len + runStart - 1; j++) paPlan[j * 10 + 9] = PLAN_WALL;
                    }
                    else
                    {
                        paPlan[runStart * 10 + 9] = PLAN_LOCK;
                        for (int j = runStart + 1; j < len + runStart; j++) paPlan[j * 10 + 9] = PLAN_WALL;
                    }
                    placed = true;
                }
            }
            if (placed || ++attempts > 200)
                done++;
        } while (done == 0);
    }
}

// ===========================================================================
// IndyAssignQuestStepCells — DESKADV 1020:1426
// ===========================================================================
// Walk the finished plan grid and assign sequential ORDER ids (into paOrder[]) to the cells
// that form the quest chain, returning the number of quest steps assigned (== max order + 1).
// Phases:
//   1. Count island-body/path (0x68/1/300) cells, goal (0x65) cells, and fork (301..304)
//      cells; derive a target step count `nTarget = rand()%(bodies/5+1) + bodies/4 - forks
//      - goals - 2`, floored at 4.
//   2. Convert each fork head (301..304) whose corridor tail is PLAN_CORRIDOR into an ORDERED
//      cell (0x132) and assign it the next order id, choosing the tail cell by the fork dir.
//   3. For each island-seed (0x66) cell, assign an order id to the far end of its island body
//      run (direction from IndyGetIslandOrientation).
//   4. Random-fill remaining PLAN_PATH/CORRIDOR cells with order ids until nTarget reached
//      (bounded ~200 attempts), preferring interior cells (grid order >= 3) once desperate.
//   5. If the LAST-order (goal) cell landed on a CENTRAL cell (grid order < 3, i.e. near the
//      start), move the goal OUTWARD: swap its order id with an outer (grid order > 2) ordered
//      cell so the goal sits far from the start.
// paOrder and paPlan are short[100]. Returns the step count (Indy doc+0x13a).
//
// RESOLVED (re-decompiled 1020:1426): phase-2 fork-tail offsets corrected (mark the corridor
//   TAIL cell idx±1/±2/±10/±20, not the head); phase-5 direction clarified (goal pushed outward).
int CDeskcppDoc::IndyAssignQuestStepCells(short *paOrder, short *paPlan)
{
    int nBodies = 0, nGoals = 0, nForks = 0;
    for (int i = 0; i < 100; i++)
    {
        int t = paPlan[i];
        if (t == PLAN_WALL /*0x68 body*/ || t == PLAN_PATH || t == PLAN_CORRIDOR)
            nBodies++;
        else if (t == PLAN_GOAL)
            nGoals++;
        else if (t >= PLAN_FORK_W && t <= PLAN_FORK_S)   // 301..304
            nForks++;
    }
    int nTarget = rand() % (nBodies / 5 + 1) + (nBodies >> 2) - nForks - nGoals - 2;
    if (nTarget < 4)
        nTarget = 4;

    int nOrder = 0;   // next order id (DESKADV local_16)

    // Phase 2: fork heads -> ORDERED, tail assigned
    for (int row = 0, base = 0; row < 10; row++, base += 10)
    {
        for (int col = 0; col < 10; col++)
        {
            int idx = base + col;
            int head = paPlan[idx];
            // RESOLVED (re-decompiled): DESKADV walks piVar11=&paPlan[idx-1], piVar12=&paOrder
            // [idx-2]; it marks the CORRIDOR TAIL cell (one or two steps out along the fork's
            // direction), NOT the head. It prefers the FARTHER tail (idx±2/±20) only when in
            // bounds AND that farther cell is also a corridor; else the nearer tail (idx±1/±10).
            if (head == PLAN_FORK_W)              // 0x12d: corridor extends WEST
            {
                if (paPlan[idx - 1] == PLAN_CORRIDOR)          // *piVar11
                {
                    if (col < 2 || paPlan[idx - 2] != PLAN_CORRIDOR)   // piVar11[-1]
                    { paPlan[idx - 1] = PLAN_ORDERED; paOrder[idx - 1] = nOrder; }
                    else
                    { paPlan[idx - 2] = PLAN_ORDERED; paOrder[idx - 2] = nOrder; }
                    nOrder++;
                }
            }
            else if (head == PLAN_FORK_E)         // 0x12e: corridor extends EAST
            {
                if (paPlan[idx + 1] == PLAN_CORRIDOR)          // piVar11[2]
                {
                    if (col < 8 && paPlan[idx + 2] == PLAN_CORRIDOR)   // piVar11[3]
                    { paPlan[idx + 2] = PLAN_ORDERED; paOrder[idx + 2] = nOrder; }
                    else
                    { paPlan[idx + 1] = PLAN_ORDERED; paOrder[idx + 1] = nOrder; }
                    nOrder++;
                }
            }
            else if (head == PLAN_FORK_N)         // 0x12f: corridor extends NORTH
            {
                if (paPlan[idx - 10] == PLAN_CORRIDOR)         // piVar11[-9]
                {
                    // base<0xb  <=>  row 0 (rowBase == 0); DESKADV iStack_8 = row*10
                    if (base < 0xb || paPlan[idx - 20] != PLAN_CORRIDOR)   // piVar11[-0x13]
                    { paPlan[idx - 10] = PLAN_ORDERED; paOrder[idx - 10] = nOrder; }
                    else
                    { paPlan[idx - 20] = PLAN_ORDERED; paOrder[idx - 20] = nOrder; }
                    nOrder++;
                }
            }
            else if (head == PLAN_FORK_S)         // 0x130: corridor extends SOUTH
            {
                if (paPlan[idx + 10] == PLAN_CORRIDOR)         // piVar11[0xb]
                {
                    // base<0x50  <=>  row < 8
                    if (base < 0x50 && paPlan[idx + 20] == PLAN_CORRIDOR) // piVar11[0x15]
                    { paPlan[idx + 20] = PLAN_ORDERED; paOrder[idx + 20] = nOrder; }
                    else
                    { paPlan[idx + 10] = PLAN_ORDERED; paOrder[idx + 10] = nOrder; }
                    nOrder++;
                }
            }
        }
    }

    // Phase 3: island seeds (0x66) -> order the far body end
    for (row = 0, base = 0; row < 10; row++, base += 10)
    {
        for (int col = 0; col < 10; col++)
        {
            int idx = base + col;
            if (paPlan[idx] != PLAN_LOCK)         // 0x66 island seed
                continue;
            int orient = IndyGetIslandOrientation(paPlan, row, col);
            int target = -1;
            if (orient == 1)          // west: walk left while body (0x68)
            {
                int j = col - 1, guard = 0;
                do { if (j < 0) { j = 0; guard++; }
                     else if (paPlan[base + j] != PLAN_WALL) { j++; guard++; } else j--; } while (guard == 0);
                target = base + j;
            }
            else if (orient == 3)     // east
            {
                int j = col + 1, guard = 0;
                do { if (j >= 10) { j = 9; guard++; }
                     else if (paPlan[base + j] != PLAN_WALL) { j--; guard++; } else j++; } while (guard == 0);
                target = base + j;
            }
            else if (orient == 2)     // north
            {
                int j = row - 1, guard = 0;
                do { if (j < 0) { j = 0; guard++; }
                     else if (paPlan[j * 10 + col] != PLAN_WALL) { j++; guard++; } else j--; } while (guard == 0);
                target = j * 10 + col;
            }
            else if (orient == 4)     // south
            {
                int j = row + 1, guard = 0;
                do { if (j >= 10) { j = 9; guard++; }
                     else if (paPlan[j * 10 + col] != PLAN_WALL) { j--; guard++; } else j++; } while (guard == 0);
                target = j * 10 + col;
            }
            if (target >= 0)
            {
                paPlan[target] = PLAN_ORDERED;
                paOrder[target] = nOrder;
                nOrder++;
            }
        }
    }

    // Phase 4: random-fill remaining path/corridor cells to reach nTarget
    {
        int done = 0, tries = 0;
        do
        {
            if (nTarget <= nOrder) done++;
            if (tries > 200) done++;
            int col, row;
            if (tries < 0x32)
            {
                col = rand() % 10;
                if (col < 1 || col > 8) { row = rand() % 10; }
                else                    row = (rand() & 1) ? 0 : 9;
            }
            else { col = rand() % 10; row = rand() % 10; }
            if (nTarget <= nOrder) break;
            if (GetZoneGridOrder(row, col) > 2 || tries >= 0x96)
            {
                int idx = row * 10 + col;
                if (paPlan[idx] == PLAN_PATH || paPlan[idx] == PLAN_CORRIDOR)
                {
                    bool w = (col == 0 || paPlan[idx - 1]  != PLAN_ORDERED);
                    bool e = (col == 9 || paPlan[idx + 1]  != PLAN_ORDERED);
                    bool n = (row == 0 || paPlan[idx - 10] != PLAN_ORDERED);
                    bool s = (row == 9 || paPlan[idx + 10] != PLAN_ORDERED);
                    if (w && e && n && s)
                    {
                        paPlan[idx] = PLAN_ORDERED;
                        paOrder[idx] = nOrder;
                        nOrder++;
                    }
                }
                if (nTarget <= nOrder) break;
                tries++;
            }
        } while (done == 0);
    }

    // Phase 5: if the goal (max-order) cell is too central (grid order < 3), move it outward by
    // swapping its order id with an outer ordered cell (grid order > 2).
    int lastOrder = nOrder - 1;
    bool found = false;
    int lastRow = 0, lastCol = 0;
    for (row = 0; row < 10 && !found; row++)
        for (int col = 0; col < 10; col++)
            if (paOrder[row * 10 + col] == lastOrder && GetZoneGridOrder(row, col) < 3)
            { lastRow = row; lastCol = col; found = true; break; }
    if (found)
    {
        for (int row = 0; row < 10; row++)
            for (int col = 0; col < 10; col++)
            {
                int idx = row * 10 + col;
                if (paOrder[idx] >= 0 && GetZoneGridOrder(row, col) > 2 && paOrder[idx] != lastOrder)
                {
                    int tmp = paOrder[idx];
                    paOrder[idx] = lastOrder;
                    paOrder[lastRow * 10 + lastCol] = tmp;
                    return nOrder;
                }
            }
    }
    return nOrder;
}

// ===========================================================================
// IndyPickCellForItemZone — DESKADV 1010:9b98
// ===========================================================================
// Choose a grid cell to host a puzzle "useful object" zone (node 0x11). Builds three
// candidate lists over the 10x10 plan while scanning grid order vs nMinOrder:
//   listA: interior PLAN_PATH/CORRIDOR cells with all four sides != PLAN_ORDERED (best),
//   listB: interior PLAN_PATH/CORRIDOR cells otherwise,
//   listC: cells whose grid order > nMinOrder and plan == PLAN_PATH (fallback).
// Pick a random cell from the first non-empty of listA/listB/listC; write its (col,row) into
// *pnX/*pnY. Returns 1 if a cell was chosen, else 0.
int CDeskcppDoc::IndyPickCellForItemZone(int *pnY, int *pnX, short *paPlan, int nMinOrder)
{
    CWordArray listA, listB, listC;   // each stores packed (row*10+col)
    for (int row = 0; row < 10; row++)
    {
        for (int col = 0; col < 10; col++)
        {
            int idx = row * 10 + col;
            int order = GetZoneGridOrder(row, col);
            short t = paPlan[idx];
            if (order > nMinOrder)
            {
                if (t == PLAN_PATH)
                    listC.SetAtGrow(listC.GetSize(), (short)idx);
            }
            else if (t == PLAN_PATH || t == PLAN_CORRIDOR)
            {
                bool w = (col < 1 || paPlan[idx - 1]  != PLAN_ORDERED);
                bool e = (col > 8 || paPlan[idx + 1]  != PLAN_ORDERED);
                bool n = (row < 1 || paPlan[idx - 10] != PLAN_ORDERED);
                bool s = (row > 8 || paPlan[idx + 10] != PLAN_ORDERED);
                if (w && e && n && s)
                    listA.SetAtGrow(listA.GetSize(), (short)idx);
                else
                    listB.SetAtGrow(listB.GetSize(), (short)idx);
            }
        }
    }
    int pick = -1;
    if (listA.GetSize() > 0)      pick = (short)listA.GetAt(rand() % listA.GetSize());
    else if (listB.GetSize() > 0) pick = (short)listB.GetAt(rand() % listB.GetSize());
    else if (listC.GetSize() > 0) pick = (short)listC.GetAt(rand() % listC.GetSize());
    if (pick < 0)
        return 0;
    *pnX = pick % 10;   // col
    *pnY = pick / 10;   // row
    return 1;
}

// ===========================================================================
// IndyPlacePuzzlesPass — DESKADV 1010:9ebc
// ===========================================================================
// Final placement pass. (1) Queue two seed puzzle items (a random "map" item 0x1c2/0x203/
// 0x1c6/0x1b6 + item 0x1bb) into the worklist. (2) For each queued item, pick a cell
// (IndyPickCellForItemZone) and place a useful-object node (0x11) there, writing the cell's
// zone/type into the mapGrid. (3) Free the queue + reset uniqueRequiredItemsMaybe. (4) Sweep
// the plan grid: every PLAN_PATH/CORRIDOR/PLAN_WALL(0x68 island body) cell that is still
// unassigned gets a plain node-type-1 zone placed (indyPlaceOnEdge gates near-start cells);
// count teleporter-bearing zones. (5) If exactly one teleporter zone was placed, force one
// more plain node onto its cell. Returns 1.
// NOTE: the map-item constants (0x1c2/0x203/0x1c6/0x1b6/0x1bb) are Indy item ids taken
//   verbatim; the mapGrid writes mirror Yoda's cell-write idiom (see header (d)).
int CDeskcppDoc::IndyPlacePuzzlesPass(short *paPlan)
{
    // (1) queue seed items
    int r = rand() % 5;
    int mapItem = 0x1c2;
    if (r == 2) mapItem = 0x203;
    else if (r == 3) mapItem = 0x1c6;
    else if (r == 4) mapItem = 0x1b6;
    IndyQueueItemForPlacement(2, (short)mapItem);
    IndyQueueItemForPlacement(1, 0x1bb);

    // (2) place a useful-object node for each queued item
    int nQueued = worldgenPendingZones.GetSize();
    for (int qi = 0; qi < nQueued; qi++)
    {
        WorldgenZoneEntry *pEnt = (WorldgenZoneEntry *)worldgenPendingZones.GetAt(qi);
        short item  = pEnt->zoneId;   // {zoneId:=item}
        short order = pEnt->val;      // {val:=order/tag}  (DESKADV entry+6 -> order; entry+4 -> item)
        int cy, cx;
        if (IndyPickCellForItemZone(&cy, &cx, paPlan, order) != 1)
            return 0;
        int nZone = IndyPlaceQuestNode(-1, item, -1, 0x11);
        if (nZone < 0)
            return 0;
        int cell = cy * 10 + cx;
        mapGrid[cell].zoneType = 0x11;
        mapGrid[cell].cellItemA = genCellItemBScratch;   // DESKADV cell+0x39c = doc+0x1d
        mapGrid[cell].id = (short)nZone;
        apZoneGrid[cell] = (Zone *)zones.GetAt(nZone);
        paPlan[cell] = PLAN_ORDERED;
        AddPlacedZoneId((short)nZone);                   // IndyMarkZoneUsed
    }
    // (3) free the queue, reset used-required-items
    {
        int n = worldgenPendingZones.GetSize();
        for (int i = 0; i < n; i++)
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
        worldgenPendingZones.SetSize(0, -1);
        uniqueRequiredItemsMaybe.SetSize(0, -1);
    }

    // (4) fill remaining path cells with plain node-type-1 zones
    int nTeleZones = 0;
    int teleRow = 0, teleCol = 0;
    for (int row = 0; row < 10; row++)
    {
        for (int col = 0; col < 10; col++)
        {
            int cell = row * 10 + col;
            int t = paPlan[cell];
            if (t == PLAN_PATH || t == PLAN_CORRIDOR || t == PLAN_WALL /*0x68 island body*/)
            {
                int order = GetZoneGridOrder(row, col);
                indyPlaceOnEdge = (t == PLAN_WALL || order < 2) ? 1 : 0;
                int nZone = IndyPlaceQuestNode((short)order, -1, -1, 1);
                if (nZone >= 0)
                {
                    Zone *pZone = (Zone *)zones.GetAt(nZone);
                    if (indyPlaceOnEdge == 0)
                    {
                        // count zones that hold a TELEPORTER (obj type 0xd)
                        int nObj = pZone->objects.GetSize();
                        bool hasTele = false;
                        for (int i = 0; i < nObj; i++)
                            if (((ZoneObj *)pZone->objects.GetAt(i))->type == 0xd) { hasTele = true; break; }
                        if (hasTele) { nTeleZones++; teleRow = row; teleCol = col; }
                    }
                    mapGrid[cell].zoneType = 1;
                    mapGrid[cell].id = (short)nZone;
                    apZoneGrid[cell] = pZone;
                    mapGrid[cell].cellItemA = -1;   // DESKADV writes 0xffff to the extra slots
                    mapGrid[cell].cellItemB = -1;
                    AddPlacedZoneId((short)nZone);   // IndyMarkZoneUsed
                }
            }
        }
    }
    // (5) if exactly one teleporter zone, force a plain node onto its cell (paired teleporter)
    if (nTeleZones == 1)
    {
        indyPlaceOnEdge = 1;
        int order = GetZoneGridOrder(teleRow, teleCol);
        int nZone = IndyPlaceQuestNode((short)order, -1, -1, 1);
        if (nZone >= 0)
        {
            int cell = teleRow * 10 + teleCol;
            mapGrid[cell].zoneType = 1;
            mapGrid[cell].id = (short)nZone;
            apZoneGrid[cell] = (Zone *)zones.GetAt(nZone);
            AddPlacedZoneId((short)nZone);
        }
    }
    return 1;
}

// ===========================================================================
// IndyMaterializePlacedItemTiles — DESKADV 1020:07ae
// ===========================================================================
// After all zones are placed, walk a zone's objects and stamp the actual tile graphic for
// each activated (state==1) item-carry object: item696 hosts (type 0xb) become tile 0x2b8;
// ITEM/SPAWN/useful hosts (types 0..2 and 5..8) with a valid arg get their item's tile.
// The stamping calls FUN_1010_183c/1896 (zone tile setters) — modeled here as SetItemTile().
// NOTE: the exact type predicate is `t<3 || (t-2>2 && t-5<4)` from the decomp = {0,1,2,5,6,7,8}.
void CDeskcppDoc::IndyMaterializePlacedItemTiles(int nZoneId)
{
    if (nZoneId < 0)
        return;
    Zone *pZone = (Zone *)zones.GetAt(nZoneId);
    if (pZone == NULL)
        return;
    int nObj = pZone->objects.GetSize();
    for (int i = 0; i < nObj; i++)
    {
        ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(i);
        unsigned int t = pObj->type;
        // (obj+6==0 sub-test dropped)
        if (t == 0xb)
        {
            if (pObj->state == 1)
            {
                pObj->arg = 0x2b8;   // item696 tile id
                // SetItemTile(pZone, layer=1, x=pObj->x, y=pObj->y, tile=0x2b8) with fallback:
                // if (SetTileForItem(...) < 0) ForceTileForItem(0x2b8, x, y);
            }
        }
        else if (t < 0xc)
        {
            unsigned char b = (unsigned char)t;
            bool carries = (b < 3) || (((unsigned char)(b - 2) > 2) && ((unsigned char)(b - 5) < 4));
            if (carries && pObj->state == 1 && pObj->arg >= 0)
            {
                // SetItemTile(pZone, layer=1, x, y, tileForItem(pObj->arg)); fallback ForceTile.
            }
        }
    }
}

// ===========================================================================
// IndyGenerate — DESKADV 1010:8524   (entry point; Yoda Generate analog)
// ===========================================================================
// Contract: return 1 on success, 0 to retry (caller loops re-seeding via Randomize()).
// RNG draw order (verified against DESKADV): srand(abs(seed)); rand()%3 (maxGoals),
// rand()%4 (maxSplits), rand() DISCARDED, rand()%2+4 (startX), rand()%2+4 (startY).
//
// This orchestrator mirrors Yoda CDeskcppDoc::Generate's shape but uses Indy's node model
// (no planets, single-item puzzles). The per-cell mapGrid writes follow Yoda's idiom
// (header (d)). The doc-side scratch grids in DESKADV (per-cell record @doc+0x394 stride
// 0x16 = MapZone) are expressed directly through mapGrid[]/apZoneGrid[] here.
int CDeskcppDoc::IndyGenerate(unsigned int nSeed)
{
    short aPlan[100];    // DESKADV stack &local_108 — the plan grid (tokens)
    short aOrder[100];   // DESKADV local_1d0     — the order grid

    // View busy flag (mirrors Yoda Generate head)
    POSITION pos = GetFirstViewPosition();
    CDeskcppView *pView = NULL;
    if (pos != NULL)
        pView = (CDeskcppView *)GetNextView(pos);
    if (pView != NULL)
        pView->bBusy = 1;

    worldSeed = nSeed;
    nFrameMode = 0;                          // DESKADV *(doc+0x12*?)=0
    uniqueRequiredItemsMaybe.SetSize(0, -1); // DESKADV FUN_..(+0x16a) clear
    worldSeed = nSeed;

    srand(abs((int)nSeed));
    int maxGoals  = rand() % 3;              // local_16
    int maxSplits = rand() % 4;              // local_24
    rand();                                  // discarded
    int nGoals = 0, nSplits = 0, nPlaced = 0;
    int nStartX = rand() % 2 + 4;            // local_38
    int nStartY = rand() % 2 + 4;            // local_3a

    // clear grids + mapGrid cells
    for (int y = 0; y < 10; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            aPlan[y * 10 + x] = 0;
            aOrder[y * 10 + x] = -1;
            mapGrid[y * 10 + x].id = -1;
            mapGrid[y * 10 + x].cellQuestSlot0 = -1;
            mapGrid[y * 10 + x].cellQuestSlot1 = -1;
            mapGrid[y * 10 + x].cellQuestSlot5 = -1;
            mapGrid[y * 10 + x].cellQuestSlot6 = -1;
            mapGrid[y * 10 + x].cellItemA = -1;
            mapGrid[y * 10 + x].cellItemB = -1;
            mapGrid[y * 10 + x].cellItemC = -1;
            mapGrid[y * 10 + x].field30 = 0;
        }
    }

    // clear worklists (DESKADV FUN_..(+0x132/+0x57/+0x65) + free +0x5f/+0x18a object lists +
    //   +0x50/+0x14e).  Our reused members:
    goalTileList.SetSize(0, -1);             // +0x132 per-step puzzle ids
    // (+0x57 / +0x65 / +0x5f / +0x18a are the load-side aux/object CObArrays being reset;
    //  those are engine bookkeeping — reset the equivalents if needed at integration.)
    storyHistoryNevada.SetSize(0, -1);       // +0x140 used puzzles
    placedZoneIds.SetSize(0, -1);            // +0x14e used zones

    // free + reset the placed-items (+0x186) and queued-items (+0x178) worklists:
    {
        int n = worldgenRefZones.GetSize();
        int i;
        for (i = 0; i < n; i++)
            delete (WorldgenZoneEntry *)worldgenRefZones.GetAt(i);
        worldgenRefZones.SetSize(0, -1);
        n = worldgenPendingZones.GetSize();
        for (i = 0; i < n; i++)
            delete (WorldgenZoneEntry *)worldgenPendingZones.GetAt(i);
        worldgenPendingZones.SetSize(0, -1);
    }

    // load prior placed/story lists if requestedGoal < 0 (DESKADV: +0x52 < 0):
    if (nRequestedGoalItem < 0)
    {
        // IndyLoadPlacedZoneList(); IndyLoadStoryHistory();   // TODO: INI persistence (SKIP)
    }
    int nOldPlaced = placedZoneIds.GetSize();   // local_40
    // (DESKADV trims the used-puzzle history to <= ~11 entries; keep equivalent if desired)

    // difficulty node-count band tables (DESKADV +0x109e):  {t2Hi,t2Lo,t3Hi,t3Lo,t4Hi,t4Lo,extraHi,extraLo}
    int t2Hi, t2Lo, t3Hi, t3Lo, t4Hi, t4Lo, extraHi, extraLo;
    if (difficulty == 1)          // easy
    { t2Hi=8; t2Lo=5; t3Hi=6; t3Lo=4; t4Hi=1; t4Lo=1; extraHi=1; extraLo=1; }
    else if (difficulty == 3)     // hard
    { t2Hi=0xe; t2Lo=7; t3Hi=0x11; t3Lo=8; t4Hi=0xc; t4Lo=0xc; extraHi=0xc; extraLo=5; }
    else                          // medium
    { t2Hi=0xc; t2Lo=6; t3Hi=0xc; t3Lo=6; t4Hi=0xb; t4Lo=0xb; extraHi=0xb; extraLo=4; }

    // plant the start marker
    nPlaced = 4;                                  // DESKADV local_18 = 4 (seeded room count)
    aPlan[nStartY * 10 + nStartX] = PLAN_START;   // 0xc9

    // three quest rings (tiers 2,3,4) with random budgets from the band table
    int budget2 = rand() % (t2Hi - t2Lo + 1) + t2Lo + maxSplits + maxGoals;
    if (budget2 > 0xc) budget2 = 0xc;
    IndyCarveQuestPath(&nPlaced, &nSplits, maxSplits, &nGoals, maxGoals, aPlan, budget2, 2);
    int budget3 = t3Lo + rand() % (t3Hi - t3Lo + 1);
    IndyCarveQuestPath(&nPlaced, &nSplits, maxSplits, &nGoals, maxGoals, aPlan, budget3, 3);
    int budget4 = rand() % (t4Hi - t4Lo + 1) + t4Lo;
    IndyCarveQuestPath(&nPlaced, &nSplits, maxSplits, &nGoals, maxGoals, aPlan, budget4, 4);
    int nIslands = rand() % (extraHi - extraLo + 1) + extraLo;

    // ---- retry loop: grow edge islands until the map is "full enough", then assemble ----
    // DESKADV wraps the assembly in a do/while that keeps stamping edge islands (0x66/0x68)
    // and only proceeds once a stop condition trips (budget exhausted or > 400 attempts).
    int nEdgeAttempts = 0;   // DESKADV piStack_c
    for (;;)
    {
        int stop = 0;
        // ---- RESOLVED (re-decompiled 1010:8524, the do/while at ~0x8524..0x8d74): one edge-growth
        // step. Pick a random BORDER cell (one axis pinned to {0,9}), and if it is empty, extend a
        // corridor/path INWARD toward the first occupied interior neighbor, decrementing the island
        // budget (nIslands = DESKADV local_4). Growth direction: from the pinned border look at the
        // interior neighbor (top->south, bottom->north, left->east, right->west); if occupied,
        // record the growth vector. Then stamp based on the TARGET cell one step further in.
        {
            int a, b;                       // a = row (stride 10), b = col
            int growRow = 0, growCol = 0;   // DESKADV local_10 / puVar13 (+/-1 = grow up/down|L/R)
            bool aPinned;
            if ((rand() & 1) == 0) { int c2 = rand(); a = ((c2 & 1) == 0) ? 9 : 0; b = rand() % 10; aPinned = true; }
            else                   { int c2 = rand(); b = ((c2 & 1) == 0) ? 9 : 0; a = rand() % 10; aPinned = false; }
            nEdgeAttempts++;
            int cell = a * 10 + b;
            if (aPlan[cell] == 0)
            {
                if (aPinned)
                {
                    if (a == 0)              { short s = aPlan[cell + 10]; if (s != 0 && s != PLAN_BLOCKED) growRow = -1; }  // grow down
                    else if (a == 9)         { short n = aPlan[cell - 10]; if (n != 0 && n != PLAN_BLOCKED) growRow =  1; }  // grow up
                }
                else
                {
                    if (b == 0)              { short e = aPlan[cell + 1];  if (e != 0 && e != PLAN_BLOCKED) growCol = -1; }  // grow right
                    else if (b == 9)         { short w = aPlan[cell - 1];  if (w != 0 && w != PLAN_BLOCKED) growCol =  1; }  // grow left
                }
                if (growRow != 0 || growCol != 0)
                {
                    int drow = -growRow, dcol = -growCol;
                    short tgt = aPlan[cell + drow * 10 + dcol];        // the target one step further in
                    // "clear" perpendicular neighbor = off-grid, empty, or >= PLAN_FORK_S (DESKADV 0x12f<x)
                    bool nsClear = ((a == 0 || aPlan[cell-10] == 0 || aPlan[cell-10] >= PLAN_FORK_S) &&
                                    (a == 9 || aPlan[cell+10] == 0 || aPlan[cell+10] >= PLAN_FORK_S));
                    bool ewClear = ((b == 0 || aPlan[cell-1]  == 0 || aPlan[cell-1]  >= PLAN_FORK_S) &&
                                    (b == 9 || aPlan[cell+1]  == 0 || aPlan[cell+1]  >= PLAN_FORK_S));
                    if (tgt == PLAN_FORK_W || tgt == PLAN_FORK_E)      // horizontal target
                    {
                        bool aligned = (tgt == PLAN_FORK_W) ? (growCol == -1) : (growCol == 1);
                        if (aligned && nsClear)
                        {
                            aPlan[cell] = PLAN_CORRIDOR;
                            if (a > 0) aPlan[cell-10] = PLAN_BLOCKED;
                            if (a < 9) aPlan[cell+10] = PLAN_BLOCKED;
                            nPlaced++; nIslands--;
                        }
                    }
                    else if (tgt == PLAN_FORK_N || tgt == PLAN_FORK_S) // vertical target
                    {
                        bool aligned = (tgt == PLAN_FORK_N) ? (growRow == -1) : (growRow == 1);
                        if (aligned && ewClear)
                        {
                            aPlan[cell] = PLAN_CORRIDOR;
                            if (b > 0) aPlan[cell-1] = PLAN_BLOCKED;
                            if (b < 9) aPlan[cell+1] = PLAN_BLOCKED;
                            nPlaced++; nIslands--;
                        }
                    }
                    else if (tgt == PLAN_CORRIDOR)                     // continue corridor (no budget change)
                    {
                        aPlan[cell] = PLAN_CORRIDOR;
                        if (growRow != 0) { if (b > 0) aPlan[cell-1]  = PLAN_BLOCKED; if (b < 9) aPlan[cell+1]  = PLAN_BLOCKED; }
                        else              { if (a > 0) aPlan[cell-10] = PLAN_BLOCKED; if (a < 9) aPlan[cell+10] = PLAN_BLOCKED; }
                    }
                    else if (tgt == PLAN_PATH || tgt == PLAN_GOAL ||
                             tgt == PLAN_START || tgt == PLAN_ORDERED)
                    {
                        aPlan[cell] = PLAN_PATH;
                        nPlaced++; nIslands--;
                    }
                }
            }
        }
        if (nIslands < 1) stop++;             // DESKADV local_4 < 1
        if (nEdgeAttempts > 400) stop++;      // DESKADV piStack_c > 400
        if (stop == 0)
            continue;

        // finalize the island strips (DESKADV passes local_1c = the goal counter)
        IndyPlaceIslandStrips(aPlan, nGoals);

        // clear every zone's activatedFlag (DESKADV: for each zone, zone+8 = 0)
        {
            int nz = zones.GetSize();
            for (int i = 0; i < nz; i++)
            {
                Zone *pz = (Zone *)zones.GetAt(i);
                if (pz != NULL)
                    pz->activatedFlag = 0;
            }
        }

        // assign quest-step order ids; nSteps = number of quest cells
        int nSteps = IndyAssignQuestStepCells(aOrder, aPlan);
        goalTileList.SetSize(nSteps + 1, -1);   // DESKADV +0x132 sized to steps

        // choose the world-mission goal puzzle (requestedGoal override, else pick one)
        short goal;
        if (nRequestedGoalItem < 0)
            goal = IndySelectPuzzle(0, 9999, 0xffff, 0xffff);
        else
            goal = (short)nRequestedGoalItem;
        if (goal < 0)
            return 0;                            // no world-mission available -> retry

        goalTileList.SetAt(nSteps, goal);        // DESKADV: goal puzzle at slot nSteps
        // record the goal item into the doc (DESKADV: doc+0xc36 = puzzles[goal].itemA),
        // mark the goal puzzle used, then main-quest assembly:
        // g_finalGoalItem = ((Puzzle*)puzzles.GetAt(goal))->itemA;   // doc+0xc36
        storyHistoryNevada.SetAtGrow(storyHistoryNevada.GetSize(), goal);

        // --- PASS 1: VEHICLE/COMPANION pass. For each unclaimed PLAN_GOAL cell, place a node-type-6
        //     zone (the "FROM_ANOTHER_MAP" side), read its VEHICLE_TO (obj type 3) destination zone,
        //     then search the plan for a border PLAN_LOCK cell to host the destination ("TO_ANOTHER_
        //     MAP", zoneType 7). Transcribed from the SAME-ENGINE Yoda CDeskcppDoc::Generate,
        //     src/Worldgen.cpp lines 2828-2949 (verified against DESKADV IndyGenerate; Indy node
        //     types 6/7 == Yoda ZONE_TYPE_FROM/TO_ANOTHER_MAP). If no free LOCK cell is found, the
        //     whole goal cell is UNDONE (reverted to PLAN_PATH).
        for (int y = 0; y < 10; y++)
        {
            for (int x = 0; x < 10; x++)
            {
                genCellQuestSlot6Scratch = -1;
                genCellItemAScratch = -1;
                genCellItemCScratch = -1;
                int cell = y * 10 + x;
                if (aPlan[cell] != PLAN_GOAL || apZoneGrid[cell] != NULL)
                    continue;
                int nZone = IndyPlaceQuestNode((short)GetZoneGridOrder(x, y), -1, -1, 6);
                int nDest = -1;
                if (nZone < 0)
                    continue;
                mapGrid[cell].id = (unsigned short)nZone;
                mapGrid[cell].zoneType = 6;                        // Yoda ZONE_TYPE_FROM_ANOTHER_MAP
                mapGrid[cell].cellItemA = (short)genCellItemAScratch;
                Zone *pZone = (Zone *)zones.GetAt((short)nZone);
                int nObjs = pZone->objects.GetSize();
                for (int k = 0; k < nObjs; k++)
                {
                    ZoneObj *pObj = (ZoneObj *)pZone->objects.GetAt(k);
                    if (pObj->type == 3)                          // OBJ_VEHICLE_TO
                    { nDest = (short)pObj->arg; break; }
                }
                int bFound = 0, xLock = 0, yLock = 0;
                if (nDest >= 0)
                {
                    // pass 1: left column (col 0), scan rows
                    for (int k = 0; k < 10; k++)
                        if (aPlan[k * 10] == PLAN_LOCK && apZoneGrid[k * 10] == NULL)
                        { bFound = 1; yLock = k; break; }
                    // pass 2: within row yLock, scan columns
                    if (!bFound)
                        for (int k = 0; k < 10; k++)
                            if (aPlan[yLock * 10 + k] == PLAN_LOCK && apZoneGrid[yLock * 10 + k] == NULL)
                            { bFound = 1; xLock = k; break; }
                    // pass 3: right column (col 9), scan rows
                    if (!bFound)
                    {
                        xLock = 9;
                        for (int k = 0; k < 10; k++)
                            if (aPlan[9 + k * 10] == PLAN_LOCK && apZoneGrid[9 + k * 10] == NULL)
                            { bFound = 1; yLock = k; break; }
                    }
                    // pass 4: bottom row (row 9), scan columns
                    if (!bFound)
                    {
                        yLock = 9; xLock = 0;
                        for (int k = 0; k < 10; k++)
                            if (aPlan[90 + k] == PLAN_LOCK && apZoneGrid[90 + k] == NULL)
                            { bFound = 1; xLock = k; break; }
                    }
                }
                if (bFound)
                {
                    if (IsZoneUsed((short)nDest) == 0)
                    {
                        int nCell2 = xLock + yLock * 10;
                        apZoneGrid[nCell2] = (Zone *)zones.GetAt(nDest);
                        mapGrid[nCell2].id = (short)nDest;
                        mapGrid[nCell2].zoneType = 7;             // Yoda ZONE_TYPE_TO_ANOTHER_MAP
                        mapGrid[nCell2].cellItemA = (short)genCellItemAScratch;
                        apZoneGrid[cell] = pZone;                 // commit the goal cell
                        AddPlacedZoneId((short)nZone);
                        AddPlacedZoneId((short)nDest);
                    }
                }
                else
                {
                    // no free LOCK cell — undo the whole goal placement
                    RemoveZoneEntry((short)genCellItemAScratch);
                    aPlan[cell] = PLAN_PATH;
                    apZoneGrid[cell] = NULL;
                    mapGrid[cell].id = -1;
                    mapGrid[cell].zoneType = -1;
                    mapGrid[cell].cellItemA = -1;
                }
            }
        }

        // --- PASS 2: MAIN QUEST CHAIN. Walk order ids high->low; the highest is the goal
        //     (node type 10), the rest alternate TRADE(0xf)/TRANSACTION(0x10). Each order id
        //     maps to a plan cell (aOrder[cell]==order). ---
        for (int order = nSteps; order > 0; order--)
        {
            // find the cell carrying this order
            int cell = -1;
            for (int c = 0; c < 100 && cell < 0; c++)
                if (aOrder[c] == order - 1) cell = c;   // DESKADV: *p2 - nOrderVal == -1
            if (cell < 0)
                continue;
            int y = cell / 10, x = cell % 10;
            short gtl = (short)goalTileList.GetAt(order);
            short reqItem = (gtl >= 0) ? ((Puzzle *)puzzles.GetAt(gtl))->itemA : (short)-1;
            int nZone = -1;
            int nodeType;
            if (order == nSteps)
            {
                nZone = IndyPlaceQuestNode((short)GetZoneGridOrder(y, x), reqItem, (short)(order - 1), 10);
                nodeType = 10;
                if (nZone < 0) { goto fail_reset; }
            }
            else
            {
                nodeType = (rand() % 2 == 0) ? 0x10 : 0xf;
                nZone = IndyPlaceQuestNode((short)GetZoneGridOrder(y, x), reqItem, (short)(order - 1), (short)nodeType);
                if (nZone < 0)
                {
                    nodeType = (nodeType == 0x10) ? 0xf : 0x10;   // try the other trade/transaction
                    nZone = IndyPlaceQuestNode((short)GetZoneGridOrder(y, x), reqItem, (short)(order - 1), (short)nodeType);
                    if (nZone < 0) { goto fail_reset; }
                }
            }
            mapGrid[cell].zoneType = nodeType;
            mapGrid[cell].cellQuestSlot0 = (short)order;
            mapGrid[cell].cellItemA = (short)genCellItemAScratch;
            mapGrid[cell].cellItemB = (short)genCellItemBScratch;
            mapGrid[cell].cellItemC = (short)genCellItemCScratch;
            mapGrid[cell].cellQuestSlot5 = (short)genCellQuestSlot5Scratch;
            mapGrid[cell].field30 = 1;
            mapGrid[cell].id = (short)nZone;
            apZoneGrid[cell] = (Zone *)zones.GetAt(nZone);
            AddPlacedZoneId((short)nZone);
        }

        // --- PASS 3: token pass — remaining fork/start cells become their node types. ---
        for (y = 0; y < 10; y++)
        {
            for (int x = 0; x < 10; x++)
            {
                int cell = y * 10 + x;
                if (apZoneGrid[cell] != NULL)
                    continue;
                short t = aPlan[cell];
                int nodeType = -1, ctrlType = -1;
                if (t == PLAN_START)      { nodeType = 0xb; ctrlType = 0xb; }
                else if (t == PLAN_FORK_W){ nodeType = 5; }
                else if (t == PLAN_FORK_E){ nodeType = 4; }
                else if (t == PLAN_FORK_N){ nodeType = 2; }
                else if (t == PLAN_FORK_S){ nodeType = 3; }
                else if (t != PLAN_PATH && t != PLAN_WALL && t != PLAN_CORRIDOR)
                    nodeType = 1;        // generic
                if (nodeType < 0)
                    continue;
                int nZone = IndyPlaceQuestNode((short)GetZoneGridOrder(y, x), -1, -1, (short)nodeType);
                if (nZone < 0)
                    continue;
                mapGrid[cell].zoneType = nodeType;
                mapGrid[cell].id = (short)nZone;
                apZoneGrid[cell] = (Zone *)zones.GetAt(nZone);
                if (nodeType != 0xb)                 // DESKADV: the start (0xb) node skips MarkZoneUsed
                    AddPlacedZoneId((short)nZone);
                (void)ctrlType;
            }
        }

        // --- puzzle/useful-object placement pass ---
        if (IndyPlacePuzzlesPass(aPlan) != 0)
        {
            // materialize item tiles across every placed mapGrid zone (DESKADV walks the 10x10
            // record grid; each cell with a valid zone id -> IndyMaterializePlacedItemTiles):
            for (int cell = 0; cell < 100; cell++)
            {
                short zoneId = mapGrid[cell].id;
                if (zoneId >= 0)
                    IndyMaterializePlacedItemTiles(zoneId);
            }
            if (nOldPlaced > 0)
            {
                // DESKADV FUN_..(+0x14e, nOldPlaced, 0): trim the FIRST nOldPlaced entries off
                // placedZoneIds (the pre-existing used-zone ids carried in from a loaded game).
                placedZoneIds.RemoveAt(0, nOldPlaced);
            }
            // IndyFilterEnemyZonesFromPlacedList(); IndySavePlacedZoneList();
            // IndySaveStoryHistory();              // TODO: INI persistence (SKIP per brief)
            nRequestedGoalItem = -1;                 // DESKADV doc+0x52 = 0xffff

            // ---- play-state tail (DESKADV 1010:8524 tail; field ROLES cross-checked vs Yoda
            //      CDeskcppDoc::Generate/Populate — offsets differ, roles match) ----
            // totalZones (completion-score denominator) = (nSplits + nGoals)*2 + quest-step count.
            //   DESKADV: (local_1a + local_1c)*2 + doc+0x13a; local_1a=nSplits, local_1c=nGoals,
            //   doc+0x13a = goalTileList size (nSteps+1).
            totalZones = (nSplits + nGoals) * 2 + goalTileList.GetSize();

            playerX = nStartX;                       // DESKADV local_38 -> player start col
            playerY = nStartY;                       // DESKADV local_3a -> player start row
            gameState = 0;                           // in-progress (kept from head)
            // DESKADV also resets the HERO ENTITY's health to 120 (0x78) at entity+0x90 and clears
            // entity+0x2c. That is the player Character's HP; TODO(integration) to wire the hero
            // Character HP. DESKADV additionally sets a UI/turn-timer (doc+0x10a2) + a clock stamp.
            // ---- world-entry transition (Indy skips Yoda's Populate(), so replicate its tail —
            //      src/Worldgen.cpp Populate() lines ~6163-6174 — the world-view handoff). Without
            //      pView->bBusy=0 the view never renders past the STUP graphic. ----
            // The initial world-entry transition targets the START zone (the MAP_START_AREA
            // cell the generator placed at (nStartX,nStartY)). Yoda's Populate hardcodes 0
            // because its demo layout puts the intro zone at id 0; Indy's start zone id is
            // dynamic, so read it from the grid cell the player spawns in. Using 0 rendered
            // and ran scripts against the wrong zone (its objects/doors never matched).
            if (pView != NULL)
                pView->nTargetZoneId = GetZoneCell(nStartX, nStartY);
            cameraX = 0x140;
            cameraY = 0x140;
            nFrameMode = 0xb;                        // DESKADV doc+0x4a = 0xb (== Yoda Populate)
            // bWorldInvalid=1 routes the OnTimer case-0xb entry through ZoneTransitionStep, which
            // self-climbs step 0->10 to play mode (nFrameMode=3); case 0xb clears bWorldInvalid=0
            // when it completes. This is the CORRECT Indy path, not a workaround (v61):
            // WorldEntryStepMaybe (the bWorldInvalid==0 scripted path) can only advance past step 5
            // if a COND_FirstEnter(0)/COND_Enter(1) entry script sets nFrameMode, and headless
            // inspection of the real Indy start zone (now targeted correctly, type 11 MAP_START_AREA,
            // 45 scripts) shows it has ZERO cond-0/cond-1 scripts — Indy has no scripted intro entry.
            // (Yoda's Hoth intro zone DOES have one, which is why Yoda uses WorldEntryStepMaybe.)
            // The v59-pickup "revert once ACTN distributed" note was based on analysing the WRONG
            // zone (id 0) — the target-zone bug above; ACTN distribution was orthogonal.
            bWorldInvalid = 1;
            bQuestCellsResident = 1;
            BackupRecords();                         // snapshot the as-loaded quest-record block
            if (pView != NULL)
                pView->bBusy = 0;                    // release the view so it draws the world
            return 1;
        }

    fail_reset:
        // teardown this attempt's lists and RETRY (return 0) — DESKADV clears +0x132/+0x57/
        // +0x65 and the object lists, then returns 0 so the caller reseeds.
        goalTileList.SetSize(0, -1);
        // (reset the reused lists as at function head)
        return 0;
    }
}

#endif // GAME_INDY
