// Records — DTA record-classes TU (see Records.h). Flags: /nologo /c /MT /W3 /GX /O2
//   /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
// Functions in .text/source order. v1 = ctors/dtors + Character frame lookups.
// TODO (v2): Puzzle::Read (0x404480), Character::Read (0x4047a0),
//            Character::GetProjectileTile (0x404910); then fold in ZoneObj.cpp + Zone.cpp
//            (same TU!) and retire those files — full-TU context may fix Zone's 3 residuals.
#include "GameObjects.h"
#include <string.h>
#pragma intrinsic(memset, memcpy, strcmp)

// ============================== Puzzle ==============================

// FUNCTION: YODA 0x004042b0  [EFFECTIVE MATCH: DIFF(24) — clean ESI<->EDI bijection (orig: this=EDI,
//   zero=ESI; ours swapped). Structure/stores identical. Fresh-TU first function, decl order inert,
//   body order verified — allocator tie-break, not source-steerable. cf. Canvas residuals.]
Puzzle::Puzzle()
{
    unk14 = 0;
    itemA = -1;
    itemB = -1;
    unk3 = 0;
    unk2 = 0;
    nType = 0;
}

// FUNCTION: YODA 0x004043a0  (compiler-generated scalar-deleting destructor ??_GPuzzle)

// FUNCTION: YODA 0x004043c0
Puzzle::~Puzzle()
{
}

// FUNCTION: YODA 0x00404480
// Parses one IPUZ record: tag + size, 3 dwords + unk14, five length-prefixed texts
// (shared 0x800 buffer, zeroed before each), then the two item ids.
void Puzzle::Read(CFile *pFile)
{
    char  buf[0x800];
    int   size = 0;
    char  tag[5];
    short len;

    pFile->Read(tag, 4);
    tag[4] = 0;
    pFile->Read(&size, 4);
    if (strcmp(tag, "IPUZ") == 0) {
        pFile->Read(&nType, 4);
        pFile->Read(&unk2, 4);
#ifndef GAME_INDY
        pFile->Read(&unk3, 4);   // Indy's IPUZ record drops unk3 (verified: aligns 157 puzzles)
#endif
        pFile->Read(&unk14, 2);
        pFile->Read(&len, 2);
        if (len > 0) {
            memset(buf, 0, sizeof(buf));
            pFile->Read(buf, len);
            text1 = buf;
        }
        pFile->Read(&len, 2);
        if (len > 0) {
            memset(buf, 0, sizeof(buf));
            pFile->Read(buf, len);
            text2 = buf;
        }
        pFile->Read(&len, 2);
        if (len > 0) {
            memset(buf, 0, sizeof(buf));
            pFile->Read(buf, len);
            text3 = buf;
        }
        pFile->Read(&len, 2);
        if (len > 0) {
            memset(buf, 0, sizeof(buf));
            pFile->Read(buf, len);
            text4 = buf;
        }
        pFile->Read(&len, 2);
        if (len > 0) {
            memset(buf, 0, sizeof(buf));
            pFile->Read(buf, len);
            text5 = buf;
        }
        pFile->Read(&itemA, 2);
#ifndef GAME_INDY
        pFile->Read(&itemB, 2);   // Indy's IPUZ record drops itemB
#endif
    }
}

// ============================== Character ==============================

// FUNCTION: YODA 0x00404670
Character::Character()
{
    unk48 = 0;
    unk44 = 0;
}

// FUNCTION: YODA 0x004046e0  (compiler-generated scalar-deleting destructor ??_GCharacter)

// FUNCTION: YODA 0x00404700
Character::~Character()
{
}

// FUNCTION: YODA 0x00404750
void Character::Init(short nTypeFlags, short nMoveType, short nUnk40, int nUnk44)
{
    memset(frames, 0xff, sizeof(frames));
    typeFlags = nTypeFlags;
    weaponCharId = -1;  // after typeFlags= (EAX no longer holds the fill's 0xffffffff) -> immediate
                  // store, which the scheduler hoists to just below the rep stosd.
    moveType = nMoveType;
    health = 1;
    damage = 1;
    unk40 = nUnk40;
    unk44 = nUnk44;
}

// FUNCTION: YODA 0x004047a0
// Parses one ICHA record body: 8B header, 16B name (discarded), 3 shorts + dword -> Init,
// then the 24-short frame table.  (Defined here, right after Init, to match the original
// AppData→Records .obj emission order: Init, Read, GetWalkFrameTile, ... — Phase G2 layout.)
void Character::Read(CFile *pFile)
{
    short frameBuf[24];
    char  name[16];
    char  hdr[8];
    int   dw;
    short w1, w2, w3;

    pFile->Read(hdr, 8);
    pFile->Read(name, 0x10);
    pFile->Read(&w1, 2);
    pFile->Read(&w2, 2);
#ifdef GAME_INDY
    // Indy's ICHR record (0x4E vs Yoda 0x54) has only TWO 2-byte fields after the name, then a
    // FULL 24-short (0x30) frame block — it drops Yoda's third short + dword (DESKADV
    // FUN_1010_069c reads 8+16+2+2 then 0x30 into char+4). The frame LAYOUT/stride is identical
    // to Yoda (DESKADV FUN_1010_076e == our GetFrameTile). Previously we read Yoda's w3+dw here,
    // consuming 6 bytes of frame data and shifting every frame by 3 shorts (garbled animation);
    // the total record size was coincidentally unchanged (we also under-read frames by 0x6), so
    // CHWP still aligned. w3/dw have no Indy record fields -> pass 0.
    Init(w1, w2, 0, 0);
    pFile->Read(frameBuf, 0x30);
    memcpy(frames, frameBuf, 0x30);
#else
    pFile->Read(&w3, 2);
    pFile->Read(&dw, 4);
    Init(w1, w2, w3, dw);
    pFile->Read(frameBuf, 0x30);
    memcpy(frames, frameBuf, 0x30);
#endif
}

// FUNCTION: YODA 0x00404830
void *Character::GetWalkFrameTile(int dx, int dy, CObArray *paTiles)
{
    return GetFrameTile(dx, dy, paTiles, 0);
}

// FUNCTION: YODA 0x00404850  [EFFECTIVE MATCH: DIFF(2) — the two direction LEAs emit
//   [edx+eax+k] vs the original's [eax+edx+k] (base/index swap, same insn). All operand
//   orders/parenthesizations of dy+bank+k canonicalize the same way (temp-age tie-break,
//   lesson #6). Semantically identical.]
// Picks the sprite tile for a facing direction (dx,dy in -1..1) + animation bank.
// Frame rows: 0=up, 1=down/idle, 2..4=left(dy -1/0/1), 5..7=right; banks at +0/+8/+16 shorts.
// NOTE: bank/idx are deliberately left uninitializable on impossible inputs — the original
// reads an uninitialized stack slot there (see the [esp+4] load in the disasm).
void *Character::GetFrameTile(int dx, int dy, CObArray *paTiles, int nAnimBank)
{
    int bank;                   // two locals sharing one stack slot (disjoint lifetimes);
    int idx;                    // the original reloads [esp+4] on the never-assigned paths
    if (nAnimBank % 2 != 0) {   // negated: puts the bank=0 block out of line like the original
        if (nAnimBank == 1)
            bank = 8;
        else if (nAnimBank == 3)
            bank = 16;
    } else {
        bank = 0;
    }

    if (dx == 1)
        idx = bank + (dy + 6);
    else if (dx == -1)
        idx = bank + (dy + 3);
    else if (dx == 0) {
        if (dy == -1)
            idx = bank;             // row 0 (up)
        else if (dy == 0 || dy == 1)
            idx = bank + 1;         // down
    }

    if (frames[idx] < 0) {
        currentFrame = frames[1];
        return paTiles->GetAt(currentFrame);   // same-lvalue load: forwards from the store (AX)
    }
    currentFrame = frames[idx];
    return paTiles->GetAt(frames[idx]);        // reload: the currentFrame store may alias frames[]
}

// FUNCTION: YODA 0x00404910
// Projectile sprite for a firing direction: rows 3/6/0/1 (left/right/up/down) in the mode-0
// case, or the bank-1/bank-2 cells selected by (a, d) otherwise.
void *Character::GetProjectileTile(int a, int dx, int dy, int d, CObArray *paTiles)
{
    if (dx == -1) {
        if (d == 0) {
            if (frames[3] < 0)
                return 0;
            return paTiles->GetAt(frames[3]);
        }
        if (d == 1) {
            if (a == 1) {
            if (frames[0xb] < 0)
                return 0;
            return paTiles->GetAt(frames[0xb]);
            }
            if (a == 2) {
            if (frames[0x13] < 0)
                return 0;
            return paTiles->GetAt(frames[0x13]);
            }
        }
        else if (d == 2) {
            if (a == 1) {
            if (frames[0xd] < 0)
                return 0;
            return paTiles->GetAt(frames[0xd]);
            }
            if (a == 2) {
            if (frames[0x15] < 0)
                return 0;
            return paTiles->GetAt(frames[0x15]);
            }
        }
    }
    else if (dx == 1) {
        if (d == 0) {
            if (frames[6] < 0)
                return 0;
            return paTiles->GetAt(frames[6]);
        }
        if (d == 1) {
            if (a == 1) {
            if (frames[0xe] < 0)
                return 0;
            return paTiles->GetAt(frames[0xe]);
            }
            if (a == 2) {
            if (frames[0x16] < 0)
                return 0;
            return paTiles->GetAt(frames[0x16]);
            }
        }
        else if (d == 2) {
            if (a == 1) {
            if (frames[0xf] < 0)
                return 0;
            return paTiles->GetAt(frames[0xf]);
            }
            if (a == 2) {
            if (frames[0x17] < 0)
                return 0;
            return paTiles->GetAt(frames[0x17]);
            }
        }
    }
    else if (dy == -1) {
        if (d == 0) {
            if (frames[0] < 0)
                return 0;
            return paTiles->GetAt(frames[0]);
        }
        if (d == 1) {
            if (a == 1) {
            if (frames[8] < 0)
                return 0;
            return paTiles->GetAt(frames[8]);
            }
            if (a == 2) {
            if (frames[0x10] < 0)
                return 0;
            return paTiles->GetAt(frames[0x10]);
            }
        }
        else if (d == 2) {
            if (a == 1) {
            if (frames[0xa] < 0)
                return 0;
            return paTiles->GetAt(frames[0xa]);
            }
            if (a == 2) {
            if (frames[0x12] < 0)
                return 0;
            return paTiles->GetAt(frames[0x12]);
            }
        }
    }
    else if (dy == 1) {
        if (d == 0) {
            if (frames[1] < 0)
                return 0;
            return paTiles->GetAt(frames[1]);
        }
        if (d == 1) {
            if (a == 1) {
            if (frames[9] < 0)
                return 0;
            return paTiles->GetAt(frames[9]);
            }
            if (a == 2) {
            if (frames[0x11] < 0)
                return 0;
            return paTiles->GetAt(frames[0x11]);
            }
        }
        else if (d == 2) {
            if (a == 1) {
            if (frames[0xc] < 0)
                return 0;
            return paTiles->GetAt(frames[0xc]);
            }
            if (a == 2) {
            if (frames[0x14] < 0)
                return 0;
            return paTiles->GetAt(frames[0x14]);
            }
        }
    }
    return 0;
}

// ============================== MapEntity ==============================

// FUNCTION: YODA 0x00404c80  [DIFF(73) pending v2: per-value-register stream reconstruction proves
//   this source order is the original's (zero/-1/one streams each match emission order exactly);
//   only the SCHEDULE differs (vtable-store slotting) => TU-position state. Expect this to snap to
//   MATCH once the functions between Puzzle::Ctor and here (Puzzle::Read, Character::Read,
//   GetProjectileTile) are present. v2 UPDATE: full preceding TU present, still DIFF(73) —
//   scheduling tie-break like Puzzle::Ctor; parked as effective match.]
MapEntity::MapEntity()
{
    damageTaken = 0;
    charId = -1;
    x = -1;
    y = -1;
    active = 1;
    unk10 = 0;
    bulletX = 0;
    unk18 = 0;
    bulletY = 0;
    unk20 = 0;
    bulletDX = 0;
    bRetreating = 0;
    bulletDY = 0;
    unk2c = 0;
    bulletStep = 0;
    aiStepCounter = 0;
    bRefreshFrame = 1;
    seqIdx = 0;
    timer = 0;
    wanderDir = 1;
    unk60 = 0;
}

// FUNCTION: YODA 0x00404d30  (compiler-generated scalar-deleting destructor ??_GMapEntity)

// FUNCTION: YODA 0x00404d50
MapEntity::~MapEntity()
{
}

// ============================== Tile ==============================

// FUNCTION: YODA 0x00404da0  [EFFECTIVE MATCH: DIFF(22) — was byte-exact until the
//   DamageEntityAt/HitEntityAt declarations were added to Zone's class decl (required for the TU);
//   header-decl state perturbs this ctor's scheduling. The same additions moved FindObjectAt 7->2.
//   One coupled allocator system — resolve jointly at the TU endgame, don't whack-a-mole.]
Tile::Tile()
{
    flags = 0;
    name = "";
}

// FUNCTION: YODA 0x00404e40  (compiler-generated scalar-deleting destructor ??_GTile)

// FUNCTION: YODA 0x00404e60
Tile::~Tile()
{
}

// ============================== ZoneObj ==============================

// FUNCTION: YODA 0x00404ed0
ZoneObj::ZoneObj()
{
    y = 0;
    x = 0;
    state = 0;
    type = 0;
    arg = -1;
}

// FUNCTION: YODA 0x00404f40  (compiler-generated scalar-deleting destructor ??_GZoneObj)

// FUNCTION: YODA 0x00404f60
ZoneObj::ZoneObj(unsigned int t, unsigned short px, unsigned short py)
{
    type = t;
    x = px;
    y = py;
    arg = -1;
}

// FUNCTION: YODA 0x00404fe0
void ZoneObj::Read(CFile *pFile)
{
    unsigned short buf6[3];
    unsigned short buf2;
    pFile->Read(&type, 4);
    pFile->Read(buf6, 6);
    x = buf6[0];
    y = buf6[1];
    state = buf6[2];
    switch (type) {
    case 0: case 1: case 2: case 5:
        pFile->Read(&buf2, 2); state = 0; arg = buf2; break;
    case 3: case 4: case 9: case 12: case 14: case 15:
        pFile->Read(&buf2, 2); state = 1; arg = buf2; break;
    case 6: case 7: case 8:
        pFile->Read(&buf2, 2); state = 1; arg = buf2; break;
    default:
        pFile->Read(&buf2, 2); arg = 0xffff; state = 1; break;
    }
}

// FUNCTION: YODA 0x00405100
ZoneObj::~ZoneObj()
{
}

// ============================== Zone ==============================

// FUNCTION: YODA 0x00405150
Zone::Zone(short w, short h)
{
    activatedFlag = 0;
    width = w;
    height = h;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++) {
            tiles[(i * 18 + j) * 3 + 0] = -1;
            tiles[(i * 18 + j) * 3 + 1] = -1;
            tiles[(i * 18 + j) * 3 + 2] = -1;
        }
    type = 1;
    tempVar = 0;
    globalVar = -1;
    randVar = -1;
    planet = -1;
}

// FUNCTION: YODA 0x00405300  (compiler-generated scalar-deleting destructor ??_GZone -- MATCH, no source)

// FUNCTION: YODA 0x00405330  [EFFECTIVE MATCH: DIFF(13). PERMUTER-CONFIRMED (2026-07-05) as pure
//   register allocation: asmscore drops to align=0 (instructions 1:1 identical), residual is a clean
//   3-register rotation (ecx/edx/esi for walk-ptr/x/obj -- orig reuses the dead `this` in ECX for the
//   walk, mine for the obj) + loop guard `test edi,edi` vs `cmp edi,eax` + counter-cmp operand order.
//   No stmt/decl/cmp lever reaches it (one leading decl). Semantically identical.]
ZoneObj *Zone::FindObjectAt(int x, int y)
{
    ZoneObj *result = 0;
    for (int i = 0; i < objects.GetSize(); i++) {
        ZoneObj *obj = (ZoneObj *)objects[i];
        if (obj->x == x && obj->y == y && obj->state == 1) {
            result = obj;
            break;
        }
    }
    return result;
}

// FUNCTION: YODA 0x00405380
int Zone::GetEdgeCode(int x, int y)
{
    if (x >= 0 && x < width && y >= 0 && y < height) return 0;
    if (type == 8) return 1;
    if (x < 0 && y >= 0 && y < height) return 4;
    if (x >= width && y >= 0 && y < height) return 5;
    if (y < 0 && x >= 0 && x < width) return 2;
    if (y >= height && x >= 0 && x < width) return 3;
    return 99;
}

// FUNCTION: YODA 0x00405430
unsigned short Zone::GetTile(int x, int y, int layer)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        return (unsigned short)tiles[(y * 18 + x) * 3 + layer];
    return 0xffff;
}

// FUNCTION: YODA 0x00405480
void Zone::SetTile(int x, int y, int layer, short val)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        tiles[(y * 18 + x) * 3 + layer] = val;
}

// FUNCTION: YODA 0x004054d0  [EFFECTIVE MATCH: DIFF(12) on 506 bytes -- pure ESI<->EDI reg-alloc
//   (count vs offset register) in the 3 element-deletion loops; the objects loop matches, iact/
//   entities drew the opposite phase. Structurally identical.]
// Destructor: delete the CObject* elements of objects/iactScripts/entities (virtual dtor via delete),
// SetSize(0,-1) each, then SetSize the 4 CDWordArray scratch lists; members auto-destruct after.
Zone::~Zone()
{
    int i, n;
    n = objects.GetSize();
    for (i = 0; i < n; i++) { CObject *p = objects[i]; if (p) delete p; }
    objects.SetSize(0, -1);
    n = iactScripts.GetSize();
    for (i = 0; i < n; i++) { CObject *p = iactScripts[i]; if (p) delete p; }
    iactScripts.SetSize(0, -1);
    n = entities.GetSize();
    for (i = 0; i < n; i++) { CObject *p = entities[i]; if (p) delete p; }
    entities.SetSize(0, -1);
    providedItemsA.SetSize(0, -1);
    providedItemsB.SetSize(0, -1);
    genCandidateA.SetSize(0, -1);
    genCandidateB.SetSize(0, -1);
}

// FUNCTION: YODA 0x004056d0
// Marks every "quest" object (ObjType 6..10 = item/npc/weapon/door-in/door-out) active (state=1).
void Zone::FlagQuestObjects()
{
    int n = objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObj *obj = (ZoneObj *)objects[i];
        if (obj->type >= 6 && obj->type <= 10)
            obj->state = 1;
    }
}

// FUNCTION: YODA 0x00405710  [EFFECTIVE MATCH: structure fully recovered — 229/230 insns, 132
//   identical; the residual is one `this`-reload (our allocator puts n in ECX, killing this) plus
//   the ECX<->EDX role swap cascading through the body. True length 690 vs ours 696. All source
//   shapes tried (n/nChars order, id-local, shared-return-1 nesting, drop=int, single no-var);
//   allocator tie-break — same parked class as Puzzle/MapEntity ctors.]
// Apply weapon damage to the entity at (x,y). On kill: clear the projectile tile, then drop
// the carried item (or the zone's quest item) as a new type-6 ZoneObj on layer 1.
int Zone::DamageEntityAt(int x, int y, CObArray *paChars, short damage, CDeskcppDoc *pWorld, CDeskcppView *pView)
{
    int n = entities.GetSize();
    int nChars = paChars->GetSize();
    for (int i = 0; i < n; i++) {
        MapEntity *e = (MapEntity *)entities[i];
        if (e && e->active != 0 && e->x == x && e->y == y) {
            int id = e->charId;
            if (nChars > id && id >= 0) {
            Character *ch = (Character *)paChars->GetAt(id);
            if (ch->health != -1) {
                e->damageTaken = e->damageTaken + damage;
                if (ch->health <= e->damageTaken) {
                    pView->PlaySound(5);
                    short w = pWorld->characters[e->charId]->weaponCharId;
                    if (w >= 0) {
                        void *t = pWorld->characters[w]->GetProjectileTile(0, e->bulletDX, e->bulletDY, 0, &pWorld->tiles);
                        int ti = pWorld->FindTile(t);
                        if ((short)GetTile(e->bulletX, e->bulletY, 1) == ti) {
                            SetTile(e->bulletX, e->bulletY, 1, -1);
                            pView->DrawZoneCell(e->bulletX, e->bulletY);
                        }
                    }
                    e->charId = -1;
                    int drop = 0;
                    ZoneObj *no;
                    if (e->numItems != 0) {
                        if (e->item == 0) {
                            int m = objects.GetSize();
                            for (int j = 0; j < m; j++) {
                                ZoneObj *o = (ZoneObj *)objects[j];
                                if (o->type == 0 && (short)o->arg > 0) {
                                    drop = o->arg;   // movsx: short -> int
                                    break;
                                }
                            }
                            if (drop > 0) {
                                no = new ZoneObj(6, (unsigned short)x, (unsigned short)y);
                                no->arg = drop;
                                no->state = 1;
                                objects.SetAtGrow(objects.GetSize(), no);
                                SetTile(x, y, 1, drop);
                                pView->DrawZoneCell((short)x, (short)y);
                            }
                        } else {
                            no = new ZoneObj(6, (unsigned short)x, (unsigned short)y);
                            no->arg = e->item - 1;
                            no->state = 1;
                            objects.SetAtGrow(objects.GetSize(), no);
                            SetTile(x, y, 1, e->item - 1);
                            pView->DrawZoneCell((short)x, (short)y);
                        }
                    }
                    return 1;
                }
            }
            break;
            }
        }
    }
    return 0;
}

// FUNCTION: YODA 0x004059d0  [EFFECTIVE MATCH: DIFF(20) at exact length (266) — i/nChars/scratch
//   register 3-cycle + one cmp-operand flip + one movsx pair order. Permuter-confirmed (2 runs,
//   stmt+cmp) not source-reachable. Cracked to this point by: nested `int id = e->charId` (single
//   movsx serving both range tests), int timerVal, SetTile(short val) => push -1.]
// Non-lethal hit: stun the entity at (x,y) (timer), and clear its in-flight projectile tile.
int Zone::HitEntityAt(int x, int y, CObArray *paChars, int timerVal, CDeskcppDoc *pWorld, CDeskcppView *pView)
{
    int n = entities.GetSize();
    int nChars = paChars->GetSize();
    for (int i = 0; i < n; i++) {
        MapEntity *e = (MapEntity *)entities[i];
        if (e && e->active != 0 && e->x == x && e->y == y) {
            int id = e->charId;
            if (nChars > id && id >= 0) {
            e->timer = (short)timerVal;
            pView->PlaySound(5);
            short w = pWorld->characters[e->charId]->weaponCharId;
            if (w >= 0) {
                void *t = pWorld->characters[w]->GetProjectileTile(0, e->bulletDX, e->bulletDY, 0, &pWorld->tiles);
                int ti = pWorld->FindTile(t);
                if ((short)GetTile(e->bulletX, e->bulletY, 1) == ti) {
                    SetTile(e->bulletX, e->bulletY, 1, -1);
                    pView->DrawZoneCell(e->bulletX, e->bulletY);
                }
                return 1;
            }
            return 0;
            }
        }
    }
    return 0;
}
