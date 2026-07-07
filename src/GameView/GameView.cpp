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
