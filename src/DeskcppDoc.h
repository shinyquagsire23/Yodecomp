// WorldDoc — the REAL CDeskcppDoc (= World) class declaration, reconstructed from the
// ctor/dtor pair at 0x41a870/0x41b2f0 (this TU, "the doc main source file",
// 0x419ed0–0x41bee0). The ctor's EH-state chain is the DEFINITIVE member order:
//
//   base CDocument (0x00, sizeof 0x50)
//   EH  0- 4  five CObArray      @0x80,0x94,0xa8,0xbc,0xd0   (tiles/zones/inventory/characters/puzzles)
//   EH  5     CString[64]        @0xe4    soundNames (vector ctor 0x42b030)
//   EH  6-11  six CWordArray     @0x1e4.. (all nine word-arrays call the SAME ctor 0x43806c;
//   EH 12-13  two CObArray       @0x25c,0x270            CWordArray proven by the GameData loaders)
//   EH 14-16  three CWordArray   @0x284.. story histories
//   EH 17-19  MapZone[100]/[100]/[4] @0x4b0,0x1900,0x2d50 (ctor 0x4010b0 / dtor 0x401180 —
//              grids START at 0x4b0, NOT the long-believed 0x4b4; MapZone has a vptr at +0)
//   EH 20     CString            @0x33bc installPath
//
// Scalars between class members are plain fields (no EH states). sizeof == 0x33c0
// (operator_new(0x33c0) in CreateObject 0x419ed0).
//
// NOTE: src/GameData/WorldStub.h still carries the old shifted view (zones[200]@0x4b4,
// MapZone without vptr). Offsets agree byte-for-byte; consolidation is a deliberate
// later step (dial re-verification required — see CLAUDE.md TU-phase dial).
#ifndef WORLDDOC_H
#define WORLDDOC_H
#include <afxwin.h>
#include <afxcoll.h>
#include <afxcmn.h>                      // common controls — original built every TU with afxcmn
                                         // (v37): its decls flip ~World() to EXACT (WorldDoc 7→8).
#include "GameObjectClasses.h"

// Canvas stub (real module: src/Canvas/, byte-matched). Modeled here with the ctor/dtor
// forms this TU's codegen needs: `new Canvas(w,h)` emits new+nullcheck+ctor — the ctor IS
// src/Canvas's `Init` (0x407df0, a __thiscall returning this; identical codegen either way).
// Canvas — canonical declaration (de-dup steps 3+4). The old local stub's `~Canvas()
// 0x408010` was a stale comment error — the dtor is 0x407eb0.
#include "Canvas.h"

// A 10x10 world-map grid cell — canonical definition promoted to ../Worldgen/MapZone.h
// (de-dup step 2); included HERE to preserve the original declaration order (dial rule).
#include "MapZone.h"

class CDeskcppDoc : public CDocument       // CDeskcppDoc; sizeof(CDocument) == 0x50 in MFC 4.2
{
public:
    int         unk50;               // +0x0050
    int         unk54;               // +0x0054
    int         totalZones;          // +0x0058
    int         nFrameMode;          // +0x005c
    int         nMapChangeReason;    // +0x0060
    int         abortFrame;          // +0x0064
    int         gameState;           // +0x0068  -1=in progress, 1=won
    int         nRequestedGoalItem;  // +0x006c
    int         score;               // +0x0070
    int         nFrameDelay;               // +0x0074  ctor: copied from CWinApp+0xc4
    int         timeBase;            // +0x0078
    int         timeOffset;          // +0x007c
    CObArray    tiles;               // +0x0080  Tile*
    CObArray    zones;               // +0x0094  Zone*
    CObArray    inventory;           // +0x00a8  InvItem*
    CObArray    characters;          // +0x00bc  Character*
    CObArray    puzzles;             // +0x00d0  Puzzle*
    CString     soundNames[64];      // +0x00e4  (0x100 bytes)
    CWordArray  questItemsA;         // +0x01e4
    CWordArray  questItemsB;         // +0x01f8
    CWordArray  goalTileList;        // +0x020c
    CWordArray  placedZoneIds;       // +0x0220
    CWordArray  uniqueRequiredItemsMaybe;              // +0x0234
    CWordArray  unk248;              // +0x0248
    CObArray    worldgenPendingZones;  // +0x025c  worklist (push-front): zones pending expansion
    CObArray    worldgenRefZones;     // +0x0270  dedup set: zones referenced/required by the quest
                                     //           (elements = 8-byte {u16 zoneId@4, u16 val@6}; ctor 0x401390)
    CWordArray  storyHistoryNevada;  // +0x0284  planet 1 = Tatooine (Indy-engine key names)
    CWordArray  storyHistoryAlaska;  // +0x0298  planet 2 = Hoth (the demo planet)
    CWordArray  storyHistoryOregon;  // +0x02ac  planet 3 = Endor
    Zone       *currentZone;         // +0x02c0
    CPalette   *pPalette;            // +0x02c4  ctor: new CPalette
    int         unk2c8;              // +0x02c8
    int         worldSeed;           // +0x02cc
    Zone       *apZoneGrid[100];     // +0x02d0  10x10 Zone* grid (OnNewDocument zeroes row-wise)
    Tile       *apUiTiles[20];       // +0x0460  cached UI tile ptrs (CacheUiTilePtrs)
    MapZone     mapGrid[100];        // +0x04b0  active 10x10 grid
    MapZone     mapGridBackup[100];  // +0x1900  backup grid (RestoreGridFromBackup copies back)
    MapZone     mapScratch[4];       // +0x2d50
    int         playerX;             // +0x2e20
    int         playerY;             // +0x2e24
    int         unk2e28;             // +0x2e28
    Character  *currentWeapon;       // +0x2e2c
    int         unk2e30;             // +0x2e30
    int         unk2e34;             // +0x2e34  ctor: -1
    short       startItem;           // +0x2e38
    short       startItem2;          // +0x2e3a
    int         currentPlanet;       // +0x2e3c  1=Nevada/Tatooine 2=Alaska/Hoth 3=Oregon/Endor
    int         bStartingGame;       // +0x2e40
    int         unk2e44;             // +0x2e44
    int         nWeaponHitX;         // +0x2e48
    int         nWeaponHitY;         // +0x2e4c
    int         goalItemTileId;      // +0x2e50
    int         bHidePlayer;         // +0x2e54
    int         bSkipNewWorldConfirm;             // +0x2e58  ctor: 0
    int         bPaletteAnimEnabled; // +0x2e5c  OnNewDocument: 1 — gates GameView::CyclePalette
    int         unk2e60;             // +0x2e60  ctor: 0
    int         unk2e64;             // +0x2e64
    WORD        palVersion;          // +0x2e68  ctor: 0x300   } inline LOGPALETTE
    WORD        palNumEntries;       // +0x2e6a  ctor: 0x100   }
    PALETTEENTRY sysPalette[256];    // +0x2e6c  } entries (GetSystemPaletteEntries target)
    BYTE       *pSysColorTable;      // +0x326c  ctor: &DAT_00456230
    Canvas     *pCanvas;             // +0x3270
    RECT        rectViewport;        // +0x3274  ctor: (8,7)-(0x128,0x127) play area
    RECT        rectInventory;       // +0x3284  ctor: (0x133,6)-(0x1e9,0xe6)
    RECT        rectRightPane;       // +0x3294  ctor: (0x1f0,6)-(0x200,0xe6)
    int         nWeaponBoxLeft;      // +0x32a4  ctor: 400
    int         nWeaponBoxTop;       // +0x32a8  ctor: 0xfc
    int         nWeaponBoxRight;     // +0x32ac  ctor: 0x1b0
    int         nWeaponBoxBottom;    // +0x32b0  ctor: 0x11c
    int         unk32b4;             // +0x32b4  ctor: 0x180
    int         unk32b8;             // +0x32b8  ctor: 0xfc
    int         unk32bc;             // +0x32bc  ctor: 0x189
    int         unk32c0;             // +0x32c0  ctor: 0x11c
    int         unk32c4;             // +0x32c4  ctor: 0x1c9
    int         unk32c8;             // +0x32c8  ctor: 0xfb
    int         unk32cc;             // +0x32cc  ctor: 0x1ea
    int         unk32d0;             // +0x32d0  ctor: 0x11c
    int         nViewLeft;           // +0x32d4  ctor: 0    (288x288 view window rect —
    int         nViewTop;            // +0x32d8  ctor: 0     UpdateCamera-proven; was
    int         nViewRight;          // +0x32dc  ctor: 0x120 misnamed nHealthDial*)
    int         nViewBottom;         // +0x32e0  ctor: 0x120
    int         nArrowBoxLeft;       // +0x32e4  ctor: 0x141
    int         nArrowBoxTop;        // +0x32e8  ctor: 0xf6
    int         nArrowBoxRight;      // +0x32ec  ctor: 0x169
    int         nArrowBoxBottom;     // +0x32f0  ctor: 0x11e
    int         bWorldReady;         // +0x32f4
    int         unk32f8;             // +0x32f8  ctor: 0
    int         unk32fc;             // +0x32fc  ctor: 0
    int         nextCameraX;         // +0x3300
    int         nextCameraY;         // +0x3304
    Zone       *pendingZone;         // +0x3308
    int         nSoundEnabled;       // +0x330c
    int         nMusicEnabled;       // +0x3310
    int         healthLo;            // +0x3314  ctor: 1
    int         healthHi;            // +0x3318  ctor: 1
    int         difficulty;          // +0x331c  ctor: 0x32
    int         counter;             // +0x3320  ctor: difficulty copy
    int         gameSpeed;           // +0x3324  ctor: 0x8c, clamped [0x5f,0xb9]
    int         worldSize;           // +0x3328  ctor default 2; demo forces 1
    int         completionCount;     // +0x332c
    int         cameraX;             // +0x3330  ctor: 0x100
    int         cameraY;             // +0x3334  ctor: 0xc0
    int         unk3338;             // +0x3338  ctor: 0
    int         unk333c;             // +0x333c  ctor: 0
    int         unk3340;             // +0x3340
    int         unk3344;             // +0x3344
    short       unk3348;             // +0x3348  ctor: 0
    short       unk334a;             // +0x334a  ctor: 0
    short       weaponState[4];      // +0x334c  ctor: 0,0,0,0
    short       nCurrentAmmo;        // +0x3354  ctor: 0
    char        _pad3356[2];         // +0x3356
    Tile       *pPlayerFrameTile;    // +0x3358  ctor: 0
    Character  *pPlayerChar;         // +0x335c  ctor: 0
    int         unk3360;             // +0x3360  ctor: 0
    int         unk3364;             // +0x3364  ctor: 0
    int         unk3368;             // +0x3368  ctor: 0
    int         unk336c;             // +0x336c  ctor: 0
    int         unk3370;             // +0x3370
    Tile       *pEquippedItem;       // +0x3374  ctor: 0
    int         unk3378;             // +0x3378
    int         bWorldInvalid;       // +0x337c  ctor: 0
    int         genScratch[8];       // +0x3380  worldgen cell scratch (0x3380..0x33a0)
    int         nCurrentGoalItem;    // +0x33a0
    int         unk33a4;             // +0x33a4  ctor: -1
    int         lastCount;           // +0x33a8
    int         highScore;           // +0x33ac
    int         lastScore;           // +0x33b0
    int         unk33b4;             // +0x33b4  ctor: -1
    int         bQuestCellsResident;             // +0x33b8
    CString     installPath;         // +0x33bc  ctor: registry Install Path / drive scan
                                     // sizeof 0x33c0

    // ---- this TU's methods (markers in WorldDoc.cpp) ----
    CDeskcppDoc();                                              // 0x0041a870
    virtual ~CDeskcppDoc();                                     // 0x0041b2f0 (ScalarDtor 0x0041b2d0)
    int  FindAdjacentGateDirMaybe(int x, int y, short *paGrid);   // 0x00419f60
    int  ParseTilesMaybe(CFile *pFile, unsigned int nBytes);      // 0x0041a030 TILE chunk
    unsigned int GetLocatorIconMaybe(int x, int y, int bAlt);     // 0x0041a1c0
    void CacheUiTilePtrsMaybe();                          // 0x0041a5d0
    void DrawPlayer();                                    // 0x0041a6d0
    virtual BOOL OnNewDocument();                         // 0x0041bb10
    virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);    // 0x0041b8a0
    virtual void Serialize(CArchive& ar);                 // (doc TU; vcall slot +8)
    // CDocument overrides whose bodies live in the Worldgen source file (this=World). Declared
    // here (the IMPLEMENT_DYNCREATE TU) so the emitted World vtable slots point at OUR overrides
    // — else /OPT:REF drops them as unreferenced. Existing base slots (no new slot / sizeof
    // change) ⇒ codegen-neutral for this TU's functions.
    virtual BOOL IsModified();                            // 0x00422f40
    virtual void SetModifiedFlag(BOOL bModified = TRUE);  // 0x00422f50

    // ---- command/update handlers referenced by the message map (data @0x44c2d0) ----
    // Bodies live in the GameData & Worldgen source files (this=World); declared here only so
    // BEGIN_MESSAGE_MAP below can take their addresses. Address-only refs (no call sites in this
    // TU) ⇒ codegen-neutral for the WorldDoc functions; they exist to keep the handlers alive
    // under /OPT:REF (the original references them via this map) + reproduce the .rdata array.
    afx_msg void OnToggleSound();                         // 0x004242a0
    afx_msg void OnUpdateToggleSound(CCmdUI *pCmdUI);     // 0x004242f0
    afx_msg void OnToggleMusic();                         // 0x00424310
    afx_msg void OnUpdateToggleMusic(CCmdUI *pCmdUI);     // 0x00424360
    afx_msg void OnNewWorld();                            // 0x00424450
    afx_msg void OnSaveWorld();                           // 0x00424540
    afx_msg void OnLoadWorld();                           // 0x00424fc0
    afx_msg void OnReplayStory();                         // 0x00403620
    afx_msg void OnUpdateFileSave(CCmdUI *pCmdUI);        // 0x00403510
    afx_msg void OnUpdateAppExit(CCmdUI *pCmdUI);         // 0x00403520
    afx_msg void OnUpdateHideMe(CCmdUI *pCmdUI);          // 0x00403550
    afx_msg void OnUpdateNewWorld(CCmdUI *pCmdUI);        // 0x00403580
    afx_msg void OnUpdateLoadWorld(CCmdUI *pCmdUI);       // 0x00403600
    afx_msg void OnUpdateReplayStory(CCmdUI *pCmdUI);     // 0x00403610

protected:
    DECLARE_DYNCREATE(CDeskcppDoc)                              // CreateObject 0x00419ed0
    DECLARE_MESSAGE_MAP()                                 // GetMessageMap 0x00419f50
};

#endif
