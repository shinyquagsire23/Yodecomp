// GameView TU (0x408c60-0x418700, incl. the mislabeled "Core utils" head): the CDeskcppView
// class (our GameView struct) plus the small view-owned classes InvItem / InvScrollBar /
// TextDialog. Promoted out of Worldgen.h in Phase E (v16) so the doc TU and the view TU share
// one authoritative GameView declaration. See docs/game-logic.md for the runtime model.
#ifndef GAMEVIEW_H
#define GAMEVIEW_H
// NOTE: include this ONLY after <afxwin.h> / <afxext.h> (CView, CBitmapButton) and
// ../Records/RecordClasses.h (Tile, Zone, ZoneObj) — Worldgen.h does. It deliberately does NOT
// re-include them: re-including the (guarded) MFC headers emits ~6000 blank lines + #line
// directives that, while token-neutral, perturb the compiler's tie-break state and rotate the
// Worldgen dial by one function. Keeping the physical layout identical holds verify at 34/90.

// Inventory entry (element type of World.inventory): sizeof 0xc (OnLoadWorld: new(0xc)).
class InvItem : public CObject
{
public:
    Tile   *pTile;                   // +0x04
    CString name;                    // +0x08  copied from pTile->name
    InvItem();                                            // 0x004011d0 (first app TU)
    InvItem(Tile *pTile, const char *pszName);            // 0x00401270 (first app TU)
};

// Canvas stub: only what the doc/view TUs touch (real module: src/Canvas/, byte-matched).
class Canvas
{
public:
    HDC  hdc;                        // +0x000  memory DC (CreateCompatibleDC)
    char _pad[0x438];                // no vptr; sizeof == 0x43c
    Canvas(int width, int height);                        // 0x00407df0 (Canvas TU's "Init" —
                                                          //  the guarded new-expr shape in
                                                          //  OnInitialUpdate proves ctor-hood)
    ~Canvas();                                            // 0x00408400 (Canvas TU; non-virtual)
    void *GetData();                                      // 0x00407f50 (Canvas TU)
    UINT  SetPalette(UINT start, UINT count, RGBQUAD *colors); // 0x00407fd0 (Canvas TU)
    int   BitBlt(CDC *dest, int destX, int destY,         // 0x00408000 (Canvas TU)
                 int width, int height, int srcX, int srcY);
    void  Fill(unsigned char value);                      // 0x004080a0 (Canvas TU)
    void  BlitFast(void *src, int flags, short height,    // 0x00408110 (Canvas TU)
                   unsigned short srcStride, short destX, short destY);
    void  BlitMasked(char *src, unsigned short srcStride, short height, // 0x00408240
                     short destX, short destY, char key);
};

class World;
class GameView;
class TextDialog;                    // defined after GameView (needs the fwd decl)

// InvScrollBar stub (sizeof 0x44 = CScrollBar 0x3c + 8; its little TU sits at 0x4085c0,
// between the Canvas TU and the GameView TU head).
class InvScrollBar : public CScrollBar
{
public:
    int  scrollMax;                  // +0x3c
    int  scrollPos;                  // +0x40
    InvScrollBar(GameView *pView, RECT *pRect);           // 0x004085c0 (Ghidra: CtorCreateMaybe)
    void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00409340 (fwd->OnVScroll)
    //{{AFX_MSG(InvScrollBar)
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00409360
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

// GameView (= CDeskcppView, sizeof 0x310): full ctor-era layout from the Ghidra DB (the
// 2026-07-05 reader sweep); the doc TU defines its tail methods (0x426c40-0x429150), while the
// bulk of the class lives in the GameView TU (0x408c60-0x418700). The declaration set below is
// the Phase-E dial: it rotates allocator/cmp tie-breaks across BOTH TUs, so it grows only with
// REAL methods, ordered by .text address.
class GameView : public CView        // sizeof(CView) == 0x40 (m_pDocument @ +0x3c)
{
public:
    InvScrollBar *pInvScrollBar;     // +0x040
    World       *pWorld;             // +0x044
    TextDialog  *pTextDialog;        // +0x048
    int          bBusy;              // +0x04c
    int          bOneShotStubMaybe;  // +0x050
    int          bViewActive;        // +0x054
    int          unk58;              // +0x058
    int          bInitialized;       // +0x05c  OnInitialUpdate one-shot
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
    UINT         nGameSpeed;         // +0x0b4  copied from World.gameSpeed
    int          unkB8_always1;      // +0x0b8
    int          bInputLocked;       // +0x0bc
    int          bMapTeleportEnabled; // +0x0c0
    int          soundSession;       // +0x0c4  0 until SoundInit opens the WAVMIX session
    HCURSOR      hCursor3;           // +0x0c8  res 0x6a
    HCURSOR      hCursor9;           // +0x0cc  res 0x6b
    HCURSOR      hCursor;            // +0x0d0  IDC_ARROW
    HCURSOR      hCursor2;           // +0x0d4  res 0x71
    HCURSOR      hCursor4;           // +0x0d8  res 0x73
    HCURSOR      hCursor5;           // +0x0dc  res 0x6c
    HCURSOR      hCursor7;           // +0x0e0  res 0x72
    HCURSOR      hCursor8;           // +0x0e4  res 0x74
    HCURSOR      hCursor6;           // +0x0e8  res 0x6d
    HCURSOR      hCursor10;          // +0x0ec  res 0x76
    HCURSOR      hCursor11;          // +0x0f0  res 0xc2
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
    Canvas      *pDragTileCanvas;    // +0x13c  32x32
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
    CString      strCheatBuffer;     // +0x17c  (constructed in ctor: CString::CString)
    int          bInvincibleCheat;   // +0x180
    CBitmapButton btnDialogClose;    // +0x184  id 0x1389, CLOSE* bitmaps (CButton+4 CBitmap=0x5c)
    CBitmapButton btnDialogDown;     // +0x1e0  id 0x138a, DNA* bitmaps
    CBitmapButton btnDialogUp;       // +0x23c  id 0x138b, UPA* bitmaps
    CEdit        wndDialogText;      // +0x298  id 0x138c, MS Sans Serif 8 (vft 0x44dcd4)
    int          unk2d4;             // +0x2d4
    int          unk2d8;             // +0x2d8
    int          bDialogCloseClicked; // +0x2dc
    int          unk2e0;             // +0x2e0
    int          bBlockBumpUntilClick; // +0x2e4
    int          unk2e8_always1;     // +0x2e8
    int          artooAnyhowHelpIdx; // +0x2ec
    int          bSuppressWalkSound; // +0x2f0
    int          bWeaponIactActiveMaybe; // +0x2f4  set around UseWeapon's hit + IactRun
    int          bShowEmptyDialogOnceMaybe; // +0x2f8
    void        *pMusicThread;       // +0x2fc
    int          bArtooBeepPending0Maybe; // +0x300
    int          bDropOnArtooMaybe;  // +0x304
    int          bDraggedArtooBlockedMaybe; // +0x308
    int          bDropOutsideViewMaybe; // +0x30c
                                     // sizeof 0x310

    // ============================================================================
    // Method decl set reconstructed in Phase E (v16) — THE DIAL. Sources:
    //  * virtual overrides: GameView vtable (0x44b638) diffed vs base CView vtable
    //    (0x44d4ac) — only 6 slots differ (dtor, PreCreateWindow, OnInitialUpdate,
    //    OnActivateView, OnUpdate, OnDraw); GetRuntimeClass/GetMessageMap/CreateObject
    //    come from the DYNCREATE + message-map macros, not hand decls.
    //  * afx_msg handlers + their MFC-standard signatures: message map @0x44b240
    //    (GetMessageMap 0x408570). Declared in //{{AFX_MSG in message-MAP order.
    //  * plain helpers: per-function disasm sweep (widths read from call-site pushes).
    //    Ones marked (sig?) had their param widths inferred, not yet byte-proven —
    //    refine during transcription. Ordered by .text address.
    // NOTE ON THE DIAL: this is an intermediate point, NOT the fixed point. The exact
    // count in src/Worldgen WILL breathe as this set fills in — that's expected
    // (roadmap G1 resolves it). Do not grind per-function residuals against this.
    // ============================================================================
    DECLARE_DYNCREATE(GameView)                           // CreateObject 0x4084f0 / GetRuntimeClass 0x408560
protected:
    GameView();                                           // 0x00408710 (protected, DYNCREATE ctor)

public:
    // ---- Operations / view helpers (GameView TU, in .text order) ----
    static UINT MusicThreadProcMaybe(void *pParam);       // 0x00408590 (AfxBeginThread proc)
    void PlaySound(int nSoundId);                          // 0x00409060
    // (0x00409340 is InvScrollBar::OnHScroll, not a GameView method — see that class above)
    void DrawZoneCell(short x, short y);                   // 0x00409460 (bare-return guards ⇒ void)
    void DrawZoneCellRect(int x1, int y1, int x2, int y2); // 0x004095d0
    void DrawWholeZone();                                  // 0x00409610
    int  ZoneTransitionStep(short nZoneId, short nStep);   // 0x00409650
    int  WorldEntryStepMaybe(short nZoneId, short nStep);  // 0x00409c10
    void DrawGameArea(CDC *pDC);                           // 0x0040a200
    void BlitTile(short y, short x, int nUnused, Tile *pTile); // 0x0040a320 (byte-proven v19)
    void DrawTileAt(short x, short y, short frame);        // 0x0040a3a0
    static int IsUsableTileMaybe(short tileId);            // 0x0040a620 (__cdecl static)
    void FireWeaponStep(int nStep);                        // 0x0040a710
    void DrawEntities();                                   // 0x0040b160
    short FindEntityAt(int x, int y);                      // 0x0040b210
    void Tick();                                           // 0x0040b270 (10.8KB frame loop)
    void StepDetonatorEffect();                            // 0x0040e400
    int  ApplyHotspotCamera(ZoneObj *pObj);               // 0x0040e500
    int  TransitionZoneScript(int nUnused, int nZoneId);   // 0x0040e750
    int  TransitionZoneXWing(ZoneObj *pObj);              // 0x0040e7c0
    void TransitionZoneDoor(ZoneObj *pDoor);              // 0x0040e9d0
    void ReenableHotspotObjects();                        // 0x0040ebe0
    int  TriggerHotspotsMaybe();                          // 0x0040ec30 (was DrawObjects; fires vehicle/xwing hotspots at the camera tile)
    void UpdateItemObjectsMaybe();                        // 0x0040ed90 (was DrawMap; picks up / re-places item-object tiles)
    void DrawText(CDC *pDC);                              // 0x0040f060
    void ShowWinMessage(int x, int y, int dx, int dy);   // 0x0040f4b0
    int  ClassifyTile(int x, int y);                     // 0x0040fca0
    void OnDragItem(int x, int y, Tile *pTile);          // 0x004102d0
    void ScrollZoneTransition();                         // 0x00411180
    void SoundInit();                                    // 0x00411520
    void UpdateDragCursor(int bClear);                   // 0x00412cc0
    void EmptyFrameHookMaybe();                          // 0x00413be0 (real but empty stub, called from OnTimer)
    void SoundFlush();                                   // 0x00413bf0
    void RedrawPlayerCellMaybe();                        // 0x00413dd0
    void OnBumpTile(int dx, int dy);                     // 0x00413df0
    void UpdatePlayerWalkFrame();                        // 0x004150a0
    void CheckCheat();                                   // 0x00415820
    void CyclePalette();                                 // 0x00415af0
    void ConfirmExit();                                  // 0x00416030
    // tail block (doc TU, 0x426c40-0x429150; transcribed in src/Worldgen)
    void DrawDirectionArrows(CDC *pDC);                  // 0x004270f0
    int  ShowTextDialog(CString &strText, int a, int b, int c); // 0x00427310
    void DrawHealthDial(CDC *pDC);                        // 0x00427490
    void AddHealth(int nDelta);                           // 0x00427690
    void DrawHealthNeedle(CDC *pDC);                      // 0x004278a0
    void UseWeapon(int x, int y, int dx, int dy, int nStep); // 0x00427d20
    void DetonateAdjacentTiles(int x, int y);             // 0x00428680
    void DrawWeaponBox(CDC *pDC);                          // 0x00428ac0
    void DrawWeaponIcon(CDC *pDC);                         // 0x00428c40
    void BlitViewportDither();                            // 0x00428e30
    void AddItemToInv(Tile *pTile);                       // 0x00428f50
    void RemoveItem(Tile *pTile);                         // 0x00429150

    // ---- Overrides (ClassWizard virtuals; address order) ----
    virtual void OnActivateView(BOOL bActivate, CView *pActivateView, CView *pDeactiveView); // 0x00408df0
    virtual void OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint); // 0x00408e70
    virtual void OnDraw(CDC *pDC);                         // 0x00409110
    virtual void OnInitialUpdate();                       // 0x00426c40
    virtual BOOL PreCreateWindow(CREATESTRUCT &cs);       // 0x00428f30

    // ---- Implementation ----
protected:
    virtual ~GameView();                                  // 0x00408c60 (ScalarDtor 0x408c40)

    // ---- Message handlers (//{{AFX_MSG order == BEGIN_MESSAGE_MAP @0x44b240) ----
    //{{AFX_MSG(GameView)
    afx_msg void OnCmdMinimize();                         // 0x00428aa0  WM_COMMAND 0x8001
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point); // 0x00411730
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);   // 0x00412250
    afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message); // 0x004131a0
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);   // 0x00413580
    afx_msg BOOL OnEraseBkgnd(CDC *pDC);                   // 0x00413b20
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point); // 0x00413c10
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags); // 0x004150f0
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);   // 0x00415a50
    afx_msg void OnTimer(UINT nIDEvent);                   // 0x0040d470
    afx_msg void OnDestroy();                              // 0x00415ac0
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00415ff0
    afx_msg void OnAppExit();                              // 0x00416110  ID_APP_EXIT
    afx_msg void OnCmdDifficulty();                        // 0x00416120  cmd 0x8005
    afx_msg void OnUpdateDifficultyUi(CCmdUI *pCmdUI);     // 0x004165b0
    afx_msg void OnTogglePause();                          // 0x00416220  cmd 0x8002
    afx_msg void OnUpdatePauseUi(CCmdUI *pCmdUI);          // 0x004162a0
    afx_msg void OnCmdGameSpeed();                         // 0x00416310  cmd 0x800c
    afx_msg void OnUpdateGameSpeedUi(CCmdUI *pCmdUI);      // 0x00416460
    afx_msg void OnCmdWorldSizeMaybe();                    // 0x004164d0  cmd 0x800d (demo-disabled)
    afx_msg void OnUpdateWorldSizeUi(CCmdUI *pCmdUI);      // 0x004165a0
    afx_msg void OnCmdStatsMaybe();                        // 0x00416620  cmd 0x800e (demo-disabled)
    afx_msg void OnUpdateStatsUi(CCmdUI *pCmdUI);          // 0x00416800
    afx_msg void OnDialogCloseBtn();                       // 0x00416a60  BN 0x1389
    afx_msg void OnDialogDownBtnNop();                     // 0x00416a80  BN 0x138a
    afx_msg void OnDialogUpBtnNop();                       // 0x00416a70  BN 0x138b
    afx_msg HBRUSH OnCtlColor(CDC *pDC, CWnd *pWnd, UINT nCtlColor); // 0x00416a90  WM_CTLCOLOR
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags); // 0x00416ae0
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
};

// TextDialog (sizeof 0xc8): the modal in-game text/dialogue box. NOT MFC-derived (its dtor
// destroys only the text CString and calls no base dtor). Ctor/Run live in the GameView TU;
// the implicit ~TextDialog is emitted per-TU (the doc TU's copy = 0x427440, used by
// ShowTextDialog's stack instance).
class TextDialog
{
public:
    char    _pad00[0x10];            // +0x00
    int     unk10;                   // +0x10  ShowTextDialog arg a
    int     unk14;                   // +0x14  ShowTextDialog arg b
    char    _pad18[0x3c];            // +0x18
    int     unk54;                   // +0x54  ShowTextDialog arg c
    char    _pad58[0x60];            // +0x58
    CString strText;                 // +0xb8
    char    _padbc[4];               // +0xbc
    GameView *pParentView;           // +0xc0  (Ghidra name, backported v15)
    int     soundSession;            // +0xc4  copied from GameView.soundSession
                                     // sizeof 0xc8
    TextDialog(GameView *pView);                          // 0x00416b90 (GameView TU)
    int Run();                                            // 0x00416c40 (GameView TU)
};

// Three near-identical slider option dialogs embedded in the GameView TU
// (0x417e50-0x4186e0). Each = CDialog + an int m_nValue@0x5c, an OnInitDialog that
// sizes a scrollbar control to the setting's range and seeds the thumb from m_nValue,
// and an OnHScroll that steps the slider (SB_LINE/PAGE/THUMB) and stores back into
// m_nValue. All demo-disabled (their ON_UPDATE_COMMAND_UI handlers grey the items).
// Class names follow the Ghidra namespaces (by the OnCmd* handler that runs each).
class DifficultyDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    DifficultyDlg(CWnd *pParent);                        // 0x00417e50 (template 0x6f)
    virtual BOOL OnInitDialog();                         // 0x00417f50 (ctrl 0x67, 1..100)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00417fa0
    DECLARE_MESSAGE_MAP()
};

class GameSpeedDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    GameSpeedDlg(CWnd *pParent);                         // 0x00418130 (template 0xd7)
    virtual BOOL OnInitDialog();                         // 0x00418230 (ctrl 0x8f, 1..0x5a)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00418280
    DECLARE_MESSAGE_MAP()
};

class WorldSizeDlg : public CDialog
{
public:
    int m_nValue;                    // +0x5c  slider value
    WorldSizeDlg(CWnd *pParent);                         // 0x00418410 (template 0xda)
    virtual BOOL OnInitDialog();                         // 0x00418510 (ctrl 0x90, 1..3)
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar); // 0x00418560
    DECLARE_MESSAGE_MAP()
};

#endif
