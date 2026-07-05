// Records — DTA record-classes TU (see Records.h). Flags: /nologo /c /MT /W3 /GX /O2
//   /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
// Functions in .text/source order. v1 = ctors/dtors + Character frame lookups.
// TODO (v2): Puzzle::Read (0x404480), Character::Read (0x4047a0),
//            Character::GetProjectileTile (0x404910); then fold in ZoneObj.cpp + Zone.cpp
//            (same TU!) and retire those files — full-TU context may fix Zone's 3 residuals.
#include "Records.h"
#include <string.h>
#pragma intrinsic(memset, memcpy)

// ============================== Puzzle ==============================

// FUNCTION: YODA 0x004042b0
Puzzle::Puzzle()
{
    unk14 = 0;
    itemA = -1;
    itemB = -1;
    unk3 = 0;
    unk2 = 0;
    unk1 = 0;
}

// FUNCTION: YODA 0x004043a0  (compiler-generated scalar-deleting destructor ??_GPuzzle)

// FUNCTION: YODA 0x004043c0
Puzzle::~Puzzle()
{
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
void Character::Init(short a, short mt, short c, int d)
{
    memset(frames, 0xff, sizeof(frames));
    unk34 = a;
    unk38 = -1;   // after unk34=a (EAX now holds a, not the fill's 0xffffffff) -> immediate
                  // store, which the scheduler hoists to just below the rep stosd.
    moveType = mt;
    unk3a = 1;
    damage = 1;
    unk40 = c;
    unk44 = d;
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

// TODO v2: Character::Read (0x004047a0), Character::GetProjectileTile (0x00404910)

// ============================== MapEntity ==============================

// FUNCTION: YODA 0x00404c80
MapEntity::MapEntity()
{
    damageTaken = 0;
    charId = -1;
    x = -1;
    y = -1;
    active = 1;
    unk10 = 0;
    homeX = 0;
    unk18 = 0;
    homeY = 0;
    unk20 = 0;
    dx = 0;
    unk1c = 0;
    dy = 0;
    unk2c = 0;
    animFlag = 0;
    unk16 = 0;
    unk34 = 1;
    unk3e = 0;
    timer = 0;
    unk30 = 1;
    unk60 = 0;
}

// FUNCTION: YODA 0x00404d30  (compiler-generated scalar-deleting destructor ??_GMapEntity)

// FUNCTION: YODA 0x00404d50
MapEntity::~MapEntity()
{
}

// ============================== Tile ==============================

// FUNCTION: YODA 0x00404da0
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
