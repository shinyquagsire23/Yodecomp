// microfx app/ — real window layer (H4 M2, docs/phase-h4-sdl.md): HWND objects, the message-map
// dispatch engine, timers, the posted-message queue, and the window half of the MFC doc/view
// bootstrap (LoadFrame → WM_CREATE → OnCreateClient → view creation).
//
// Pure C++, NO SDL dependency — worldgen_smoke/zone_view stay headless-buildable. The SDL side
// (event translation + presentation) lives in mfxpump.cpp and talks to this file through
// mfxwnd.h: it feeds key/mouse state, calls MfxSendMsg/MfxPumpTimers/MfxPaintIfDirty, and
// presents MfxScreenDC()'s DIB.
//
// Screen model (M2): ONE screen DC for the whole window tree — a memory DC whose 8bpp DIB is
// created at the root window's client size. GetDC(anything) returns it; the game view sits at
// (0,0) of the root client area, so view-client coords == root coords == DIB coords. Child
// controls (buttons/edit/scrollbar) keep stub Create()s and never paint — M4 territory.

#include "mfxwnd.h"
#include <microfx.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── globals ──────────────────────────────────────────────────────────────────────────────────
int   g_mfxQuit = 0;
int   g_mfxQuitCode = 0;
BYTE  g_mfxKeyState[256];
POINT g_mfxCursorPos;
HWND  g_mfxCapture = 0;
HWND  g_mfxFocus = 0;
HCURSOR g_mfxCursor = 0;
int   g_mfxModalDepth = 0;

static HWND g_hRoot = 0;
static HDC  g_hScreenDC = 0;
static int  g_bDirty = 0;

int  MfxIsWnd(HWND h) { return h && h->nTag == MFX_TAG_WND; }
HWND MfxRootWnd()     { return g_hRoot; }
int  MfxWndVisible(HWND h) { return MfxIsWnd(h) ? h->bVisible : 0; }
int  MfxWndEnabled(HWND h) { return MfxIsWnd(h) ? h->bEnabled : 0; }

// window registry (M4): mouse hit-testing and child painting need to enumerate windows
static HWND g_aWnds[64];
static int  g_nWnds = 0;

HWND MfxWndFromPoint(POINT pt)
{
    // children are all direct children of the view/frame at fixed rects — last created wins
    HWND hHit = 0;
    for (int i = 0; i < g_nWnds; i++) {
        HWND h = g_aWnds[i];
        if (!MfxIsWnd(h) || !h->bVisible || !h->hParent) continue;
        if (h == g_hRoot) continue;
        if (h->nId == AFX_IDW_PANE_FIRST) continue;              // the view: fallback target
        if (pt.x >= h->rc.left && pt.x < h->rc.right &&
            pt.y >= h->rc.top  && pt.y < h->rc.bottom)
            hHit = h;
    }
    return hHit;
}

void MfxPaintChildren()
{
    MfxTouchHold();                    // one present for the whole child pass
    for (int i = 0; i < g_nWnds; i++) {
        HWND h = g_aWnds[i];
        if (MfxIsWnd(h) && h->bVisible && h->hParent && h->nId != AFX_IDW_PANE_FIRST && h->pWnd)
            h->pWnd->MfxCtlPaint();
    }
    MfxTouchRelease(MfxScreenDC());
}

HDC MfxScreenDC()
{
    if (!g_hScreenDC)
        g_hScreenDC = ::CreateCompatibleDC(0);   // DIB attached when the root window is sized
    return g_hScreenDC;
}

// Attach the screen DIB once the root window's client size is known (identity-gray initial
// color table; the game realizes its palette into the DC on the first paint).
static void MfxCreateScreenDib(int cx, int cy)
{
    if (cx <= 0 || cy <= 0) return;
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bmi;
    memset(&bmi, 0, sizeof bmi);
    bmi.h.biSize = sizeof(BITMAPINFOHEADER);
    bmi.h.biWidth = (cx + 3) & ~3;               // gdi pitch==width wants 4-aligned
    bmi.h.biHeight = -cy;
    bmi.h.biPlanes = 1;
    bmi.h.biBitCount = 8;
    bmi.h.biClrUsed = 256;
    for (int i = 0; i < 256; i++)
        bmi.pal[i].rgbRed = bmi.pal[i].rgbGreen = bmi.pal[i].rgbBlue = (BYTE)i;
    void *pBits = 0;
    HBITMAP hDib = ::CreateDIBSection(MfxScreenDC(), (BITMAPINFO *)&bmi, 0, &pBits, 0, 0);
    if (hDib)
        ::SelectObject(MfxScreenDC(), hDib);     // old (none) not tracked — screen DIB is permanent
    // children re-composite over every screen write (DrawGameArea would erase them otherwise)
    MfxSetScreenOverlayHook(MfxScreenDC(), MfxPaintChildren);
}

// ── message-map dispatch engine ──────────────────────────────────────────────────────────────

static const AFX_MSGMAP_ENTRY *MfxFindEntry(CCmdTarget *pTarget, UINT message, UINT nCode, UINT nID)
{
    const AFX_MSGMAP *pMap = pTarget->GetMessageMap();
    for (; pMap; pMap = pMap->pfnGetBaseMap ? pMap->pfnGetBaseMap() : 0)
        for (const AFX_MSGMAP_ENTRY *pE = pMap->lpEntries; pE->nSig != AfxSig_end; pE++) {
            if (pE->nMessage != message) continue;
            if (message == WM_COMMAND &&
                (pE->nCode != nCode || nID < pE->nID || nID > pE->nLastID)) continue;
            return pE;
        }
    return 0;
}

// Call one map entry with wParam/lParam cracked per its AfxSig (Win32 packing conventions).
// Member-pointer punning through the union is the same mechanism real MFC uses.
static LRESULT MfxCallEntry(CWnd *pWnd, const AFX_MSGMAP_ENTRY *pE, WPARAM wParam, LPARAM lParam)
{
    union {
        AFX_PMSG pfn;
        void   (CWnd::*vv)();
        void   (CWnd::*vw)(UINT);
        void   (CWnd::*vwl)(UINT, LPARAM);
        void   (CWnd::*vwww)(UINT, UINT, UINT);
        void   (CWnd::*vwp)(UINT, CPoint);
        void   (CWnd::*vwii)(UINT, int, int);
        void   (CWnd::*vbw)(BOOL, UINT);
        void   (CWnd::*vwwx)(UINT, UINT, CScrollBar *);
        void   (CWnd::*vwWb)(UINT, CWnd *, BOOL);
        void   (CWnd::*vW)(CWnd *);
        void   (CWnd::*vM)(MINMAXINFO *);
        int    (CWnd::*is)(LPCREATESTRUCT);
        BOOL   (CWnd::*bv)();
        BOOL   (CWnd::*bD)(CDC *);
        BOOL   (CWnd::*bWww)(CWnd *, UINT, UINT);
        HBRUSH (CWnd::*hDWw)(CDC *, CWnd *, UINT);
    } m;
    m.pfn = pE->pfn;
    switch (pE->nSig) {
    case AfxSig_vv:   (pWnd->*m.vv)(); return 0;
    case AfxSig_vw:   (pWnd->*m.vw)((UINT)wParam); return 0;                 // WM_TIMER / cmd range
    case AfxSig_vwl:  (pWnd->*m.vwl)((UINT)wParam, lParam); return 0;        // WM_SYSCOMMAND
    case AfxSig_vwww: (pWnd->*m.vwww)((UINT)wParam, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam)); return 0;
    case AfxSig_vwp:  { CPoint pt((int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
                        (pWnd->*m.vwp)((UINT)wParam, pt); return 0; }
    case AfxSig_vwii: (pWnd->*m.vwii)((UINT)wParam, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam)); return 0;
    case AfxSig_vbw:  (pWnd->*m.vbw)((BOOL)wParam, (UINT)lParam); return 0;  // WM_SHOWWINDOW
    case AfxSig_vwwx: (pWnd->*m.vwwx)((UINT)LOWORD(wParam), (UINT)HIWORD(wParam),
                          (CScrollBar *)CWnd::FromHandle((HWND)lParam)); return 0;
    case AfxSig_vwWb: (pWnd->*m.vwWb)((UINT)LOWORD(wParam), CWnd::FromHandle((HWND)lParam),
                          (BOOL)HIWORD(wParam)); return 0;                   // WM_ACTIVATE
    case AfxSig_vW:   (pWnd->*m.vW)(CWnd::FromHandle((HWND)wParam)); return 0; // WM_PALETTE*
    case AfxSig_vM:   (pWnd->*m.vM)((MINMAXINFO *)lParam); return 0;
    case AfxSig_is:   return (pWnd->*m.is)((LPCREATESTRUCT)lParam);          // WM_CREATE
    case AfxSig_bv:   return (pWnd->*m.bv)();
    case AfxSig_bD:   return (pWnd->*m.bD)(CDC::FromHandle((HDC)wParam));    // WM_ERASEBKGND
    case AfxSig_bWww: return (pWnd->*m.bWww)(CWnd::FromHandle((HWND)wParam),
                          (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));       // WM_SETCURSOR
    case AfxSig_hDWw: return (LRESULT)(ULONG_PTR)(pWnd->*m.hDWw)(CDC::FromHandle((HDC)wParam),
                          CWnd::FromHandle((HWND)lParam), (UINT)HIWORD(lParam)); // WM_CTLCOLOR
    }
    return 0;
}

// MFC WM_COMMAND routing order: active view → document → frame → app (CView::OnCmdMsg tries
// GetDocument() before falling through to its parent frame). CDocument is a CCmdTarget but NOT
// a CWnd, so it can't ride the CWnd-typed aTargets/MfxCallEntry path the other targets use below
// — it gets its own small dispatch, same shape as the app fallback already had.
static LRESULT MfxDispatchCommand(CWnd *pWnd, WPARAM wParam, LPARAM lParam)
{
    UINT nID = LOWORD(wParam), nCode = HIWORD(wParam);
    CWnd *pFrame = MfxRootWnd() ? MfxRootWnd()->pWnd : 0;
    CView *pView = pFrame ? ((CFrameWnd *)pFrame)->GetActiveView() : 0;

    if (pView) {
        const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(pView, WM_COMMAND, nCode, nID);
        if (pE) return MfxCallEntry(pView, pE, wParam, lParam);
    }
    CDocument *pDoc = pView ? pView->GetDocument() : 0;
    if (pDoc) {
        const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(pDoc, WM_COMMAND, nCode, nID);
        if (pE && pE->nSig == AfxSig_vv) {
            union { AFX_PMSG pfn; void (CCmdTarget::*vv)(); } m;
            m.pfn = pE->pfn;
            (pDoc->*m.vv)();
            return 0;
        }
    }
    CWnd *aTargets[2] = { pFrame, 0 };
    if (pWnd != pView && pWnd != pFrame)
        aTargets[1] = pWnd;                       // a directly-addressed control parent
    for (int i = 0; i < 2; i++) {
        if (!aTargets[i]) continue;
        const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(aTargets[i], WM_COMMAND, nCode, nID);
        if (pE) return MfxCallEntry(aTargets[i], pE, wParam, lParam);
    }
    // last stop: the app object (ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew) etc.)
    CWinApp *pApp = AfxGetApp();
    if (pApp) {
        const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(pApp, WM_COMMAND, nCode, nID);
        if (pE) {
            union { AFX_PMSG pfn; void (CCmdTarget::*vv)(); } m;
            m.pfn = pE->pfn;
            if (pE->nSig == AfxSig_vv) { (pApp->*m.vv)(); return 0; }
        }
    }
    return 0;
}

// CN_UPDATE_COMMAND_UI query (menu bar, mfxmenu.cpp): the same view→document→frame→app chain as
// MfxDispatchCommand, but calling an ON_UPDATE_COMMAND_UI handler (AfxSig_cmdui) synchronously —
// real MFC's CFrameWnd::OnInitMenuPopup shape, minus the popup-tracking part (that's mfxmenu.cpp).
// Every target here is addressed as CCmdTarget* (not CWnd*) specifically so CDocument — a
// CCmdTarget but not a CWnd — rides the exact same lookup as the others.
BOOL MfxQueryCmdUI(CCmdUI *pCmdUI)
{
    CWnd *pFrame = MfxRootWnd() ? MfxRootWnd()->pWnd : 0;
    CView *pView = pFrame ? ((CFrameWnd *)pFrame)->GetActiveView() : 0;
    CDocument *pDoc = pView ? pView->GetDocument() : 0;
    CCmdTarget *aTargets[4] = { (CCmdTarget *)pView, (CCmdTarget *)pDoc,
                                (CCmdTarget *)pFrame, (CCmdTarget *)AfxGetApp() };
    UINT nID = pCmdUI->m_nID;
    int bHaveCommand = 0;
    for (int i = 0; i < 4; i++) {
        if (!aTargets[i]) continue;
        if (!bHaveCommand && MfxFindEntry(aTargets[i], WM_COMMAND, CN_COMMAND, nID))
            bHaveCommand = 1;
        const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(aTargets[i], WM_COMMAND, CN_UPDATE_COMMAND_UI, nID);
        if (pE) {
            union { AFX_PMSG pfn; void (CCmdTarget::*cmdui)(CCmdUI *); } m;
            m.pfn = pE->pfn;
            (aTargets[i]->*m.cmdui)(pCmdUI);
            return TRUE;
        }
    }
    // no ON_UPDATE_COMMAND_UI anywhere: real MFC leaves CCmdUI's default (enabled) if a plain
    // ON_COMMAND handler exists; a totally unhandled id is dead — gray it out.
    if (!bHaveCommand) pCmdUI->m_bEnabled = FALSE;
    return bHaveCommand;
}

LRESULT MfxDispatchMsg(CWnd *pWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!pWnd) return 0;
    if (message == WM_COMMAND)
        return MfxDispatchCommand(pWnd, wParam, lParam);
    const AFX_MSGMAP_ENTRY *pE = MfxFindEntry(pWnd, message, 0, 0);
    if (!pE)                                      // unhandled → DefWindowProc-equivalent:
        return pWnd->MfxCtlProc(message, wParam, lParam);   // controls do EM_*/BM_*/paint here
    return MfxCallEntry(pWnd, pE, wParam, lParam);
}

// default "DefWindowProc": most messages ignored; base CWnd controls have no extra behavior
LRESULT CWnd::MfxCtlProc(UINT message, WPARAM, LPARAM)
{
    if (message == WM_PAINT) { MfxCtlPaint(); return 0; }
    return 0;
}
void CWnd::MfxCtlPaint() {}

LRESULT MfxSendMsg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!MfxIsWnd(hWnd)) return 0;
    return MfxDispatchMsg(hWnd->pWnd, message, wParam, lParam);
}

// ── posted-message queue (PostMessage semantics for the pump) ────────────────────────────────

static MSG  g_aPosted[64];
static int  g_nPostedHead = 0, g_nPostedCount = 0;

int MfxGetPostedMsg(MSG *pMsg)
{
    if (g_nPostedCount == 0) return 0;
    *pMsg = g_aPosted[g_nPostedHead];
    g_nPostedHead = (g_nPostedHead + 1) % 64;
    g_nPostedCount--;
    return 1;
}

// ── timers ───────────────────────────────────────────────────────────────────────────────────

struct MfxTimer { HWND hWnd; UINT nId; UINT nElapse; DWORD nNext; int bUsed; };
static MfxTimer g_aTimers[16];

// one due timer → a WM_TIMER MSG (modal GetMessage loops retrieve timers as messages,
// exactly like Win32 — TextDialog::Run's auto-repeat is a WM_TIMER wParam 1 in the queue)
int MfxNextDueTimer(MSG *pMsg)
{
    DWORD now = GetTickCount();
    for (int i = 0; i < 16; i++) {
        if (!g_aTimers[i].bUsed || (LONG)(now - g_aTimers[i].nNext) < 0) continue;
        // Freeze the game-loop heartbeat (0x1d1d → CDeskcppView::OnTimer) while a microfx modal is
        // up: Win32 modals deactivate+pause the frame, so its OnTimer no-ops. Without this the tick
        // runs behind the dialog and can spawn a speech-bubble (TextDialog::Run) NESTED inside the
        // modal loop — that game loop then becomes the active message drainer and mis-dispatches the
        // dialog's own button WM_COMMAND to the window proc (a no-op) → the dialog can't be closed.
        // Left un-advanced (no nNext bump) so the game resumes on the very next tick after the modal.
        if (g_mfxModalDepth > 0 && g_aTimers[i].nId == 0x1d1d) continue;
        g_aTimers[i].nNext = now + g_aTimers[i].nElapse;   // coalesce missed ticks (Win32-like)
        pMsg->hwnd = g_aTimers[i].hWnd;
        pMsg->message = WM_TIMER;
        pMsg->wParam = g_aTimers[i].nId;
        pMsg->lParam = 0;
        pMsg->time = now;
        pMsg->pt = g_mfxCursorPos;
        return 1;
    }
    return 0;
}

void MfxPumpTimers()
{
    MSG msg;
    while (MfxNextDueTimer(&msg))
        MfxSendMsg(msg.hwnd, msg.message, msg.wParam, msg.lParam);
}

// ── paint ────────────────────────────────────────────────────────────────────────────────────

void MfxSetDirty() { g_bDirty = 1; }

void MfxPaintIfDirty()
{
    if (!g_bDirty || !g_hRoot) return;
    CFrameWnd *pFrame = (CFrameWnd *)g_hRoot->pWnd;
    CView *pView = pFrame ? pFrame->GetActiveView() : 0;
    if (!pView || !MfxIsWnd(pView->m_hWnd) || !pView->m_hWnd->bVisible) return;
    g_bDirty = 0;
    // Win32 BeginPaint erases first: CDeskcppView::OnEraseBkgnd PatBlts the clip box with
    // COLOR_3DFACE — DrawGameArea's GetPixel probe at (0x138,0x11c) DEPENDS on that gray
    // (a non-gray probe pixel triggers a full RedrawWindow every blit — a redraw storm).
    MfxDispatchMsg(pView, WM_ERASEBKGND, (WPARAM)MfxScreenDC(), 0);
    MfxDispatchMsg(pView, WM_PAINT, 0, 0);        // → CView::OnPaint → OnDraw(screen DC)
    MfxPaintChildren();                           // controls paint OVER the view (z-order)
}

// ── USER C API (real implementations; the M0 stubs for these are gone) ───────────────────────
extern "C" {

HDC GetDC(HWND) { return MfxScreenDC(); }          // one screen DC for everyone, incl. GetDC(NULL)
int ReleaseDC(HWND, HDC) { return 1; }             // cached — never freed

BOOL GetClientRect(HWND hWnd, LPRECT r)
{
    if (!r) return FALSE;
    if (MfxIsWnd(hWnd))
        SetRect(r, 0, 0, hWnd->rc.right - hWnd->rc.left, hWnd->rc.bottom - hWnd->rc.top);
    else
        SetRect(r, 0, 0, 0, 0);
    return TRUE;
}
BOOL GetWindowRect(HWND hWnd, LPRECT r) { return GetClientRect(hWnd, r); }  // window at (0,0)

UINT SetTimer(HWND hWnd, UINT nId, UINT nElapse, void *)
{
    if (nElapse < 10) nElapse = 10;
    int nFree = -1;
    for (int i = 0; i < 16; i++) {
        if (g_aTimers[i].bUsed && g_aTimers[i].hWnd == hWnd && g_aTimers[i].nId == nId) { nFree = i; break; }
        if (!g_aTimers[i].bUsed && nFree < 0) nFree = i;
    }
    if (nFree < 0) return 0;
    g_aTimers[nFree].hWnd = hWnd;
    g_aTimers[nFree].nId = nId;
    g_aTimers[nFree].nElapse = nElapse;
    g_aTimers[nFree].nNext = GetTickCount() + nElapse;
    g_aTimers[nFree].bUsed = 1;
    return nId;
}

BOOL KillTimer(HWND hWnd, UINT nId)
{
    for (int i = 0; i < 16; i++)
        if (g_aTimers[i].bUsed && g_aTimers[i].hWnd == hWnd && g_aTimers[i].nId == nId)
            { g_aTimers[i].bUsed = 0; return TRUE; }
    return FALSE;
}

LRESULT SendMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    { return MfxSendMsg(hWnd, message, wParam, lParam); }

BOOL PostMessageA(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_nPostedCount >= 64) return FALSE;
    MSG *p = &g_aPosted[(g_nPostedHead + g_nPostedCount) % 64];
    p->hwnd = hWnd; p->message = message; p->wParam = wParam; p->lParam = lParam;
    p->time = GetTickCount(); p->pt = g_mfxCursorPos;
    g_nPostedCount++;
    return TRUE;
}

void PostQuitMessage(int nExitCode) { g_mfxQuit = 1; g_mfxQuitCode = nExitCode; }

SHORT GetAsyncKeyState(int vKey)
    { return (vKey >= 0 && vKey < 256 && (g_mfxKeyState[vKey] & 0x80)) ? (SHORT)0x8000 : 0; }

BOOL GetCursorPos(LPPOINT p) { if (p) *p = g_mfxCursorPos; return TRUE; }

int g_mfxCursorEverSet = 0;    // before the game's first SetCursor, Win32 shows the class arrow
HCURSOR SetCursor(HCURSOR h)
    { HCURSOR hOld = g_mfxCursor; g_mfxCursor = h; g_mfxCursorEverSet = 1; return hOld; }

HWND GetParent(HWND h) { return MfxIsWnd(h) ? h->hParent : 0; }

HWND SetCapture(HWND h)   { HWND hOld = g_mfxCapture; g_mfxCapture = h; return hOld; }
BOOL ReleaseCapture(void) { g_mfxCapture = 0; return TRUE; }
HWND SetFocus(HWND h)     { HWND hOld = g_mfxFocus; g_mfxFocus = h; return hOld; }
HWND GetActiveWindow(void) { return g_hRoot; }

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    int bWas = hWnd->bVisible;
    hWnd->bVisible = (nCmdShow != SW_HIDE);
    if (hWnd->bVisible) {
        if (hWnd->hParent && hWnd->nId != AFX_IDW_PANE_FIRST) {
            (void)bWas;                           // even a redundant show repaints (the game
            if (hWnd->pWnd)                       // re-shows AFTER blitting over the controls)
                hWnd->pWnd->MfxCtlPaint();
        } else
            MfxSetDirty();                        // frame/view: every show marks a paint
    }
    return TRUE;
}

BOOL EnableWindow(HWND hWnd, BOOL bEnable)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    int bWas = hWnd->bEnabled;
    hWnd->bEnabled = bEnable ? 1 : 0;
    // NOT gated on m_mfxRedraw (unlike ShowWindow just above, same reasoning: an explicit
    // state-changing call always repaints). TextDialog::ScrollTextLine's keyboard-scroll path
    // calls btnDialogClose.EnableWindow(1) with no accompanying ShowWindow — the WM_SETREDRAW(0)
    // batched at dialog setup (src/DeskcppView.cpp TextDialog::Run) is never explicitly turned
    // back on for the button, only for wndDialogText; gating here left the Close button visually
    // stuck grayed after reaching bottom-of-text via keyboard until some other repaint happened.
    if (bWas != hWnd->bEnabled && hWnd->bVisible && hWnd->pWnd)
        hWnd->pWnd->MfxCtlPaint();                // bitmap buttons swap to the X/U face
    return !bWas;
}

BOOL MoveWindow(HWND hWnd, int x, int y, int cx, int cy, BOOL bRepaint)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    SetRect(&hWnd->rc, x, y, x + cx, y + cy);
    if (bRepaint && hWnd->bVisible && hWnd->pWnd && hWnd->pWnd->m_mfxRedraw)
        hWnd->pWnd->MfxCtlPaint();
    return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND, int x, int y, int cx, int cy, UINT nFlags)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    int w = (nFlags & SWP_NOSIZE) ? hWnd->rc.right - hWnd->rc.left : cx;
    int h = (nFlags & SWP_NOSIZE) ? hWnd->rc.bottom - hWnd->rc.top : cy;
    int nx = (nFlags & SWP_NOMOVE) ? hWnd->rc.left : x;
    int ny = (nFlags & SWP_NOMOVE) ? hWnd->rc.top : y;
    SetRect(&hWnd->rc, nx, ny, nx + w, ny + h);
    return TRUE;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR psz)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    MfxSendMsg(hWnd, WM_SETTEXT, 0, (LPARAM)psz);
    return TRUE;
}

// SB_CTL scroll state (M4e — the inventory scrollbar; nBar is always SB_CTL in this app)
int SetScrollPos(HWND hWnd, int, int nPos, BOOL bRedraw)
{
    if (!MfxIsWnd(hWnd)) return 0;
    int nOld = hWnd->nScrollPos;
    hWnd->nScrollPos = nPos;
    if (bRedraw && hWnd->bVisible && hWnd->pWnd)
        hWnd->pWnd->MfxCtlPaint();
    return nOld;
}

int GetScrollPos(HWND hWnd, int)
{
    return MfxIsWnd(hWnd) ? hWnd->nScrollPos : 0;
}

BOOL SetScrollRange(HWND hWnd, int, int nMin, int nMax, BOOL bRedraw)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    hWnd->nScrollMin = nMin;
    hWnd->nScrollMax = nMax;
    if (hWnd->nScrollPos < nMin) hWnd->nScrollPos = nMin;
    if (hWnd->nScrollPos > nMax) hWnd->nScrollPos = nMax;
    if (bRedraw && hWnd->bVisible && hWnd->pWnd)
        hWnd->pWnd->MfxCtlPaint();
    return TRUE;
}

BOOL GetScrollRange(HWND hWnd, int, LPINT lpMin, LPINT lpMax)
{
    if (lpMin) *lpMin = MfxIsWnd(hWnd) ? hWnd->nScrollMin : 0;
    if (lpMax) *lpMax = MfxIsWnd(hWnd) ? hWnd->nScrollMax : 0;
    return TRUE;
}

BOOL ShowScrollBar(HWND hWnd, int, BOOL bShow)
{
    return ShowWindow(hWnd, bShow ? SW_SHOW : SW_HIDE);
}

BOOL InvalidateRect(HWND, const RECT *, BOOL) { MfxSetDirty(); return TRUE; }
BOOL UpdateWindow(HWND) { MfxPaintIfDirty(); return TRUE; }
BOOL RedrawWindow(HWND, const RECT *, HRGN, UINT) { MfxSetDirty(); MfxPaintIfDirty(); return TRUE; }

BOOL DestroyWindow(HWND hWnd)
{
    if (!MfxIsWnd(hWnd)) return FALSE;
    MfxSendMsg(hWnd, WM_DESTROY, 0, 0);
    for (int i = 0; i < 16; i++)
        if (g_aTimers[i].bUsed && g_aTimers[i].hWnd == hWnd) g_aTimers[i].bUsed = 0;
    if (g_mfxCapture == hWnd) g_mfxCapture = 0;
    if (g_mfxFocus == hWnd) g_mfxFocus = 0;
    if (hWnd->pWnd) hWnd->pWnd->m_hWnd = 0;
    if (g_hRoot == hWnd) { g_hRoot = 0; g_mfxQuit = 1; }   // main window gone → leave the pump
    for (int i = 0; i < g_nWnds; i++)
        if (g_aWnds[i] == hWnd) { g_aWnds[i] = g_aWnds[--g_nWnds]; break; }
    hWnd->nTag = 0;
    free(hWnd);
    return TRUE;
}

} // extern "C"

// ── CWnd window half ─────────────────────────────────────────────────────────────────────────

CWnd *CWnd::FromHandle(HWND hWnd)
{
    if (!hWnd) return 0;
    if (MfxIsWnd(hWnd)) return hWnd->pWnd;
    static CWnd wrap;                               // foreign/fake handle: temporary wrapper
    wrap.m_hWnd = hWnd;
    return &wrap;
}

BOOL CWnd::Create(LPCSTR, LPCSTR, DWORD dwStyle, const RECT &rect,
                  CWnd *pParentWnd, UINT nID, CCreateContext *pContext)
{
    HWND h = (HWND)calloc(1, sizeof(HWND__));
    if (!h) return FALSE;
    h->nTag = MFX_TAG_WND;
    h->pWnd = this;
    h->hParent = pParentWnd ? pParentWnd->m_hWnd : 0;
    h->nId = nID;
    h->rc = rect;
    h->bVisible = (dwStyle & WS_VISIBLE) ? 1 : 0;
    h->bEnabled = 1;
    m_hWnd = h;
    if (g_nWnds < 64) g_aWnds[g_nWnds++] = h;
    if (!h->hParent && !g_hRoot) {
        g_hRoot = h;
        MfxCreateScreenDib(rect.right - rect.left, rect.bottom - rect.top);
    }
    CREATESTRUCT cs;
    memset(&cs, 0, sizeof cs);
    cs.lpCreateParams = pContext;
    cs.hwndParent = h->hParent;
    cs.x = rect.left; cs.y = rect.top;
    cs.cx = rect.right - rect.left; cs.cy = rect.bottom - rect.top;
    cs.style = (LONG)dwStyle;
    if (MfxDispatchMsg(this, WM_CREATE, 0, (LPARAM)&cs) == -1) {
        ::DestroyWindow(h);
        return FALSE;
    }
    return TRUE;
}

BOOL CWnd::DestroyWindow()
{
    if (!MfxIsWnd(m_hWnd)) return FALSE;
    return ::DestroyWindow(m_hWnd);
}

CWnd *CWnd::GetParent() const
{
    if (MfxIsWnd(m_hWnd) && MfxIsWnd(m_hWnd->hParent))
        return m_hWnd->hParent->pWnd;
    return 0;
}

CFrameWnd *CWnd::GetParentFrame() const
{
    for (HWND h = MfxIsWnd(m_hWnd) ? m_hWnd->hParent : 0; MfxIsWnd(h); h = h->hParent)
        if (h->pWnd && h->pWnd->GetRuntimeClass()->IsDerivedFrom(RUNTIME_CLASS(CFrameWnd)))
            return (CFrameWnd *)h->pWnd;
    return g_hRoot && g_hRoot->pWnd != this ? (CFrameWnd *)g_hRoot->pWnd : 0;
}

// GetDlgItem: a direct child of this window with the given control id (M5 dialogs — the
// slider dialogs call GetDlgItem(0x67) then ::SetScrollRange on its m_hWnd).
CWnd *CWnd::GetDlgItem(int nID) const
{
    if (!MfxIsWnd(m_hWnd)) return 0;
    for (int i = 0; i < g_nWnds; i++) {
        HWND h = g_aWnds[i];
        if (MfxIsWnd(h) && h->hParent == m_hWnd && h->nId == (UINT)nID)
            return h->pWnd;
    }
    return 0;
}

// CenterWindow (M5): center this window in the root client area, shifting every direct child
// (dialog controls carry absolute root-client rects, so they move with the frame).
void CWnd::CenterWindow(CWnd *)
{
    if (!MfxIsWnd(m_hWnd) || !g_hRoot) return;
    int rootW = g_hRoot->rc.right - g_hRoot->rc.left;
    int rootH = g_hRoot->rc.bottom - g_hRoot->rc.top;
    int w = m_hWnd->rc.right - m_hWnd->rc.left;
    int h = m_hWnd->rc.bottom - m_hWnd->rc.top;
    int newX = (rootW - w) / 2, newY = (rootH - h) / 2;
    if (newX < 0) newX = 0;
    if (newY < 0) newY = 0;
    int dx = newX - m_hWnd->rc.left, dy = newY - m_hWnd->rc.top;
    if (!dx && !dy) return;
    for (int i = 0; i < g_nWnds; i++) {
        HWND h2 = g_aWnds[i];
        if (!MfxIsWnd(h2)) continue;
        if (h2 == m_hWnd || h2->hParent == m_hWnd) {
            h2->rc.left += dx; h2->rc.right += dx;
            h2->rc.top  += dy; h2->rc.bottom += dy;
        }
    }
    MfxSetDirty();
}

// ── frame creation (real MFC shape: LoadFrame → WM_CREATE → OnCreate → OnCreateClient) ──────

BOOL CFrameWnd::LoadFrame(UINT /*nIDResource*/, DWORD dwDefaultStyle,
                          CWnd *pParentWnd, CCreateContext *pContext)
{
    CREATESTRUCT cs;
    memset(&cs, 0, sizeof cs);
    cs.style = (LONG)dwDefaultStyle;
    cs.cx = 640; cs.cy = 480;
    if (!PreCreateWindow(cs))                      // virtual — CMainFrame pins the game size
        return FALSE;
    RECT rc = { 0, 0, cs.cx, cs.cy };
    return Create(cs.lpszClass, cs.lpszName, (DWORD)cs.style, rc, pParentWnd, 0, pContext);
}

int CFrameWnd::OnCreate(LPCREATESTRUCT lpcs)
{
    if (CWnd::OnCreate(lpcs) == -1)
        return -1;
    CCreateContext *pContext = (CCreateContext *)lpcs->lpCreateParams;
    if (!OnCreateClient(lpcs, pContext))           // virtual — the game realizes its palette here
        return -1;
    return 0;
}

BOOL CFrameWnd::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext)
{
    if (!pContext || !pContext->m_pNewViewClass)
        return TRUE;
    CView *pView = (CView *)pContext->m_pNewViewClass->CreateObject();
    if (!pView)
        return FALSE;
    RECT rc = { 0, 0, lpcs->cx, lpcs->cy };
    if (!pView->Create(0, 0, WS_CHILD | WS_VISIBLE, rc, this, AFX_IDW_PANE_FIRST, pContext))
        return FALSE;
    if (pContext->m_pCurrentDoc)
        pContext->m_pCurrentDoc->AddView(pView);
    SetActiveView(pView, FALSE);
    g_mfxFocus = pView->m_hWnd;
    return TRUE;
}

// ── CView paint plumbing (real MFC: WM_PAINT → CPaintDC → OnDraw) ───────────────────────────

void CView::OnPaint()
{
    CPaintDC dc(this);
    OnDraw(&dc);
}

// ── SDI bootstrap (real MFC OpenDocumentFile(NULL) order: doc → frame(+view) → OnNewDocument
//    → initial update → show). No synchronous paint here: the first WM_PAINT comes from the
//    pump (or an explicit UpdateWindow), so headless harnesses see the same flow as before. ──

void CWinApp::OnFileNew()
{
    CDocTemplate *pTemplate = m_pDocTemplate;
    if (!pTemplate || !pTemplate->m_pDocClass)
        return;
    if (pTemplate->m_pDoc)                         // SDI: one document, ever
        return;
    CDocument *pDoc = (CDocument *)pTemplate->m_pDocClass->CreateObject();
    pTemplate->m_pDoc = pDoc;
    CFrameWnd *pFrame = pTemplate->m_pFrameClass
        ? (CFrameWnd *)pTemplate->m_pFrameClass->CreateObject() : 0;
    CView *pView = 0;
    if (pFrame) {
        m_pMainWnd = pFrame;
        CCreateContext ctx;
        ctx.m_pCurrentDoc = pDoc;
        ctx.m_pNewViewClass = pTemplate->m_pViewClass;
        ctx.m_pNewDocTemplate = pTemplate;
        ctx.m_pCurrentFrame = pFrame;
        pFrame->LoadFrame(pTemplate->m_nIDResource, 0, 0, &ctx);
        pView = pFrame->GetActiveView();
    }
    if (!pView && pTemplate->m_pViewClass) {       // no frame class: bare doc/view graph (M0 shape)
        pView = (CView *)pTemplate->m_pViewClass->CreateObject();
        if (pView)
            pDoc->AddView(pView);
    }
    pDoc->OnNewDocument();
    if (pView)
        pView->OnInitialUpdate();
    if (pFrame)
        pFrame->ShowWindow(m_nCmdShow);            // marks visible + dirty; pump paints
}
