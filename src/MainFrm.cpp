// Frame TU (0x419000–0x419720): CMainFrame (CFrameWnd-derived).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "MainFrm.h"

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
    CDeskcppView *pView = (CDeskcppView *)GetActiveView();
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
        if (pView->pWorld->nFrameMode != 12) {
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

// FUNCTION: YODA 0x00419210  [WIP: 148 B -> 4, LENGTH 180 -> 184 = Ghidra's extent (v120).
//   The old note here read the residual backwards: it called the bRet spill an "allocator
//   tie-break", but the spill is a CONSEQUENCE, not the defect. The original register-homes
//   the window WIDTH in a named local -- `lea edi,[eax*2+0x20d]` computes it into EDI instead
//   of our `add eax,eax; add eax,0x20d` straight into the rc.right slot -- and that one extra
//   long-lived value is what pushes `bRet` out of EDI and into [esp+0x24]. The +4 length
//   decomposes EXACTLY: the original's `mov [esp+0x24],eax` / `mov eax,[esp+0x20]` pair (8 B)
//   against our `mov edi,eax` / `mov eax,edi` (4 B). ⇒ lesson #49 in its purest form: read the
//   LENGTH, decompose it, and the "register difference" names its own source construct.
//   Second half of the fix is the store ORDER: the original writes top, left, right, bottom
//   TOGETHER after all four GetSystemMetrics calls (frame slots E-16, E-20, E-12, E-8 at
//   +0x3f/+0x44/+0x48/+0x4c), where we assigned rc.right first and the zeros last.
//   Measured family (all at the exact length): one accumulating `cy` = 4 B, a separate `cb`
//   initialised from cy = 11 B, `rc.bottom` before `rc.right` = 8 B, `rc.top = rc.left = 0`
//   = 6 B. Idiomatic member picked (lesson #36). ⚠ `rc.SetRect(0, 0, cx, cy)` and a
//   `CRect rc(0, 0, cx, cy)` ctor are REFUTED from the other side: both write LEFT before TOP
//   and the original writes TOP first (25 B measured). GetSystemMetrics raw args 7/8/0xf/4/0/1
//   and the cs field-write order verified against disasm.
//   Residual = ONE 2-instruction schedule swap (`add eax,ebp` sits before `xor ecx,ecx` in the
//   original, after it in ours) at insns 63/63, reg_pen 0, matching length and matching save
//   set = the lesson-#44 signature. Swept flat: 7 body arrangements, 9 cy/cb accumulation
//   forms, 6 store orders. The `xor` materialises the 0 shared by rc.top, rc.left AND the
//   SM_CXSCREEN argument, so this is lesson #39's axis -- and moving the zeros costs 128-159 B.
//   ⚠ the 6-line body shape is deliberate (line-neutral vs the pre-v120 text, lesson #23).]
BOOL CMainFrame::PreCreateWindow(CREATESTRUCT &cs)
{
    BOOL bRet = CFrameWnd::PreCreateWindow(cs);
    CRect rc;
    int cx = GetSystemMetrics(SM_CXDLGFRAME) * 2 + MAIN_WINDOW_WIDTH;
    int cy = GetSystemMetrics(SM_CYDLGFRAME) * 2 + MAIN_WINDOW_HEIGHT;
    cy += GetSystemMetrics(SM_CYMENU);
    cy += GetSystemMetrics(SM_CYCAPTION);
    rc.top = 0; rc.left = 0;
    rc.right = cx; rc.bottom = cy;
    int dx = GetSystemMetrics(SM_CXSCREEN) / 2 - MAIN_WINDOW_WIDTH / 2;
    int dy = GetSystemMetrics(SM_CYSCREEN) / 2 - MAIN_WINDOW_HEIGHT / 2;
    rc.OffsetRect(dx, dy);
    cs.x = rc.left;
    cs.y = rc.top;
    cs.style = WS_VISIBLE | WS_MAXIMIZE | WS_SYSMENU | WS_MINIMIZEBOX;   // 0x110a0000
    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.cx = rc.right - rc.left;
    cs.cy = rc.bottom - rc.top;
    return bRet;
}

// FUNCTION: YODA 0x004192d0
void CMainFrame::OnGetMinMaxInfo(MINMAXINFO *lpMMI)
{
    Default();
    lpMMI->ptMaxSize.x = MAIN_WINDOW_WIDTH;
    lpMMI->ptMaxSize.y = MAIN_WINDOW_HEIGHT;
    lpMMI->ptMaxSize.y = MAIN_WINDOW_HEIGHT + GetSystemMetrics(SM_CYCAPTION);
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
    CDeskcppView *pView = (CDeskcppView *)GetActiveView();
    if (nState == 0) {
        if (pView != NULL) {
            FrameWorld *pWorld = pView->pWorld;
            pWorld->timeOffset += (int)difftime(pWorld->timeBase, time(NULL));
            switch (pView->pWorld->nFrameMode) {
            case 0: case 1: case 2: case 3: case 5:
            case 6: case 7: case 8: case 9: case 11:
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
    CDeskcppView *pView = (CDeskcppView *)GetActiveView();
    if (pView->pWorld->nFrameMode != 12)
        pView->ConfirmExit();
    return FALSE;
}
