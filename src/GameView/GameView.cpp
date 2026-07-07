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
// WAVMIX32 MIXPLAYPARAMS (packed — a WORD then 4-byte fields at unaligned offsets).
#pragma pack(push, 1)
struct MIXPLAYPARAMS
{
    WORD wSize;         // +0x00
    int  hMixSession;   // +0x02
    int  iChannel;      // +0x06
    int  lpMixWave;     // +0x0a
    int  hWndNotify;    // +0x0e
    int  dwFlags;       // +0x12
    WORD wReserved;     // +0x16   (struct size 0x18)
};
#pragma pack(pop)

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
// FUNCTION: YODA 0x00408e70
// GameView::OnUpdate — partial-repaint dispatcher keyed on lHint: 0=full RedrawWindow,
// 1=item text, 2=weapon box+icon, 3=repaint the game area through a temp DC, 4=health
// dial+needle, 399=first-time init (bind doc, start sound+music, allocate the two
// drag-save bit buffers sized to the screen depth — OOM aborts via THROW_LAST/AfxAbort).
// EFFECTIVE: the case-399 body + the hand-expanded TRY/CATCH_ALL(e)+THROW_LAST+dead-AfxAbort
//   OOM path (engine-bug #7) are BYTE-IDENTICAL to the original; the dispatch cases and all
//   calls match too. Residual is the block-layout open problem: the original sinks the single
//   function epilogue to the physical END (after the shared ReleaseDC tail) and jmps every case
//   to it, while our compile emits the epilogue right after case 0 and cross-jumps the rest —
//   which also shifts how the shared ReleaseDC tail is materialized. Same unmapped mechanism as
//   WorldDoc::GetLocatorIcon; not source-steerable (flat if/else-if vs nested both ~285). G1.
void GameView::OnUpdate(CView *pSender, LPARAM lHint, CObject *pHint)
{
    if (lHint == 0)
    {
        RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    }
    else if (lHint == 1)
    {
        DrawText(0);
    }
    else if (lHint == 2)
    {
        DrawWeaponBox(0);
        DrawWeaponIcon(0);
    }
    else
    {
        HWND hWnd;
        HDC  hdc;
        if (lHint == 3)
        {
            hdc = ::GetDC(m_hWnd);
            CDC *pDC = CDC::FromHandle(hdc);
            CPalette *pOld = pDC->SelectPalette(pWorld->pPalette, FALSE);
            DrawGameArea(pDC);
            pDC->SelectPalette(pOld, FALSE);
            hdc = pDC->m_hDC;
            hWnd = m_hWnd;
        }
        else
        {
            if (lHint == 4)
            {
                DrawHealthDial(0);
                DrawHealthNeedle(0);
                return;
            }
            if (lHint != 399)
                return;
            if (pWorld == 0)
                pWorld = (World *)m_pDocument;
            if (soundSession == 0)
            {
                SoundInit();
                if (pWorld->nMusicEnabled == 1)
                    PlaySound(0x3d);
            }
            nTransitionStep = 0;
            HDC hdcScreen = ::GetDC(NULL);
            if (hdcScreen == 0)
                return;
            int nBytes = GetDeviceCaps(hdcScreen, BITSPIXEL) / 8 << 10;
            TRY {
                paDragSaveBits = new BYTE[nBytes];
                paDragSaveBits2 = new BYTE[nBytes];
            }
            }
            catch (CException *e)
            {
                _afxExceptionLink.m_pException = e;
                AfxMessageBox(0xe01e, 0, (UINT)-1);
                THROW_LAST();
                AfxAbort();                        // sic: dead code after THROW_LAST (#7)
            }
            }
            hWnd = NULL;
            hdc = hdcScreen;
        }
        ::ReleaseDC(hWnd, hdc);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409060
// GameView::PlaySound — gate the effect by sound/music-enabled flags (ids 0x37 & 0x3a-0x3f
// are music, gated by nMusicEnabled; others by nSoundEnabled; id 3 muted while the walk-sound
// suppression flag is set) then submit it to WAVMIX32. The wave handle comes from the loaded
// table; ids >= 0x40 map only 0x25 (else lpMixWave is left uninitialized — sic, engine quirk).
// EFFECTIVE (48/48 insns, byte_diff 21): logic + MIXPLAYPARAMS layout exact. Residual is a
//   register-allocation tie-break — the original keeps nSoundEnabled in EAX (reused by the
//   else-branch sound gate) and pWorld in EDX; our compile assigns them the opposite registers,
//   a consistent bijection that propagates (identity_miss=9). Not source-steerable (goto/epilogue
//   variants gave the identical score); a TU-phase reg-alloc residual, G1 fodder.
void GameView::PlaySound(int nSoundId)
{
    MIXPLAYPARAMS mix;
    World *pW = pWorld;
    if (pW->nSoundEnabled == 0 && pW->nMusicEnabled == 0)
        return;
    if (nSoundId == 3 && bSuppressWalkSound != 0)
        return;
    if (nSoundId == 0x37 || (nSoundId >= 0x3a && nSoundId <= 0x3f))
    {
        if (pW->nMusicEnabled == 0)
            return;
    }
    else if (pW->nSoundEnabled == 0)
        return;
    int session = soundSession;
    if (session == 0)
        return;
    mix.hMixSession = session;
    mix.iChannel = 0;
    mix.wSize = 0x18;
    if (nSoundId < 0x40)
        mix.lpMixWave = g_waveHandles[nSoundId];
    else if (nSoundId == 0x25)
        mix.lpMixWave = g_waveHandles[5];
    mix.wReserved = 0;
    mix.dwFlags = 2;
    mix.hWndNotify = 0;
    WaveMixPlay(&mix);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409110
// GameView::OnDraw — the main frame paint: lazy-load the .wld save state once, select the
// world palette, bevel-border the viewport/inventory/scrollbar rects, draw the HUD (health
// dial+needle, direction arrows, item text, weapon box+icon), blit the 288x288 canvas, then
// on the very first paint lazy-run World::Load (the .dta worldgen) — aborting on failure.
// EFFECTIVE (logic/structure faithful; the World::Load tail is byte-identical to the original):
//   our compile hoists the constant 0 into EDI (reused across the ~7 `push 0`/FALSE arg sites:
//   SelectPalette x2, DrawRect bRaised x3, BitBlt srcX/srcY), which pulls EBP in as a 4th
//   callee-saved register (`push ebp`) and cascades a register-rename through the middle. The
//   original pushes 0 immediates and uses only EBX/ESI/EDI. Pure enregistration-heuristic
//   tie-break (lesson #7/#8) — not source-steerable; expected to settle in G1.
void GameView::OnDraw(CDC *pDC)
{
    CRect rc;
    if (pWorld == 0)
        pWorld = (World *)m_pDocument;
    if (pWorld == 0)
        return;
    if (pWorld->bStateFileLoadedMaybe == 0)
    {
        pWorld->bStateFileLoadedMaybe++;
        pWorld->LoadWorldStateFile();
    }
    CPalette *pOldPalette = pDC->SelectPalette(pWorld->pPalette, FALSE);
    RealizePalette(pDC->m_hDC);
    GetClientRect(&rc);
    rc.left = pWorld->rectUnk3274.left - 3;
    rc.top = pWorld->rectUnk3274.top - 3;
    rc.right = pWorld->rectUnk3274.right + 3;
    rc.bottom = pWorld->rectUnk3274.bottom + 3;
    DrawRect(pDC, &rc, 0, 3);
    rc.left = pWorld->rectUnk3284.left - 2;
    rc.right = pWorld->rectUnk3284.right + 2;
    rc.top = pWorld->rectUnk3284.top - 2;
    rc.bottom = pWorld->rectUnk3284.bottom + 2;
    DrawRect(pDC, &rc, 0, 2);
    rc.left = pWorld->rectInvScrollMaybe.left - 2;
    rc.right = pWorld->rectInvScrollMaybe.right + 2;
    rc.top = pWorld->rectInvScrollMaybe.top - 2;
    rc.bottom = pWorld->rectInvScrollMaybe.bottom + 2;
    DrawRect(pDC, &rc, 0, 2);
    DrawHealthDial(pDC);
    DrawDirectionArrows(pDC);
    DrawHealthNeedle(pDC);
    DrawText(pDC);
    DrawWeaponBox(pDC);
    DrawWeaponIcon(pDC);
    if (pWorld->pCanvas != 0)
    {
        int srcX, srcY;
        if (bMapAtCanvasOriginMaybe == 1)
        {
            srcX = 0;
            srcY = 0;
        }
        else
        {
            srcY = pWorld->nViewTop;
            srcX = pWorld->nViewLeft;
        }
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                0x120, 0x120, srcX, srcY);
        pDC->SelectPalette(pOldPalette, FALSE);
        if (pWorld != 0 && pWorld->bDtaLoadedMaybe == 0)
        {
            pWorld->nFrameMode = 7;
            pWorld->nMapChangeReason = 1;
            bBusy = 0;
            pWorld->bDtaLoadedMaybe++;
            if (pWorld->Load() == 0)
            {
                AfxMessageBox(0xe01d, 0x10, (UINT)-1);
                pWorld->OnCloseDocument();
                AfxAbort();
            }
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409340
// InvScrollBar::OnHScroll — the inventory bar reuses its vertical handler for horizontal
// scroll (GameView::OnHScroll forwards WM_HSCROLL here). Plain tail-forward (no frame).
void InvScrollBar::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
    OnVScroll(nSBCode, nPos, pScrollBar);
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

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409460
// GameView::DrawZoneCell — composite one map cell (zone coords x,y) into the back-
// buffer canvas. Layer 0 is the opaque ground (always BlitFast); layers 1 & 2 are
// the overlay tiles (masked blit when the tile's transparency flag @+0x404 bit0 is
// set, otherwise opaque). Bounds-guarded against the current zone's extent — note
// the asymmetry the 1997 code has: valid x is [0,width) but valid y is [0,height].
// void return: every guard is a bare `return;` and no epilogue sets EAX (Ghidra's
// "int"/longlong return + phantom param_2 were __thiscall ABI-confusion artifacts).
//
// Register/CSE notes for matching: the guard caches pWorld->currentZone (ECX) and the
// LAYER-0 GetTile reuses that cached pointer, while layers 1 & 2 reload
// pWorld->currentZone fresh — hence pZone-> for layer 0 vs pWorld->currentZone-> for
// 1 & 2. x<<5 / y<<5 (the destination pixel coords, 16-bit because destX/destY are
// short) are hoisted once into ESI/EBX and shared across all three layers' blits.
void GameView::DrawZoneCell(short x, short y)
{
    Tile *pTile;
    short sx, sy;

    // currentZone is evaluated lazily inside the guard (after x<0) and CSE'd across the
    // width/height tests AND layer 0's GetTile — one load held in a register; layers 1 & 2
    // reload it because the intervening blit calls clobber the caller-saved register.
    if (x < 0 || pWorld->currentZone->width <= x ||
        y < 0 || pWorld->currentZone->height < y)
        return;
    sx = x << 5;                     // destination pixel coords, held persistently
    sy = y << 5;
    // layer 0 — opaque ground (tile id is a signed short: -1 = empty ⇒ movsx)
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 0));
    if (pTile != 0)
        pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    // layer 1 — overlay
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 1));
    if (pTile != 0)
    {
        if (pTile->flags & 1)
            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
        else
            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    }
    // layer 2 — overlay
    pTile = pWorld->GetTileData((short)pWorld->currentZone->GetTile(x, y, 2));
    if (pTile != 0)
    {
        if (pTile->flags & 1)
            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
        else
            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x004095d0
// GameView::DrawZoneCellRect — repaint the rectangular block of cells [x1..x2] ×
// [y1..y2] (inclusive), y-outer / x-inner. int args pass straight to DrawZoneCell's
// short params (the compiler pushes the full dword; the callee reads the low word).
// EFFECTIVE (align=22, 27/27 insns, control flow identical): the residual is a pure
//   register-role ROTATION — the original assigns this→EBP (least-used ⇒ last-choice
//   reg), y→EDI, x2→EBX, x→ESI; ours rotates them differently, which reschedules the
//   4 callee-saved pushes and the incoming-arg loads (hence the [esp+0x18] vs [esp+
//   0x10] offset drift). Plus the compiler-emitted inner-loop entry guard picks
//   `cmp x,x2; jg` (orig) vs our `cmp x2,x; jl` — the canonicalized cmp-direction knob
//   MSVC selects internally (lesson #6, proven not source-steerable). No source shape
//   flips `this` out of the last-choice register here; a G1 permuter/dial pass resolves
//   it. Decl order x,y↔y,x only trades reg_pen (both stay align=22).
void GameView::DrawZoneCellRect(int x1, int y1, int x2, int y2)
{
    int y, x;

    for (y = y1; y <= y2; y++)
        for (x = x1; x <= x2; x++)
            DrawZoneCell(x, y);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409610
// GameView::DrawWholeZone — repaint every cell of the current zone, back-to-front
// (both loops count down from width-1). Uses the zone WIDTH for both axes; zones are
// square (18×18) so this covers the whole grid. width-1 is hoisted into EBP and
// copied into the outer (y) counter + reset into the inner (x) counter each row.
// EFFECTIVE (align=0, reg_pen=4, 27/27 insns): structure is byte-exact; the sole
//   residual is the outer(y)/inner(x) counters landing in swapped registers (orig
//   y→EBX x→ESI, ours the reverse). This function was BYTE-EXACT under the prior dial
//   (before DrawZoneCell's currentZone-CSE form landed) — it is now PHASE-DISPLACED by
//   the very edit that made DrawZoneCell exact (they can't both be exact under one
//   global dial; the 361-byte DrawZoneCell wins). Decl-order permutations (x,y / y,x /
//   m-first) only trade reg_pen; none re-flip it. G1 joint pass resolves it.
void GameView::DrawWholeZone()
{
    int x, y;
    int m = pWorld->currentZone->width - 1;

    for (y = m; y >= 0; y--)
        for (x = m; x >= 0; x--)
            DrawZoneCell(x, y);
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409650
// GameView::ZoneTransitionStep — one step of the square-wipe zone transition,
// driven by OnTimer frame-mode 6 (nStep 0..10):
//   steps 0..4  — paint one shrinking ring of black 32x32 tiles into the canvas
//                 (top/bottom/left/right edges per iteration), then blit the whole
//                 288x288 viewport to the screen;
//   step  5     — the actual switch: currentZone = GetZoneById(nZoneId) (restoring
//                 the old zone if the id is bad), locate the new zone in the 10x10
//                 map grid (world-map player position + ZONE_TYPE 7 flagA), run
//                 IACT zone-entry triggers 4 and 5 (bIactZoneEntryMaybe wraps
//                 trigger 5), set frame mode 6 (or keep 0xb), DrawEntities +
//                 DrawWholeZone, mark the grid cell visited (flagSolved);
//   steps 6..9  — blit an expanding centered square of the (redrawn) viewport;
//   step 10     — DrawDirectionArrows + a final IACT trigger 5 (skipped when
//                 bSkipEntryIactMaybe), frame mode 6 / kept.
// Returns 0 when trigger 5 at step 5 reported a warp (0x800) that trigger 4 hadn't
// already claimed (caller restarts the transition); else 1.
// The black source tile is a heap Tile allocated under TRY/CATCH_ALL every call —
// the catch is the recurring dead OOM box (AfxMessageBox(0xe01e)+AfxAbort).
// sic: engine-bugs.md #13 — step 10's nMask is READ UNINITIALIZED when its IactRun
// is skipped (bSkipEntryIactMaybe set or world invalid): the &4 / &0x20 redraw
// tests then act on stack garbage.
//
// EFFECTIVE MATCH (441/441 insns, align=48, reg_pen=7 — ~97% identical; the raw
// byte-diff lies here: a 4-byte frame delta shifts every tail EBP offset). Structure
// proven: register roles (i→EBX, sy→EDI, ESI=&pWorld), all four blit sites, both
// loops, the whole step-5/10 logic and the duplicated cleanup arms are insn-identical.
// Residual autopsy (all probed; none source-steerable — G1 fodder):
//  * KEY CRACKS that got here (reuse for WorldEntryStepMaybe): (a) NO `x` local for
//    `sx + i` — the original's slot -0x40 is the compiler's OWN CSE temp (written +
//    reloaded); declaring `short x` promoted it to a register, cascaded sy into a
//    slot, and made the invariant `span + sy` get LICM-hoisted out of the loop
//    (align 188 → 48 from removing it). (b) `span` IS a real local (slot -0x1c),
//    summed at the call site each iteration.
//  * blit4 window: orig emits `movsx ebx,bx` before `add bx,0x20` (maintains (int)i
//    for the `i + sy` leas, base=EBX) and increments BETWEEN the y/x pushes; ours
//    picks lea base=EDI, drops the movsx and increments before the pushes. Probed:
//    sy+i vs i+sy (inert), (int)i+sy (worse), cnt--/i+= order swap (much worse) —
//    IL value-numbering, not source-steerable.
//  * head: orig calls GetDC via `mov ebx,[__imp_GetDC]; call ebx` — UNIQUE in the
//    binary (OnUpdate/DrawDirectionArrows/... all `call [iat]`). Probed: member
//    GetDC()/ReleaseDC(pDC) (identical output — kept), the explicit
//    FromHandle-of-::GetDC spelling (identical), HDC local (worse). The EBX free
//    there is what hands the original nStep→BX at the ladder; ours converges to the
//    same roles by the loop anyway.
//  * 1-insn scheduling drift of `xor di,di` (nMask = 0 emitted before vs after the
//    unk50 store), nSavedMode in DX vs AX, and the 4B-smaller frame (ours packs the
//    cnt/sx2 word slots where the orig dword-spaces them).
int GameView::ZoneTransitionStep(short nZoneId, short nStep)
{
    Tile *pTile;
    TRY {
        pTile = new Tile;
    }
    }              // closes the try block the TRY macro opened
    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
        _afxExceptionLink.m_pException = e;
        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: dead OOM dialog
        AfxAbort();                            //      (docs/engine-bugs.md #7)
    }
    }              // closes the TRY macro's outer (link-scope) brace
    unsigned char *pPixels = pTile->pixels;
    memset(pPixels, 0, 0x400);
    short nCellX = (short)pWorld->playerX;     // world-map grid coords; conditionally
    short nCellY = (short)pWorld->playerY;     // re-derived by step 5's grid search
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int bAborted = 0;
    if (nStep < 5)
    {
        short sx = (short)pWorld->nViewLeft + nStep * 32;
        short sy = (short)pWorld->nViewTop + nStep * 32;
        short n = 9 - nStep * 2;               // ring edge length, in tiles
        if (n > 0)
        {
            short i = 0;
            short span = (n - 1) * 32;
            short sx2 = span + sx;
            short cnt = n;
            do
            {
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx + i, sy);    // top
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx + i,
                                          span + sy);                                // bottom
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx, i + sy);    // left
                pWorld->pCanvas->BlitFast(pPixels, 0x20, 0x20, 0x20, sx2, i + sy);   // right
                i += 32;
                cnt--;
            } while (cnt != 0);
        }
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left, pWorld->rectUnk3274.top,
                                0x120, 0x120, pWorld->nViewLeft, pWorld->nViewTop);
    }
    else if (nStep == 5)
    {
        Zone *pOldZone = pWorld->currentZone;
        pWorld->currentZone = pWorld->GetZoneById(nZoneId);
        if (pWorld->currentZone == NULL)
            pWorld->currentZone = pOldZone;
        if (pWorld->bWorldInvalidMaybe == 0)
        {
            short y = 0;
            nCellX = -1;
            nCellY = -1;
            do
            {
                short x = 0;
                do
                {
                    if (pWorld->GetZoneById(pWorld->GetZoneCell(x, y)) == pWorld->currentZone)
                    {
                        nCellX = x;
                        nCellY = y;
                        break;
                    }
                    x++;
                } while (x < 10);
                y++;
            } while (y < 10);
            if (nCellX >= 0)
            {
                if (nCellY >= 0)
                {
                    pWorld->playerX = nCellX;
                    pWorld->playerY = nCellY;
                }
                if (nCellX >= 0 && nCellY >= 0 &&
                    pWorld->mapGrid[nCellX + nCellY * 10].zoneType == 7)
                    pWorld->mapGrid[nCellX + nCellY * 10].flagA = 1;
            }
        }
        pWorld->unk50 = nZoneId;
        unsigned short nMask = 0;
        pWorld->UpdateCamera();
        pWorld->RefreshZone();
        short nSavedMode = (short)pWorld->nFrameMode;
        if (pWorld->bWorldInvalidMaybe == 0)
        {
            nMask = (unsigned short)pWorld->currentZone->IactRun(4, 0, 0, 0, 0, 0,
                                                                 pDC, pWorld, this);
            if (nMask & 0x800)
                bAborted = 1;
        }
        bIactZoneEntryMaybe = 1;
        if (pWorld->bWorldInvalidMaybe == 0)
            nMask |= (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                  pDC, pWorld, this);
        bIactZoneEntryMaybe = 0;
        if (nMask & 4)
            pWorld->UpdateCamera();
        if (nSavedMode != 0xb)
            pWorld->nFrameMode = 6;
        else
            pWorld->nFrameMode = 0xb;
        pWorld->currentZone->activatedFlag = 1;
        if (pWorld->bWorldInvalidMaybe == 0)
            DrawEntities();
        DrawWholeZone();
        pWorld->mapGrid[nCellX + nCellY * 10].flagSolved = 1;
        if (bAborted == 0 && (nMask & 0x800))
        {
            pDC->SelectPalette(pOldPal, 0);
            ReleaseDC(pDC);
            delete pTile;
            return 0;
        }
    }
    else if (nStep == 10)
    {
        DrawDirectionArrows(pDC);
        short nSavedMode = (short)pWorld->nFrameMode;
        unsigned short nMask;                  // sic: uninitialized when the IactRun
                                               //      below is skipped (engine-bugs #13)
        if (bSkipEntryIactMaybe == 0 && pWorld->bWorldInvalidMaybe == 0)
            nMask = (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                 pDC, pWorld, this);
        if (nMask & 4)
            pWorld->UpdateCamera();
        bSkipEntryIactMaybe = 0;
        if (nSavedMode != 0xb)
            pWorld->nFrameMode = 6;
        else
            pWorld->nFrameMode = nSavedMode;
        if (nMask & 0x20)
            DrawWholeZone();
    }
    else
    {
        short nOff = (10 - nStep) * 32;
        short nSize = (nStep - 1) * 32;
        pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left + nOff,
                                pWorld->rectUnk3274.top + nOff, nSize - nOff, nSize - nOff,
                                pWorld->nViewLeft + nOff, pWorld->nViewTop + nOff);
    }
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    delete pTile;
    return 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x00409c10
// GameView::WorldEntryStepMaybe — twin of ZoneTransitionStep, driven by OnTimer
// frame-mode 0xb (fresh world entry): steps 0..4 are NO-OPS (no black ring — the
// whole body sits under `if (nStep >= 5)`); step 5 = the same zone switch/IACT 4+5
// sequence but with nTransitionStep = -1 instead of the mode save/restore, and the
// grid-cell visited mark GUARDED by the found coords (unlike ZoneTransitionStep's
// unguarded store); steps 6..9 = the expanding reveal blit; step 10 = direction
// arrows + final IACT 5, then chains into mode 6 (nMapChangeReason = 1,
// nTransitionStep = 0). Returns 0 on an unclaimed warp (0x800) from trigger 5.
// sic: engine-bugs.md #13 (same bug as ZoneTransitionStep): step 10's nMask is
// read uninitialized when its IactRun is skipped — here the compiler even emits an
// explicit `mov di,[ebp-0x18]` load of the never-written slot on the skip path.
// NOTE the head here vindicates ZoneTransitionStep's parked head residuals: THIS
// function's original calls GetDC directly via `call [__imp_GetDC]`, loads nStep
// into DI and pWorld into EAX (cx/ax word loads) — exactly the shapes our compiler
// produces for both functions. The memset has no pixel-pointer local (nothing
// reuses it — no ring blits), so it's written inline here.
//
// EFFECTIVE MATCH (349/349 insns, align=8, reg_pen=14 — 13 diff rows, ALL register-
// role). The residual is a clean PARITY CROSSING with ZoneTransitionStep: for the
// same two code shapes (head pWorld/word-load regs: EAX+cx/ax vs ECX+ax/dx; grid
// search: y→EBX,x→DI vs y→EDI,x→BX) the ORIGINAL uses shape A here and shape B in
// ZoneTransitionStep, while OUR compile emits them exactly swapped — both shapes
// reproduce, on the wrong functions. Same TU-phase drift family as the original's
// own loader jg/jl/jg triplet; almost certainly one dial state with the
// DrawZoneCellRect/DrawWholeZone rotations. Do not grind — G1 joint pass.
int GameView::WorldEntryStepMaybe(short nZoneId, short nStep)
{
    Tile *pTile;
    TRY {
        pTile = new Tile;
    }
    }              // closes the try block the TRY macro opened
    catch (CException *e) {                // hand-expanded CATCH_ALL(e)
        _afxExceptionLink.m_pException = e;
        AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: dead OOM dialog
        AfxAbort();                            //      (docs/engine-bugs.md #7)
    }
    }              // closes the TRY macro's outer (link-scope) brace
    memset(pTile->pixels, 0, 0x400);
    short nCellX = (short)pWorld->playerX;     // world-map grid coords; conditionally
    short nCellY = (short)pWorld->playerY;     // re-derived by step 5's grid search
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    int bAborted = 0;
    if (nStep >= 5)
    {
        if (nStep == 5)
        {
            Zone *pOldZone = pWorld->currentZone;
            pWorld->currentZone = pWorld->GetZoneById(nZoneId);
            if (pWorld->currentZone == NULL)
                pWorld->currentZone = pOldZone;
            if (pWorld->bWorldInvalidMaybe == 0)
            {
                short y = 0;
                nCellX = -1;
                nCellY = -1;
                do
                {
                    short x = 0;
                    do
                    {
                        if (pWorld->GetZoneById(pWorld->GetZoneCell(x, y)) == pWorld->currentZone)
                        {
                            nCellX = x;
                            nCellY = y;
                            break;
                        }
                        x++;
                    } while (x < 10);
                    y++;
                } while (y < 10);
                if (nCellX >= 0)
                {
                    if (nCellY >= 0)
                    {
                        pWorld->playerX = nCellX;
                        pWorld->playerY = nCellY;
                    }
                    if (nCellX >= 0 && nCellY >= 0 &&
                        pWorld->mapGrid[nCellX + nCellY * 10].zoneType == 7)
                        pWorld->mapGrid[nCellX + nCellY * 10].flagA = 1;
                }
            }
            unsigned short nMask = 0;
            pWorld->unk50 = nZoneId;
            pWorld->UpdateCamera();
            pWorld->RefreshZone();
            if (pWorld->bWorldInvalidMaybe == 0)
            {
                nMask = (unsigned short)pWorld->currentZone->IactRun(4, 0, 0, 0, 0, 0,
                                                                     pDC, pWorld, this);
                if (nMask & 0x800)
                    bAborted = 1;
            }
            bIactZoneEntryMaybe = 1;
            if (pWorld->bWorldInvalidMaybe == 0)
                nMask |= (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                      pDC, pWorld, this);
            bIactZoneEntryMaybe = 0;
            if (nMask & 4)
                pWorld->UpdateCamera();
            nTransitionStep = -1;
            pWorld->currentZone->activatedFlag = 1;
            if (pWorld->bWorldInvalidMaybe == 0)
                DrawEntities();
            DrawWholeZone();
            if (nCellY >= 0 && nCellX >= 0)
                pWorld->mapGrid[nCellX + nCellY * 10].flagSolved = 1;
            if (bAborted == 0 && (nMask & 0x800))
            {
                pDC->SelectPalette(pOldPal, 0);
                ReleaseDC(pDC);
                delete pTile;
                return 0;
            }
        }
        else if (nStep == 10)
        {
            DrawDirectionArrows(pDC);
            unsigned short nMask;              // sic: uninitialized when the IactRun
                                               //      below is skipped (engine-bugs #13)
            if (bSkipEntryIactMaybe == 0 && pWorld->bWorldInvalidMaybe == 0)
                nMask = (unsigned short)pWorld->currentZone->IactRun(5, 0, 0, 0, 0, 0,
                                                                     pDC, pWorld, this);
            if (nMask & 4)
                pWorld->UpdateCamera();
            bSkipEntryIactMaybe = 0;
            pWorld->nFrameMode = 6;
            pWorld->nMapChangeReason = 1;
            nTransitionStep = 0;
            if (nMask & 0x20)
                DrawWholeZone();
        }
        else
        {
            short nOff = (10 - nStep) * 32;
            short nSize = (nStep - 1) * 32;
            pWorld->pCanvas->BitBlt(pDC, pWorld->rectUnk3274.left + nOff,
                                    pWorld->rectUnk3274.top + nOff, nSize - nOff,
                                    nSize - nOff, pWorld->nViewLeft + nOff,
                                    pWorld->nViewTop + nOff);
        }
    }
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    delete pTile;
    return 1;
}

// =============================================================================
// MFC GDI COMDAT copies emitted into this TU by CBitmap/CBitmapButton member
// usage (this .cpp instantiates CBitmap locals + the btnDialog* members) — all
// six byte-match the originals with zero source lines. Ghidra's
// "CBitmap_CtorMaybe" @0x40a0c0 is really CGdiObject::CGdiObject (rename queued).
// The CBitmap trio sits later in .text (0x40a4e0-0x40a5d0, after DrawTileAt).
// =============================================================================
// FUNCTION: YODA 0x0040a0c0  (??0CGdiObject@@ ctor COMDAT)
// FUNCTION: YODA 0x0040a120  (??_GCGdiObject@@ scalar-deleting dtor COMDAT)
// FUNCTION: YODA 0x0040a1a0  (??1CGdiObject@@ dtor COMDAT)
// FUNCTION: YODA 0x0040a4e0  (??0CBitmap@@ ctor COMDAT)
// FUNCTION: YODA 0x0040a560  (??_GCBitmap@@ scalar-deleting dtor COMDAT)
// FUNCTION: YODA 0x0040a5d0  (??1CBitmap@@ dtor COMDAT)

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a200
// GameView::DrawGameArea — blit the 288x288 game viewport to the screen at client
// (8,7). With no DC supplied, grabs one (with palette) and releases it after. The
// GetPixel probe at (0x138,0x11c) — just outside the viewport — detects the frame
// having been painted over (e.g. by a dialog): if that pixel is valid and no longer
// COLOR_3DFACE, the whole window is invalidated first. Locator-map modes (mode 5
// with the map open, mode 7, or the map open at all — the last || term makes the
// mode-5 check redundant, faithful dev code) blit the canvas from its origin
// (the map is composited at 0,0); play modes blit the camera window (nViewLeft/Top).
void GameView::DrawGameArea(CDC *pDC)
{
    CPalette *pOldPal = 0;
    if (pDC == 0)
    {
        pDC = GetDC();
        if (pDC == 0)
            return;
        pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    }
    int nMode = pWorld->nFrameMode;
    COLORREF pixel = ::GetPixel(pDC->m_hDC, 0x138, 0x11c);
    DWORD clr = ::GetSysColor(0xf);
    if (pixel != 0xffffffff && clr != pixel)
        ::RedrawWindow(m_hWnd, 0, 0, 0x105);
    if ((nMode == 5 && bMapViewOpen != 0) || nMode == 7 || bMapViewOpen != 0)
        pWorld->pCanvas->BitBlt(pDC, 8, 7, 0x120, 0x120, 0, 0);
    else
        pWorld->pCanvas->BitBlt(pDC, 8, 7, 0x120, 0x120, pWorld->nViewLeft,
                                pWorld->nViewTop);
    if (pOldPal != 0)
    {
        pDC->SelectPalette(pOldPal, 0);
        ReleaseDC(pDC);
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a320
// GameView::BlitTile — draw one 32x32 tile into the back-buffer canvas at zone
// cell (x,y) (pixel x<<5, y<<5), masked when the tile's transparency flag is set.
// Bounds use the zone WIDTH for both axes (square zones). The 3rd param is passed
// by every caller but never read (byte-proven: RET 0x10 = 4 dword params, no
// [esp+0x14] access) — the header's "(sig?)" tag can come off.
// EFFECTIVE (47/47 insns, align=20): one consistent register rotation (orig x→SI
//   y→DX w→AX; ours x→DX y→AX w→SI) + the induced cmp mirror (`cmp w,x; jle` vs
//   `cmp x,w; jge` — `w > x` and `x < w` spellings compile identically, lesson #6).
//   Crack that mattered: sx/sy MUST be locals — written at the call sites the
//   x<<5/y<<5 pair gets re-emitted per arm instead of hoisted above the flags test.
void GameView::BlitTile(short y, short x, int nUnused, Tile *pTile)
{
    Canvas *pCanvas = pWorld->pCanvas;
    if (pCanvas != 0 && pTile != 0)
    {
        short w = pWorld->currentZone->width;
        if (x >= 0 && y >= 0 && x < w && y < w)
        {
            short sx = x << 5;
            short sy = y << 5;
            if (pTile->flags & 1)
            {
                pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
                return;
            }
            pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a3a0
// GameView::DrawTileAt — redraw zone cell (x,y) into the canvas: frame < 0 paints
// all 3 layers back-to-front (the DrawZoneCell composite, but via the raw tile
// array); frame 0..2 paints that single layer only. Tile ids are signed shorts
// (-1 = empty). NOTE the register split the decompile confirms: the 3-layer loop
// re-mentions pWorld->pCanvas / pWorld->tiles fresh each iteration (the calls
// clobber the caller-saved copies), while the single-frame arm reuses the pCanvas
// local and the entry pWorld — write it exactly that way.
// EFFECTIVE (109/109 insns, align=32): pure two-register swap (orig this→EBP,
//   pCell→EBX; ours the reverse) + the same cmp mirror as BlitTile + a 1-insn
//   entry scheduling shuffle. Structure, both arms, the 3-layer countdown loop and
//   every call site are insn-identical. DrawZoneCellRect rotation family — G1.
void GameView::DrawTileAt(short x, short y, short frame)
{
    Canvas *pCanvas = pWorld->pCanvas;
    if (pCanvas != 0)
    {
        short *pCell = &pWorld->currentZone->tiles[(y * 18 + x) * 3];
        short w = pWorld->currentZone->width;
        if (x >= 0 && y >= 0 && w > x && w > y)
        {
            short sx = x << 5;
            short sy = y << 5;
            if (frame < 0)
            {
                int n = 3;
                do
                {
                    if (*pCell >= 0)
                    {
                        Tile *pTile = (Tile *)pWorld->tiles.GetAt(*pCell);
                        if (pTile->flags & 1)
                            pWorld->pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20,
                                                        sx, sy, 0);
                        else
                            pWorld->pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20,
                                                      sx, sy);
                    }
                    pCell++;
                    n--;
                } while (n != 0);
            }
            else if (pCell[frame] >= 0)
            {
                Tile *pTile = (Tile *)pWorld->tiles.GetAt(pCell[frame]);
                if (pTile->flags & 1)
                {
                    pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, sx, sy, 0);
                    return;
                }
                pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, sx, sy);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a620
// GameView::IsUsableTileMaybe — hardcoded tile-id predicate: 0 for the special-
// behaviour tile ranges below, 1 for everything else. Written as a switch (66
// cases after range folding) — VC4.2 builds the balanced comparison tree with
// per-subtree copies of the default `return 1` epilogue (the ReadZone whitelist
// pattern, lesson #11).
int GameView::IsUsableTileMaybe(short tileId)
{
    switch (tileId)
    {
    case 0x67: case 0x68: case 0x69:
    case 0x6b:
    case 0x163:
    case 0x176: case 0x177: case 0x178:
    case 0x17c: case 0x17d: case 0x17e: case 0x17f: case 0x180: case 0x181:
    case 0x182: case 0x183: case 0x184: case 0x185: case 0x186:
    case 0x271: case 0x272:
    case 0x346: case 0x347: case 0x348: case 0x349: case 0x34a: case 0x34b:
    case 0x34c: case 0x34d: case 0x34e: case 0x34f: case 0x350: case 0x351:
    case 0x352: case 0x353: case 0x354: case 0x355: case 0x356: case 0x357:
    case 0x396: case 0x397: case 0x398:
    case 0x39b:
    case 0x39d: case 0x39e: case 0x39f: case 0x3a0: case 0x3a1: case 0x3a2:
    case 0x3a3: case 0x3a4:
    case 0x3fd:
    case 0x47b: case 0x47c: case 0x47d: case 0x47e: case 0x47f: case 0x480:
    case 0x481: case 0x482: case 0x483: case 0x484: case 0x485: case 0x486:
    case 0x53d:
    case 0x740:
        return 0;
    }
    return 1;
}

// -----------------------------------------------------------------------------
// FUNCTION: YODA 0x0040a710
// GameView::FireWeaponStep — one frame of the weapon-fire animation, driven by
// OnTimer mode 8 with nStep = nFireStep 0..3. The player's cell comes from the
// camera (cameraX/32, cameraY/32). Step 0 plays the fire sound and pays ammo
// (weapon tiles 0x1fe "The Force" and 0x12 lightsaber are free; the five ammo
// counters 0x334c.. map to weapon tiles 0x1ff/0x200/0x201/0x204/0x205). Each step
// draws the muzzle tile at the player, the beam tile one cell ahead, and walks
// the projectile tile outward (alternating animation via nStep % 2 + 1); the
// blaster (0x1ff) leaves its impact cell in nWeaponHitX/YMaybe, other weapons
// stop flight (nFireStep = 4) on a usable/blocking overlay tile. After drawing,
// the viewport is flushed and UseWeapon resolves hits; an emptied weapon is
// deselected, removed from inventory, re-armed from a spare in the inventory
// (with per-weapon ammo refill + sound) or falls back to the lightsaber
// character (frames[7] == 0x12).
// NOTE the X (horizontal) and Y (vertical) projectile arms are genuinely
// ASYMMETRIC in the original: the X arm duplicates the draw-triple in all three
// no-stop paths, the Y arm shares one copy behind skip-gotos; the Y arm also
// tests (flags & 0x60000) before IsUsableTileMaybe where the X arm combines
// them. Faithful dev code — do not "clean up".
//
// EFFECTIVE MATCH (820 vs 828 insns, ~97% identical; part of the align score is
// the 7-entry ammo jump-table DATA at the COMDAT end misaligning as insns).
// Structure proven: both dir arms, all draw triples, the erase/beam blocks, the
// dead-weapon rescan loops and the refill switch are insn-identical up to one
// consistent register rotation (this: EBX↔ESI etc.). Cracks that landed it:
// int nWeaponTile local (movsx+dword slot) with the CONDITIONS re-mentioning
// pWeapon->frames[7] (16-bit cmps); nStep == 0 duplicated into BOTH head
// conditions (the binary jump-threads them); t != -1 arms FIRST everywhere;
// the X non-blaster stop test is `IsUsable && bBlocked == 0` with an int
// bBlocked set from a chained GetTileData(t)->flags test; refill cases assign
// pWeapon->unk48 in BOTH if/else arms (cross-jumped); `unk48 <= 0` (zero-reg
// cmp) not `< 1`. Parked residuals (probed, not source-steerable):
//  * OPEN CODEGEN AXIS — the original tests tile flags as `mov eax,[pT+0x404];
//    test eax,0x60000` (4 sites) where every spelling we tried (direct field,
//    pT local, flags local, outer-scope flags, (int) cast, chained call)
//    byte-narrows to `test byte [pT+0x406],6`. Detonate's original DOES use the
//    narrow form for `& 0x20000` — so the wide form here is a real source shape
//    we haven't found. Related: the compiler VN-eliminates our bBlocked into a
//    spilled flags copy at the X non-blaster site.
//  * erase-block `dir * nStep + x`: orig folds nStep as an imul-mem operand and
//    sign-tests via the add (js); ours materializes nStep and uses lea+test.
//  * 1-insn scheduling drifts at the cleanup GetDC head and the two scan-loop
//    heads; backedge cmp directions (jl vs jg — canonicalization, both source
//    spellings identical); jump-table byte differences from reloc masking.
void GameView::FireWeaponStep(int nStep)
{
    int x = pWorld->cameraX / 32;
    int y = pWorld->cameraY / 32;
    int w = pWorld->currentZone->width;
    int ty = nFireDirY + y;
    int tx = nFireDirX + x;
    Character *pWeapon = pWorld->currentWeapon;
    if (pWeapon == 0)
        return;
    bBusy = 1;
    int nWeaponTile = pWeapon->frames[7];
    if (nStep == 0 && (pWeapon->frames[7] == 0x1fe || pWeapon->frames[7] == 0x12))
    {
        PlaySound(pWeapon->weaponCharId);
        DrawWeaponIcon(0);
    }
    else if (nStep == 0 && pWeapon->unk48 > 0)
    {
        PlaySound(pWeapon->weaponCharId);
        pWeapon->unk48--;
        DrawWeaponIcon(0);
        pWorld->bWeaponHitPendingMaybe = 0;
        switch (nWeaponTile)
        {
        case 0x1ff:
            pWorld->weaponState[0]--;
            break;
        case 0x200:
            pWorld->weaponState[1]--;
            break;
        case 0x201:
            pWorld->weaponState[2]--;
            break;
        case 0x204:
            pWorld->weaponState[3]--;
            break;
        case 0x205:
            pWorld->nCurrentAmmoMaybe--;
            break;
        }
    }
    Tile *pBeamH = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                      1, &pWorld->tiles);
    Tile *pBeamV = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                      2, &pWorld->tiles);
    Tile *pProj = (Tile *)pWeapon->GetProjectileTile(nStep % 2 + 1, nFireDirX, nFireDirY,
                                                     0, &pWorld->tiles);
    if (nFireStep < 3)
        DrawTileAt((short)x, (short)y, 0);
    if (nFireDirX != 0)
    {
        if (tx >= 0 && tx < w)
            DrawZoneCell((short)tx, (short)y);
    }
    else if (nFireDirY != 0)
    {
        if (ty >= 0 && ty < w)
            DrawZoneCell((short)x, (short)ty);
    }
    if (nStep < 3)
    {
        BlitTile((short)y, (short)x, 1, pBeamH);
        DrawTileAt((short)x, (short)y, 2);
        if (nFireDirX != 0 && pBeamV != 0)
        {
            if (tx >= 0 && tx < w)
            {
                BlitTile((short)y, (short)tx, 1, pBeamV);
                DrawTileAt((short)tx, (short)y, 2);
            }
        }
        else if (nFireDirY != 0 && pBeamV != 0 && ty >= 0 && ty < w)
        {
            BlitTile((short)ty, (short)x, 1, pBeamV);
            DrawTileAt((short)x, (short)ty, 2);
        }
    }
    if (pProj != 0)
    {
        if (nStep > 0)
        {
            if (nFireDirX != 0)
            {
                int px = nFireDirX * nStep + x;
                if (px >= 0 && w > px)
                    DrawTileAt((short)px, (short)y, -1);
            }
            else if (nFireDirY != 0)
            {
                int py = nFireDirY * nStep + y;
                if (py >= 0 && w > py)
                    DrawTileAt((short)x, (short)py, -1);
            }
        }
        if (nStep >= 0 && nStep < 3)
        {
            int dx = nFireDirX;
            if (dx != 0)
            {
                int nx = (nStep + 1) * dx + x;
                if (nx >= 0 && w > nx)
                {
                    if (pWeapon->frames[7] == 0x1ff)
                    {
                        short t = pWorld->currentZone->GetTile(nx - dx, y, 1);
                        if (t == -1)
                        {
                            DrawTileAt((short)nx, (short)y, 0);
                            DrawTileAt((short)nx, (short)y, 1);
                            BlitTile((short)y, (short)nx, 1, pProj);
                            DrawTileAt((short)nx, (short)y, 2);
                            pWorld->nWeaponHitXMaybe = nx;
                            pWorld->nWeaponHitYMaybe = y;
                        }
                        else
                        {
                            if ((pWorld->GetTileData(t)->flags & 0x60000) == 0)
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                                pWorld->nWeaponHitXMaybe = nx;
                                pWorld->nWeaponHitYMaybe = y;
                            }
                            else
                            {
                                nFireStep = 4;
                            }
                        }
                    }
                    else
                    {
                        int bBlocked = 0;
                        short t = pWorld->currentZone->GetTile(nx, y, 1);
                        if (t != -1)
                        {
                            if (pWorld->GetTileData(t)->flags & 0x60000)
                                bBlocked = 1;
                            if (IsUsableTileMaybe(t) != 0 && bBlocked == 0)
                            {
                                nFireStep = 4;
                            }
                            else
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                            }
                        }
                        else
                        {
                            t = pWorld->currentZone->GetTile(nx - nFireDirX, y, 1);
                            if (t != -1)
                            {
                                if (IsUsableTileMaybe(t) == 0)
                                {
                                    DrawTileAt((short)nx, (short)y, 0);
                                    DrawTileAt((short)nx, (short)y, 1);
                                    BlitTile((short)y, (short)nx, 1, pProj);
                                    DrawTileAt((short)nx, (short)y, 2);
                                }
                                else
                                {
                                    nFireStep = 4;
                                }
                            }
                            else
                            {
                                DrawTileAt((short)nx, (short)y, 0);
                                DrawTileAt((short)nx, (short)y, 1);
                                BlitTile((short)y, (short)nx, 1, pProj);
                                DrawTileAt((short)nx, (short)y, 2);
                            }
                        }
                    }
                }
            }
            else
            {
                int dy = nFireDirY;
                if (dy != 0)
                {
                    int ny = (nStep + 1) * dy + y;
                    if (ny >= 0 && w > ny)
                    {
                        if (pWeapon->frames[7] == 0x1ff)
                        {
                            short t = pWorld->currentZone->GetTile(x, ny - dy, 1);
                            if (t == -1)
                            {
                                DrawTileAt((short)x, (short)ny, 0);
                                DrawTileAt((short)x, (short)ny, 1);
                                BlitTile((short)ny, (short)x, 1, pProj);
                                DrawTileAt((short)x, (short)ny, 2);
                                pWorld->nWeaponHitXMaybe = x;
                                pWorld->nWeaponHitYMaybe = ny;
                            }
                            else
                            {
                                if ((pWorld->GetTileData(t)->flags & 0x60000) == 0)
                                {
                                    DrawTileAt((short)x, (short)ny, 0);
                                    DrawTileAt((short)x, (short)ny, 1);
                                    BlitTile((short)ny, (short)x, 1, pProj);
                                    DrawTileAt((short)x, (short)ny, 2);
                                    pWorld->nWeaponHitXMaybe = x;
                                    pWorld->nWeaponHitYMaybe = ny;
                                }
                                else
                                {
                                    nFireStep = 4;
                                }
                            }
                        }
                        else
                        {
                            short t = pWorld->currentZone->GetTile(x, ny, 1);
                            if (t != -1)
                            {
                                Tile *pT = pWorld->GetTileData(t);
                                unsigned int flags = pT->flags;
                                int usable = IsUsableTileMaybe(t);
                                if (flags & 0x60000)
                                    goto cleanup;
                                if (usable != 0)
                                {
                                    nFireStep = 4;
                                    goto cleanup;
                                }
                            }
                            else
                            {
                                t = pWorld->currentZone->GetTile(x, ny - nFireDirY, 1);
                                if (t != -1 && IsUsableTileMaybe(t) != 0)
                                {
                                    nFireStep = 4;
                                    goto cleanup;
                                }
                            }
                            DrawTileAt((short)x, (short)ny, 0);
                            DrawTileAt((short)x, (short)ny, 1);
                            BlitTile((short)ny, (short)x, 1, pProj);
                            DrawTileAt((short)x, (short)ny, 2);
                        }
                    }
                }
            }
        }
    }
cleanup:
    ;
    int i = 0;
    CDC *pDC = GetDC();
    CPalette *pOldPal = pDC->SelectPalette(pWorld->pPalette, 0);
    DrawGameArea(pDC);
    pDC->SelectPalette(pOldPal, 0);
    ReleaseDC(pDC);
    UseWeapon(x, y, nFireDirX, nFireDirY, nStep);
    if (pWeapon->unk48 <= 0 && pWeapon->frames[7] != 0x1fe && pWeapon->frames[7] != 0x12)
    {
        pWorld->currentWeapon = 0;
        DrawWeaponBox(0);
        DrawWeaponIcon(0);
        Tile *pTile = pWorld->GetTileData(pWeapon->frames[7]);
        RemoveItem(pTile);
        int bFound = 0;
        int nInv = pWorld->inventory.GetSize();
        if (nInv > 0)
        {
            do
            {
                InvItem *pItem = (InvItem *)pWorld->inventory.GetAt(i);
                if (pItem->pTile == pTile)
                {
                    pWorld->currentWeapon = pWeapon;
                    switch (nWeaponTile)
                    {
                    case 0x1ff:
                        if (pWorld->weaponState[0] > 0)
                            pWeapon->unk48 = pWorld->weaponState[0];
                        else
                        {
                            pWorld->weaponState[0] = 15;
                            pWeapon->unk48 = pWorld->weaponState[0];
                        }
                        PlaySound(0x34);
                        break;
                    case 0x200:
                        if (pWorld->weaponState[1] > 0)
                            pWeapon->unk48 = pWorld->weaponState[1];
                        else
                        {
                            pWorld->weaponState[1] = 30;
                            pWeapon->unk48 = pWorld->weaponState[1];
                        }
                        PlaySound(0x20);
                        break;
                    case 0x201:
                        if (pWorld->weaponState[2] > 0)
                            pWeapon->unk48 = pWorld->weaponState[2];
                        else
                        {
                            pWorld->weaponState[2] = 10;
                            pWeapon->unk48 = pWorld->weaponState[2];
                        }
                        PlaySound(0x20);
                        break;
                    case 0x204:
                        if (pWorld->weaponState[3] > 0)
                            pWeapon->unk48 = pWorld->weaponState[3];
                        else
                        {
                            pWorld->weaponState[3] = 15;
                            pWeapon->unk48 = pWorld->weaponState[3];
                        }
                        break;
                    case 0x205:
                        if (pWorld->nCurrentAmmoMaybe > 0)
                            pWeapon->unk48 = pWorld->nCurrentAmmoMaybe;
                        else
                        {
                            pWorld->nCurrentAmmoMaybe = 15;
                            pWeapon->unk48 = pWorld->nCurrentAmmoMaybe;
                        }
                        break;
                    }
                    DrawWeaponBox(0);
                    DrawWeaponIcon(0);
                    bFound = 1;
                    break;
                }
                i++;
            } while (nInv > i);
        }
        if (bFound == 0)
        {
            int j = 0;
            int nChars = pWorld->characters.GetSize();
            if (nChars > 0)
            {
                do
                {
                    Character *pChar = (Character *)pWorld->characters.GetAt(j);
                    if (pChar->frames[7] == 0x12)
                    {
                        pChar->unk48 = 30;
                        pWorld->currentWeapon = pChar;
                        PlaySound(0x1f);
                        break;
                    }
                    j++;
                } while (nChars > j);
            }
            DrawWeaponBox(0);
            DrawWeaponIcon(0);
        }
    }
    bBusy = 0;
}
