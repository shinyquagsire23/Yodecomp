// Worldgen TU (0x41c340–0x429000): the doc class's .dta-load + worldgen + .wld-save source file
// (a third CDeskcppDoc source file, alongside GameData and WorldDoc). All methods are World::.
// This header will accrete the doc-TU World method decls as they are transcribed; see
// docs/worldgen.md for the full algorithm map.
#ifndef WORLDGEN_H
#define WORLDGEN_H
#include <afxwin.h>
#include <afxcoll.h>
#include <afxcmn.h>
#include <afxdlgs.h>
#include <afxext.h>
#include "../Records/RecordClasses.h"
#include "../IactScript/IactScriptClasses.h"
#include "../App/App.h"

// The static 10x10 worldgen grid-order priority table (.data 0x00456630).
extern int gWorldgenGridOrderTable[100];

// Quarter-circle needle-offset table (radius 16), shared by all four dial quadrants.
extern int gNeedleTable[26];         // 0x00456938


// Replay-a-story globals (App TU sets them from the command line; Frame TU owns the CString).
extern int g_bReplayMode;            // 0x00459e2c (defined in src/App/App.cpp)
extern CString g_strReplayPath;      // 0x00459e20 (defined in src/Frame/Frame.cpp)

// Tokens of the transient 10x10 short PLAN grid that Generate carves the quest into
// (CarveQuestPath writes PATH/GOAL/forks/blockers; PlaceBlockades stamps LOCK/WALL;
// BuildQuestPath assigns ORDERED markers; PlacePuzzles keys off ORDERED adjacency).
// Fork tokens are named for the direction their CORRIDOR extends. Generate consumes them:
// START->MAP_START zone, FORK_W->ITEM_TO_PASS(5), FORK_E->FIND_USEFUL_NPC(4),
// FORK_N->FINAL_DESTINATION(2), FORK_S->ITEM_FOR_ITEM(3); GOAL cells become the
// FROM/TO_ANOTHER_MAP vehicle pairs; leftovers fall back to ENEMY_TERRITORY.
enum PlanToken
{
    PLAN_EMPTY    = 0,
    PLAN_PATH     = 1,     // plain path room
    PLAN_GOAL     = 101,   // goal room (CarveQuestPath random promotion)
    PLAN_LOCK     = 102,   // blockade lock spot (PlaceBlockades)
    PLAN_WALL     = 104,   // blockade wall (PlaceBlockades)
    PLAN_START    = 201,   // the seed cell (Generate plants it; becomes MAP_START)
    PLAN_CORRIDOR = 300,   // corridor body (perpendicular neighbors blocked)
    PLAN_FORK_W   = 301,   // fork cell, corridor continues west
    PLAN_FORK_E   = 302,   // fork cell, corridor continues east
    PLAN_FORK_N   = 303,   // fork cell, corridor continues north
    PLAN_FORK_S   = 304,   // fork cell, corridor continues south
    PLAN_BLOCKED  = 305,   // reserved/blocker
    PLAN_ORDERED  = 306,   // order-id assigned (BuildQuestPath)
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

// A 10x10 world-map grid cell — canonical definition promoted to MapZone.h (de-dup step 2);
// included HERE (not at file top) to preserve the original declaration order (dial rule).
#include "MapZone.h"

// InvItem, Canvas, InvScrollBar, GameView, TextDialog were promoted to this shared header in
// Phase E (v16). It is included HERE (not at file top) to preserve the original declaration
// order — WorldgenZoneEntry, MapZone, [InvItem, Canvas, InvScrollBar, GameView, TextDialog],
// World — so the TU-phase dial is unchanged (verify stays 34/90). Do not hoist this include.
#include "../GameView/GameView.h"

// World facade for the functions transcribed so far (offsets from the ctor-derived layout in
// src/WorldDoc/WorldDoc.h; grows toward the real CDeskcppDoc as the TU fills in).
class World : public CDocument       // sizeof(CDocument) == 0x50 in MFC 4.2
{
public:
    int         unk50;               // +0x0050
    int         nZonesLoaded; // +0x0054  Load: = zones.GetSize() after the .dta parse
    int         totalZones;          // +0x0058  (CalcCompletionScore denominator)
    int         nFrameMode;          // +0x005c
    int         nMapChangeReason;    // +0x0060  (names: WorldDoc.h ctor-proven layout)
    int         abortFrame;          // +0x0064
    int         gameState;           // +0x0068  -1=in progress, 1=won
    int         nRequestedGoalItem;  // +0x006c
    int         score;               // +0x0070
    int         nFrameDelay;               // +0x0074  ctor: copied from CWinApp+0xc4 (frame delay)
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
    CPalette   *pPalette;            // +0x02c4  plain `new CPalette` in World::World (v15: the
                                     //          "app class" theory retired — vftable 0x44c4f0 IS
                                     //          CPalette's; ??_GCPalette COMDAT = 0x41e8b0)
    int         unk2c8;              // +0x02c8
    int         worldSeed;           // +0x02cc
    Zone       *apZoneGrid[100];     // +0x02d0
    Tile       *apUiTiles[20];       // +0x0460
    MapZone     mapGrid[100];        // +0x04b0  active 10x10 grid
    MapZone     mapGridBackup[100];  // +0x1900  backup grid
    MapZone     mapScratch[4];       // +0x2d50
    int         playerX;             // +0x2e20
    int         playerY;             // +0x2e24
    char        _pad2e28[4];         // +0x2e28
    Character  *currentWeapon;       // +0x2e2c
    int         unk2e30;             // +0x2e30  equipped-weapon character index + 8
                                     //          (OnDragItem/OnLButtonUp write it; no reader found)
    int         unk2e34;             // +0x2e34
    short       startItem;           // +0x2e38  (Ghidra names)
    short       startItem2Maybe;     // +0x2e3a
    int         currentPlanet;       // +0x2e3c  1=Nevada/Tatooine 2=Alaska/Hoth 3=Oregon/Endor
    int         bStartingGame;  // +0x2e40  nonzero skips the planet re-pick in LoadWorld
    int         bWeaponHitPending; // +0x2e44  FireWeaponStep zeroes per shot fired
    int         nWeaponHitX;    // +0x2e48  (Ghidra names)
    int         nWeaponHitY;    // +0x2e4c
    int         goalItemTileId;      // +0x2e50
    int         bHidePlayer;    // +0x2e54  (Ghidra name)
    int         bSkipNewWorldConfirm;             // +0x2e58  nonzero skips OnNewWorld's confirm box
    int         bPaletteAnimEnabled; // +0x2e5c  gates CyclePalette (doc sets 0/1)
    int         unk2e60;             // +0x2e60
    int         genSkipTeleCheck; // +0x2e64  worldgen: skip the teleporter-distance test
    WORD        palVersion;          // +0x2e68  ctor: 0x300   } inline LOGPALETTE
    WORD        palNumEntries;       // +0x2e6a  ctor: 0x100   } (WorldDoc.h names)
    PALETTEENTRY sysPalette[256];    // +0x2e6c  live palette mirror (GetSystemPaletteEntries
                                     //          target; CyclePalette ring-shifts it and feeds
                                     //          AnimatePalette from it)
    RGBQUAD    *pSysColorTable;      // +0x326c  DIB color-table mirror, ring-shifted in
                                     //          lockstep; Canvas::SetPalette source (the doc
                                     //          TU writes its bytes via (BYTE*) math)
    Canvas     *pCanvas;             // +0x3270
    RECT        rectUnk3274;         // +0x3274  locator-map blit origin (left/top used)
    RECT        rectUnk3284;         // +0x3284
    RECT        rectInvScroll;  // +0x3294  passed to the InvScrollBar ctor
    RECT        rectWeaponBox;       // +0x32a4  current-weapon icon box
    RECT        rectAmmoBar;         // +0x32b4  ammo bar box
    RECT        rectHealthDial;      // +0x32c4  (the 0x32d4 quad in WorldDoc.h was misnamed)
    int         nViewLeft;           // +0x32d4  visible 288x288 window (UpdateCamera writes;
    int         nViewTop;            // +0x32d8   named nHealthDial* in WorldDoc.h — TODO reconcile)
    int         nViewRight;          // +0x32dc
    int         nViewBottom;         // +0x32e0
    RECT        rectArrowBox;        // +0x32e4  (DrawDirectionArrows struct-copies it whole)
    int         bWorldReadyMaybe;    // +0x32f4
    int         bDtaLoaded;     // +0x32f8  set once by Load on first successful .dta open
    int         bStateFileLoaded; // +0x32fc  gates World::LoadWorldStateFile (OnDraw)
    int         nextCameraX;    // +0x3300  (Ghidra names)
    int         nextCameraY;    // +0x3304
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
    int         nQueuedMoveDX;  // +0x3338  pending bump/walk delta (OnBumpTile tail zeroes,
    int         nQueuedMoveDY;  // +0x333c   zone-transition arms chain nMoveDX=nMoveDY=DX)
    int         nWalkTargetX;   // +0x3340  OnLButtonDown mode-3 click target (fine coords, *32)
    int         nWalkTargetY;   // +0x3344
    short       ammoTheForce;   // +0x3348  saved charge for weapon tile 0x1fe (The Force)
    short       ammoLightsaber; // +0x334a  saved charge for weapon tile 0x12 (lightsaber)
    short       weaponState[4];      // +0x334c  (Ghidra name; OnSaveWorld writes [0..2])
    short       nCurrentAmmoMaybe;   // +0x3354
    char        _pad3356[2];         // +0x3356
    Tile       *pPlayerFrameTile;    // +0x3358
    Character  *pPlayerChar;         // +0x335c
    int         scrollDirX;          // +0x3360  edge-scroll direction (ScrollZoneTransition)
    int         scrollDirY;          // +0x3364
    char        _pad3368[8];         // +0x3368
    int         unk3370;             // +0x3370  cleared by GameView::ShowWinMessage tail
    Tile       *equippedItem;        // +0x3374  (UseWeapon saves/overrides it)
    int         unk3378;             // +0x3378  zeroed when STUP world-view state is entered
    int         bWorldInvalid;  // +0x337c  (Ghidra name; OnLoadWorld sets 1)
    int         genCellItemCScratch;      // +0x3380  worldgen per-cell scratch block
    int         genCellQuestSlot5Scratch; // +0x3384
    int         genCellItemAScratch;      // +0x3388
    int         genCellItemBScratch;      // +0x338c
    int         genCellQuestSlot6Scratch; // +0x3390
    int         genCellQuestSlot0Scratch; // +0x3394
    int         genCellQuestSlot1Scratch; // +0x3398
    int         genZoneTypeScratch;       // +0x339c
    int         nCurrentGoalItem;   // +0x33a0  Generate: = the goal puzzle id (demo hardcode 0x6c)
    char        _pad33a4[4];         // +0x33a4
    int         lastCount;           // +0x33a8  (Ghidra names, backported v15)
    int         highScore;           // +0x33ac
    int         lastScore;           // +0x33b0
    char        _pad33b4[4];         // +0x33b4
    int         bQuestCellsResident;             // +0x33b8  0 = quest cells swapped out (save reads mapScratch)
    char       *lpszSaveDir;    // +0x33bc  save dialog's m_ofn.lpstrInitialDir

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
    void DrawPlayer();                                   // 0x0041a6d0 (WorldDoc TU)
    void DrawLocatorMap(CDC *pDC, int bDrawPlayer, int bAlt); // 0x00423df0
    void UpdateCamera();                                 // 0x00423f50
    afx_msg void OnNewWorld();                           // 0x00424450
    afx_msg void OnSaveWorld();                          // 0x00424540
    afx_msg void OnLoadWorld();                          // 0x00424fc0 (0x424fb0 = a jmp thunk)
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
    int            GetVictoryZoneIndexMaybe();           // 0x00401a40 (scorers TU)
    Zone          *GetLossZoneMaybe();                   // 0x00401a60 (scorers TU)
    unsigned short GetZoneCell(int x, int y);            // 0x00401a80 (scorers TU)
    void SaveZoneRecursive(CFile *pFile, short zoneId, int bFull); // 0x004033b0 (GameData TU)
    void LoadZoneRecursive(CFile *pFile, short zoneId, int nArg); // 0x00403450 (GameData TU)
    int  StartGame(unsigned int nSeed, int bSkipGenerate); // 0x004037a0 (GameData TU)
    int  FindTile(void *pTile);                          // 0x00403aa0 (GameData TU)
    unsigned char GetExitDirections();                   // 0x004032c0 (GameData TU)
    Tile *GetTileData(int idx);                          // 0x00403a40 (GameData TU)
    Zone *GetZoneById(short id);                         // 0x00403a70 (GameData TU)
    void DrawRect(CDC *pDC, RECT *pRect, int bRaised, int nThickness); // 0x00424010 bevel border (thiscall-proven: DrawText loads ECX=pWorld)
    int  FindZoneCellById(short id, int *pX, int *pY);   // 0x00403250 (GameData TU)
    unsigned int GetLocatorIconMaybe(int x, int y, int bAlt); // 0x0041a1c0 (WorldDoc TU)
    void LoadStoryHistoryNevada();                       // 0x00401ac0 (GameData TU)
    void LoadStoryHistoryAlaska();                       // 0x00401ea0 (GameData TU)
    void LoadStoryHistoryOregon();                       // 0x00402280 (GameData TU)
    void Nop1();                                         // 0x00402660 (GameData TU)
    void SaveStoryHistoryNevada();                       // 0x00402670 (GameData TU)
    void SaveStoryHistoryAlaska();                       // 0x004029c0 (GameData TU)
    void SaveStoryHistoryOregon();                       // 0x00402d10 (GameData TU)
    void Nop2();                                         // 0x00403060 (GameData TU)
    void RemoveEmptyZonesFromPlacedList();               // 0x00403070 (GameData TU)
    int  BuildQuestPathMaybe(short *paGrid, short *paOrder); // 0x00403c80 (GameData TU)
    void RefreshZone();                                  // 0x00403ae0 (GameData TU)
    void PlaceZoneObjectTiles(short zoneId);             // 0x00403140 (GameData TU)
    int  ParseTilesMaybe(CFile *pFile, unsigned int nBytes); // 0x0041a030 (WorldDoc TU)
    void CacheUiTilePtrsMaybe();                         // 0x0041a5d0 (WorldDoc TU)
};

#endif
