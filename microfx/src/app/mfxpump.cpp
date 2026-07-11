// microfx app/ — the SDL event pump (H4 M2, docs/phase-h4-sdl.md). CWinThread::Run:
// SDL events → WM_* synthesis → the game's EXISTING message maps (via mfxwnd.cpp's dispatch
// engine); a real timer heartbeat (SetTimer 0x1d1d → WM_TIMER → CDeskcppView::OnTimer = the
// game loop); and per-frame presentation of the screen DC's 8bpp DIB to the SDL window
// (SDL_CreateRGBSurfaceWithFormatFrom INDEX8 wrap — the zone_view --show mechanism, made
// permanent). This is the ONLY microfx source that touches SDL; without SDL2 Run() degrades
// to a no-op so headless targets keep building.

#include "mfxwnd.h"
#include <microfx.h>

#ifndef MICROFX_HAS_SDL

int CWinThread::Run() { return 0; }    // headless build: nothing to pump

#else

#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

// ── SDL keycode → Win32 VK (only what the game's OnKeyDown/OnKeyUp/GetAsyncKeyState read) ───
static int MfxKeyToVk(SDL_Keycode sym)
{
    switch (sym) {
    case SDLK_UP:        case SDLK_KP_8: return VK_UP;
    case SDLK_DOWN:      case SDLK_KP_2: return VK_DOWN;
    case SDLK_LEFT:      case SDLK_KP_4: return VK_LEFT;
    case SDLK_RIGHT:     case SDLK_KP_6: return VK_RIGHT;
    case SDLK_HOME:      case SDLK_KP_7: return VK_HOME;   // up-left
    case SDLK_END:       case SDLK_KP_1: return VK_END;    // down-left
    case SDLK_PAGEUP:    case SDLK_KP_9: return VK_PRIOR;  // up-right
    case SDLK_PAGEDOWN:  case SDLK_KP_3: return VK_NEXT;   // down-right
    case SDLK_SPACE:     return VK_SPACE;
    case SDLK_INSERT:    case SDLK_KP_0: return VK_INSERT;
    case SDLK_DELETE:    return VK_DELETE;
    case SDLK_RETURN:    return VK_RETURN;
    case SDLK_BACKSPACE: return VK_BACK;
    case SDLK_TAB:       return VK_TAB;
    case SDLK_ESCAPE:    return VK_ESCAPE;
    case SDLK_LSHIFT:    case SDLK_RSHIFT: return VK_SHIFT;
    case SDLK_LCTRL:     case SDLK_RCTRL:  return VK_CONTROL;
    case SDLK_LALT:      case SDLK_RALT:   return VK_MENU;
    default:
        if (sym >= SDLK_a && sym <= SDLK_z) return 'A' + (int)(sym - SDLK_a);
        if (sym >= SDLK_0 && sym <= SDLK_9) return '0' + (int)(sym - SDLK_0);
        if (sym >= SDLK_F1 && sym <= SDLK_F10) return VK_F1 + (int)(sym - SDLK_F1);
        return 0;
    }
}

static UINT MfxMouseFlags(void)
{
    Uint32 nBtn = SDL_GetMouseState(0, 0);
    UINT nFlags = 0;
    if (nBtn & SDL_BUTTON(SDL_BUTTON_LEFT))  nFlags |= MK_LBUTTON;
    if (nBtn & SDL_BUTTON(SDL_BUTTON_RIGHT)) nFlags |= MK_RBUTTON;
    if (g_mfxKeyState[VK_SHIFT] & 0x80)      nFlags |= MK_SHIFT;
    if (g_mfxKeyState[VK_CONTROL] & 0x80)    nFlags |= MK_CONTROL;
    return nFlags;
}

// software cursor state (MfxApplyCursor decides; MfxPresent composites it over the frame)
static SDL_Surface *g_pMfxCursorSurf = 0;
static int g_nMfxCursorHotX = 0, g_nMfxCursorHotY = 0;

static void MfxPresent(SDL_Window *pWin)
{
    MFXDIB dib;
    if (!MfxGetDCDib(MfxScreenDC(), &dib)) return;
    SDL_Surface *pSrc = SDL_CreateRGBSurfaceWithFormatFrom(dib.pBits,
        dib.nWidth, dib.nHeight, 8, dib.nWidth, SDL_PIXELFORMAT_INDEX8);
    if (!pSrc) return;
    SDL_Color aColors[256];
    for (int i = 0; i < 256; i++) {
        aColors[i].r = dib.pPal[i].rgbRed;
        aColors[i].g = dib.pPal[i].rgbGreen;
        aColors[i].b = dib.pPal[i].rgbBlue;
        aColors[i].a = 255;
    }
    SDL_SetPaletteColors(pSrc->format->palette, aColors, 0, 256);
    SDL_Surface *pWinSurf = SDL_GetWindowSurface(pWin);
    if (pWinSurf) {
        int nScale = 1;
        if (pWinSurf->w == pSrc->w && pWinSurf->h == pSrc->h)
            SDL_BlitSurface(pSrc, 0, pWinSurf, 0);
        else {
            SDL_BlitScaled(pSrc, 0, pWinSurf, 0);   // integer window scale (YODA_SCALE)
            nScale = pSrc->w ? pWinSurf->w / pSrc->w : 1;
            if (nScale < 1) nScale = 1;
        }
        // software cursor: composite over the scaled frame (hardware path leaves this 0)
        SDL_Surface *pCur = g_pMfxCursorSurf;
        if (pCur) {
            SDL_Rect rcDst;
            rcDst.x = (g_mfxCursorPos.x - g_nMfxCursorHotX) * nScale;
            rcDst.y = (g_mfxCursorPos.y - g_nMfxCursorHotY) * nScale;
            rcDst.w = pCur->w; rcDst.h = pCur->h;
            SDL_BlitSurface(pCur, 0, pWinSurf, &rcDst);
        }
        SDL_UpdateWindowSurface(pWin);
    }
    SDL_FreeSurface(pSrc);
}

// present-on-screen-write hook target (MfxSetScreenWriteHook): fired by gdi BitBlt for every
// write to the screen DC, so single-handler animations (scroll transitions, the X-Wing STUP
// flight) reach the window mid-handler — the pump loop's own MfxPresent never runs there.
static SDL_Window *g_pMfxPresentWin = 0;
static void MfxPresentOnScreenWrite(void)
{
    if (g_pMfxPresentWin) MfxPresent(g_pMfxPresentWin);
}

// ── cursors (M4): apply the game's SetCursor through SDL ────────────────────────────────────
// The game drives cursor choice via WM_SETCURSOR → CDeskcppView::OnSetCursor → ::SetCursor
// (11 directional/interaction .res cursors + IDC_ARROW + hide-in-keyboard-mode/drag NULL).
// DEFAULT: a SOFTWARE cursor — the decoded cursor image is composited over the window
// surface at present time (chunky at the window scale, matches the game pixels; also the
// path a DS port needs, and YODA_HWCURSOR=1 opts into SDL hardware color cursors).
// IDC_ARROW / never-set keep the OS arrow (Win32 shows the class cursor until the game's
// first SetCursor).
static SDL_Surface *MfxMakeCursorSurface(const MFXIMG *pImg, int nScale)
{
    int w = pImg->nWidth * nScale, h = pImg->nHeight * nScale;
    SDL_Surface *pSurf = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!pSurf) return 0;
    Uint32 *pPix = (Uint32 *)pSurf->pixels;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            size_t n = (size_t)(y / nScale) * pImg->nWidth + (x / nScale);
            Uint32 v = 0;
            if (!pImg->pMask || !pImg->pMask[n]) {
                const RGBQUAD *pQ = &pImg->pPal[pImg->pIdx[n]];
                v = 0xff000000u | ((Uint32)pQ->rgbRed << 16) |
                    ((Uint32)pQ->rgbGreen << 8) | pQ->rgbBlue;
            }
            pPix[(size_t)y * (pSurf->pitch / 4) + x] = v;
        }
    return pSurf;
}

static void MfxApplyCursor(int nScale)
{
    static HCURSOR hLast = (HCURSOR)(ULONG_PTR)-1;
    static struct { HCURSOR h; SDL_Surface *pSurf; SDL_Cursor *pHw; int xHot, yHot; } aCache[24];
    static int nCache = 0;
    static int nHwMode = -1;
    if (nHwMode < 0) nHwMode = getenv("YODA_HWCURSOR") ? 1 : 0;

    if (g_mfxCursor == hLast && g_mfxCursorEverSet) return;
    hLast = g_mfxCursor;

    if (!g_mfxCursorEverSet) {                       // boot: class arrow (the OS one)
        g_pMfxCursorSurf = 0;
        SDL_ShowCursor(SDL_ENABLE);
        return;
    }
    if (!g_mfxCursor) {                              // SetCursor(NULL) = hidden (faithful)
        g_pMfxCursorSurf = 0;
        SDL_ShowCursor(SDL_DISABLE);
        return;
    }
    MFXIMG img;
    if (!MfxGetImage(g_mfxCursor, &img) || img.nSysCursor) {
        g_pMfxCursorSurf = 0;                        // IDC_ARROW/WAIT: the OS arrow suffices
        SDL_ShowCursor(SDL_ENABLE);
        return;
    }
    int nHit = -1;
    for (int i = 0; i < nCache; i++)
        if (aCache[i].h == g_mfxCursor) { nHit = i; break; }
    if (nHit < 0 && nCache < 24) {
        nHit = nCache;
        aCache[nHit].h = g_mfxCursor;
        aCache[nHit].pSurf = MfxMakeCursorSurface(&img, nScale);
        aCache[nHit].pHw = nHwMode && aCache[nHit].pSurf
            ? SDL_CreateColorCursor(aCache[nHit].pSurf, img.xHot * nScale, img.yHot * nScale) : 0;
        aCache[nHit].xHot = img.xHot; aCache[nHit].yHot = img.yHot;
        nCache++;
    }
    if (nHit < 0) { SDL_ShowCursor(SDL_ENABLE); return; }
    if (nHwMode && aCache[nHit].pHw) {
        g_pMfxCursorSurf = 0;
        SDL_SetCursor(aCache[nHit].pHw);
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        g_pMfxCursorSurf = aCache[nHit].pSurf;
        g_nMfxCursorHotX = aCache[nHit].xHot;
        g_nMfxCursorHotY = aCache[nHit].yHot;
        SDL_ShowCursor(SDL_DISABLE);                 // we draw it ourselves
    }
}

// ── the shared event core (M4): SDL events → MSGs in the posted queue ───────────────────────
// Both CWinThread::Run AND the game's own modal loops (TextDialog::Run via GetMessageA)
// retrieve messages from ONE queue, Win32-style. Direct-send stays for window housekeeping
// (activation, SC_CLOSE); input becomes queued MSGs so modal loops can filter/dispatch them.
static int g_nMfxScale = 2;
static int g_nMfxQuitRequests = 0;

static void MfxQueueInput(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PostMessageA(hWnd, message, wParam, lParam);   // stamps time + pt (screen == client)
}

static void MfxPumpSdlEvents(void)
{
    CFrameWnd *pFrame = MfxRootWnd() ? (CFrameWnd *)MfxRootWnd()->pWnd : 0;
    CView *pView = pFrame ? pFrame->GetActiveView() : 0;
    HWND hFrame = pFrame ? pFrame->m_hWnd : 0;
    HWND hView = pView ? pView->m_hWnd : 0;
    HWND hFocus = g_mfxFocus ? g_mfxFocus : hView;
    int nScale = g_nMfxScale;

    SDL_Event ev;
    while (SDL_PollEvent(&ev) && !g_mfxQuit) {
        switch (ev.type) {
        case SDL_QUIT:
            // window close → WM_SYSCOMMAND SC_CLOSE: the game intercepts and runs its
            // exit confirmation (auto-Yes headless). Second request = hard quit.
            if (++g_nMfxQuitRequests >= 2) g_mfxQuit = 1;
            else MfxSendMsg(hFrame, WM_SYSCOMMAND, SC_CLOSE, 0);
            break;
        case SDL_WINDOWEVENT:
            if (ev.window.event == SDL_WINDOWEVENT_EXPOSED)
                MfxSetDirty();
            else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                MfxSendMsg(hFrame, WM_ACTIVATE, MAKELONG(WA_ACTIVE, 0), 0);
            else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                MfxSendMsg(hFrame, WM_ACTIVATE, MAKELONG(WA_INACTIVE, 0), 0);
            break;
        case SDL_KEYDOWN: {
            int vk = MfxKeyToVk(ev.key.keysym.sym);
            if (!vk) break;
            g_mfxKeyState[vk] |= 0x80;
            MfxQueueInput(hFocus, WM_KEYDOWN, (WPARAM)vk, 1);
            break;
        }
        case SDL_KEYUP: {
            int vk = MfxKeyToVk(ev.key.keysym.sym);
            if (!vk) break;
            g_mfxKeyState[vk] &= (BYTE)~0x80;
            MfxQueueInput(hFocus, WM_KEYUP, (WPARAM)vk, 1);
            break;
        }
        case SDL_TEXTINPUT:
            for (const char *p = ev.text.text; *p; p++)
                if ((unsigned char)*p < 0x80)
                    MfxQueueInput(hFocus, WM_CHAR, (WPARAM)(unsigned char)*p, 1);
            break;
        case SDL_MOUSEMOTION: {
            int x = ev.motion.x / nScale, y = ev.motion.y / nScale;
            g_mfxCursorPos.x = x; g_mfxCursorPos.y = y;
            // Win32 sends WM_SETCURSOR ahead of the move — CDeskcppView::OnSetCursor
            // picks the directional/interaction cursor (SetCursor → g_mfxCursor).
            MfxSendMsg(hView, WM_SETCURSOR, (WPARAM)hView,
                       MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            HWND hTarget = g_mfxCapture ? g_mfxCapture : MfxWndFromPoint(g_mfxCursorPos);
            MfxQueueInput(hTarget ? hTarget : hView, WM_MOUSEMOVE,
                          MfxMouseFlags(), MAKELPARAM(x, y));
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int x = ev.button.x / nScale, y = ev.button.y / nScale;
            g_mfxCursorPos.x = x; g_mfxCursorPos.y = y;
            UINT nMsg = 0;
            if (ev.button.button == SDL_BUTTON_LEFT)
                nMsg = (ev.type == SDL_MOUSEBUTTONDOWN) ? WM_LBUTTONDOWN : WM_LBUTTONUP;
            else if (ev.button.button == SDL_BUTTON_RIGHT && ev.type == SDL_MOUSEBUTTONDOWN)
                nMsg = WM_RBUTTONDOWN;
            if (nMsg) {
                HWND hTarget = g_mfxCapture ? g_mfxCapture : MfxWndFromPoint(g_mfxCursorPos);
                MfxQueueInput(hTarget ? hTarget : hView, nMsg,
                              MfxMouseFlags(), MAKELPARAM(x, y));
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
    MfxApplyCursor(g_nMfxScale);
    if (g_pMfxPresentWin) MfxPresent(g_pMfxPresentWin);
}

// ── the Win32 message API, real (M4) — modal loops in game code work unmodified ─────────────
extern "C" {

BOOL GetMessageA(LPMSG pMsg, HWND, UINT, UINT)
{
    // headless (no SDL window — game_walk et al): no user input can ever arrive, so a modal
    // wait would hang forever; bail like the old stub → dialogs auto-dismiss deterministically
    if (!g_pMfxPresentWin) return 0;
    for (;;) {
        if (g_mfxQuit) return 0;                    // WM_QUIT → caller's <1 bail path
        MfxPumpSdlEvents();
        if (MfxGetPostedMsg(pMsg)) return TRUE;
        if (MfxNextDueTimer(pMsg)) return TRUE;
        MfxIdle();
        SDL_Delay(5);
    }
}

BOOL TranslateMessage(const MSG *) { return FALSE; }   // WM_CHAR comes pre-made (SDL_TEXTINPUT)

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
    g_nMfxScale = nScale;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "microfx: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    CWinApp *pApp = AfxGetApp();
    const char *pszTitle = (pApp && pApp->m_pszAppName) ? pApp->m_pszAppName : "microfx";
    SDL_Window *pWin = SDL_CreateWindow(pszTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        nW * nScale, nH * nScale, 0);
    if (!pWin) {
        fprintf(stderr, "microfx: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_StartTextInput();                      // WM_CHAR synthesis (the cheat-code buffer)
    g_pMfxPresentWin = pWin;
    MfxSetScreenWriteHook(MfxScreenDC(), MfxPresentOnScreenWrite);

    while (!g_mfxQuit) {
        CView *pViewNow = MfxRootWnd() ? ((CFrameWnd *)MfxRootWnd()->pWnd)->GetActiveView() : 0;
        HWND hView = pViewNow ? pViewNow->m_hWnd : 0;

        MfxPumpSdlEvents();

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
        MfxApplyCursor(nScale);                // SetCursor state → SDL hardware cursor
        MfxPresent(pWin);                      // screen DIB → SDL window

        // debug oracle: YODA_AUTOKEY=<startms>:<vk>:<durms> holds a virtual key over the
        // real WM_KEYDOWN/WM_KEYUP dispatch path — the input half of the walk oracle.
        if (const char *pszKey = getenv("YODA_AUTOKEY")) {
            static int nKeyPhase = 0;          // 0=waiting 1=held 2=done
            int nStart = 0, nVk = 0, nDur = 0;
            if (SDL_sscanf(pszKey, "%d:%d:%d", &nStart, &nVk, &nDur) == 3 && nVk > 0 && nVk < 256) {
                Uint32 now = SDL_GetTicks();
                if (nKeyPhase == 0 && now >= (Uint32)nStart) {
                    nKeyPhase = 1;
                    g_mfxKeyState[nVk] |= 0x80;
                }
                if (nKeyPhase == 1) {
                    if (now < (Uint32)(nStart + nDur))
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
            if (!bCmdSent && SDL_sscanf(pszCmd, "%d:%x", &nStart, &nCmd) == 2 &&
                SDL_GetTicks() >= (Uint32)nStart) {
                bCmdSent = 1;
                PostMessageA(hFrameNow, WM_COMMAND, MAKELONG((WORD)nCmd, 0), 0);
            }
        }

        // debug oracle: YODA_SHOT=<prefix>[:count] dumps the screen DIB as <prefix>NN.bmp
        // every 2s (default 8 shots) — lets a headless-ish driver verify boot/render/
        // animation without eyes.
        static int nShots = 0;
        static Uint32 nNextShot = 0;
        if (const char *pszShot = getenv("YODA_SHOT")) {
            char szPfx[256];
            int nMaxShots = 8;
            if (SDL_sscanf(pszShot, "%255[^:]:%d", szPfx, &nMaxShots) < 2)
                nMaxShots = 8;
            Uint32 now = SDL_GetTicks();
            if (nShots < nMaxShots && now >= nNextShot) {
                char szPath[512];
                SDL_snprintf(szPath, sizeof szPath, "%s%02d.bmp", szPfx, nShots++);
                MfxWriteDibBMP(MfxScreenDC(), szPath);
                nNextShot = now + 2000;
            }
        }
        SDL_Delay(5);
    }

    MfxSetScreenWriteHook(0, 0);
    g_pMfxPresentWin = 0;

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

    SDL_DestroyWindow(pWin);
    SDL_Quit();
    return g_mfxQuitCode;
}

#endif // MICROFX_HAS_SDL
