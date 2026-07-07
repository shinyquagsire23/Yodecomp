// Worldgen TU (0x41c340–0x429000): the doc class's .dta-load + worldgen + .wld-save source file
// (a third CDeskcppDoc source file, alongside GameData and WorldDoc). All methods are World::.
// This header will accrete the doc-TU World method decls as they are transcribed; see
// docs/worldgen.md for the full algorithm map.
#ifndef WORLDGEN_H
#define WORLDGEN_H
#include <afxwin.h>
#include <afxcoll.h>
#include <afxcmn.h>
#include "../Records/RecordClasses.h"
#include "../IactScript/IactScriptClasses.h"
#include "../App/App.h"

// The static 10x10 worldgen grid-order priority table (.data 0x00456630).
extern int gWorldgenGridOrderTable[100];

// Tokens of the transient 10x10 short PLAN grid that Generate carves the quest into
// (CarveQuestPath writes PATH/GOAL/forks/blockers; PlaceBlockades stamps LOCK/WALL;
// BuildQuestPath assigns ORDERED markers; PlacePuzzles keys off ORDERED adjacency).
// Fork tokens are named for the direction their CORRIDOR extends (confirm vs Generate).
enum PlanToken
{
    PLAN_EMPTY    = 0,
    PLAN_PATH     = 1,      //       plain path room
    PLAN_GOAL     = 0x65,   // 101   goal room (CarveQuestPath random promotion)
    PLAN_LOCK     = 0x66,   // 102   blockade lock spot (PlaceBlockades)
    PLAN_WALL     = 0x68,   // 104   blockade wall (PlaceBlockades)
    PLAN_START    = 0xc9,   // 201   attachable seed cell Maybe (extendable like PATH/GOAL)
    PLAN_CORRIDOR = 0x12c,  // 300   corridor body (perpendicular neighbors blocked)
    PLAN_FORK_W   = 0x12d,  // 301   fork cell, corridor continues west
    PLAN_FORK_E   = 0x12e,  // 302   fork cell, corridor continues east
    PLAN_FORK_N   = 0x12f,  // 303   fork cell, corridor continues north
    PLAN_FORK_S   = 0x130,  // 304   fork cell, corridor continues south
    PLAN_BLOCKED  = 0x131,  // 305   reserved/blocker
    PLAN_ORDERED  = 0x132,  // 306   order-id assigned (BuildQuestPath)
};

// Worldgen zone-entry record (8 bytes, vtable 0x44b080, ctor Mfc::0x401390): the element type
// of the two worldgen zone-entry lists (worldgenPendingZones worklist / worldgenRefZones dedup set).
class WorldgenZoneEntry : public CObject
{
public:
    short zoneId;                    // +0x04
    short val;                       // +0x06
    WorldgenZoneEntry(short zoneId, short val);   // 0x00401390 (first app TU)
    virtual ~WorldgenZoneEntry();                 // vtable 0x44b080; delete = vcall slot +4
};                                   // sizeof 0x08

// A 10x10 world-map grid cell (0x34 bytes) — CObject-DERIVED (vftable 0x44b050; ctor 0x4010b0 /
// dtor 0x401180 live in the first app TU). Layout is ctor-proven — see src/WorldDoc/WorldDoc.h.
class MapZone : public CObject
{
public:                              // +0x00 vftable (0x44b050)
    short id;                        // +0x04  zone id; -1 = empty cell
    char  _pad06[2];                 // +0x06
    int   zoneType;                  // +0x08
    short cellQuestSlot0;            // +0x0c
    short cellQuestSlot1;            // +0x0e
    short cellItemA;                 // +0x10
    short cellItemB;                 // +0x12
    short cellItemC;                 // +0x14
    short cellQuestSlot5;            // +0x16
    short cellQuestSlot6;            // +0x18
    char  _pad1a[2];                 // +0x1a
    int   flagSolved;                // +0x1c
    int   flagC;                     // +0x20
    int   flagA;                     // +0x24
    int   flagB;                     // +0x28
    int   flagD;                     // +0x2c
    short field30;                   // +0x30
    char  _pad32[2];                 // +0x32
                                     // sizeof 0x34
    MapZone();                                            // 0x004010b0 (first app TU)
    virtual ~MapZone();                                   // 0x00401180
};

// Canvas stub: only what this TU touches (real module: src/Canvas/, byte-matched).
class Canvas
{
public:
    char _pad[0x43c];                // no vptr; sizeof == 0x43c
    void *GetData();                                      // 0x00407f50 (Canvas TU)
};

// GameView stub: only what this TU touches (real class = CDeskcppView, GameView TU).
class GameView
{
public:
    char _pad00[0x4c];               // +0x000
    int  bBusy;                      // +0x04c
    char _pad50[0x74];               // +0x050
    int  soundSession;               // +0x0c4  0 until SoundInit opens the WAVMIX session
    char _padc8[0x4c];               // +0x0c8
    int  nTargetZoneId;              // +0x114
    void SoundInit();                                     // 0x00411520 (GameView TU)
};

// World facade for the functions transcribed so far (offsets from the ctor-derived layout in
// src/WorldDoc/WorldDoc.h; grows toward the real CDeskcppDoc as the TU fills in).
class World : public CDocument       // sizeof(CDocument) == 0x50 in MFC 4.2
{
public:
    int         unk50;               // +0x0050
    int         zoneCountLoadedMaybe; // +0x0054  Load: = zones.GetSize() after the .dta parse
    int         totalZones;          // +0x0058  (CalcCompletionScore denominator)
    int         nFrameMode;          // +0x005c
    int         nMapChangeReason;    // +0x0060  (names: WorldDoc.h ctor-proven layout)
    int         abortFrame;          // +0x0064
    int         gameState;           // +0x0068  -1=in progress, 1=won
    int         nRequestedGoalItem;  // +0x006c
    int         score;               // +0x0070
    int         unk74;               // +0x0074  ctor: copied from CWinApp+0xc4 (frame delay)
    int         timeBase;            // +0x0078
    int         timeOffset;          // +0x007c
    CObArray    tiles;               // +0x0080  Tile*
    CObArray    zones;               // +0x0094  Zone*
    CObArray    inventory;           // +0x00a8  InvItem*
    CObArray    characters;          // +0x00bc  Character*
    CObArray    puzzles;             // +0x00d0  Puzzle*
    CString     soundNames[64];      // +0x00e4
    CWordArray  questItemsA;         // +0x01e4
    CWordArray  questItemsB;         // +0x01f8
    CWordArray  goalTileList;        // +0x020c
    CWordArray  placedZoneIds;       // +0x0220
    CWordArray  uniqueRequiredItemsMaybe;              // +0x0234
    CWordArray  unk248;              // +0x0248
    CObArray    worldgenPendingZones; // +0x025c  worklist (push-front)
    CObArray    worldgenRefZones;    // +0x0270  dedup set (WorldgenZoneEntry*)
    CWordArray  storyHistoryNevada;  // +0x0284
    CWordArray  storyHistoryAlaska;  // +0x0298
    CWordArray  storyHistoryOregon;  // +0x02ac
    Zone       *currentZone;         // +0x02c0
    CPalette   *pPalette;            // +0x02c4
    int         unk2c8;              // +0x02c8
    int         worldSeed;           // +0x02cc
    Zone       *apZoneGrid[100];     // +0x02d0
    Tile       *apUiTiles[20];       // +0x0460
    MapZone     mapGrid[100];        // +0x04b0  active 10x10 grid
    MapZone     mapGridBackup[100];  // +0x1900  backup grid
    MapZone     mapScratch[4];       // +0x2d50
    int         playerX;             // +0x2e20
    int         playerY;             // +0x2e24
    char        _pad2e28[0xc];       // +0x2e28
    int         unk2e34;             // +0x2e34
    short       startItem;           // +0x2e38  (Ghidra names)
    short       startItem2Maybe;     // +0x2e3a
    int         currentPlanet;       // +0x2e3c  1=Nevada/Tatooine 2=Alaska/Hoth 3=Oregon/Endor
    int         bStartingGameMaybe;  // +0x2e40  nonzero skips the planet re-pick in LoadWorld
    char        _pad2e44[4];         // +0x2e44
    int         nWeaponHitXMaybe;    // +0x2e48  (Ghidra names)
    int         nWeaponHitYMaybe;    // +0x2e4c
    int         goalItemTileId;      // +0x2e50
    int         bHidePlayerMaybe;    // +0x2e54  (Ghidra name)
    char        _pad2e58[8];         // +0x2e58
    int         unk2e60;             // +0x2e60
    int         genSkipTeleCheckMaybe; // +0x2e64  worldgen: skip the teleporter-distance test
    char        _pad2e68[0x404];     // +0x2e68
    BYTE       *pSysColorTable;      // +0x326c
    Canvas     *pCanvas;             // +0x3270
    char        _pad3274[0x60];      // +0x3274  (viewport/inventory/weapon-box rects)
    int         nViewLeft;           // +0x32d4  visible 288x288 window (UpdateCamera writes;
    int         nViewTop;            // +0x32d8   named nHealthDial* in WorldDoc.h — TODO reconcile)
    int         nViewRight;          // +0x32dc
    int         nViewBottom;         // +0x32e0
    int         nArrowBoxLeft;       // +0x32e4  (Ghidra names)
    int         nArrowBoxTop;        // +0x32e8
    int         nArrowBoxRight;      // +0x32ec
    int         nArrowBoxBottom;     // +0x32f0
    int         bWorldReadyMaybe;    // +0x32f4
    int         bDtaLoadedMaybe;     // +0x32f8  set once by Load on first successful .dta open
    char        _pad32fc[4];         // +0x32fc
    int         nextCameraXMaybe;    // +0x3300  (Ghidra names)
    int         nextCameraYMaybe;    // +0x3304
    Zone       *pPendingZone;        // +0x3308
    int         nSoundEnabled;       // +0x330c
    int         nMusicEnabled;       // +0x3310
    int         healthLo;            // +0x3314  (Ghidra names)
    int         healthHi;            // +0x3318
    int         difficulty;          // +0x331c
    int         counter;             // +0x3320
    int         gameSpeed;           // +0x3324
    int         worldSize;           // +0x3328  1/2/3 (teleporter min-distance tier)
    int         completionCount;     // +0x332c  worlds completed (5/10/15 milestones gate planets)
    int         cameraX;             // +0x3330
    int         cameraY;             // +0x3334
    char        _pad3338[0x20];      // +0x3338
    Tile       *pPlayerFrameTile;    // +0x3358
    Character  *pPlayerChar;         // +0x335c
    char        _pad3360[0x20];      // +0x3360
    int         genCellItemCScratch;      // +0x3380  worldgen per-cell scratch block
    int         genCellQuestSlot5Scratch; // +0x3384
    int         genCellItemAScratch;      // +0x3388
    int         genCellItemBScratch;      // +0x338c
    int         genCellQuestSlot6Scratch; // +0x3390
    int         genCellQuestSlot0Scratch; // +0x3394
    int         genCellQuestSlot1Scratch; // +0x3398
    int         genZoneTypeScratch;       // +0x339c
    char        _pad33a0[0x18];      // +0x33a0
    int         unk33b8;             // +0x33b8

    // ---- this TU's methods (grow one decl at a time as functions land) ----
    int  ZoneHasIzxItemMaybe(short zoneId, short itemId, int sel); // 0x0041bfa0
    int  ZoneRequiresItemMaybe(short zoneId, short itemId); // 0x0041c0b0
    int  PickUnplacedItemMaybe(short zoneId);            // 0x0041c200
    int  ZoneProvidesItem(short zoneId, short itemId);   // 0x0041c3b0
    int  ZoneFindInIzxList(short zoneId, short itemId, int sel); // 0x0041c490
    int  WorldgenFillQuestItemSpot(short zoneId, short itemId); // 0x0041c580
    int  WorldgenFillSpawn(short zoneId, short itemId);  // 0x0041c730
    int  WorldgenPopulateGoalZone(short zoneId, short iA, short iB,
                                  short nOrder, short a5);  // 0x0041c8f0 (a5 unread in body;
                                                            // caller pushes it un-widened => short)
    int  WorldgenPlaceUsefulDropChainMaybe(short zoneId, short idx,
                                           short nOrder, short sel); // 0x0041cbe0
    int  WorldgenPlaceItemOnLock(short zoneId, int a2, int nVal,
                                 short itemId, int sel);  // 0x0041cdc0
    int  WorldgenFillQuestItemSpot2Maybe(short zoneId, short a2, short nVal,
                                         unsigned short itemId);  // 0x0041cf10
    int  WorldgenPlaceItemForLockChainMaybe(short zoneId, short idx,
                                            short nOrder, short sel); // 0x0041d0c0 (nOrder unread
                                                            // in body; caller pushes un-widened)
    int  WorldgenPlaceUsefulObjectMaybe(short zoneId, short itemId,
                                        short nOrder);       // 0x0041d260
    int  WorldgenAssignTransitItemMaybe(short zoneId, short nOrder,
                                        int sel);            // 0x0041d480
    int  IsItemPlaced(short itemId);                     // 0x0041d670
    void WorldgenPushZoneEntry(short zoneId, short val); // 0x0041d6b0
    void RemoveZoneEntry(short zoneId);                  // 0x0041d740
    void RemoveZoneEntry2(short zoneId);                 // 0x0041d7a0
    void WorldgenAddZoneEntry(short zoneId, short val);  // 0x0041d800
    int  IsZoneUsed(short zoneId);                       // 0x0041d8d0
    void AddPlacedZoneId(short zoneId);                  // 0x0041d920
    void WorldgenCarveQuestPath(int nTier, int nBudget, short *paPlanGrid,
                                int maxGoals, int *pnGoals, int maxSplits,
                                int *pnSplits, int *pnPlaced); // 0x0041d940
    void WorldgenPlaceBlockades(int nCount, short *paPlanGrid); // 0x0041e350
    int  WorldgenPickItemFromZone(short zoneId, short a2, int sel); // 0x0041e920
    void WorldgenShuffleList(CWordArray *pList);         // 0x0041ef90
    int  WorldgenSelectPuzzle(short nItem, short nItem2, short nType,
                              int bFirst);           // 0x0041eab0 (Ghidra: WorldgenSelectPuzzleMaybe;
                                                     // nItem2/bFirst unread in the body)
    unsigned short PlaceQuestNode(short nType, short a2, short a3, short a4,
                                  short a5, short nOrder, short a7);  // 0x0041f120
    int  CheckZoneItemsAvailable(short zoneId);          // 0x0041f830
    void WorldgenCollectZoneRefs(short zoneId);          // 0x0041f8e0
    int  Generate(unsigned int nSeed);                   // 0x0041f960 (this TU, later)
    void BackupZoneGrid();                               // 0x00421460
    void RestoreGridFromBackup();                        // 0x00421520
    int  IsTileInGoalList(unsigned int tileId);          // 0x004215e0
    int  PlacePuzzle(short nOrderMax, short *paPlanGrid, int *pX, int *pY); // 0x00421620
    int  WorldgenPlacePuzzles(short *paPlanGrid);        // 0x00421930
    int  GetZoneGridOrder(int x, int y);                 // 0x00421e50
    virtual BOOL IsModified();                           // 0x00422f40
    virtual void SetModifiedFlag(BOOL bModified = TRUE); // 0x00422f50
    int  ParseChar(CFile *pFile);                        // 0x00421e70
    int  LoadWorld();                                    // 0x00421fd0
    int  Load();                                         // 0x00422670  (.dta chunk dispatcher)
    int  ParseZone(CFile *pFile);                        // 0x00422f60
    int  ParsePuz2(CFile *pFile);                        // 0x00422fd0
    int  ParseZaux(CFile *pFile);                        // 0x00423110
    int  ParseZax3(CFile *pFile);                        // 0x00423190
    int  ParseZax2(CFile *pFile);                        // 0x00423210
    int  ParseCaux(CFile *pFile);                        // 0x00423290
    int  ParseChwp(CFile *pFile);                        // 0x00423300
    int  ParseTnam(CFile *pFile);                        // 0x00423380
    int  ParseSnds(CFile *pFile);                        // 0x004233f0
    int  ParseActn(CFile *pFile);                        // 0x00423510
    int  ParseHtsp(CFile *pFile);                        // 0x004236b0
    void LoadWorldStateFile();                           // 0x00423850
    virtual void Serialize(CArchive &ar);                // 0x00423b30
    void SetCurrentToIntroZone();                        // 0x00423d20
    void ReadStupCanvas(CFile *pFile);                   // 0x00423d60
    int  GetZoneIndex(Zone *pZone);                      // 0x00423dc0 (Ghidra: EnterZone)
    void UpdateCamera();                                 // 0x00423f50
    afx_msg void OnToggleSound();                        // 0x004242a0
    afx_msg void OnUpdateToggleSound(CCmdUI *pCmdUI);    // 0x004242f0
    afx_msg void OnToggleMusic();                        // 0x00424310
    afx_msg void OnUpdateToggleMusic(CCmdUI *pCmdUI);    // 0x00424360
    unsigned int Randomize();                            // 0x00424380
    int  Populate();                                     // 0x00425e30
    int  PlaceZone(short zoneId, unsigned short tileId); // 0x004260e0
    void RestoreRecords();                               // 0x00426380
    void BackupRecords();                                // 0x00426690
    void SetupGrid();                                    // 0x004269a0

    Zone *ReadZone(CFile *pFile, int idx);               // 0x00426a00 (this TU, later)

    // ---- cross-TU stubs (defined in other TUs; calls are masked relocs) ----
    void           UpdateScore();                        // 0x00401450 (scorers TU, src/World/)
    int            CalcCompletionScore();                // 0x00401490 (scorers TU)
    int            CalcScoreFromCounter();               // 0x004016d0 (scorers TU)
    int            CalcSolvedScore();                    // 0x00401780 (scorers TU)
    int            CalcTimeScore();                      // 0x004019c0 (scorers TU)
    unsigned short GetZoneCell(int x, int y);            // 0x00401a80 (scorers TU)
    Zone *GetZoneById(short id);                         // 0x00403a70 (GameData TU)
    void RefreshZone();                                  // 0x00403ae0 (GameData TU)
    void PlaceZoneObjectTiles(short zoneId);             // 0x00403140 (GameData TU)
    int  ParseTilesMaybe(CFile *pFile, unsigned int nBytes); // 0x0041a030 (WorldDoc TU)
    void CacheUiTilePtrsMaybe();                         // 0x0041a5d0 (WorldDoc TU)
};

#endif
