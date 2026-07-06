// World stub for the GameData TU (the second doc-TU source file, 0x401ac0–0x4042b0).
// Field layout mirrors the Ghidra `World` struct (the source of truth); pads keep exact offsets.
// This stub ACCRETES toward the real shared World header as more TUs match (see roadmap).
// NOTE: only fields this TU touches are named; everything else is _pad. sizeof == 0x33c0.
#ifndef GAMEDATA_WORLDSTUB_H
#define GAMEDATA_WORLDSTUB_H
#include <afxwin.h>
#include <afxcoll.h>

class Zone;      // full defs live in src/Records/Records.h; opaque here except stubs below
class Tile;
class Character;
class Puzzle;

// Minimal Zone stub: GameData only reads type@+4.
class ZoneStub
{
public:
    void *vftable;               // +0x00
    int   type;                  // +0x04  ZoneType (1=Empty ... 8=Room ... 16=Find)
};

// A 10x10 world-map grid cell (0x34 bytes).
struct MapZone
{
    short id;                    // +0x00  zone id; -1 = empty cell
    char  _pad02[6];             // +0x02
    short cellQuestSlot0;        // +0x08
    short cellQuestSlot1;        // +0x0a
    short cellItemA;             // +0x0c
    short cellItemB;             // +0x0e
    short cellItemC;             // +0x10
    short cellQuestSlot5;        // +0x12
    short cellQuestSlot6;        // +0x14
    char  _pad16[2];             // +0x16
    int   flagSolved;            // +0x18
    int   flagC;                 // +0x1c
    int   flagA;                 // +0x20
    int   flagB;                 // +0x24
    int   flagD;                 // +0x28
    short field2c;               // +0x2c
    char  _pad2e[6];             // +0x2e
};                               // sizeof 0x34

class World
{
public:
    char        _pad00[0x58];            // +0x0000  (vtbl + CDocument innards)
    int         totalZones;              // +0x0058
    int         nFrameMode;              // +0x005c
    int         nMapChangeReason;        // +0x0060
    int         abortFrame;              // +0x0064
    int         gameState;               // +0x0068  -1=in progress, 1=won
    int         nRequestedGoalItem;      // +0x006c
    int         score;                   // +0x0070
    char        _pad74[4];               // +0x0074
    int         timeBase;                // +0x0078
    int         timeOffset;              // +0x007c
    int         unk80;                   // +0x0080
    Tile      **tileArray;               // +0x0084
    int         tileCount;               // +0x0088
    char        _pad8c[0xc];             // +0x008c
    ZoneStub  **zoneObjects;             // +0x0098
    int         zoneCount;               // +0x009c
    char        _pada0[0x8];             // +0x00a0
    void       *pInvArrayVtbl;           // +0x00a8
    void      **inventory;               // +0x00ac
    int         inventoryCount;          // +0x00b0
    char        _padb4[0xc];             // +0x00b4
    Character **characters;              // +0x00c0
    char        _padc4[0xc];             // +0x00c4
    Puzzle    **puzzles;                 // +0x00d0
    int         unkd4;                   // +0x00d4
    int         puzzleCount;             // +0x00d8
    char        _paddc[0x8];             // +0x00dc
    char       *soundNames;              // +0x00e4
    char        _pade8[0xfc];            // +0x00e8
    CWordArray  questItemsA;             // +0x01e4
    CWordArray  questItemsB;             // +0x01f8
    CWordArray  goalTileList;            // +0x020c
    CWordArray  placedZoneIds;           // +0x0220
    CWordArray  unk234;                  // +0x0234
    char        _pad248[0x3c];           // +0x0248
    CWordArray  storyHistoryNevada;      // +0x0284  planet 1 = Tatooine (Indy-engine key name)
    CWordArray  storyHistoryAlaska;      // +0x0298  planet 2 = Hoth (the demo planet)
    CWordArray  storyHistoryOregon;      // +0x02ac  planet 3 = Endor
    ZoneStub   *currentZone;             // +0x02c0
    char       *palette;                 // +0x02c4
    char        _pad2c8[4];              // +0x02c8
    int         worldSeed;               // +0x02cc
    char        _pad2d0[0x1e4];          // +0x02d0
    MapZone     zones[200];              // +0x04b4  [0..99] active 10x10 grid, [100..199] the
                                         //          backup grid (RestoreGridFromBackup copies
                                         //          100..199 -> 0..99); ends +0x2d54
    char        _pad2d54[0xcc];          // +0x2d54
    int         playerX;                 // +0x2e20
    int         playerY;                 // +0x2e24
    char        _pad2e28[4];             // +0x2e28
    Character  *currentWeapon;           // +0x2e2c
    char        _pad2e30[0xc];           // +0x2e30
    int         currentPlanet;           // +0x2e3c
    char        _pad2e40[0x580];         // +0x2e40
    // 0x33c0 total

    // ---- this TU's methods (v1 set; markers in GameData.cpp) ----
    void Nop1();                                          // 0x00402660  empty (compiled-out debug hook)
    void Nop2();                                          // 0x00403060  empty
    int  FindZoneCellById(short id, int *pX, int *pY);    // 0x00403250
    unsigned char GetExitDirections();                    // 0x004032c0  W=8 E=4 N=1 S=2 (returned in AL)
    void OnUpdateFileSave(CCmdUI *pCmdUI);                // 0x00403510  demo: grayed
    void OnUpdateAppExit(CCmdUI *pCmdUI);                 // 0x00403520
    void OnUpdateHideMe(CCmdUI *pCmdUI);                  // 0x00403550
    void OnUpdateNewWorld(CCmdUI *pCmdUI);                // 0x00403580
    void OnUpdateLoadWorld(CCmdUI *pCmdUI);               // 0x00403600  demo: grayed
    void OnUpdateReplayStory(CCmdUI *pCmdUI);             // 0x00403610  demo: grayed
    Tile *GetTileData(int idx);                           // 0x00403a40
    ZoneStub *GetZoneById(short id);                      // 0x00403a70
    int  FindTile(void *pTile);                           // 0x00403aa0
};

#endif
