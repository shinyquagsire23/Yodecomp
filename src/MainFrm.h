// Frame TU (0x419000–0x419720): CMainFrame, the CFrameWnd-derived main window.
// Pauses/resumes the game on activate & sys-command, tracks the game's saved frame-mode,
// and manages the game palette (realize on paint/palette messages).
#ifndef FRAME_H
#define FRAME_H
#include <afxwin.h>

#ifdef YODA_PORTABLE
// ── 64-bit-safe stub views (H4 — docs/phase-h4-sdl.md lesson 5) ──────────────────────────────
// The MSVC-4.2-pinned stubs in the #else model the view/doc/thread as raw 32-bit pads, which
// cannot hold on LP64. These branches keep the member NAMES MainFrm.cpp reads but use real
// types in the full declarations' order (DeskcppView.h / DeskcppDoc.h / microfx CWinThread),
// so the layout equals the real classes BY CONSTRUCTION on any ABI. Comments carry the 32-bit
// reference offsets. Never constructed here — pointer access only; the virtual-dtor decl on
// CDeskcppView pins its Itanium key function to the full-view TU (lesson 1).
class Zone; class ZoneObj; class Tile; class Canvas;
class InvScrollBar; class TextDialog; struct MusicThread; class FrameWorld;

// = CDeskcppDoc through +0x2c4 (pPalette ≡ CDeskcppDoc::palette); fields past it unused here.
class FrameWorld : public CDocument
{
public:
    int         unk50;               // +0x0050
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
    CObArray    tiles;               // +0x0080
    CObArray    zonesCArr;           // +0x0094
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
    CPalette   *pPalette;            // +0x02c4  (= CDeskcppDoc::palette)
};

// = microfx CWinThread (CCmdTarget base + {m_pMainWnd, m_bAutoDelete, m_hThread}).
struct MusicThread : public CCmdTarget
{
    CWnd       *_pMainWnd;
    BOOL        _bAutoDelete;
    HANDLE      hThread;             // +0x28  (= CWinThread::m_hThread)
};

// = CDeskcppView through +0x2fc (pMusicThread); fields past it unused here.
class CDeskcppView : public CView
{
public:
    virtual ~CDeskcppView();         // key-function pin → vtable stays with DeskcppView.cpp
    InvScrollBar *pInvScrollBar;     // +0x040
    FrameWorld  *pWorld;             // +0x044  (real type: CDeskcppDoc*)
    TextDialog  *pTextDialog;        // +0x048
    int          bBusy;              // +0x04c
    int          bOneShotStubMaybe;  // +0x050
    int          bViewActive;        // +0x054
    int          unk58;              // +0x058
    int          bInitialized;       // +0x05c
    int          bMidiProfileInitMaybe; // +0x060
    int          nSavedFrameMode;    // +0x064
    UINT         nTimerId;           // +0x068
    int          bMouseCaptured;     // +0x06c
    int          bInvClickPending;   // +0x070
    int          bPickupClickPendingMaybe; // +0x074
    int          nMovePending;       // +0x078
    int          bKeyboardMoveActive; // +0x07c
    int          bIactZoneEntryMaybe; // +0x080
    int          nMoveCommand;       // +0x084
    int          bLocatorKeyLatchMaybe; // +0x088
    int          bMapAtCanvasOriginMaybe; // +0x08c
    int          bSkipEntryIactMaybe; // +0x090
    int          bBlinkState;        // +0x094
    int          nMouseX;            // +0x098
    int          nMouseY;            // +0x09c
    UINT         nPaletteClock;      // +0x0a0
    int          bRearmHotspotsMaybe; // +0x0a4
    void        *paDragSaveBits;     // +0x0a8
    void        *paDragSaveBits2;    // +0x0ac
    short        frameCounter;       // +0x0b0
    char         _padb2[2];          // +0x0b2
    UINT         nGameSpeed;         // +0x0b4
    int          unkB8_always1;      // +0x0b8
    int          bInputLocked;       // +0x0bc
    int          bMapTeleportEnabled; // +0x0c0
    int          soundSession;       // +0x0c4
    HCURSOR      hCursor3;           // +0x0c8
    HCURSOR      hCursor9;           // +0x0cc
    HCURSOR      hCursor;            // +0x0d0
    HCURSOR      hCursor2;           // +0x0d4
    HCURSOR      hCursor4;           // +0x0d8
    HCURSOR      hCursor5;           // +0x0dc
    HCURSOR      hCursor7;           // +0x0e0
    HCURSOR      hCursor8;           // +0x0e4
    HCURSOR      hCursor6;           // +0x0e8
    HCURSOR      hCursor10;          // +0x0ec
    HCURSOR      hCursor11;          // +0x0f0
    int          bFireKeyLatchMaybe; // +0x0f4
    int          nFireDirX;          // +0x0f8
    int          nFireDirY;          // +0x0fc
    int          nFireStep;          // +0x100
    int          nPickupX;           // +0x104
    int          nPickupY;           // +0x108
    int          nPickupTileId;      // +0x10c
    ZoneObj     *pPickupObj;         // +0x110
    int          nTargetZoneId;      // +0x114
    int          nTransitionStep;    // +0x118
    int          bShiftHeld;         // +0x11c
    int          bDebugFlagMaybe;    // +0x120
    UINT         nWalkFramePhase;    // +0x124
    int          nMoveDX;            // +0x128
    int          nMoveDY;            // +0x12c
    int          bMapViewOpen;       // +0x130
    Zone        *pMapReturnZone;     // +0x134
    int          bPauseOverlayDrawn; // +0x138
    Canvas      *pDragTileCanvas;    // +0x13c
    Tile        *draggedTile;        // +0x140
    short        nDragSlot;          // +0x144
    char         _pad146[2];         // +0x146
    int          bDragActive;        // +0x148
    int          nDragLastScreenX;   // +0x14c
    int          nDragLastScreenY;   // +0x150
    int          unk154;             // +0x154
    int          nDetonatorPhase;    // +0x158
    int          nDetonatorX;        // +0x15c
    int          nDetonatorY;        // +0x160
    char         _pad164[4];         // +0x164
    int          bTextDialogShown;   // +0x168
    int          bDialogClickDismissMaybe; // +0x16c
    int          nSavedCameraX;      // +0x170
    int          nSavedCameraY;      // +0x174
    int          unk178;             // +0x178
    CString      strCheatBuffer;     // +0x17c
    int          bInvincibleCheat;   // +0x180
    CBitmapButton btnDialogClose;    // +0x184
    CBitmapButton btnDialogDown;     // +0x1e0
    CBitmapButton btnDialogUp;       // +0x23c
    CEdit        wndDialogText;      // +0x298
    int          unk2d4;             // +0x2d4
    int          unk2d8;             // +0x2d8
    int          bDialogCloseClicked; // +0x2dc
    int          unk2e0;             // +0x2e0
    int          bBlockBumpUntilClick; // +0x2e4
    int          unk2e8_always1;     // +0x2e8
    int          artooAnyhowHelpIdx; // +0x2ec
    int          bSuppressWalkSound; // +0x2f0
    int          bWeaponIactActiveMaybe; // +0x2f4
    int          bShowEmptyDialogOnceMaybe; // +0x2f8
    MusicThread *pMusicThread;       // +0x2fc  (real type: void*)

    void DrawText(CDC *pDC);                 // 0x0040f060
    void UpdateDragCursor(int a);            // 0x00412cc0
    void ConfirmExit();                      // 0x00416030
};
#else
// --- minimal stubs for the doc/view the frame drives (real classes elsewhere) ---
class FrameWorld;

// GameView (CDeskcppView) — only the fields/methods the frame touches. vftable elsewhere.
class CDeskcppView
{
public:
    char        _pad00[0x44];
    FrameWorld *pWorld;              // +0x44
    int         _pad48;             // +0x48
    int         bBusy;              // +0x4c  (game paused while the window is inactive/moving)
    char        _pad50[0xf4];       // +0x50
    short       nDragSlot;          // +0x144
    char        _pad146[2];         // +0x146
    int         bDragActive;        // +0x148
    char        _pad14c[0x1b0];     // +0x14c
    struct MusicThread *pMusicThread; // +0x2fc  (+0x28 = the HANDLE)

    void DrawText(CDC *pDC);                 // 0x0040f060 (repaint status/frame text)
    void UpdateDragCursor(int a);            // 0x00412cc0
    void ConfirmExit();                      // 0x00416030
};

// The music playback thread object — only the HANDLE the frame resumes/suspends.
struct MusicThread
{
    char   _pad00[0x28];
    HANDLE hThread;                 // +0x28
};

class FrameWorld
{
public:
    char       _pad00[0x5c];
    int        nFrameMode;          // +0x5c  (1..9,0xb play states; 0xc = shutting down)
    char       _pad60[0x18];
    int        timeBase;            // +0x78
    int        timeOffset;          // +0x7c
    char       _pad80[0x244];
    CPalette  *pPalette;            // +0x2c4
};
#endif // YODA_PORTABLE

extern CString g_strReplayPath;      // 0x00459e20 — defined in this TU (its init thunks live here)

class CMainFrame : public CFrameWnd
{
public:
    char       _pad_bc[0x10];       // +0xbc  (CFrameWnd base ends at 0xbc)
    CPalette  *m_pOldPalette;       // +0xcc  saved palette across realize
    int        _pad_d0;             // +0xd0
    int        m_nSavedFrameMode;   // +0xd4  ctor: -1  (World.nFrameMode parked while inactive)
                                    // sizeof 0xd8

    CMainFrame();                                        // 0x00419090
    virtual ~CMainFrame();                               // 0x00419120

    virtual BOOL PreCreateWindow(CREATESTRUCT &cs);      // 0x00419210
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext); // 0x00419370

    afx_msg void OnGetMinMaxInfo(MINMAXINFO *lpMMI);     // 0x004192d0
    afx_msg void OnSize(UINT nType, int cx, int cy);     // 0x00419320
    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);          // 0x00419340
    afx_msg void OnPaletteChanged(CWnd *pFocusWnd);      // 0x004193f0
    afx_msg void OnPaletteIsChanging(CWnd *pRealizeWnd); // 0x00419460
    afx_msg BOOL OnQueryNewPalette();                    // 0x004194d0
    afx_msg void OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized); // 0x00419540
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus); // 0x004196a0
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);  // 0x00419170
    afx_msg BOOL OnQueryEndSession();                    // 0x004196c0

protected:
    DECLARE_DYNCREATE(CMainFrame)                        // CreateObject 0x00419000 / GetRuntimeClass 0x00419070
    DECLARE_MESSAGE_MAP()                                // 0x00419080
};

#endif

// ── portable time shim (GOAL 4; identical copy in Worldgen.h — keep synced) ─────────────────
// The 1997 CRT signatures the byte-matched TUs re-declare (`extern "C" long time(long*)` —
// Score.cpp/MainFrm.cpp) match VC4.2 + 64-bit-long desktop hosts by luck, but clash with the
// 64-bit time_t of Emscripten AND modern MSVC ucrt. On those (portable builds only — the
// YODA_PORTABLE gate keeps the anchor's tokens intact) redirect the NAMES to 32-bit wrappers
// in microfx (mfxcore.cpp); <time.h> lands first so the real decls exist un-renamed.
#if defined(YODA_PORTABLE) && (defined(__EMSCRIPTEN__) || defined(_WIN32)) && !defined(MFX_TIME32_SHIM)
#define MFX_TIME32_SHIM
#include <time.h>
extern "C" long   mfx_time32(long *);
extern "C" double mfx_difftime32(long, long);
#define time     mfx_time32
#define difftime mfx_difftime32
#endif // MFX_TIME32_SHIM
