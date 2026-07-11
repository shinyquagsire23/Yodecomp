// World stub for the GameData TU (the second doc-TU source file, 0x401ac0–0x4042b0).
// Field layout mirrors the Ghidra `World` struct (the source of truth); pads keep exact offsets.
// This stub ACCRETES toward the real shared World header as more TUs match (see roadmap).
// NOTE: only fields this TU touches are named; everything else is _pad. sizeof == 0x33c0.
#ifndef GAMEDATA_WORLDSTUB_H
#define GAMEDATA_WORLDSTUB_H
#include <afxwin.h>
#include <afxext.h>
#include <afxcmn.h>                      // common controls (CProgressCtrl/CToolTipCtrl/…) — the
                                         // original built EVERY TU with afxcmn (AppWizard stdafx.h);
                                         // its decls are a real TU-context dial input (v37): adding
                                         // it flips FindTile+PlaceZoneObjectTiles to EXACT (GameData
                                         // 12→13). Proven a DECL effect, NOT a /Yu PCH effect (PCH
                                         // itself is net-negative + can't flip jl/jg — v37).
#include <afxcoll.h>
#include "GameObjectClasses.h"   // real (byte-match-proven) Puzzle/Character/MapEntity/
                                        // Tile/ZoneObj/Zone — never stub matched modules

// InvItem / Canvas / InvScrollBar / GameView / TextDialog — the real shared view header
// (de-dup step 5, 2026-07-07; was a partial-field GameView stub + a 3-field InvItem stub).
// Stub fixups folded into the .cpps: OnWalk(int,short) was really ZoneTransitionStep
// (short,short) — the (0x5d, i) walk-in call; PlayerMove was PlaySound;
// bSuppressWalkSound@0x2f4 is bWeaponIactActiveMaybe.
#include "DeskcppView.h"

// A 10x10 world-map grid cell — canonical vptr-true definition (de-dup step 2). The old
// local struct here was the SAME layout shifted -4 (id@+0, no vptr), compensated by
// zones@0x4b4 and a 121-int pointer array swallowing cell 0's vptr; retired 2026-07-07.
#include "MapZone.h"

#ifdef YODA_PORTABLE
// ── 64-bit-safe stub view (H4 — docs/phase-h4-sdl.md) ────────────────────────────────────────
// The MSVC-4.2-pinned view in the #else models raw MFC internals (CObArray guts as
// tileArray/tileCount, pointer grids as int[]), which cannot hold on LP64. This branch keeps
// the member NAMES this TU's code reads but uses real types in the full declaration's order,
// so the layout equals DeskcppDoc.h/Worldgen.h BY CONSTRUCTION on any ABI. The raw-guts names
// are anonymous-union overlays of the microfx CObArray layout {vptr,m_pData,m_nSize,m_nMaxSize}.
class CDeskcppDoc : public CDocument
{
public:
    // Itanium key-function pin: vtable emission belongs to DeskcppDoc.cpp (full view) — same
    // anchor as Worldgen.h's portable dtor decl.
    virtual ~CDeskcppDoc();
    int         unk50;               // +0x0050 (comments carry the 32-bit reference offsets)
    int         unk54;               // +0x0054
    int         totalZones;          // +0x0058
    int         nFrameMode;          // +0x005c
    int         nMapChangeReason;    // +0x0060
    int         abortFrame;          // +0x0064
    int         gameState;           // +0x0068
    int         nRequestedGoalItem;  // +0x006c
    int         score;               // +0x0070
    int         nFrameDelay;         // +0x0074
    int         timeBase;            // +0x0078
    int         timeOffset;          // +0x007c
    union { CObArray tiles;          // +0x0080  Tile*
            struct { void *_vTiles; Tile **tileArray; int tileCount; int _mTiles; }; };
    union { CObArray zonesCArr;      // +0x0094  Zone*  (this TU reads only the guts)
            struct { void *_vZones; Zone **zoneObjects; int zoneCount; int _mZones; }; };
    CObArray    inventory;           // +0x00a8
    CObArray    characters;          // +0x00bc
    CObArray    puzzles;             // +0x00d0
    CString     soundNames[64];      // +0x00e4
    CWordArray  questItemsA;         // +0x01e4
    CWordArray  questItemsB;         // +0x01f8
    CWordArray  goalTileList;        // +0x020c
    CWordArray  placedZoneIds;       // +0x0220
    CWordArray  unk234;              // +0x0234
    CWordArray  unk248;              // +0x0248
    CObArray    worldgenPendingZones; // +0x025c
    CObArray    worldgenRefZones;    // +0x0270
    CWordArray  storyHistoryNevada;  // +0x0284
    CWordArray  storyHistoryAlaska;  // +0x0298
    CWordArray  storyHistoryOregon;  // +0x02ac
    Zone       *currentZone;         // +0x02c0
    CPalette   *palette;             // +0x02c4
    int         _pad2c8;             // +0x02c8
    int         worldSeed;           // +0x02cc
    union { struct { Zone *apZoneGrid[100]; Tile *apUiTiles[20]; };  // +0x02d0/+0x0460
            intptr_t paZonePtrGrid[120]; };  // the one reader clears it via PTRINT
    MapZone     zones[200];          // +0x04b0  = mapGrid[100] + mapGridBackup[100]
    MapZone     mapScratch[4];       // +0x2d50
    int         playerX;             // +0x2e20
    int         playerY;             // +0x2e24
    int         _pad2e28;            // +0x2e28
    Character  *currentWeapon;       // +0x2e2c
    int         unk2e30;             // +0x2e30
    int         unk2e34;             // +0x2e34
    short       startItem;           // +0x2e38
    short       startItem2;          // +0x2e3a
    int         currentPlanet;       // +0x2e3c
    int         bStartingGame;       // +0x2e40
    int         bWeaponHitPending;   // +0x2e44
    int         nWeaponHitX;         // +0x2e48
    int         nWeaponHitY;         // +0x2e4c
    int         goalItemTileId;      // +0x2e50
    int         bHidePlayer;         // +0x2e54
    char        _pad2e58[8];         // +0x2e58  (ints only)
    int         unk2e60;             // +0x2e60
    int         genSkipTeleCheck;    // +0x2e64
    WORD        palVersion;          // +0x2e68
    WORD        palNumEntries;       // +0x2e6a
    PALETTEENTRY sysPalette[256];    // +0x2e6c
    RGBQUAD    *pSysColorTable;      // +0x326c
    Canvas     *pCanvas;             // +0x3270
    RECT        rectViewport;        // +0x3274
    RECT        rectInventory;       // +0x3284
    RECT        rectInvScroll;       // +0x3294
    RECT        rectWeaponBox;       // +0x32a4
    RECT        rectAmmoBar;         // +0x32b4
    RECT        rectHealthDial;      // +0x32c4
    int         nViewLeft;           // +0x32d4
    int         nViewTop;            // +0x32d8
    int         nViewRight;          // +0x32dc
    int         nViewBottom;         // +0x32e0
    RECT        rectArrowBox;        // +0x32e4
    int         bWorldReady;         // +0x32f4
    int         bDtaLoaded;          // +0x32f8
    int         bStateFileLoaded;    // +0x32fc
    int         nextCameraX;         // +0x3300
    int         nextCameraY;         // +0x3304
    Zone       *pendingZone;         // +0x3308
    int         nSoundEnabled;       // +0x330c
    int         nMusicEnabled;       // +0x3310
    int         healthLo;            // +0x3314
    int         healthHi;            // +0x3318
    int         difficulty;          // +0x331c
    int         counter;             // +0x3320
    int         gameSpeed;           // +0x3324
    int         worldSize;           // +0x3328
    int         completionCount;     // +0x332c
    int         cameraX;             // +0x3330
    int         cameraY;             // +0x3334
    char        _pad3338[0x14];      // +0x3338  move/ammo scalars (ints+shorts only)
    short       weaponState[4];      // +0x334c
    short       nCurrentAmmo;        // +0x3354
    char        _pad3356[2];         // +0x3356
    Tile       *pPlayerFrameTile;    // +0x3358
    Character  *pPlayerChar;         // +0x335c
    char        _pad3360[0x14];      // +0x3360  scroll scalars (ints only)
    Tile       *pEquippedItem;       // +0x3374
    int         unk3378;             // +0x3378
    int         bWorldInvalid;       // +0x337c
    char        _pad3380[0x20];      // +0x3380  worldgen cell scratch (ints only)
    int         nCurrentGoalItem;    // +0x33a0
    int         _pad33a4;            // +0x33a4
    int         lastCount;           // +0x33a8
    int         highScore;           // +0x33ac
    int         lastScore;           // +0x33b0
    int         _pad33b4;            // +0x33b4
    int         bQuestCellsResident; // +0x33b8
    char       *lpszSaveDir;         // +0x33bc
#else
class CDeskcppDoc : public CDocument          // sizeof(CDocument) == 0x50 in MFC 4.2
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
    int         bWeaponHitPending;       // +0x2e44
    int         nWeaponHitX;        // +0x2e48  last weapon-hit cell (written by
    int         nWeaponHitY;        // +0x2e4c  GameView::FireWeaponStep; blinked by IactRun)
    int         goalItemTileId;          // +0x2e50
    int         bHidePlayer;             // +0x2e54
    char        _pad2e58[8];             // +0x2e58
    int         unk2e60;                 // +0x2e60
    char        _pad2e64[0x40c];         // +0x2e64
    Canvas     *pCanvas;                 // +0x3270
    RECT        rectViewport;            // +0x3274  play area
    RECT        rectInventory;           // +0x3284  inventory panel
    RECT        rectInvScroll;           // +0x3294  inventory scrollbar column
    RECT        rectWeaponBox;           // +0x32a4  equipped-weapon icon box
    RECT        rectAmmoBar;             // +0x32b4  weapon charge/ammo column
    RECT        rectHealthDial;          // +0x32c4  health dial
    int         nViewLeft;               // +0x32d4  (288x288 offscreen view window; 4 ints per
    int         nViewTop;                // +0x32d8   Ghidra — was misnamed nHealthDial* here)
    int         nViewRight;              // +0x32dc
    int         nViewBottom;             // +0x32e0
    RECT        rectArrowBox;            // +0x32e4  direction-arrows box
    int         bWorldReady;             // +0x32f4
    int         bDtaLoaded;              // +0x32f8
    int         bStateFileLoaded;        // +0x32fc
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
#endif // YODA_PORTABLE

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
#ifdef GAME_INDY
    int          IndyGenerate(unsigned int nSeed);        // DESKADV 1010:8524 (Indy worldgen)
    void         IndyLoadStoryHistory();                  // DESKADV 1018:e7af ([GameData] Wyoming<N>)
    void         IndyLoadPlacedZoneList();                // DESKADV 1018:eb39 ([GameData] Hawaii<N>)
    void         IndySaveStoryHistory();                  // DESKADV 1020:0000
    void         IndySavePlacedZoneList();                // DESKADV 1020:0339
#endif
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

// ── YODA_BUGFIX: guarded fixes for ORIGINAL engine bugs (docs/engine-bugs.md) ───────────────
// YODA_SIC_FIX(x) expands to NOTHING in byte-match builds (anchor preprocessed tokens are
// unchanged; same trick as PTRINT) and to the fix code under -D YODA_BUGFIX (CMake YODA_BUGFIX,
// default ON for every non-anchor config). Fix sites sit on the SAME source line as the
// original statement so no function's line number moves (lesson #23). BUGLOG(("fmt", ...)) —
// double parens, VC4.2 has no variadic macros — appends to yoda_bugfix.log in the cwd,
// recording that the ORIGINAL engine would have hit the edge condition. Crash/UB/leak fixes
// only: behavior-shaping bugs (worldgen quirks, script-index clobber) stay faithful.
// This block is duplicated in Worldgen.h / DeskcppStub.h / DeskcppDoc.h (no shared header
// exists across those TUs, and adding an include to a byte-matched TU is forbidden — lesson 6);
// the #ifndef makes the copies idempotent. Keep them identical.
#ifndef YODA_SIC_FIX
#ifdef YODA_BUGFIX
#include <stdio.h>
#include <stdarg.h>
static inline void YodaBugLog(const char *fmt, ...)
{
    FILE *f = fopen("yoda_bugfix.log", "a");
    if (f == NULL)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fclose(f);
}
#define YODA_SIC_FIX(x) x
#define YODA_SIC_RETURN(x) { x return; }
#define BUGLOG(args) YodaBugLog args
#else
#define YODA_SIC_FIX(x)
#define YODA_SIC_RETURN(x) return;
#define BUGLOG(args) ((void)0)
#endif
#endif // YODA_SIC_FIX
