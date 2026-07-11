// microfx platform/ — the SDL3 video/input backend (proof of the mfxplat.h modularity: this TU
// was added WITHOUT touching the neutral pump). Same shape as mfxplat_sdl2.cpp, ported to the
// SDL3 API: window + present (window-surface blit, or YODA_ACCEL=1 renderer + streaming
// texture), SDL3 events → MFXPLATEVENT (SDL keycodes → Win32 VKs), cursor display (software
// composite at present time by default, YODA_HWCURSOR=1 for SDL color cursors).
// Selected by cmake/PortableSDL.cmake when SDL3 is found (preferred over SDL2).

#include <mfxplat.h>

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *s_pWin = 0;
static int s_nScale = 2;

// YODA_ACCEL=1 (opt-in): SDL_Renderer + streaming texture present path (see the SDL2 twin
// for the full rationale). YODA_VSYNC=1 re-adds vsync via SDL_SetRenderVSync.
static SDL_Renderer *s_pRen = 0;
static SDL_Texture  *s_pFrameTex = 0;

// soft-cursor state (MfxPlatSetCursor decides; MfxPlatPresent composites)
static SDL_Surface *s_pCursorSurf = 0;
static int s_xHot = 0, s_yHot = 0;

// ── init / teardown ──────────────────────────────────────────────────────────────────────────

extern "C" int MfxPlatInit(const char *pszTitle, int nW, int nH, int nScale)
{
    s_nScale = (nScale >= 1) ? nScale : 1;
    // (no SDL_SetMainReady: SDL3 only wraps main() if SDL3/SDL_main.h is included)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "microfx: SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    s_pWin = SDL_CreateWindow(pszTitle, nW * s_nScale, nH * s_nScale, 0);
    if (!s_pWin) {
        fprintf(stderr, "microfx: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    SDL_StartTextInput(s_pWin);                // MFXPLAT_EV_CHAR synthesis (cheat-code buffer)

    if (getenv("YODA_ACCEL")) {
        s_pRen = SDL_CreateRenderer(s_pWin, NULL);
        if (s_pRen) {
            if (getenv("YODA_VSYNC")) SDL_SetRenderVSync(s_pRen, 1);
            SDL_SetRenderLogicalPresentation(s_pRen, nW * s_nScale, nH * s_nScale,
                                             SDL_LOGICAL_PRESENTATION_LETTERBOX);
            s_pFrameTex = SDL_CreateTexture(s_pRen, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, nW, nH);
            if (s_pFrameTex)
                SDL_SetTextureScaleMode(s_pFrameTex, SDL_SCALEMODE_NEAREST);
        }
        if (!s_pRen || !s_pFrameTex) {
            fprintf(stderr, "microfx: YODA_ACCEL renderer init failed (%s) — "
                            "falling back to window-surface present\n", SDL_GetError());
            if (s_pFrameTex) { SDL_DestroyTexture(s_pFrameTex); s_pFrameTex = 0; }
            if (s_pRen) { SDL_DestroyRenderer(s_pRen); s_pRen = 0; }
        }
    }
    return 1;
}

extern "C" void MfxPlatShutdown(void)
{
    if (s_pFrameTex) { SDL_DestroyTexture(s_pFrameTex); s_pFrameTex = 0; }
    if (s_pRen) { SDL_DestroyRenderer(s_pRen); s_pRen = 0; }
    if (s_pWin) { SDL_DestroyWindow(s_pWin); s_pWin = 0; }
    SDL_Quit();
}

// ── events ───────────────────────────────────────────────────────────────────────────────────

// SDL keycode → Win32 VK (only what the game's OnKeyDown/OnKeyUp/GetAsyncKeyState read)
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
        if (sym >= SDLK_A && sym <= SDLK_Z) return 'A' + (int)(sym - SDLK_A);
        if (sym >= SDLK_0 && sym <= SDLK_9) return '0' + (int)(sym - SDLK_0);
        if (sym >= SDLK_F1 && sym <= SDLK_F10) return VK_F1 + (int)(sym - SDLK_F1);
        return 0;
    }
}

extern "C" int MfxPlatPollEvent(MFXPLATEVENT *pEv)
{
    // one SDL_EVENT_TEXT_INPUT can carry several chars — drain one MFXPLAT_EV_CHAR at a time
    static char s_szTextPend[32];
    static int  s_nTextPend = 0;

    pEv->nType = MFXPLAT_EV_NONE;
    pEv->nVk = pEv->nChar = pEv->x = pEv->y = 0;

    if (s_nTextPend > 0 && s_szTextPend[0]) {
        pEv->nType = MFXPLAT_EV_CHAR;
        pEv->nChar = (unsigned char)s_szTextPend[0];
        memmove(s_szTextPend, s_szTextPend + 1, --s_nTextPend);
        s_szTextPend[s_nTextPend] = 0;
        return 1;
    }

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            pEv->nType = MFXPLAT_EV_QUIT;
            return 1;
        case SDL_EVENT_WINDOW_EXPOSED:
            pEv->nType = MFXPLAT_EV_EXPOSED;
            return 1;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
            pEv->nType = MFXPLAT_EV_FOCUS;
            return 1;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            pEv->nType = MFXPLAT_EV_UNFOCUS;
            return 1;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            int vk = MfxKeyToVk(ev.key.key);
            if (!vk) break;                         // unmapped key: keep polling
            pEv->nType = (ev.type == SDL_EVENT_KEY_DOWN) ? MFXPLAT_EV_KEYDOWN
                                                         : MFXPLAT_EV_KEYUP;
            pEv->nVk = vk;
            return 1;
        }
        case SDL_EVENT_TEXT_INPUT: {
            int n = 0;
            for (const char *p = ev.text.text; *p && n + 1 < (int)sizeof s_szTextPend; p++)
                if ((unsigned char)*p < 0x80)
                    s_szTextPend[n++] = *p;
            s_szTextPend[n] = 0;
            if (!n) break;
            pEv->nType = MFXPLAT_EV_CHAR;
            pEv->nChar = (unsigned char)s_szTextPend[0];
            memmove(s_szTextPend, s_szTextPend + 1, n);   // includes the NUL
            s_nTextPend = n - 1;
            return 1;
        }
        case SDL_EVENT_MOUSE_MOTION:
            pEv->nType = MFXPLAT_EV_MOUSEMOVE;
            pEv->x = (int)ev.motion.x / s_nScale;
            pEv->y = (int)ev.motion.y / s_nScale;
            return 1;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int bDown = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            if (ev.button.button == SDL_BUTTON_LEFT)
                pEv->nType = bDown ? MFXPLAT_EV_LDOWN : MFXPLAT_EV_LUP;
            else if (ev.button.button == SDL_BUTTON_RIGHT)
                pEv->nType = bDown ? MFXPLAT_EV_RDOWN : MFXPLAT_EV_RUP;
            else
                break;                              // other buttons: keep polling
            pEv->x = (int)ev.button.x / s_nScale;
            pEv->y = (int)ev.button.y / s_nScale;
            return 1;
        }
        }
    }
    return 0;
}

// ── present ──────────────────────────────────────────────────────────────────────────────────

static void MfxPresentAccel(const MFXDIB *pDib, int xCur, int yCur)
{
    Uint32 aArgb[256];
    for (int i = 0; i < 256; i++)
        aArgb[i] = 0xff000000u | ((Uint32)pDib->pPal[i].rgbRed << 16) |
                   ((Uint32)pDib->pPal[i].rgbGreen << 8) | pDib->pPal[i].rgbBlue;

    void *pPix; int nPitch;
    if (SDL_LockTexture(s_pFrameTex, 0, &pPix, &nPitch)) {
        for (int y = 0; y < pDib->nHeight; y++) {
            Uint32 *pDstRow = (Uint32 *)((Uint8 *)pPix + (size_t)y * nPitch);
            const unsigned char *pSrcRow = pDib->pBits + (size_t)y * pDib->nWidth;
            for (int x = 0; x < pDib->nWidth; x++) pDstRow[x] = aArgb[pSrcRow[x]];
        }
        SDL_UnlockTexture(s_pFrameTex);
    }
    SDL_RenderClear(s_pRen);
    SDL_RenderTexture(s_pRen, s_pFrameTex, 0, 0);

    // software cursor: s_pCursorSurf is pre-scaled by s_nScale (MfxMakeCursorSurface)
    if (s_pCursorSurf) {
        static SDL_Surface *pLastSurf = 0;
        static SDL_Texture *pLastTex = 0;
        if (pLastSurf != s_pCursorSurf) {
            if (pLastTex) SDL_DestroyTexture(pLastTex);
            pLastTex = SDL_CreateTextureFromSurface(s_pRen, s_pCursorSurf);
            if (pLastTex) {
                SDL_SetTextureBlendMode(pLastTex, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(pLastTex, SDL_SCALEMODE_NEAREST);
            }
            pLastSurf = s_pCursorSurf;
        }
        if (pLastTex) {
            SDL_FRect rcDst;
            rcDst.x = (float)((xCur - s_xHot) * s_nScale);
            rcDst.y = (float)((yCur - s_yHot) * s_nScale);
            rcDst.w = (float)s_pCursorSurf->w; rcDst.h = (float)s_pCursorSurf->h;
            SDL_RenderTexture(s_pRen, pLastTex, 0, &rcDst);
        }
    }
    SDL_RenderPresent(s_pRen);
}

extern "C" void MfxPlatPresent(const MFXDIB *pDib, int xCur, int yCur)
{
    if (!s_pWin) return;
    if (s_pRen && s_pFrameTex) { MfxPresentAccel(pDib, xCur, yCur); return; }

    SDL_Surface *pSrc = SDL_CreateSurfaceFrom(pDib->nWidth, pDib->nHeight,
        SDL_PIXELFORMAT_INDEX8, pDib->pBits, pDib->nWidth);
    if (!pSrc) return;
    SDL_Palette *pPal = SDL_CreateSurfacePalette(pSrc);
    if (pPal) {
        SDL_Color aColors[256];
        for (int i = 0; i < 256; i++) {
            aColors[i].r = pDib->pPal[i].rgbRed;
            aColors[i].g = pDib->pPal[i].rgbGreen;
            aColors[i].b = pDib->pPal[i].rgbBlue;
            aColors[i].a = 255;
        }
        SDL_SetPaletteColors(pPal, aColors, 0, 256);
    }
    SDL_Surface *pWinSurf = SDL_GetWindowSurface(s_pWin);
    if (pWinSurf) {
        int nScale = 1;
        if (pWinSurf->w == pSrc->w && pWinSurf->h == pSrc->h)
            SDL_BlitSurface(pSrc, 0, pWinSurf, 0);
        else {
            SDL_BlitSurfaceScaled(pSrc, 0, pWinSurf, 0,   // integer window scale (YODA_SCALE)
                                  SDL_SCALEMODE_NEAREST);
            nScale = pSrc->w ? pWinSurf->w / pSrc->w : 1;
            if (nScale < 1) nScale = 1;
        }
        // software cursor: composite over the scaled frame (hardware path leaves this 0)
        if (s_pCursorSurf) {
            SDL_Rect rcDst;
            rcDst.x = (xCur - s_xHot) * nScale;
            rcDst.y = (yCur - s_yHot) * nScale;
            rcDst.w = s_pCursorSurf->w; rcDst.h = s_pCursorSurf->h;
            SDL_BlitSurface(s_pCursorSurf, 0, pWinSurf, &rcDst);
        }
        SDL_UpdateWindowSurface(s_pWin);
    }
    SDL_DestroySurface(pSrc);
}

// ── cursor display ───────────────────────────────────────────────────────────────────────────
// DEFAULT: a SOFTWARE cursor composited at present time; YODA_HWCURSOR=1 opts into SDL color
// cursors. SYSTEM/HIDDEN come from the neutral decision layer.

static SDL_Surface *MfxMakeCursorSurface(const MFXIMG *pImg, int nScale)
{
    int w = pImg->nWidth * nScale, h = pImg->nHeight * nScale;
    SDL_Surface *pSurf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
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

extern "C" void MfxPlatSetCursor(int nMode, const MFXIMG *pImg, const void *pKey,
                                 int xHot, int yHot)
{
    static struct { const void *pKey; SDL_Surface *pSurf; SDL_Cursor *pHw; } aCache[24];
    static int nCache = 0;
    static int nHwMode = -1;
    if (nHwMode < 0) nHwMode = getenv("YODA_HWCURSOR") ? 1 : 0;

    if (nMode == MFXPLAT_CURSOR_SYSTEM) {
        s_pCursorSurf = 0;
        SDL_ShowCursor();
        return;
    }
    if (nMode == MFXPLAT_CURSOR_HIDDEN || !pImg) {
        s_pCursorSurf = 0;
        SDL_HideCursor();
        return;
    }
    int nHit = -1;
    for (int i = 0; i < nCache; i++)
        if (aCache[i].pKey == pKey) { nHit = i; break; }
    if (nHit < 0 && nCache < 24) {
        nHit = nCache;
        aCache[nHit].pKey = pKey;
        aCache[nHit].pSurf = MfxMakeCursorSurface(pImg, s_nScale);
        aCache[nHit].pHw = nHwMode && aCache[nHit].pSurf
            ? SDL_CreateColorCursor(aCache[nHit].pSurf, xHot * s_nScale, yHot * s_nScale) : 0;
        nCache++;
    }
    if (nHit < 0) { s_pCursorSurf = 0; SDL_ShowCursor(); return; }
    if (nHwMode && aCache[nHit].pHw) {
        s_pCursorSurf = 0;
        SDL_SetCursor(aCache[nHit].pHw);
        SDL_ShowCursor();
    } else {
        s_pCursorSurf = aCache[nHit].pSurf;
        s_xHot = xHot;
        s_yHot = yHot;
        SDL_HideCursor();                            // we draw it ourselves
    }
}

extern "C" void MfxPlatDelay(unsigned nMs) { SDL_Delay(nMs); }
