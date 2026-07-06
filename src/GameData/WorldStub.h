// World stub for the GameData TU (the second doc-TU source file, 0x401ac0–0x4042b0).
// Field layout mirrors the Ghidra `World` struct (the source of truth); pads keep exact offsets.
// This stub ACCRETES toward the real shared World header as more TUs match (see roadmap).
// NOTE: only fields this TU touches are named; everything else is _pad. sizeof == 0x33c0.
#ifndef GAMEDATA_WORLDSTUB_H
#define GAMEDATA_WORLDSTUB_H
#include <afxwin.h>
#include <afxcoll.h>

class Zone;      // full defs live in src/Records/Records.h; opaque here except stubs below
class Character;
class Puzzle;
class GameView;

// TILE record stub (0x40c): pixels + flags is all this TU reads.
class Tile
{
public:
    void         *vftable;           // +0x000
    unsigned char pixels[0x400];     // +0x004
    unsigned int  flags;             // +0x404  TileFlags (bit 0 = TILE_GAME_OBJECT)
};

// Canvas stub: blit entry points (full class in src/Canvas/Canvas.h).
class Canvas
{
public:
    void Clear();                                                          // 0x00408040
    void BlitFast(void *src, int flags, short height,
                  unsigned short srcStride, short destX, short destY);     // 0x00408110
    void BlitMasked(char *src, unsigned short srcStride, short height,
                    short destX, short destY, char key);                   // 0x00408240
};

// GameView stub: fields/methods StartGame touches.
class GameViewStub
{
public:
    char _pad00[0x4c];               // +0x000
    int  bBusy;                      // +0x04c
    char _pad50[0xc4];               // +0x050
    int  nTargetZoneId;              // +0x114
    int  unk118;                     // +0x118

    void OnWalk(int cmd, short n);                   // 0x00409510-ish (walk-in anim step)
    void SoundFlush();                               // sound queue flush
    void PlayerMove(int n);                          // 0x00409060
    void DrawGameArea(CDC *pDC);                     // full redraw
};

// A placed object / hotspot stub (0x10 bytes).
class ZoneObjStub
{
public:
    void        *vftable;        // +0x00
    unsigned int type;           // +0x04  ObjType (9 = OBJ_DOOR_IN, visible = child zone id)
    short        state;          // +0x08
    short        x;              // +0x0a
    short        y;              // +0x0c
    short        visible;        // +0x0e  tile id / child zone id
};

// Zone stub: fields/methods this TU touches (full class in src/Records/Records.h).
class ZoneStub
{
public:
    void       *vftable;         // +0x000
    int         type;            // +0x004  ZoneType (1=Empty ... 8=Room ... 16=Find)
    int         activatedFlag;   // +0x008
    short       width;           // +0x00c  (18)
    short       height;          // +0x00e  (18)
    char        _pad10[0x798];   // +0x010
    CObArray    objects;         // +0x7a8  ZoneObj* elements

    unsigned short GetTile(int x, int y, int layer);            // 0x00405430
    void           SetTile(int x, int y, int layer, short val); // 0x00405480
    void           ReadSavedState(CFile *pFile, int bFull);     // 0x00405bd0
    void           WriteSavedState(CFile *pFile, int bFull);    // 0x00405f30
};

// A 10x10 world-map grid cell (0x34 bytes).
struct MapZone
{
    short id;                    // +0x00  zone id; -1 = empty cell
    char  _pad02[2];             // +0x02
    int   zoneType;              // +0x04  (StartGame clears as dword -1)
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

class World : public CDocument          // sizeof(CDocument) == 0x50 in MFC 4.2
{
public:
    int         unk50;                   // +0x0050  (doc fields before totalZones)
    int         unk54;                   // +0x0054
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
    CObArray    inventory;               // +0x00a8  (vtbl@a8, m_pData@ac, m_nSize@b0)
    char        _padbc[0x4];             // +0x00bc
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
    int         paZonePtrGrid[0x79];     // +0x02d0  (10x10 used; region is 121 ints)
    MapZone     zones[200];              // +0x04b4  [0..99] active 10x10 grid, [100..199] the
                                         //          backup grid (RestoreGridFromBackup copies
                                         //          100..199 -> 0..99); ends +0x2d54
    char        _pad2d54[0xcc];          // +0x2d54
    int         playerX;                 // +0x2e20
    int         playerY;                 // +0x2e24
    char        _pad2e28[4];             // +0x2e28
    Character  *currentWeapon;           // +0x2e2c
    int         unk2e30;                 // +0x2e30
    int         unk2e34;                 // +0x2e34
    short       startItem;               // +0x2e38
    short       startItem2;              // +0x2e3a
    int         currentPlanet;           // +0x2e3c
    int         bStartingGame;           // +0x2e40
    char        _pad2e44[0xc];           // +0x2e44
    int         goalItemTileId;          // +0x2e50
    int         bHidePlayer;             // +0x2e54
    char        _pad2e58[8];             // +0x2e58
    int         unk2e60;                 // +0x2e60
    char        _pad2e64[0x40c];         // +0x2e64
    Canvas     *pCanvas;                 // +0x3270
    char        _pad3274[0x30];          // +0x3274  3 RECTs
    int         nWeaponBoxLeft;          // +0x32a4
    int         nWeaponBoxTop;           // +0x32a8
    int         nWeaponBoxRight;         // +0x32ac
    int         nWeaponBoxBottom;        // +0x32b0
    char        _pad32b4[0x20];          // +0x32b4
    int         nHealthDialLeft;         // +0x32d4
    int         nHealthDialTop;          // +0x32d8
    int         nHealthDialRight;        // +0x32dc
    int         nHealthDialBottom;       // +0x32e0
    int         nArrowBoxLeft;           // +0x32e4
    int         nArrowBoxTop;            // +0x32e8
    int         nArrowBoxRight;          // +0x32ec
    int         nArrowBoxBottom;         // +0x32f0
    int         bWorldReady;             // +0x32f4
    char        _pad32f8[8];             // +0x32f8
    int         nextCameraX;             // +0x3300
    int         nextCameraY;             // +0x3304
    ZoneStub   *pendingZone;             // +0x3308
    int         nSoundEnabled;           // +0x330c
    int         nMusicEnabled;           // +0x3310
    int         healthLo;                // +0x3314
    int         healthHi;                // +0x3318
    int         difficulty;              // +0x331c
    int         counter;                 // +0x3320
    int         gameSpeed;               // +0x3324
    int         worldSize;               // +0x3328
    int         completionCount;         // +0x332c
    int         cameraX;                 // +0x3330
    int         cameraY;                 // +0x3334
    char        _pad3338[0x14];          // +0x3338
    short       weaponState[4];          // +0x334c
    short       nCurrentAmmo;            // +0x3354
    char        _pad3356[2];             // +0x3356
    Tile       *pPlayerFrameTile;        // +0x3358
    Character  *pPlayerChar;             // +0x335c
    char        _pad3360[0x14];          // +0x3360
    int         equippedItem;            // +0x3374
    int         unk3378;                 // +0x3378
    int         bWorldInvalid;           // +0x337c
    char        _pad3380[0x20];          // +0x3380  worldgen cell scratch
    int         nCurrentGoalItem;        // +0x33a0
    char        _pad33a4[0x1c];          // +0x33a4
    // 0x33c0 total

    // ---- this TU's methods (v1 set; markers in GameData.cpp) ----
    void Nop1();                                          // 0x00402660  empty (compiled-out debug hook)
    void Nop2();                                          // 0x00403060  empty
    int  FindZoneCellById(short id, int *pX, int *pY);    // 0x00403250
    unsigned char GetExitDirections();                    // 0x004032c0  W=8 E=4 N=1 S=2 (returned in AL)
    void LoadStoryHistoryNevada();                        // 0x00401ac0
    void LoadStoryHistoryAlaska();                        // 0x00401ea0
    void LoadStoryHistoryOregon();                        // 0x00402280
    void SaveStoryHistoryNevada();                        // 0x00402670
    void SaveStoryHistoryAlaska();                        // 0x004029c0
    void SaveStoryHistoryOregon();                        // 0x00402d10
    void RemoveEmptyZonesFromPlacedList();                // 0x00403070
    void PlaceZoneObjectTiles(short zoneId);              // 0x00403140
    void SaveZoneRecursive(CFile *f, short zoneId, int bFull);          // 0x004033b0
    void LoadZoneRecursive(CFile *f, short zoneId, int bFull);          // 0x00403450
    void OnUpdateFileSave(CCmdUI *pCmdUI);                // 0x00403510  demo: grayed
    void OnUpdateAppExit(CCmdUI *pCmdUI);                 // 0x00403520
    void OnUpdateHideMe(CCmdUI *pCmdUI);                  // 0x00403550
    void OnUpdateNewWorld(CCmdUI *pCmdUI);                // 0x00403580
    void OnUpdateLoadWorld(CCmdUI *pCmdUI);               // 0x00403600  demo: grayed
    void OnUpdateReplayStory(CCmdUI *pCmdUI);             // 0x00403610  demo: grayed
    void OnReplayStory();                                 // 0x00403620
    int  StartGame(unsigned int nSeed, int bSkipGenerate);// 0x004037a0
    void RefreshZone();                                   // 0x00403ae0
    // ---- cross-TU externs (doc TU / worldgen) ----
    int          LoadWorldMaybe();                        // 0x00421fd0
    int          Generate(unsigned int nSeed);            // worldgen driver
    unsigned int Randomize();                             // 0x00424380  reseed rand
    void         BackupZoneGrid();                        // zones[0..99] -> [100..199]
    void         Populate();                              // 0x00425e30
    void         FindSpecialZoneMaybe();                  //
    void         UpdateCamera();                          //
    Tile *GetTileData(int idx);                           // 0x00403a40
    ZoneStub *GetZoneById(short id);                      // 0x00403a70
    int  FindTile(void *pTile);                           // 0x00403aa0
};

#endif
