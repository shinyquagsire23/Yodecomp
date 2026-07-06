// Records — the DTA record-classes TU (0x4042b0–0x405ae0, one .obj):
//   Puzzle (vtable 0x44b148) · Character (0x44b160) · MapEntity (0x44b178) · Tile (0x44b190)
//   · ZoneObj (0x44b1a8) · Zone (0x44b1c0)
// All CObject-derived (consecutive vtables in one .rdata cluster prove the single TU).
// Layouts pinned from the ctor/Init/accessor disassembly — see docs/structs.md.
// NOTE: unkNN field names are physical placeholders; a naming sweep runs in parallel —
// rename here when it lands (names don't affect the byte-match).
#ifndef RECORDS_H
#define RECORDS_H
#include <afxwin.h>
#include <afxcoll.h>

// PUZ2 record (0x2c). 5 CStrings + item ids.
class Puzzle : public CObject
{
public:                              // +0x00 CObject vtable (0x44b148)
    int      nType;                  // +0x04  ctor: 0; 0=transaction 1=trade 2=goal-prize 3=world-mission-goal
    int      unk2;                   // +0x08  ctor: 0
    int      unk3;                   // +0x0c  ctor: 0
    short    itemA;                  // +0x10  ctor: -1
    short    itemB;                  // +0x12  ctor: -1
    short    unk14;                  // +0x14  ctor: 0
    char     _pad16[2];              // +0x16
    CString  text1;                  // +0x18
    CString  text2;                  // +0x1c
    CString  text3;                  // +0x20
    CString  text4;                  // +0x24
    CString  text5;                  // +0x28

    Puzzle();                                            // 0x004042b0
    virtual ~Puzzle();                                   // 0x004043c0 (ScalarDtor 0x004043a0)
    void Read(CFile *pFile);                             // 0x00404480
};

// CHAR record (0x4c). In-memory layout != file layout (name is read to stack, not stored).
class Character : public CObject
{
public:                              // +0x00 CObject vtable (0x44b160)
    short    frames[24];             // +0x04  3 anim banks x 8 facing dirs; -1 = none
    short    typeFlags;              // +0x34  ICHR flags low word: 1=hero 2=enemy 4=weapon
    short    moveType;               // +0x36  CharMoveType (enemy-AI switch in GameView::Tick)
    short    weaponCharId;           // +0x38  ctor via Init: -1 (none); filled by ParseChwp
    short    health;                 // +0x3a  Init: 1; filled by ParseChwp (chwp_entry)
    short    currentFrame;           // +0x3c  cached by GetFrameTile
    short    damage;                 // +0x3e  Init default: 1
    short    unk40;                  // +0x40  ICHR unk_4 (parse-only, no runtime reader)
    char     _pad42[2];              // +0x42
    int      unk44;                  // +0x44  ICHR unk_5 (parse-only); ctor 0
    short    unk48;                  // +0x48  ctor: 0
    char     _pad4a[2];              // +0x4a

    Character();                                         // 0x00404670
    virtual ~Character();                                // 0x00404700 (ScalarDtor 0x004046e0)
    void  Init(short nTypeFlags, short nMoveType, short nUnk40, int nUnk44); // 0x00404750
    void  Read(CFile *pFile);                            // 0x004047a0
    void *GetWalkFrameTile(int dx, int dy, CObArray *paTiles);            // 0x00404830
    void *GetFrameTile(int dx, int dy, CObArray *paTiles, int nAnimBank); // 0x00404850
    void *GetProjectileTile(int a, int dx, int dy, int d, CObArray *paTiles); // 0x00404910
};

// Placed entity on the map (0x64). Semantic names provisional (verification sweep in flight).
class MapEntity : public CObject
{
public:                              // +0x00 CObject vtable (0x44b178)
    short    charId;                 // +0x04  ctor: -1
    short    x;                      // +0x06  ctor: -1
    short    y;                      // +0x08  ctor: -1
    short    damageTaken;            // +0x0a  ctor: 0
    int      active;                 // +0x0c  ctor: 1
    short    unk10;                  // +0x10  ctor: 0
    short    bulletX;                // +0x12  ctor: 0; projectile pos (layer 1)
    short    bulletY;                // +0x14  ctor: 0
    short    aiStepCounter;          // +0x16  ctor: 0
    int      unk18;                  // +0x18  ctor: 0
    int      bRetreating;            // +0x1c  ctor: 0
    int      unk20;                  // +0x20  ctor: 0
    short    timer;                  // +0x24  ctor: 0
    unsigned short item;             // +0x26
    unsigned short numItems;         // +0x28
    short    unk2a;                  // +0x2a  IZAX unk3
    int      unk2c;                  // +0x2c  ctor: 0
    short    wanderDir;              // +0x30  ctor: 1 (-1..2 dir code)
    char     _pad32[2];              // +0x32
    int      bRefreshFrame;          // +0x34  ctor: 1
    short    bulletDX;               // +0x38  ctor: 0
    short    bulletDY;               // +0x3a  ctor: 0
    short    bulletStep;             // +0x3c  ctor: 0 (travel counter, <4 = range)
    short    seqIdx;                 // +0x3e  ctor: 0 (waypoint/anim seq idx)
    int      waypoints[8];           // +0x40  4 patrol (x,y) pairs from IZAX tail
    short    unk60;                  // +0x60  ctor: 0
    char     _pad62[2];              // +0x62

    MapEntity();                                         // 0x00404c80
    virtual ~MapEntity();                                // 0x00404d50 (ScalarDtor 0x00404d30)
};

// TILE record (0x40c): 32x32 8-bit pixels + flags + name.
class Tile : public CObject
{
public:                              // +0x00 CObject vtable (0x44b190)
    unsigned char pixels[0x400];     // +0x004  32x32 8-bpp
    unsigned int  flags;             // +0x404  ctor: 0
    CString       name;              // +0x408  ctor: ""

    Tile();                                              // 0x00404da0
    virtual ~Tile();                                     // 0x00404e60 (ScalarDtor 0x00404e40)
};

#endif
