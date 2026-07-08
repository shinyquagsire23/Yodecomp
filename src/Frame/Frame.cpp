// Frame TU (0x419000–0x419720): CMainFrame (CFrameWnd-derived).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "Frame.h"

extern "C" long   time(long *);          // CRT _time (0x0042a400)
extern "C" double difftime(long, long);  // CRT       (0x0042a3e0)

// g_strReplayPath's ctor/dtor thunks (0x004196e0–0x00419710) are emitted here, so it is
// defined in this TU (App.cpp declares it extern).
CString g_strReplayPath;

// FUNCTION: YODA 0x00419000  (CreateObject)
// FUNCTION: YODA 0x00419070  (GetRuntimeClass)
IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

// FUNCTION: YODA 0x00419080  (GetMessageMap)
BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
    ON_WM_GETMINMAXINFO()
    ON_WM_SIZE()
    ON_WM_CREATE()
    ON_WM_PALETTECHANGED()
    ON_WM_PALETTEISCHANGING()
    ON_WM_QUERYNEWPALETTE()
    ON_WM_ACTIVATE()
    ON_WM_SHOWWINDOW()
    ON_WM_SYSCOMMAND()
    ON_WM_QUERYENDSESSION()
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419090
// FUNCTION: YODA 0x00419100  (??_GCMainFrame scalar-deleting dtor — compiler-generated, calls ~)
CMainFrame::CMainFrame()
{
    m_nSavedFrameMode = -1;
}

// FUNCTION: YODA 0x00419120
CMainFrame::~CMainFrame()
{
}

// NOTE (Phase G2 layout): the message-handler definitions below are ordered to match the
// original Frame.obj emission (= source) order — OnSysCommand, PreCreateWindow, OnGetMinMaxInfo,
// OnSize, OnCreate, OnCreateClient, OnPaletteChanged, OnPaletteIsChanging, OnQueryNewPalette,
// OnActivate, OnShowWindow, OnQueryEndSession — so this TU is internally in-order (tools/g2_order.py
// --scramble). Content-neutral (14/18 exact holds); order does not affect any function's bytes.

// FUNCTION: YODA 0x00419170
// Pause the game while the user drags/sizes the window (WM_SYSCOMMAND SC_MOVE/SIZE), resume
// on restore; intercept SC_CLOSE to route through the game's exit confirmation.
void CMainFrame::OnSysCommand(UINT nID, LPARAM lParam)
{
    GameView *pView = (GameView *)GetActiveView();
    switch (nID & 0xfff0) {
    case SC_MINIMIZE:
        pView->bBusy = 1;
        CFrameWnd::OnSysCommand(nID, lParam);
        return;
    case SC_MAXIMIZE:
    case SC_RESTORE:
        pView->bBusy = 0;
        CFrameWnd::OnSysCommand(nID, lParam);
        return;
    case SC_CLOSE:
        if (pView->pWorld->nFrameMode != 0xc) {
            pView->ConfirmExit();
            return;
        }
        CFrameWnd::OnSysCommand(nID, lParam);
        return;
    default:
        CFrameWnd::OnSysCommand(nID, lParam);
        return;
    }
}

// FUNCTION: YODA 0x00419210  [EFFECTIVE MATCH: structurally identical (instr-selection all
//   correct); residual is CRect stack-slot placement + the base-return spill (orig parks bRet
//   in a stack slot and reloads; ours keeps it in EDI → our frame is 4B smaller). Pure
//   allocator/scheduling tie-break — endgame/permuter territory. GetSystemMetrics raw args
//   7/8/0xf/4/0/1 and the cs field-write order verified against disasm.]
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT &cs)
{
    BOOL bRet = CFrameWnd::PreCreateWindow(cs);
    CRect rc;
    rc.right = GetSystemMetrics(7) * 2 + 0x20d;
    int cy = GetSystemMetrics(8) * 2 + 0x136;
    cy += GetSystemMetrics(0xf);
    rc.bottom = GetSystemMetrics(4) + cy;
    rc.top = 0;
    rc.left = 0;
    int dx = GetSystemMetrics(0) / 2 - 0x106;
    int dy = GetSystemMetrics(1) / 2 - 0x9b;
    rc.OffsetRect(dx, dy);
    cs.x = rc.left;
    cs.y = rc.top;
    cs.style = 0x110a0000;
    cs.dwExStyle &= 0xfffffdff;
    cs.cx = rc.right - rc.left;
    cs.cy = rc.bottom - rc.top;
    return bRet;
}

// FUNCTION: YODA 0x004192d0
void CMainFrame::OnGetMinMaxInfo(MINMAXINFO *lpMMI)
{
    Default();
    lpMMI->ptMaxSize.x = 0x20d;
    lpMMI->ptMaxSize.y = 0x136;
    lpMMI->ptMaxSize.y = 0x136 + GetSystemMetrics(SM_CYCAPTION);
    lpMMI->ptMaxSize.y += GetSystemMetrics(SM_CYMENU);
    lpMMI->ptMaxTrackSize.x = lpMMI->ptMaxSize.x;
    lpMMI->ptMaxTrackSize.y = lpMMI->ptMaxSize.y;
}

// FUNCTION: YODA 0x00419320
void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);
}

// FUNCTION: YODA 0x00419340
int CMainFrame::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CFrameWnd::OnCreate(lpcs) == -1)
        return -1;
    CenterWindow();
    return 0;
}

// FUNCTION: YODA 0x00419370
// Realize the active document's game palette when the client area is created.
BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
    FrameWorld *pDoc = (FrameWorld *)GetActiveDocument();
    if (pDoc != NULL) {
        CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
        m_pOldPalette = pDC->SelectPalette(pDoc->pPalette, FALSE);
        ::RealizePalette(pDC->m_hDC);
        pDC->SelectPalette(m_pOldPalette, FALSE);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
    return CFrameWnd::OnCreateClient(lpcs, pContext);
}

// FUNCTION: YODA 0x004193f0  [EFFECTIVE MATCH: DIFF(54) — the bForceBackground arg
//   (this != pFocusWnd) is materialized by MSVC as the sbb idiom vs the original's push-1/
//   push-0 branch; instruction-selection tie-break (cmp-direction family), proven by the
//   FALSE-constant twins OnCreateClient/OnQueryNewPalette matching exactly. Rest identical.]
void CMainFrame::OnPaletteChanged(CWnd *pFocusWnd)
{
    FrameWorld *pDoc = (FrameWorld *)GetActiveDocument();
    if (pDoc != NULL) {
        CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
        CPalette *pOld = pDC->SelectPalette(pDoc->pPalette, this != pFocusWnd);
        ::RealizePalette(pDC->m_hDC);
        pDC->SelectPalette(pOld, FALSE);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x00419460  [EFFECTIVE MATCH: same bForceBackground sbb-vs-branch as
//   OnPaletteChanged; structure identical.]
void CMainFrame::OnPaletteIsChanging(CWnd *pRealizeWnd)
{
    Default();
    FrameWorld *pDoc = (FrameWorld *)GetActiveDocument();
    if (pDoc != NULL) {
        CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
        CPalette *pOld = pDC->SelectPalette(pDoc->pPalette, pRealizeWnd != this);
        ::RealizePalette(pDC->m_hDC);
        pDC->SelectPalette(pOld, FALSE);
        ::ReleaseDC(m_hWnd, pDC->m_hDC);
    }
}

// FUNCTION: YODA 0x004194d0
BOOL CMainFrame::OnQueryNewPalette()
{
    FrameWorld *pDoc = (FrameWorld *)GetActiveDocument();
    if (pDoc == NULL)
        return FALSE;
    CDC *pDC = CDC::FromHandle(::GetDC(m_hWnd));
    CPalette *pOld = pDC->SelectPalette(pDoc->pPalette, FALSE);
    ::RealizePalette(pDC->m_hDC);
    pDC->SelectPalette(pOld, FALSE);
    ::ReleaseDC(m_hWnd, pDC->m_hDC);
    return TRUE;
}

// FUNCTION: YODA 0x00419540  [EFFECTIVE MATCH: structure identical (align 452->134 after
//   ordering the deactivate path as the fall-through); residual is the this/nState EDI<->ESI
//   register 2-cycle + the original's zero-in-EBP CSE (it parks 0 in EBP and reuses it for the
//   bDragActive/nFrameMode stores and push-0 args; ours uses immediates). Allocator tie-breaks
//   (zero-reg-reuse family), endgame/permuter territory. Control flow, x87 time accounting,
//   the dense frame-mode switch, and all field accesses verified against disasm.]
// Pause/resume the game across window activation: on deactivate, bank the elapsed time into
// the world clock, park the play frame-mode, and suspend the music thread; on reactivate,
// restore the parked mode + clock and resume music.
void CMainFrame::OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized)
{
    GameView *pView = (GameView *)GetActiveView();
    if (nState == 0) {
        if (pView != NULL) {
            FrameWorld *pWorld = pView->pWorld;
            pWorld->timeOffset += (int)difftime(pWorld->timeBase, time(NULL));
            switch (pView->pWorld->nFrameMode) {
            case 0: case 1: case 2: case 3: case 5:
            case 6: case 7: case 8: case 9: case 0xb:
                m_nSavedFrameMode = pView->pWorld->nFrameMode;
                pView->bDragActive = 0;
                break;
            case 4:
                m_nSavedFrameMode = 3;
                pView->bDragActive = 0;
                pView->UpdateDragCursor(0);
                pView->UpdateDragCursor(1);
                break;
            default:
                m_nSavedFrameMode = -1;
                goto suspend;
            }
            pView->nDragSlot = -1;
            pView->DrawText(NULL);
            pView->pWorld->nFrameMode = 0;
        suspend:
            if (pView->pMusicThread != NULL)
                SuspendThread(pView->pMusicThread->hThread);
            pView->bBusy = 1;
        }
    }
    else if (pView != NULL) {
        if (pView->pWorld->nFrameMode == 0) {
            if (m_nSavedFrameMode >= 0)
                pView->pWorld->nFrameMode = m_nSavedFrameMode;
            pView->pWorld->timeBase = (int)time(NULL);
        }
        pView->DrawText(NULL);
        if (pView->pMusicThread != NULL)
            ResumeThread(pView->pMusicThread->hThread);
        pView->bBusy = 0;
    }
    CFrameWnd::OnActivate(nState, pWndOther, bMinimized);
}

// FUNCTION: YODA 0x004196a0
void CMainFrame::OnShowWindow(BOOL bShow, UINT nStatus)
{
    if (nStatus == 0)
        CenterWindow();
    Default();
}

// FUNCTION: YODA 0x004196c0
BOOL CMainFrame::OnQueryEndSession()
{
    GameView *pView = (GameView *)GetActiveView();
    if (pView->pWorld->nFrameMode != 0xc)
        pView->ConfirmExit();
    return FALSE;
}
