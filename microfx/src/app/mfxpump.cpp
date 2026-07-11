// microfx app/ — the platform-NEUTRAL event pump (H4 M2, made backend-agnostic for the DS-port
// direction; docs/phase-h4-sdl.md). CWinThread::Run: backend events (mfxplat.h MFXPLATEVENT) →
// WM_* synthesis → the game's EXISTING message maps (via mfxwnd.cpp's dispatch engine); a real
// timer heartbeat (SetTimer 0x1d1d → WM_TIMER → CDeskcppView::OnTimer = the game loop); and
// per-tick presentation of the screen DC's 8bpp DIB through MfxPlatPresent.
//
// This TU contains NO platform calls except the MfxPlat* contract — all policy lives here
// (deferred presents + clock-hook flush, WM synthesis, focus/capture routing, accelerator
// translation, modal-loop GetMessage, quit/teardown order, debug oracles), so a new backend
// (microfx/src/platform/mfxplat_*.cpp) only moves pixels and events. With the null backend
// MfxPlatInit returns 0 → Run() no-ops and GetMessageA bails: the headless contract.

#include "mfxwnd.h"
#include <microfx.h>
#include <mfxplat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_bMfxPlatUp = 0;       // MfxPlatInit succeeded — presents/modal waits are live
static DWORD g_tMfxRunStart = 0;   // Run() epoch — YODA_AUTOKEY/AUTOCMD/SHOT times are run-relative

static DWORD MfxRunMs(void) { return GetTickCount() - g_tMfxRunStart; }

// mouse-button state tracked from backend events (WM_* wParam MK_ flags; Win32 semantics:
// the DOWN message already carries its button, the UP message no longer does)
static UINT g_nMouseBtns = 0;

static UINT MfxMouseFlags(void)
{
    UINT nFlags = g_nMouseBtns;
    if (g_mfxKeyState[VK_SHIFT] & 0x80)   nFlags |= MK_SHIFT;
    if (g_mfxKeyState[VK_CONTROL] & 0x80) nFlags |= MK_CONTROL;
    return nFlags;
}

// Deferred presents (v81, the GOAL-0 batching fix): presenting per screen-DC write cost a full
// window present PER PRIMITIVE (~vsync each on macOS's window-surface path), so any handler
// drawing many primitives visibly stepped through its draws and stalled input. Now a screen
// write only marks the frame dirty; the pending present is flushed
//   (a) once per pump iteration (CWinThread::Run's MfxPresentFrame),
//   (b) in modal GetMessage loops (MfxIdle),
//   (c) inside mfx_clock() via MfxSetClockHook, throttled to g_nPresentMs — busy-wait
//       animation loops (ScrollZoneTransition, IACT CMD_WaitTicks, palette flashes) poll
//       clock() right after each frame's draws, so mid-handler animations still present
//       per frame with no per-primitive cost.
// YODA_PRESENT_MS tunes the clock-flush throttle; 0 restores legacy present-per-write.
static int   g_nPresentMs      = 8;
static int   g_bPresentPending = 0;
static DWORD g_tLastPresent    = 0;

static void MfxPresentFrame(void)
{
    MFXDIB dib;
    if (!g_bMfxPlatUp || !MfxGetDCDib(MfxScreenDC(), &dib)) return;
    g_bPresentPending = 0;                     // every present clears the deferred-flush state
    g_tLastPresent = GetTickCount();
    MfxPlatPresent(&dib, g_mfxCursorPos.x, g_mfxCursorPos.y);
}

// present-on-screen-write hook target (MfxSetScreenWriteHook): fired by gdi for every write
// to the screen DC. Marks the frame dirty instead of presenting; single-handler animations
// reach the window through the clock-hook flush below — the pump loop's own MfxPresentFrame
// never runs mid-handler.
static void MfxPresentOnScreenWrite(void)
{
    if (!g_bMfxPlatUp) return;
    if (g_nPresentMs <= 0) { MfxPresentFrame(); return; }   // legacy per-write
    g_bPresentPending = 1;
}

// clock-hook target (MfxSetClockHook): the game's busy-wait loops poll clock() between
// animation frames — flush the deferred present there, throttled, so a frame's completed
// draws show while the handler is still running.
static void MfxFlushPendingPresent(void)
{
    if (!g_bPresentPending || !g_bMfxPlatUp) return;
    if (GetTickCount() - g_tLastPresent < (DWORD)g_nPresentMs) return;
    MfxPresentFrame();
}

// ── cursor DECISION (M4): the game's SetCursor state → a MfxPlatSetCursor mode ──────────────
// The game drives cursor choice via WM_SETCURSOR → CDeskcppView::OnSetCursor → ::SetCursor
// (11 directional/interaction .res cursors + IDC_ARROW + hide-in-keyboard-mode/drag NULL).
// Win32 fidelity rules (all neutral): before the game's first SetCursor the class arrow shows
// (SYSTEM); SetCursor(NULL) hides; IDC_ARROW/system cursors keep the platform arrow. HOW a
// cursor is displayed (software composite vs hardware) is the backend's business.
static void MfxApplyCursor(void)
{
    static HCURSOR hLast = (HCURSOR)(ULONG_PTR)-1;
    if (g_mfxCursor == hLast && g_mfxCursorEverSet) return;
    hLast = g_mfxCursor;

    if (!g_mfxCursorEverSet) {                       // boot: class arrow (the OS one)
        MfxPlatSetCursor(MFXPLAT_CURSOR_SYSTEM, 0, 0, 0, 0);
        return;
    }
    if (!g_mfxCursor) {                              // SetCursor(NULL) = hidden (faithful)
        MfxPlatSetCursor(MFXPLAT_CURSOR_HIDDEN, 0, 0, 0, 0);
        return;
    }
    MFXIMG img;
    if (!MfxGetImage(g_mfxCursor, &img) || img.nSysCursor) {
        MfxPlatSetCursor(MFXPLAT_CURSOR_SYSTEM, 0, 0, 0, 0);   // IDC_ARROW/WAIT: OS arrow
        return;
    }
    MfxPlatSetCursor(MFXPLAT_CURSOR_IMAGE, &img, (const void *)g_mfxCursor,
                     img.xHot, img.yHot);
}

// ── the shared event core (M4): backend events → MSGs in the posted queue ───────────────────
// Both CWinThread::Run AND the game's own modal loops (TextDialog::Run via GetMessageA)
// retrieve messages from ONE queue, Win32-style. Direct-send stays for window housekeeping
// (activation, SC_CLOSE); input becomes queued MSGs so modal loops can filter/dispatch them.
static int g_nMfxQuitRequests = 0;

static void MfxQueueInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PostMessageA(hWnd, message, wParam, lParam);   // stamps time + pt (screen == client)
}

static void MfxPumpPlatEvents(void)
{
    CFrameWnd *pFrame = MfxRootWnd() ? (CFrameWnd *)MfxRootWnd()->pWnd : 0;
    CView *pView = pFrame ? pFrame->GetActiveView() : 0;
    HWND hFrame = pFrame ? pFrame->m_hWnd : 0;
    HWND hView = pView ? pView->m_hWnd : 0;
    HWND hFocus = g_mfxFocus ? g_mfxFocus : hView;

    MFXPLATEVENT ev;
    while (MfxPlatPollEvent(&ev) && !g_mfxQuit) {
        switch (ev.nType) {
        case MFXPLAT_EV_QUIT:
            // window close → WM_SYSCOMMAND SC_CLOSE: the game intercepts and runs its
            // exit confirmation (auto-Yes headless). Second request = hard quit.
            if (++g_nMfxQuitRequests >= 2) g_mfxQuit = 1;
            else MfxSendMsg(hFrame, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;
        case MFXPLAT_EV_EXPOSED:
            MfxSetDirty();
            break;
        case MFXPLAT_EV_FOCUS:
            MfxSendMsg(hFrame, WM_ACTIVATE, MAKELONG(WA_ACTIVE, 0), 0);
            break;
        case MFXPLAT_EV_UNFOCUS:
            MfxSendMsg(hFrame, WM_ACTIVATE, MAKELONG(WA_INACTIVE, 0), 0);
            break;
        case MFXPLAT_EV_KEYDOWN:
            g_mfxKeyState[ev.nVk & 0xff] |= 0x80;
            MfxQueueInput(hFocus, WM_KEYDOWN, (WPARAM)ev.nVk, 1);
            break;
        case MFXPLAT_EV_KEYUP:
            g_mfxKeyState[ev.nVk & 0xff] &= (BYTE)~0x80;
            MfxQueueInput(hFocus, WM_KEYUP, (WPARAM)ev.nVk, 1);
            break;
        case MFXPLAT_EV_CHAR:
            MfxQueueInput(hFocus, WM_CHAR, (WPARAM)ev.nChar, 1);
            break;
        case MFXPLAT_EV_MOUSEMOVE: {
            g_mfxCursorPos.x = ev.x; g_mfxCursorPos.y = ev.y;
            // Win32 sends WM_SETCURSOR ahead of the move — CDeskcppView::OnSetCursor
            // picks the directional/interaction cursor (SetCursor → g_mfxCursor).
            MfxSendMsg(hView, WM_SETCURSOR, (WPARAM)hView,
                       MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            HWND hTarget = g_mfxCapture ? g_mfxCapture : MfxWndFromPoint(g_mfxCursorPos);
            MfxQueueInput(hTarget ? hTarget : hView, WM_MOUSEMOVE,
                          MfxMouseFlags(), MAKELPARAM(ev.x, ev.y));
            break;
        }
        case MFXPLAT_EV_LDOWN:
        case MFXPLAT_EV_LUP:
        case MFXPLAT_EV_RDOWN:
        case MFXPLAT_EV_RUP: {
            g_mfxCursorPos.x = ev.x; g_mfxCursorPos.y = ev.y;
            // update button state FIRST: Win32 down-messages carry their own MK_ bit,
            // up-messages already exclude it
            if (ev.nType == MFXPLAT_EV_LDOWN)      g_nMouseBtns |= MK_LBUTTON;
            else if (ev.nType == MFXPLAT_EV_LUP)   g_nMouseBtns &= ~(UINT)MK_LBUTTON;
            else if (ev.nType == MFXPLAT_EV_RDOWN) g_nMouseBtns |= MK_RBUTTON;
            else                                   g_nMouseBtns &= ~(UINT)MK_RBUTTON;
            UINT nMsg = 0;
            if (ev.nType == MFXPLAT_EV_LDOWN)      nMsg = WM_LBUTTONDOWN;
            else if (ev.nType == MFXPLAT_EV_LUP)   nMsg = WM_LBUTTONUP;
            else if (ev.nType == MFXPLAT_EV_RDOWN) nMsg = WM_RBUTTONDOWN;
            if (nMsg) {
                HWND hTarget = g_mfxCapture ? g_mfxCapture : MfxWndFromPoint(g_mfxCursorPos);
                MfxQueueInput(hTarget ? hTarget : hView, nMsg,
                              MfxMouseFlags(), MAKELPARAM(ev.x, ev.y));
            }
            break;
        }
        }
    }
}

// idle housekeeping shared by Run and modal GetMessage waits
static void MfxIdle(void)
{
    MfxPaintIfDirty();
    MfxApplyCursor();
    MfxPresentFrame();
}

// ── the Win32 message API, real (M4) — modal loops in game code work unmodified ─────────────
extern "C" {

BOOL GetMessageA(LPMSG pMsg, HWND, UINT, UINT)
{
    // headless (null backend / before Run): no user input can ever arrive, so a modal wait
    // would hang forever; bail like the old stub → dialogs auto-dismiss deterministically
    if (!g_bMfxPlatUp) return 0;
    for (;;) {
        if (g_mfxQuit) return 0;                    // WM_QUIT → caller's <1 bail path
        MfxPumpPlatEvents();
        if (MfxGetPostedMsg(pMsg)) return TRUE;
        if (MfxNextDueTimer(pMsg)) return TRUE;
        MfxIdle();
        MfxPlatDelay(5);
    }
}

BOOL TranslateMessage(const MSG *) { return FALSE; }   // WM_CHAR comes pre-made (EV_CHAR)

LRESULT DispatchMessageA(const MSG *pMsg)
{
    if (!pMsg) return 0;
    return MfxSendMsg(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
}

} // extern "C"

// ── keyboard accelerators (M5): the game's real ACCEL table (RT_ACCELERATOR id 2) maps Ctrl+
// chords to menu commands — Ctrl+A About, Ctrl+C/G/W the option sliders, etc. Translate a
// modifier-chord WM_KEYDOWN into a WM_COMMAND to the frame (Win32 TranslateAccelerator), so
// every menu command is reachable without an OS menu bar. Only in CWinThread::Run — NOT in
// modal GetMessage loops (a modal dialog doesn't translate the frame's accelerators).
extern "C" const unsigned char *MfxFindResourceData(unsigned nType, unsigned nId, unsigned *pnSize);

static int MfxTranslateAccel(const MSG *pMsg, HWND hFrame)
{
    if (pMsg->message != WM_KEYDOWN || !hFrame) return 0;
    static const unsigned char *pAccel = 0;
    static unsigned nAccelSize = 0;
    static int bLoaded = 0;
    if (!bLoaded) { pAccel = MfxFindResourceData(9 /*RT_ACCELERATOR*/, 2, &nAccelSize); bLoaded = 1; }
    if (!pAccel) return 0;
    int vk = (int)pMsg->wParam;
    int bCtrl = (g_mfxKeyState[VK_CONTROL] & 0x80) != 0;
    for (unsigned off = 0; off + 8 <= nAccelSize; off += 8) {
        WORD fFlags, key, cmd;
        memcpy(&fFlags, pAccel + off, 2);
        memcpy(&key,    pAccel + off + 2, 2);
        memcpy(&cmd,    pAccel + off + 4, 2);
        if ((fFlags & 0x01) && (fFlags & 0x08) && bCtrl && key == vk) {  // FVIRTKEY+FCONTROL chord
            PostMessageA(hFrame, WM_COMMAND, MAKELONG(cmd, 0), 0);
            return 1;
        }
        if (fFlags & 0x80) break;   // last entry
    }
    return 0;
}

int CWinThread::Run()
{
    HWND hRoot = MfxRootWnd();
    if (!hRoot) return 0;                      // no main window (headless bootstrap) — nothing to run
    int nW = hRoot->rc.right - hRoot->rc.left;
    int nH = hRoot->rc.bottom - hRoot->rc.top;
    if (nW <= 0 || nH <= 0) { nW = 640; nH = 480; }

    int nScale = 2;
    if (const char *psz = getenv("YODA_SCALE")) {
        nScale = atoi(psz);
        if (nScale < 1) nScale = 1;
        if (nScale > 8) nScale = 8;
    }

    if (const char *pszPms = getenv("YODA_PRESENT_MS"))
        g_nPresentMs = atoi(pszPms);           // clock-flush throttle; 0 = legacy per-write

    CWinApp *pApp = AfxGetApp();
    const char *pszTitle = (pApp && pApp->m_pszAppName) ? pApp->m_pszAppName : "microfx";
    int nUp = MfxPlatInit(pszTitle, nW, nH, nScale);
    if (nUp < 0) return 1;                     // backend error (it logged the cause)
    if (nUp == 0) return 0;                    // null backend: headless no-op, the M0 contract
    g_bMfxPlatUp = 1;
    g_tMfxRunStart = GetTickCount();
    MfxSetScreenWriteHook(MfxScreenDC(), MfxPresentOnScreenWrite);
    MfxSetClockHook(MfxFlushPendingPresent);

    while (!g_mfxQuit) {
        CView *pViewNow = MfxRootWnd() ? ((CFrameWnd *)MfxRootWnd()->pWnd)->GetActiveView() : 0;
        HWND hView = pViewNow ? pViewNow->m_hWnd : 0;

        MfxPumpPlatEvents();

        MSG msg;
        HWND hFrameNow = MfxRootWnd() ? MfxRootWnd()->pWnd->m_hWnd : 0;
        while (!g_mfxQuit && MfxGetPostedMsg(&msg)) {
            if (MfxTranslateAccel(&msg, hFrameNow)) continue;   // Ctrl-chord → WM_COMMAND
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (g_mfxQuit) break;
        MfxPumpTimers();                       // WM_TIMER 0x1d1d → OnTimer → the game tick
        MfxPaintIfDirty();                     // WM_PAINT → CView::OnPaint → OnDraw
        MfxApplyCursor();                      // SetCursor state → backend cursor
        MfxPresentFrame();                     // screen DIB → the platform window

        // debug oracle: YODA_AUTOKEY=<startms>:<vk>:<durms> holds a virtual key over the
        // real WM_KEYDOWN/WM_KEYUP dispatch path — the input half of the walk oracle.
        if (const char *pszKey = getenv("YODA_AUTOKEY")) {
            static int nKeyPhase = 0;          // 0=waiting 1=held 2=done
            int nStart = 0, nVk = 0, nDur = 0;
            if (sscanf(pszKey, "%d:%d:%d", &nStart, &nVk, &nDur) == 3 && nVk > 0 && nVk < 256) {
                DWORD now = MfxRunMs();
                if (nKeyPhase == 0 && now >= (DWORD)nStart) {
                    nKeyPhase = 1;
                    g_mfxKeyState[nVk] |= 0x80;
                }
                if (nKeyPhase == 1) {
                    if (now < (DWORD)(nStart + nDur))
                        MfxSendMsg(hView, WM_KEYDOWN, (WPARAM)nVk, 1);   // auto-repeat, Win32-like
                    else {
                        nKeyPhase = 2;
                        g_mfxKeyState[nVk] &= (BYTE)~0x80;
                        MfxSendMsg(hView, WM_KEYUP, (WPARAM)nVk, 1);
                    }
                }
            }
        }

        // debug oracle: YODA_AUTOCMD=<startms>:<cmdhex> posts one WM_COMMAND to the frame at a
        // set time (deterministic menu-command trigger — opens dialogs without a keystroke).
        if (const char *pszCmd = getenv("YODA_AUTOCMD")) {
            static int bCmdSent = 0;
            int nStart = 0; unsigned nCmd = 0;
            if (!bCmdSent && sscanf(pszCmd, "%d:%x", &nStart, &nCmd) == 2 &&
                MfxRunMs() >= (DWORD)nStart) {
                bCmdSent = 1;
                PostMessageA(hFrameNow, WM_COMMAND, MAKELONG((WORD)nCmd, 0), 0);
            }
        }

        // debug oracle: YODA_SHOT=<prefix>[:count] dumps the screen DIB as <prefix>NN.bmp
        // every 2s (default 8 shots) — lets a headless-ish driver verify boot/render/
        // animation without eyes.
        static int nShots = 0;
        static DWORD nNextShot = 0;
        if (const char *pszShot = getenv("YODA_SHOT")) {
            char szPfx[256];
            int nMaxShots = 8;
            if (sscanf(pszShot, "%255[^:]:%d", szPfx, &nMaxShots) < 2)
                nMaxShots = 8;
            DWORD now = MfxRunMs();
            if (nShots < nMaxShots && now >= nNextShot) {
                char szPath[512];
                snprintf(szPath, sizeof szPath, "%s%02d.bmp", szPfx, nShots++);
                MfxWriteDibBMP(MfxScreenDC(), szPath);
                nNextShot = now + 2000;
            }
        }
        MfxPlatDelay(5);
    }

    MfxSetClockHook(0);
    MfxSetScreenWriteHook(0, 0);
    g_bMfxPlatUp = 0;

    // real window teardown (M4e): the polite-exit path previously never ran the view dtor,
    // so ~CDeskcppView's WaveMixCloseSession was unreached (audio reclaimed by the OS).
    // MFC shape: destroy the view window (WM_DESTROY kills the game timer + scrollbar),
    // delete the view (dtor closes the sound session), then the frame.
    if (MfxRootWnd()) {
        CFrameWnd *pFrame = (CFrameWnd *)MfxRootWnd()->pWnd;
        CView *pView = pFrame ? pFrame->GetActiveView() : 0;
        if (pView) {
            pView->DestroyWindow();
            delete pView;
        }
        if (pFrame) {
            pFrame->DestroyWindow();
            delete pFrame;
        }
    }

    MfxPlatShutdown();
    return g_mfxQuitCode;
}
