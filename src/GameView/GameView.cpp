// GameView TU (0x4084f0-0x418700): the CDeskcppView class (our GameView) plus its embedded
// helper classes (InvScrollBar, TextDialog, the balloon/option dialogs). ONE source file /
// ONE .obj — no exception-COMDAT clusters mark an internal boundary and the functions flow
// contiguously (InvScrollBar's ctor/dtor is interleaved between GameView's DYNCREATE statics
// and GameView::GameView, which MSVC would never do across .objs). Phase E step 4:
// transcribe in .text address order. See docs/game-logic.md for the runtime frame loop.
//
// Cross-TU calls (World::*, GameData helpers, Canvas) reach through Worldgen.h's shared facade;
// the GameView method-decl set (the dial) lives in GameView.h, included via Worldgen.h.
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../Worldgen/Worldgen.h"

// --- WAVMIX32 imports (all __stdcall) -----------------------------------------
extern "C" {
    UINT WINAPI WaveMixPump(void);
    int  WINAPI WaveMixActivate(int hMixSession, BOOL fActivate);
    int  WINAPI WaveMixFreeWave(int hMixSession, int lpMixWave);
    int  WINAPI WaveMixCloseChannel(int hMixSession, int iChannel, DWORD dwFlags);
    int  WINAPI WaveMixCloseSession(int hMixSession);
    int  WINAPI WaveMixPlay(void *lpMixPlayParams);
}
// --- module globals -----------------------------------------------------------
int    g_bStopMusicThread;   // 0x00456134  set nonzero to end the pump loop
HANDLE g_hWaveMixEvent;      // 0x00459454  pump-tick event
int    g_dat459450;          // 0x00459450  cleared by the GameView ctor (music/sound related)
int    g_waveHandles[64];    // 0x00459458..0x00459558  loaded-wave handle table (SoundInit fills)

// =============================================================================
// DYNCREATE + message maps (macro-generated: CreateObject 0x4084f0,
// GetRuntimeClass 0x408560, GameView::GetMessageMap 0x408570,
// InvScrollBar::GetMessageMap 0x408580). Class string is "GameView" not the
// original "CDeskcppView"; that lives in .rdata (not checked by per-function
// verify), same accepted mismatch as WorldDoc's IMPLEMENT_DYNCREATE(World,...).
// =============================================================================
// FUNCTION: YODA 0x004084f0
// FUNCTION: YODA 0x00408560 (GetRuntimeClass)
IMPLEMENT_DYNCREATE(GameView, CView)

// FUNCTION: YODA 0x00408570
BEGIN_MESSAGE_MAP(GameView, CView)
    //{{AFX_MSG_MAP(GameView)
    ON_COMMAND(0x8001, OnCmdMinimize)
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_SETCURSOR()
    ON_WM_MOUSEMOVE()
    ON_WM_ERASEBKGND()
    ON_WM_RBUTTONDOWN()
    ON_WM_KEYDOWN()
    ON_WM_KEYUP()
    ON_WM_TIMER()
    ON_WM_DESTROY()
    ON_WM_HSCROLL()
    ON_COMMAND(ID_APP_EXIT, OnAppExit)
    ON_COMMAND(0x8005, OnCmdDifficulty)
    ON_UPDATE_COMMAND_UI(0x8005, OnUpdateDifficultyUi)
    ON_COMMAND(0x8002, OnTogglePause)
    ON_UPDATE_COMMAND_UI(0x8002, OnUpdatePauseUi)
    ON_COMMAND(0x800c, OnCmdGameSpeed)
    ON_UPDATE_COMMAND_UI(0x800c, OnUpdateGameSpeedUi)
    ON_COMMAND(0x800d, OnCmdWorldSizeMaybe)
    ON_UPDATE_COMMAND_UI(0x800d, OnUpdateWorldSizeUi)
    ON_COMMAND(0x800e, OnCmdStatsMaybe)
    ON_UPDATE_COMMAND_UI(0x800e, OnUpdateStatsUi)
    ON_BN_CLICKED(0x1389, OnDialogCloseBtn)
    ON_BN_CLICKED(0x138a, OnDialogDownBtnNop)
    ON_BN_CLICKED(0x138b, OnDialogUpBtnNop)
    ON_WM_CTLCOLOR()
    ON_WM_CHAR()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00408580
BEGIN_MESSAGE_MAP(InvScrollBar, CScrollBar)
    //{{AFX_MSG_MAP(InvScrollBar)
    ON_WM_VSCROLL()
    //}}AFX_MSG_MAP
END_MESSAGE_MAP()

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408590
// Music pump worker thread (AfxBeginThread proc): keeps WAVMIX32 fed until the
// stop flag is set. __cdecl (static member).
UINT GameView::MusicThreadProcMaybe(void *pParam)
{
    while (g_bStopMusicThread == 0)
    {
        WaveMixPump();
        WaitForSingleObject(g_hWaveMixEvent, 100);
    }
    return 0;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x004085c0
// InvScrollBar::InvScrollBar — build the inventory scroll bar as a child of the
// view, at World's inventory-scrollbar rect (@0x3294), control id 0x65.
InvScrollBar::InvScrollBar(GameView *pView, RECT *pRect)
{
    Create(0x50000001, *pRect, pView, 0x65);
    ::SetScrollRange(m_hWnd, SB_CTL, 0, 1, FALSE);
    ::SetScrollPos(m_hWnd, SB_CTL, 0, FALSE);
    ::ShowScrollBar(m_hWnd, SB_CTL, TRUE);
    scrollMax = 0;
    scrollPos = 0;
}
// Compiler-generated InvScrollBar dtors (0x408690 ??_G / 0x4086b0 ??1) are emitted from
// the class's implicit virtual dtor; their ??_G/??1 split shape needs dtor-modeling work
// (Phase F/G). Left unmarked for now — the ctor (above) and ??_GGameView both match.

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408710
// GameView::GameView — DYNCREATE default ctor. The CString (strCheatBuffer), three
// CBitmapButton (btnDialog*) and CEdit (wndDialogText) members are constructed
// implicitly by the compiler-generated member-init prologue; the body zero/inits the
// plain scalar fields in source order (NOT sorted — matched literally).
// EFFECTIVE (align~94, ~89% identical / 265 vs 264 insns): field-init order is faithful
//   to the decompile. Two known-open residuals, both G1 fodder (NOT source misses):
//   (a) imm/reg STORE-SCHEDULING — MSVC reschedules the run of `= 0`(edi) / `= -1`(ebx)
//       stores and materializes `mov ebx,-1` at a different point than the original; the
//       identical open mechanism parked in the WorldDoc World ctor (DIFF~510). Source
//       order matches; the compiler batches by value/register anyway.
//   (b) member-ctor INLINE-vs-OUT-OF-LINE: one CBitmapButton's embedded CBitmap (@+0x22c)
//       is constructed inline in the original but the compiler chose differently here — a
//       TU-context inlining-threshold artifact (lesson #7/#8), expected to settle as the
//       rest of the TU lands.
GameView::GameView()
{
    unk178 = 0;
    bInvincibleCheat = 0;
    bOneShotStubMaybe = 0;
    bKeyboardMoveActive = 0;
    bIactZoneEntryMaybe = 0;
    bMouseCaptured = 0;
    bMidiProfileInitMaybe = 0;
    bPickupClickPendingMaybe = 0;
    bInvClickPending = 0;
    nTransitionStep = -1;
    nSavedFrameMode = 3;
    pWorld = 0;
    pInvScrollBar = 0;
    bInitialized = 0;
    nPaletteClock = 0;
    frameCounter = 0;
    bDebugFlagMaybe = 0;
    bShiftHeld = 0;
    nDragLastScreenY = -1;
    nDragLastScreenX = -1;
    nMovePending = 0;
    nDragSlot = -1;
    nMoveCommand = -1;
    unkB8_always1 = 1;
    bBusy = 1;
    soundSession = 0;
    g_dat459450 = 0;
    bMapTeleportEnabled = 0;
    bViewActive = 0;
    nGameSpeed = AfxGetApp()->GetProfileInt("OPTIONS", "GameSpeed", 0x8c);
    nDetonatorPhase = 0;
    bPauseOverlayDrawn = 0;
    bDialogCloseClicked = 0;
    unk2e0 = 0;
    bTextDialogShown = 0;
    bDialogClickDismissMaybe = 0;
    bBlockBumpUntilClick = 0;
    pMusicThread = 0;
    unk2d4 = 0;
    unk2d8 = 0;
    artooAnyhowHelpIdx = 0;
    bArtooBeepPending0Maybe = 0;
    bDropOnArtooMaybe = 0;
    bDraggedArtooBlockedMaybe = 0;
    bDropOutsideViewMaybe = 0;
    bLocatorKeyLatchMaybe = 0;
    unk2e8_always1 = 1;
    bShowEmptyDialogOnceMaybe = 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408c60
// GameView::~GameView — stop the music pump, tear down the WAVMIX32 session and all
// loaded waves, free the drag save-bit buffers + drag-tile canvas. The CString /
// CBitmapButton x3 / CEdit members are destroyed by the compiler-generated epilogue.
GameView::~GameView()
{
    g_bStopMusicThread = 1;
    if (soundSession != 0)
    {
        WaveMixActivate(soundSession, FALSE);
        int *p = g_waveHandles;
        do
        {
            if (*p != 0)
                WaveMixFreeWave(soundSession, *p);
            p++;
        } while (p < g_waveHandles + 64);
        WaveMixCloseChannel(soundSession, 8, 1);
        WaveMixCloseSession(soundSession);
        soundSession = 0;
    }
    if (paDragSaveBits != 0)
        delete paDragSaveBits;
    if (paDragSaveBits2 != 0)
        delete paDragSaveBits2;
    if (pDragTileCanvas != 0)
        delete pDragTileCanvas;
}
// Compiler-generated (GameView vtable @0x44b638 slot 1):
// FUNCTION: YODA 0x00408c40 (??_GGameView scalar-deleting dtor)

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00408df0
// GameView::OnActivateView — on deactivate, cancel any in-progress inventory drag
// (only while in play mode, nFrameMode==4); track the active flag; chain to base.
void GameView::OnActivateView(BOOL bActivate, CView *pActivateView, CView *pDeactiveView)
{
    switch (bActivate)
    {
    case 0:
        if (pWorld->nFrameMode == 4 && draggedTile != 0)
        {
            bDragActive = 0;
            UpdateDragCursor(1);
            DrawText(0);
            pWorld->nFrameMode = 3;
        }
        bViewActive = 0;
        break;
    case 1:
        bViewActive = 1;
        break;
    }
    CView::OnActivateView(bActivate, pActivateView, pDeactiveView);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409360
// InvScrollBar::OnVScroll — reflected scroll handler; adjusts scrollPos per the
// SB_* code (clamped to [0, scrollMax]) then repaints the parent view's item list.
// EFFECTIVE-WIP (align~144): structure is faithful to the decompile — the max-clamp
//   `scrollPos = nMax` is ONE shared block living in case SB_LINEDOWN, reached by fall-
//   through there and `goto clampMax` from SB_PAGEDOWN + SB_THUMB*. Two reg-alloc/instr-
//   selection residuals, both TU-phase-sensitive (lesson #6/#7, expect G1 to resolve):
//   (a) our compile caches &scrollPos in EDX (`lea edx,[esi+0x40]`) and accesses [edx],
//       while the original keeps `this` in ESI and accesses [esi+0x40] directly — a pure
//       register-allocation choice driven by scrollPos's use-count across all arms + tail;
//   (b) the max-clamp compare emits `cmp;jle` vs the original's `cmp;jge` (operand order,
//       the canonicalized cmp knob MSVC picks internally — proven not source-steerable).
void InvScrollBar::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    BOOL bChanged;
    int  nMax;
    int  scrl[2];

    int nNew;
    if (this != (InvScrollBar *)pScrollBar)
        return;
    bChanged = FALSE;
    switch (nSBCode)
    {
    case SB_LINEUP:
        nNew = scrollPos - 1;
        scrollPos = nNew;
        if (nNew < 0)
            scrollPos = 0;
        break;
    case SB_LINEDOWN:
        nMax = scrollMax;
        nNew = scrollPos + 1;
        scrollPos = nNew;
        if (nMax < nNew)
        {
clampMax:
            scrollPos = nMax;
        }
        break;
    case SB_PAGEUP:
        nNew = scrollPos - 7;
        scrollPos = nNew;
        if (nNew < 0)
            scrollPos = 0;
        break;
    case SB_PAGEDOWN:
        nMax = scrollMax;
        nNew = scrollPos + 7;
        scrollPos = nNew;
        if (nMax < nNew)
            goto clampMax;
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        scrollPos = nPos;
        if ((int)nPos < 0)
            scrollPos = 0;
        nMax = scrollMax;
        if (nMax < scrollPos)
            goto clampMax;
        break;
    default:
        goto skip;
    }
    bChanged = TRUE;
skip:
    if (bChanged)
    {
        ::GetScrollPos(m_hWnd, SB_CTL);
        ::GetScrollRange(m_hWnd, SB_CTL, &scrl[0], &scrl[1]);
        ::SetScrollPos(m_hWnd, SB_CTL, scrollPos, TRUE);
        GameView *pView = (GameView *)CWnd::FromHandle(::GetParent(m_hWnd));
        if (pView != 0)
            pView->DrawText(0);
    }
}
