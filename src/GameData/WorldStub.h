// World stub for the GameData TU (the second doc-TU source file, 0x401ac0–0x4042b0).
// Field layout mirrors the Ghidra `World` struct (the source of truth); pads keep exact offsets.
// This stub ACCRETES toward the real shared World header as more TUs match (see roadmap).
// NOTE: only fields this TU touches are named; everything else is _pad. sizeof == 0x33c0.
#ifndef GAMEDATA_WORLDSTUB_H
#define GAMEDATA_WORLDSTUB_H
#include <afxwin.h>
#include <afxext.h>
#include <afxcoll.h>
#include "../Records/RecordClasses.h"   // real (byte-match-proven) Puzzle/Character/MapEntity/
                                        // Tile/ZoneObj/Zone — never stub matched modules

// InvItem / Canvas / InvScrollBar / GameView / TextDialog — the real shared view header
// (de-dup step 5, 2026-07-07; was a partial-field GameView stub + a 3-field InvItem stub).
// Stub fixups folded into the .cpps: OnWalk(int,short) was really ZoneTransitionStep
// (short,short) — the (0x5d, i) walk-in call; PlayerMove was PlaySound;
// bSuppressWalkSound@0x2f4 is bWeaponIactActiveMaybe.
#include "../GameView/GameView.h"

// A 10x10 world-map grid cell — canonical vptr-true definition (de-dup step 2). The old
// local struct here was the SAME layout shifted -4 (id@+0, no vptr), compensated by
// zones@0x4b4 and a 121-int pointer array swallowing cell 0's vptr; retired 2026-07-07.
#include "../Worldgen/MapZone.h"

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
    Zone  **zoneObjects;             // +0x0098
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
    Zone   *currentZone;             // +0x02c0
    char       *palette;                 // +0x02c4
    char        _pad2c8[4];              // +0x02c8
    int         worldSeed;               // +0x02cc
    int         paZonePtrGrid[0x78];     // +0x02d0  really apZoneGrid Zone*[100] + apUiTiles
                                         //          Tile*[20] (the old [0x79] swallowed cell 0's
                                         //          vptr for the shifted MapZone)
    MapZone     zones[200];              // +0x04b0  [0..99] active 10x10 grid (=mapGrid),
                                         //          [100..199] backup (RestoreGridFromBackup
                                         //          copies 100..199 -> 0..99); ends +0x2d50
    char        _pad2d50[0xd0];          // +0x2d50  really mapScratch MapZone[4]
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
    int         unk2e44;                 // +0x2e44
    int         nWeaponHitXMaybe;        // +0x2e48  last weapon-hit cell (written by
    int         nWeaponHitYMaybe;        // +0x2e4c  GameView::FireWeaponStep; blinked by IactRun)
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
    Zone   *pendingZone;             // +0x3308
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
    Tile       *pEquippedItem;           // +0x3374
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
    int  BuildQuestPathMaybe(short *paGrid, short *paOrder); // 0x00403c80
    // worldgen plan-grid helpers (both __thiscall members whose `this` is unused — the
    // original spills+reloads ECX at every call site, proving the member-call form):
    // dir (1=W 2=N 3=E 4=S, 0=none) of the neighbor of (x,y) holding a 0x68 gate cell
    int  FindAdjacentGateDirMaybe(int x, int y, short *paGrid); // 0x00419f60
    int  GetZoneGridOrder(int x, int y);                  // 0x00421e50 (static 10x10 table 0x456630)
    // ⚠ TU-PHASE DIAL (2026-07-06): the number & signature-shapes of World member decls rotate
    // allocator/cmp-direction tie-breaks in EVERY function of this TU (proven: loaders' backedge
    // jg/jl phase, PlaceZoneObjectTiles, FindZoneCellById, the savers' arm symmetry). These four
    // REAL World methods (all exist at the given addresses) are declared to set the phase so the
    // three loaders match (0/0/2). The unique full solution is the ORIGINAL header's complete
    // decl set — reconstruct it in Phase D; do NOT chase per-function phase with fake decls.
    void UpdateScore();                                   // 0x00401450 (scorers TU)
    int  GetZoneCell(int x, int y);                       // 0x00401a80 (scorers TU; int(int,int) shape is load-bearing)
    int  CalcTimeScore();                                 // 0x004019c0 (scorers TU)
    void RestoreGridFromBackup();                         // 0x00421520 (doc TU)
    // ---- cross-TU externs (doc TU / worldgen) ----
    int          LoadWorld();                             // 0x00421fd0
    int          Generate(unsigned int nSeed);            // worldgen driver
    unsigned int Randomize();                             // 0x00424380  reseed rand
    void         BackupZoneGrid();                        // zones[0..99] -> [100..199]
    int          Populate();                              // 0x00425e30
    void         SetCurrentToIntroZone();                 // 0x00423d20
    void         UpdateCamera();                          //
    Tile *GetTileData(int idx);                           // 0x00403a40
    Zone *GetZoneById(short id);                      // 0x00403a70
    int  FindTile(void *pTile);                           // 0x00403aa0
    // Iact interpreter callees (doc TU):
    int  GetZoneIndex(Zone *pZone);                       // 0x00423dc0  returns zone id
    void DrawPlayer();                                    // 0x0041a6d0
};


#endif
