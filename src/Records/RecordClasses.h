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
// Puzzle.nType — the puzzle's worldgen class (WorldgenSelectPuzzle maps zone types onto these:
// ZONE_TYPE_FIND_USEFUL_DROP->TRANSACTION, MAP_TO_ITEM_FOR_LOCK->TRADE, FINAL_ITEM->GOAL_PRIZE,
// and the 9999 request picks a WORLD_MISSION goal).
enum PuzzleType
{
    PUZZLE_TYPE_TRANSACTION   = 0,
    PUZZLE_TYPE_TRADE         = 1,
    PUZZLE_TYPE_GOAL_PRIZE    = 2,
    PUZZLE_TYPE_WORLD_MISSION = 3,
};

class Puzzle : public CObject
{
public:                              // +0x00 CObject vtable (0x44b148)
    int      nType;                  // +0x04  ctor: 0; a PuzzleType value (int: Records TU byte-matched)
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

// Tile.flags bits (names from ~/workspace/DesktopAdventures src/include/tile.h).
// Bits 16+ are subtype bits whose meaning depends on the category bit (WEAPON/ITEM/CHARACTER).
enum TileFlags
{
    TILE_GAME_OBJECT                = 1 << 0,
    TILE_UNDER_PLAYER_NONCOLLIDING  = 1 << 1,
    TILE_MIDDLE_LAYER_COLLIDING     = 1 << 2,
    TILE_PUSH_PULL_BLOCK            = 1 << 3,
    TILE_ABOVE_PLAYER_NONCOLLIDING  = 1 << 4,
    TILE_MINI_MAP_TILE              = 1 << 5,
    TILE_WEAPON                     = 1 << 6,
    TILE_ITEM                       = 1 << 7,
    TILE_CHARACTER                  = 1 << 8,
    // WEAPON subtypes                        // ITEM subtypes            // CHARACTER subtypes
    TILE_LIGHT_BLASTER              = 1 << 16, // TILE_KEYCARD            // TILE_PLAYER
    TILE_HEAVY_BLASTER              = 1 << 17, // TILE_PUZZLE_ITEM_1      // TILE_ENEMY
    TILE_LIGHTSABER                 = 1 << 18, // TILE_PUZZLE_ITEM_2      // TILE_FRIENDLY
    TILE_THE_FORCE                  = 1 << 19, // TILE_PUZZLE_ITEM_SEED_END
    TILE_LOCATOR                    = 1 << 20, // (ITEM group)
    TILE_HEALTH_PACK                = 1 << 22, // (ITEM group)
};

// ZoneObj.type (names = DesktopAdventures OBJ_TYPE, src/include/objectinfo.h).
enum ZoneObjType
{
    OBJ_QUEST_ITEM_SPOT = 0,
    OBJ_SPAWN           = 1,
    OBJ_THE_FORCE       = 2,
    OBJ_VEHICLE_TO      = 3,
    OBJ_VEHICLE_FROM    = 4,
    OBJ_LOCATOR         = 5,
    OBJ_ITEM            = 6,
    OBJ_PUZZLE_NPC      = 7,
    OBJ_WEAPON          = 8,
    OBJ_DOOR_IN         = 9,
    OBJ_DOOR_OUT        = 10,
    OBJ_UNKNOWN         = 11,
    OBJ_LOCK            = 12,
    OBJ_TELEPORTER      = 13,
    OBJ_XWING_FROM      = 14,
    OBJ_XWING_TO        = 15,
};

// Zone.type — the zone's worldgen role (DesktopAdventures "map_flags").
enum ZoneType
{
    ZONE_TYPE_EMPTY             = 0,
    ZONE_TYPE_ENEMY_TERRITORY   = 1,
    ZONE_TYPE_FINAL_DESTINATION = 2,
    ZONE_TYPE_ITEM_FOR_ITEM     = 3,
    ZONE_TYPE_FIND_USEFUL_NPC   = 4,
    ZONE_TYPE_ITEM_TO_PASS      = 5,
    ZONE_TYPE_FROM_ANOTHER_MAP  = 6,
    ZONE_TYPE_TO_ANOTHER_MAP    = 7,
    ZONE_TYPE_INDOOR            = 8,
    ZONE_TYPE_INTRO             = 9,
    ZONE_TYPE_FINAL_ITEM        = 10,
    ZONE_TYPE_MAP_START         = 11,
    ZONE_TYPE_VICTORY_SCREEN    = 13,   // demo: zones[76] (GetVictoryZoneIndexMaybe)
    ZONE_TYPE_LOSS_SCREEN       = 14,   // demo: zones[77] (GetLossZoneMaybe)
    ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK = 15,
    ZONE_TYPE_FIND_USEFUL_DROP  = 16,
    ZONE_TYPE_FIND_USEFUL_BUILDING = 17,
    ZONE_TYPE_FIND_THE_FORCE    = 18,
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
    short          planet;           // +0x846  planet this zone belongs to (== World.currentPlanet)

    Zone(short w = 18, short h = 18);                       // 0x00405150
    virtual ~Zone();                                        // 0x004054d0
    unsigned short GetTile(int x, int y, int layer);        // 0x00405430  MATCH
    int   DamageEntityAt(int x, int y, CObArray *paChars, int nDmg,
                         World *pWorld, GameView *pView);   // 0x00405710 (Zone TU)
    void  HitEntityAt(int x, int y, CObArray *paChars, short nDmg,
                      World *pWorld, GameView *pView);      // 0x004059d0 (Zone TU)
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
