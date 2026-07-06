// RecordClasses — the six byte-match-proven DTA record classes from the Records TU
// (0x4042b0–0x405ae0, src/Records/Records.cpp): Puzzle, Character, MapEntity, Tile,
// ZoneObj, Zone. Shared by every TU that touches records (Records, GameData, ...).
// Layouts are byte-match-proven — do NOT edit offsets without re-verifying Records.cpp.
#ifndef RECORDCLASSES_H
#define RECORDCLASSES_H
#include <afxwin.h>
#include <afxcoll.h>

class Character;
class World;
class GameView;

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
    int      numItems;               // +0x28  dword (DamageEntityAt tests ==0 as int; IZAX numItems+unk3)
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


// A placed object / hotspot in a zone (0x10 bytes).
class ZoneObj : public CObject
{
public:                          // +0x00  CObject vtable (0x44b1a8)
    unsigned int   type;         // +0x04  ObjType category
    short          state;        // +0x08  ==1 active/placed
    short          x;            // +0x0a
    short          y;            // +0x0c
    short          visible;      // +0x0e  (0xffff default)

    ZoneObj();                                                    // 0x00404ed0  default
    ZoneObj(unsigned int type, unsigned short x, unsigned short y);// 0x00404f60  spawn(type,x,y)
    virtual ~ZoneObj();                                           // 0x00405100
    void Read(CFile *pFile);                                      // 0x00404fe0  deserialize
};

// The 18x18 map zone (0x848 bytes; ctor 0x405150, vtable 0x44b1c0).
class Zone : public CObject
{
public:                              // +0x00 = CObject vtable
    int            type;             // +0x04  flags/areaType dword (== 8 => special/indoor)
    int            activatedFlag;    // +0x08
    short          width;            // +0x0c  (18)
    short          height;           // +0x0e  (18)
    short          tiles[18 * 18 * 3];// +0x10  flat grid (0x798 bytes, ends +0x7a8)
    CObArray       objects;          // +0x7a8
    CObArray       iactScripts;      // +0x7bc
    CObArray       entities;         // +0x7d0
    CWordArray     cobArray4;        // +0x7e4  PROVEN CWordArray (ReadZaux: CWordArray::SetAtGrow(ushort))
    CWordArray     cobArray5;        // +0x7f8  same — element type WORD, layout identical to CDWordArray
    CWordArray     genCandidateA;    // +0x80c  ALSO CWordArray (ReadZax2: CWordArray::SetAtGrow(ushort))
    CWordArray     genCandidateB;    // +0x820  same (ReadZax3)
    int            tempVar;          // +0x834
    int            randVar;          // +0x838
    int            zoneUnk83c;       // +0x83c  saved-state dword (ReadSavedState)
    int            zoneUnk840;       // +0x840  saved-state dword
    short          globalVar;        // +0x844
    short          zoneUnk846;       // +0x846

    Zone(short w = 18, short h = 18);                       // 0x00405150
    virtual ~Zone();                                        // 0x004054d0
    unsigned short GetTile(int x, int y, int layer);        // 0x00405430  MATCH
    void           SetTile(int x, int y, int layer, short val); // 0x00405480  MATCH
    int            GetEdgeCode(int x, int y);               // 0x00405380
    ZoneObj       *FindObjectAt(int x, int y);              // 0x00405330
    void           FlagQuestObjects();                      // 0x004056d0
    int            DamageEntityAt(int x, int y, CObArray *paChars, short damage,
                                  World *pWorld, GameView *pView);       // 0x00405710
    int            HitEntityAt(int x, int y, CObArray *paChars, int timerVal,
                               World *pWorld, GameView *pView);          // 0x004059d0
    void           ReadSavedState(CFile *pFile, int bFull);              // 0x00405bd0 (Iact .obj)
    void           WriteSavedState(CFile *pFile, int bFull);             // 0x00405f30 (Iact .obj)
    // Iact .obj (src/Iact/Iact.cpp) — .dta chunk readers + the IACT interpreter:
    void           ReadIzon(CFile *pFile);                               // 0x00405ae0 (Iact .obj)
    void           ReadZaux(CFile *pFile);                               // 0x00406270 (Iact .obj)
    void           ReadZax2(CFile *pFile);                               // 0x00406410 (Iact .obj)
    void           ReadZax3(CFile *pFile);                               // 0x00406490 (Iact .obj)
    void           ReadZax4(CFile *pFile);                               // 0x00406510 (Iact .obj, this unused)
    int            IactProbeMove(int x, int y, int dx, int dy, int a5, int bForce); // 0x00406550 (Iact .obj)
    int            IactRun(int event, int x, int y, int dx, int dy, int a5,
                           CDC *pDC, World *pWorld, GameView *pView);              // 0x00406780 (Iact .obj)
    unsigned int   IactRunCommands(int scriptIdx, CDC *pDC, World *pWorld,
                                   GameView *pView);                               // 0x004070e0 (Iact .obj)
};

#endif
