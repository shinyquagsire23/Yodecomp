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
        if (pWinSurf->w == pSrc->w && pWinSurf->h == pSrc->h)
            SDL_BlitSurface(pSrc, 0, pWinSurf, 0);
        else
            SDL_BlitScaled(pSrc, 0, pWinSurf, 0);   // integer window scale (YODA_SCALE)
        SDL_UpdateWindowSurface(pWin);
    }
    SDL_FreeSurface(pSrc);
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

    int nQuitRequests = 0;
    while (!g_mfxQuit) {
        CFrameWnd *pFrame = MfxRootWnd() ? (CFrameWnd *)MfxRootWnd()->pWnd : 0;
        CView *pView = pFrame ? pFrame->GetActiveView() : 0;
        HWND hFrame = pFrame ? pFrame->m_hWnd : 0;
        HWND hView = pView ? pView->m_hWnd : 0;
        HWND hMouseTarget = g_mfxCapture ? g_mfxCapture : hView;

        SDL_Event ev;
        while (SDL_PollEvent(&ev) && !g_mfxQuit) {
            switch (ev.type) {
            case SDL_QUIT:
                // window close → WM_SYSCOMMAND SC_CLOSE: the game intercepts and runs its
                // exit confirmation (auto-Yes headless). Second request = hard quit.
                if (++nQuitRequests >= 2) g_mfxQuit = 1;
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
                MfxSendMsg(hView, WM_KEYDOWN, (WPARAM)vk, 1);
                break;
            }
            case SDL_KEYUP: {
                int vk = MfxKeyToVk(ev.key.keysym.sym);
                if (!vk) break;
                g_mfxKeyState[vk] &= (BYTE)~0x80;
                MfxSendMsg(hView, WM_KEYUP, (WPARAM)vk, 1);
                break;
            }
            case SDL_TEXTINPUT:
                for (const char *p = ev.text.text; *p; p++)
                    if ((unsigned char)*p < 0x80)
                        MfxSendMsg(hView, WM_CHAR, (WPARAM)(unsigned char)*p, 1);
                break;
            case SDL_MOUSEMOTION: {
                int x = ev.motion.x / nScale, y = ev.motion.y / nScale;
                g_mfxCursorPos.x = x; g_mfxCursorPos.y = y;
                MfxSendMsg(hMouseTarget, WM_MOUSEMOVE, MfxMouseFlags(), MAKELPARAM(x, y));
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
                if (nMsg)
                    MfxSendMsg(hMouseTarget, nMsg, MfxMouseFlags(), MAKELPARAM(x, y));
                break;
            }
            }
        }

        MSG msg;
        while (!g_mfxQuit && MfxGetPostedMsg(&msg))
            MfxSendMsg(msg.hwnd, msg.message, msg.wParam, msg.lParam);

        if (g_mfxQuit) break;
        MfxPumpTimers();                       // WM_TIMER 0x1d1d → OnTimer → the game tick
        MfxPaintIfDirty();                     // WM_PAINT → CView::OnPaint → OnDraw
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

        // debug oracle: YODA_SHOT=<prefix> dumps the screen DIB as <prefix>NN.bmp every 2s
        // (up to 8) — lets a headless-ish driver verify boot/render/animation without eyes.
        static int nShots = 0;
        static Uint32 nNextShot = 0;
        if (const char *pszShot = getenv("YODA_SHOT")) {
            Uint32 now = SDL_GetTicks();
            if (nShots < 8 && now >= nNextShot) {
                char szPath[512];
                SDL_snprintf(szPath, sizeof szPath, "%s%02d.bmp", pszShot, nShots++);
                MfxWriteDibBMP(MfxScreenDC(), szPath);
                nNextShot = now + 2000;
            }
        }
        SDL_Delay(5);
    }

    SDL_DestroyWindow(pWin);
    SDL_Quit();
    return g_mfxQuitCode;
}

#endif // MICROFX_HAS_SDL
