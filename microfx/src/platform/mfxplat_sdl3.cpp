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

// This TU is ALSO the wasm backend (GOAL 4): emscripten ships an SDL3 port (--use-port=sdl3),
// so the browser is just another SDL3 platform — the __EMSCRIPTEN__ deltas below are the whole
// difference (yield to the browser via emscripten_sleep; no native file-picker panel). The
// blocking modal loops work because ASYNCIFY turns every MfxPlatDelay into a real browser yield.
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Android (GOAL 5 — the touch port): this TU is ALSO the Android backend. SDL3 ships a full
// Android port, so — exactly as with wasm — the phone is just another SDL3 platform. The
// __ANDROID__ deltas below are the whole difference: assets are extracted from the APK into
// private internal storage (the game's data dir), the present path is a fullscreen letterbox
// renderer (touch↔game coords via SDL_RenderCoordinatesFromWindow), and a multitouch on-screen
// button overlay synthesizes VK_SHIFT (Push/Pull) + VK_SPACE (Attack). __ANDROID__ is defined
// automatically by the NDK clang. See cmake/Android.cmake + docs BUILDING.md "Android".
#ifdef __ANDROID__
#include <sys/stat.h>
#include <stdio.h>
#endif

static SDL_Window *s_pWin = 0;
static int s_nScale = 2;
static int s_nW = 0, s_nH = 0;                 // game-pixel present size passed to MfxPlatInit
static int s_nLogicalW = 0, s_nLogicalH = 0;   // current renderer logical size (letterbox target)
static int s_nTexW = 0, s_nTexH = 0;           // current streaming-texture size

// Input-modality tracking (Android): the on-screen touch buttons follow the LAST-used input —
// shown while the player is touching, hidden once a keyboard/mouse/controller is used, re-shown
// on the next touch. Default 1 (touch is the assumed input on a phone at launch).
static int s_bTouchActive = 1;

// Soft-cursor composite scale. Desktop composites in window pixels (×s_nScale); Android composites
// in the renderer's game-pixel LOGICAL space (letterbox path), so the cursor is 1:1 there — using
// s_nScale on Android double-scaled it (the reported "2× position and size").
static int MfxCursorScale(void)
{
#ifdef __ANDROID__
    return 1;
#else
    return s_nScale;
#endif
}

// YODA_ACCEL=1 (opt-in): SDL_Renderer + streaming texture present path (see the SDL2 twin
// for the full rationale). YODA_VSYNC=1 re-adds vsync via SDL_SetRenderVSync.
static SDL_Renderer *s_pRen = 0;
static SDL_Texture  *s_pFrameTex = 0;

// soft-cursor state (MfxPlatSetCursor decides; MfxPlatPresent composites)
static SDL_Surface *s_pCursorSurf = 0;
static int s_xHot = 0, s_yHot = 0;

// ── Android: data dir + APK-asset extraction ─────────────────────────────────────────────────
// The game derives its data directory from GetModuleFileNameA (mfxstubs.cpp), which on Android
// calls MfxAndroidDataDir() below: private internal storage, into which the APK's baked assets
// (DTA/DAW + sfx + starter yoda.INI, listed in assets/manifest.txt) are copied on first launch.
// Copyrighted game data thus lands in the app sandbox; writable state (INI/saves) lives there too.
#ifdef __ANDROID__
static void MfxMkdirP(const char *path)
{
    char tmp[1024];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof tmp) return;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++)
        if (*p == '/') { *p = 0; mkdir(tmp, 0770); *p = '/'; }
    mkdir(tmp, 0770);                              // the final component too
}

static int MfxExtractAsset(const char *rel, const char *pszDir)
{
    char szDest[1024];
    snprintf(szDest, sizeof szDest, "%s%s", pszDir, rel);
    struct stat st;
    if (stat(szDest, &st) == 0 && st.st_size > 0) return 1;   // already extracted → keep user edits

    char szParent[1024];
    strncpy(szParent, szDest, sizeof szParent - 1);
    szParent[sizeof szParent - 1] = 0;
    char *pSlash = strrchr(szParent, '/');
    if (pSlash) { *pSlash = 0; MfxMkdirP(szParent); }

    SDL_IOStream *pIn = SDL_IOFromFile(rel, "rb");            // relative path → APK AAssetManager
    if (!pIn) { SDL_Log("microfx: APK asset '%s' not found", rel); return 0; }
    size_t nLen = 0;
    void *pBuf = SDL_LoadFile_IO(pIn, &nLen, true);           // true = close pIn
    if (!pBuf) return 0;
    FILE *pf = fopen(szDest, "wb");
    if (pf) { if (nLen) fwrite(pBuf, 1, nLen, pf); fclose(pf); }
    else SDL_Log("microfx: cannot write '%s'", szDest);
    SDL_free(pBuf);
    return pf != 0;
}

extern "C" const char *MfxAndroidDataDir(void)
{
    static char s_szDir[1024];
    static int  s_bDone = 0;
    if (s_bDone) return s_szDir;
    s_bDone = 1;

    const char *pszISP = SDL_GetAndroidInternalStoragePath();
    if (pszISP && *pszISP) {
        strncpy(s_szDir, pszISP, sizeof s_szDir - 2);
        s_szDir[sizeof s_szDir - 2] = 0;
    } else {
        strcpy(s_szDir, "/data/local/tmp");       // last-ditch (should never happen)
    }
    size_t n = strlen(s_szDir);
    if (n == 0 || s_szDir[n - 1] != '/') strcat(s_szDir, "/");

    // Copy every asset the manifest lists (generated at build time by tools/android_apk.sh).
    SDL_IOStream *pM = SDL_IOFromFile("manifest.txt", "rb");
    if (pM) {
        size_t nM = 0;
        char *pTxt = (char *)SDL_LoadFile_IO(pM, &nM, true);
        if (pTxt) {
            char *pSave = 0;
            for (char *pLine = strtok_r(pTxt, "\r\n", &pSave); pLine;
                 pLine = strtok_r(0, "\r\n", &pSave)) {
                while (*pLine == ' ' || *pLine == '\t') pLine++;
                if (*pLine) MfxExtractAsset(pLine, s_szDir);
            }
            SDL_free(pTxt);
        }
    } else {
        SDL_Log("microfx: no assets/manifest.txt in APK — no game data to extract");
    }
    SDL_Log("microfx: Android data dir = %s", s_szDir);
    return s_szDir;
}
#endif // __ANDROID__

// ── init / teardown ──────────────────────────────────────────────────────────────────────────

extern "C" int MfxPlatInit(const char *pszTitle, int nW, int nH, int nScale)
{
    s_nScale = (nScale >= 1) ? nScale : 1;
    s_nW = nW; s_nH = nH;
    // (no SDL_SetMainReady: SDL3 only wraps main() if SDL3/SDL_main.h is included)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "microfx: SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    SDL_InitSubSystem(SDL_INIT_GAMEPAD);       // controllers (optional; failure is non-fatal —
                                               // SDL posts GAMEPAD_ADDED for pads present at init)

    Uint32 nWinFlags = 0;
#ifdef __ANDROID__
    // The whole touch surface is ours: turn OFF SDL's touch↔mouse synthesis so a finger only
    // moves the game cursor when WE route it there (the button overlay claims corner touches).
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    nWinFlags = SDL_WINDOW_FULLSCREEN;         // Android ignores the requested size → fullscreen
#endif
    s_pWin = SDL_CreateWindow(pszTitle, nW * s_nScale, nH * s_nScale, nWinFlags);
    if (!s_pWin) {
        fprintf(stderr, "microfx: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
#ifndef __ANDROID__
    SDL_StartTextInput(s_pWin);                // MFXPLAT_EV_CHAR synthesis (cheat-code buffer)
    // Android: do NOT raise the soft keyboard on launch — it would cover the game, and touch has
    // no cheat-code entry. A hardware keyboard still delivers VK_* via SDL_EVENT_KEY_DOWN (the
    // arrows/Space/Shift the game actually reads); only the CHAR-based cheat buffer is forgone.
#endif

    // Android ALWAYS uses the renderer/letterbox path (the window is a fullscreen size unrelated
    // to the game's pixels, so the integer window-surface scale can't work); desktop opts in with
    // YODA_ACCEL. On Android the logical size is the game pixels (not ×scale), so touch coords map
    // straight through SDL_RenderCoordinatesFromWindow; MfxPresentAccel re-fits it per frame.
    int bWantRenderer = (getenv("YODA_ACCEL") != 0);
#ifdef __ANDROID__
    bWantRenderer = 1;
#endif
    if (bWantRenderer) {
        s_pRen = SDL_CreateRenderer(s_pWin, NULL);
        if (s_pRen) {
            if (getenv("YODA_VSYNC")) SDL_SetRenderVSync(s_pRen, 1);
#ifdef __ANDROID__
            int nLogW = nW, nLogH = nH;
#else
            int nLogW = nW * s_nScale, nLogH = nH * s_nScale;
#endif
            SDL_SetRenderLogicalPresentation(s_pRen, nLogW, nLogH,
                                             SDL_LOGICAL_PRESENTATION_LETTERBOX);
            s_nLogicalW = nLogW; s_nLogicalH = nLogH;
            s_pFrameTex = SDL_CreateTexture(s_pRen, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_STREAMING, nW, nH);
            if (s_pFrameTex)
                SDL_SetTextureScaleMode(s_pFrameTex, SDL_SCALEMODE_NEAREST);
            s_nTexW = nW; s_nTexH = nH;
        }
        if (!s_pRen || !s_pFrameTex) {
            fprintf(stderr, "microfx: renderer init failed (%s) — "
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

extern "C" void MfxPlatMinimize(void)
{
    if (s_pWin) SDL_MinimizeWindow(s_pWin);
}

// ── native file dialog (SDL3 SDL_ShowOpen/SaveFileDialog) ──────────────────────────────────────
// SDL3's picker is ASYNCHRONOUS (a callback fires from the event loop); the game's CFileDialog
// expects a blocking result, so we spin SDL_PumpEvents until the callback lands. The callback
// runs on the SDL event thread during pumping — no locking needed for these statics since the
// spin and the callback are the same thread.
struct MfxSdl3FileResult { int nState; char szPath[1024]; };   // nState: 0 pending, 1 got, 2 cancel

static void SDLCALL MfxSdl3FileCb(void *pUser, const char *const *paList, int /*nFilter*/)
{
    MfxSdl3FileResult *pR = (MfxSdl3FileResult *)pUser;
    if (paList && paList[0]) {
        strncpy(pR->szPath, paList[0], sizeof pR->szPath - 1);
        pR->szPath[sizeof pR->szPath - 1] = 0;
        pR->nState = 1;
    } else {
        pR->nState = 2;      // paList!=NULL but empty = user cancelled; paList==NULL = error
    }
}

extern "C" int MfxPlatShowFileDialog(int bOpen, const char *pszDir, const char *pszExt,
                                     const char *pszDef, char *pszOut, int nOutSize)
{
    if (!s_pWin) return -1;
#ifdef __EMSCRIPTEN__
    return -1;      // no native picker in a browser → CFileDialog's in-window row-list fallback
#else
    char szDesc[64];
    snprintf(szDesc, sizeof szDesc, "%s files",
             pszExt && *pszExt ? pszExt : "wld");
    SDL_DialogFileFilter aFilters[2];
    aFilters[0].name = szDesc;
    aFilters[0].pattern = (pszExt && *pszExt) ? pszExt : "wld";
    aFilters[1].name = "All files";
    aFilters[1].pattern = "*";

    // SDL wants an absolute default LOCATION; for Save, seed it with dir + default name so the
    // native panel pre-fills the slot name.
    char szLoc[1024];
    if (!bOpen && pszDef && *pszDef) {
        snprintf(szLoc, sizeof szLoc, "%s%s%s",
                 (pszDir && *pszDir) ? pszDir : ".",
                 (pszDir && *pszDir && pszDir[strlen(pszDir) - 1] != '/') ? "/" : "",
                 pszDef);
    } else {
        snprintf(szLoc, sizeof szLoc, "%s", (pszDir && *pszDir) ? pszDir : ".");
    }

    MfxSdl3FileResult res; res.nState = 0; res.szPath[0] = 0;
    if (bOpen)
        SDL_ShowOpenFileDialog(MfxSdl3FileCb, &res, s_pWin, aFilters, 2, szLoc, false);
    else
        SDL_ShowSaveFileDialog(MfxSdl3FileCb, &res, s_pWin, aFilters, 2, szLoc);

    while (res.nState == 0) {          // drive the async picker to completion
        SDL_PumpEvents();
        SDL_Delay(10);
    }

    // The panel stole keyboard focus, queueing FOCUS_LOST (and maybe FOCUS_GAINED) that the
    // spin above never polled. If we let the neutral pump drain a lone stale FOCUS_LOST later,
    // CMainFrame::OnActivate pauses the game (nFrameMode=0/bBusy=1 → stuck on STUP) with no
    // matching activate to wake it (SDL doesn't reliably re-emit FOCUS_GAINED on panel close).
    // Drop the whole focus churn and re-raise: the game window IS active now, so it should
    // simply stay in its pre-dialog state.
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_EVENT_WINDOW_FOCUS_LOST);
    SDL_FlushEvent(SDL_EVENT_WINDOW_FOCUS_GAINED);
    SDL_RaiseWindow(s_pWin);

    if (res.nState != 1) return 0;     // cancelled
    strncpy(pszOut, res.szPath, nOutSize - 1);
    pszOut[nOutSize - 1] = 0;
    return 1;
#endif // !__EMSCRIPTEN__
}

// ── window → game-pixel coordinate mapping ───────────────────────────────────────────────────
// Desktop: the window is an integer multiple of the game (÷s_nScale). Android: the window is a
// fullscreen size and the game is letterboxed inside it, so ask the renderer to invert its own
// present transform (handles the letterbox bars + any pixel density).
static void MfxScaleXY(float rx, float ry, int *px, int *py)
{
#ifdef __ANDROID__
    float gx = rx, gy = ry;
    if (s_pRen) SDL_RenderCoordinatesFromWindow(s_pRen, rx, ry, &gx, &gy);
    *px = (int)gx; *py = (int)gy;
#else
    *px = (int)rx / s_nScale;
    *py = (int)ry / s_nScale;
#endif
}

// ── Android touch: on-screen Push/Pull (Shift) + Attack (Space) buttons, multitouch ─────────
#ifdef __ANDROID__
// Two thumb targets clustered at the bottom-right (window/point space): Attack in the corner and
// Push/Pull a short diagonal up-and-left of it (a gamepad-style A/B pair, both under one thumb).
// The rest of the screen is free for tap/drag = mouse walk+interact, and a finger can hold Shift
// or Space while another taps.
struct MfxTouchBtn { float cx, cy, r; };
static void MfxComputeButtons(MfxTouchBtn *pShift, MfxTouchBtn *pSpace)
{
    int w = 0, h = 0;
    SDL_GetWindowSize(s_pWin, &w, &h);
    float r = (float)(w < h ? w : h) * 0.07f;
    if (r < 46.0f) r = 46.0f;                       // never smaller than a fingertip
    float m = r * 0.3f;                             // margin from the screen edge
    float d = r * 1.6f;                             // diagonal offset of Push/Pull from Attack
    pSpace->cx = w - m - r;      pSpace->cy = h - m - r;      pSpace->r = r;  // Attack  (corner)
    pShift->cx = pSpace->cx - (d * 1.3); pShift->cy = pSpace->cy - (d * 0.7); pShift->r = r;  // Push/Pull (↖ of it)
}
static int MfxInBtn(const MfxTouchBtn *b, float x, float y)
{
    float dx = x - b->cx, dy = y - b->cy;
    return dx * dx + dy * dy <= b->r * b->r;
}

// finger ownership (multitouch): each of the three roles is claimed by at most one finger id.
static SDL_FingerID s_fidMouse = 0, s_fidShift = 0, s_fidSpace = 0;
static int s_bMouse = 0, s_bShift = 0, s_bSpace = 0;   // is that role currently held?
static int s_bPressShift = 0, s_bPressSpace = 0;       // overlay highlight state

// A finger event → at most one neutral event. Returns 1 if pEv was filled.
static int MfxHandleFinger(const SDL_TouchFingerEvent *tf, int nType, MFXPLATEVENT *pEv)
{
    s_bTouchActive = 1;                          // touch is the active input → show the overlay
    int w = 0, h = 0;
    SDL_GetWindowSize(s_pWin, &w, &h);
    float wx = tf->x * w, wy = tf->y * h;           // normalised → window points
    MfxTouchBtn bShift, bSpace;
    MfxComputeButtons(&bShift, &bSpace);

    if (nType == 0) {                               // FINGER_DOWN
        if (!s_bShift && MfxInBtn(&bShift, wx, wy)) {
            s_bShift = 1; s_fidShift = tf->fingerID; s_bPressShift = 1;
            pEv->nType = MFXPLAT_EV_KEYDOWN; pEv->nVk = VK_SHIFT; return 1;
        }
        if (!s_bSpace && MfxInBtn(&bSpace, wx, wy)) {
            s_bSpace = 1; s_fidSpace = tf->fingerID; s_bPressSpace = 1;
            pEv->nType = MFXPLAT_EV_KEYDOWN; pEv->nVk = VK_SPACE; return 1;
        }
        if (!s_bMouse) {                            // first free finger drives the cursor (walk/interact/drag)
            s_bMouse = 1; s_fidMouse = tf->fingerID;
            MfxScaleXY(wx, wy, &pEv->x, &pEv->y);
            pEv->nType = MFXPLAT_EV_LDOWN; return 1;
        }
        return 0;
    }
    if (nType == 1) {                               // FINGER_MOTION
        if (s_bMouse && tf->fingerID == s_fidMouse) {
            MfxScaleXY(wx, wy, &pEv->x, &pEv->y);
            pEv->nType = MFXPLAT_EV_MOUSEMOVE; return 1;
        }
        return 0;                                   // a button finger sliding: keep it held
    }
    // nType == 2 → FINGER_UP / FINGER_CANCELED
    if (s_bShift && tf->fingerID == s_fidShift) {
        s_bShift = 0; s_bPressShift = 0;
        pEv->nType = MFXPLAT_EV_KEYUP; pEv->nVk = VK_SHIFT; return 1;
    }
    if (s_bSpace && tf->fingerID == s_fidSpace) {
        s_bSpace = 0; s_bPressSpace = 0;
        pEv->nType = MFXPLAT_EV_KEYUP; pEv->nVk = VK_SPACE; return 1;
    }
    if (s_bMouse && tf->fingerID == s_fidMouse) {
        s_bMouse = 0;
        MfxScaleXY(wx, wy, &pEv->x, &pEv->y);
        pEv->nType = MFXPLAT_EV_LUP; return 1;
    }
    return 0;
}
#endif // __ANDROID__

// ── game controller (all platforms) ─────────────────────────────────────────────────────────
// Basic mapping (user-set): both sticks + the D-pad → the arrow keys (8-way, using the game's own
// diagonal VKs so pushes read as diagonals); A (South) → Attack (Space); B (East) → Push/Pull
// (Shift). The game walks from discrete WM_KEYDOWN + auto-repeat (OnTimer consumes one step per
// nMovePending), so a held direction is re-emitted on a timer here. Synthesized transitions go
// through a small queue so one SDL gamepad event can yield several key events.
static int    s_padDirVk   = 0;                // the currently-held 8-way direction VK (0 = none)
static Uint32 s_padRepeatT = 0;                // last auto-repeat time for the held direction
static MFXPLATEVENT s_padQ[8];
static int    s_padQHead = 0, s_padQTail = 0;

static void MfxPadPush(int nType, int nVk)
{
    int nNext = (s_padQTail + 1) % 8;
    if (nNext == s_padQHead) return;           // full — drop (never happens with ≤2 transitions)
    s_padQ[s_padQTail].nType = nType;
    s_padQ[s_padQTail].nVk = nVk;
    s_padQ[s_padQTail].nChar = s_padQ[s_padQTail].x = s_padQ[s_padQTail].y = 0;
    s_padQTail = nNext;
}
static int MfxPadPop(MFXPLATEVENT *pEv)
{
    if (s_padQHead == s_padQTail) return 0;
    *pEv = s_padQ[s_padQHead];
    s_padQHead = (s_padQHead + 1) % 8;
    return 1;
}
static int MfxPadDirVk(int dx, int dy)         // (dx,dy)∈{-1,0,1} → the game's 8 movement VKs
{
    if (dx < 0 && dy < 0) return VK_HOME;      // ↖ 0x24
    if (dx > 0 && dy < 0) return VK_PRIOR;     // ↗ 0x21
    if (dx < 0 && dy > 0) return VK_END;       // ↙ 0x23
    if (dx > 0 && dy > 0) return VK_NEXT;      // ↘ 0x22
    if (dy < 0) return VK_UP;
    if (dy > 0) return VK_DOWN;
    if (dx < 0) return VK_LEFT;
    if (dx > 0) return VK_RIGHT;
    return 0;
}
static void MfxPadRecompute(SDL_Gamepad *pGp)  // fold D-pad + both sticks → one 8-way, emit changes
{
    if (!pGp) return;
    const int T = 16000;                       // stick deadzone (~half travel)
    int up = 0, down = 0, left = 0, right = 0;
    if (SDL_GetGamepadButton(pGp, SDL_GAMEPAD_BUTTON_DPAD_UP))    up = 1;
    if (SDL_GetGamepadButton(pGp, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  down = 1;
    if (SDL_GetGamepadButton(pGp, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  left = 1;
    if (SDL_GetGamepadButton(pGp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) right = 1;
    int lx = SDL_GetGamepadAxis(pGp, SDL_GAMEPAD_AXIS_LEFTX),  ly = SDL_GetGamepadAxis(pGp, SDL_GAMEPAD_AXIS_LEFTY);
    int rx = SDL_GetGamepadAxis(pGp, SDL_GAMEPAD_AXIS_RIGHTX), ry = SDL_GetGamepadAxis(pGp, SDL_GAMEPAD_AXIS_RIGHTY);
    if (ly < -T || ry < -T) up = 1;
    if (ly >  T || ry >  T) down = 1;
    if (lx < -T || rx < -T) left = 1;
    if (lx >  T || rx >  T) right = 1;
    int vk = MfxPadDirVk(left ? -1 : right ? 1 : 0, up ? -1 : down ? 1 : 0);
    if (vk != s_padDirVk) {
        if (s_padDirVk) MfxPadPush(MFXPLAT_EV_KEYUP, s_padDirVk);
        if (vk) MfxPadPush(MFXPLAT_EV_KEYDOWN, vk);
        s_padDirVk = vk;
        s_padRepeatT = SDL_GetTicks();         // fire the first repeat a full interval from now
    }
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

#ifdef __EMSCRIPTEN__
    // last delivered pointer position (post-scale, whole-window space) + outstanding button
    // state — used to synthesize the button-up that a wasm drag leaving the canvas never sends.
    static int s_lastX = 0, s_lastY = 0;
    static int s_bLBtnDown = 0, s_bRBtnDown = 0;
#endif

    pEv->nType = MFXPLAT_EV_NONE;
    pEv->nVk = pEv->nChar = pEv->x = pEv->y = 0;

#ifdef __EMSCRIPTEN__
    // The shell's Win95 titlebar X button (mfx_shell.html) sets a JS flag instead of calling
    // into wasm — a JS event handler firing while Asyncify has the stack unwound must not
    // re-enter exports. Polled here (normal wasm execution) and turned into the same QUIT
    // event a window close produces → the game's own exit-confirmation modal.
    if (EM_ASM_INT({
            if (window.__mfxCloseReq) { window.__mfxCloseReq = 0; return 1; }
            return 0;
        })) {
        pEv->nType = MFXPLAT_EV_QUIT;
        return 1;
    }

    // Stuck-mouse fix: SDL3's emscripten backend listens for mouse events on the canvas, and
    // SDL_CaptureMouse does not reliably retarget the browser under wasm. So a press that starts
    // on the canvas and releases elsewhere on the page fires `mouseup` on the page — SDL never
    // sees it and the game is left with the button stuck down (walk/drag/capture never ends).
    // Install (once) a window-level mouseup listener that records releases occurring OFF the
    // canvas, and synthesize the matching up here. Releases over the canvas reach SDL normally
    // and are ignored by the handler, so there's no double delivery; blur covers a release after
    // the tab loses focus (its mouseup is delivered to no one).
    {
        static int s_bUpHookInstalled = 0;
        if (!s_bUpHookInstalled) {
            s_bUpHookInstalled = 1;
            EM_ASM({
                window.__mfxUpBits = 0;   // bit0 = left released off-canvas, bit1 = right
                window.addEventListener('mouseup', function (e) {
                    var cv = Module['canvas'];
                    if (cv && e.target === cv) return;    // over canvas → SDL delivers it
                    window.__mfxUpBits |= (e.button === 2) ? 2 : 1;
                }, true);
                window.addEventListener('blur', function () { window.__mfxUpBits |= 3; });
            });
        }
        int bits = EM_ASM_INT({ var b = window.__mfxUpBits | 0; window.__mfxUpBits = 0; return b; });
        if ((bits & 1) && s_bLBtnDown) {
            s_bLBtnDown = 0;
            if (!s_bRBtnDown) SDL_CaptureMouse(false);
            pEv->nType = MFXPLAT_EV_LUP; pEv->x = s_lastX; pEv->y = s_lastY;
            return 1;
        }
        if ((bits & 2) && s_bRBtnDown) {
            s_bRBtnDown = 0;
            if (!s_bLBtnDown) SDL_CaptureMouse(false);
            pEv->nType = MFXPLAT_EV_RUP; pEv->x = s_lastX; pEv->y = s_lastY;
            return 1;
        }
    }
#endif

    if (MfxPadPop(pEv)) return 1;              // synthesized controller key transitions first

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
            if (getenv("YODA_KEYLOG"))
                fprintf(stderr, "KEYLOG %s sdlkey=0x%x scancode=0x%x mod=0x%x -> vk=0x%x\n",
                        ev.type == SDL_EVENT_KEY_DOWN ? "DOWN" : "UP ",
                        (unsigned)ev.key.key, (unsigned)ev.key.scancode, (unsigned)ev.key.mod, vk);
            if (!vk) break;                         // unmapped key: keep polling
            s_bTouchActive = 0;                     // hardware keyboard → hide the touch overlay
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
        case SDL_EVENT_GAMEPAD_ADDED:
            SDL_OpenGamepad(ev.gdevice.which);      // must open to receive its button/axis events
            s_bTouchActive = 0;
            break;
        case SDL_EVENT_GAMEPAD_REMOVED: {
            SDL_Gamepad *pGp = SDL_GetGamepadFromID(ev.gdevice.which);
            if (pGp) SDL_CloseGamepad(pGp);
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            s_bTouchActive = 0;
            int bDown = (ev.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
            if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)        // A → Attack (Space)
                MfxPadPush(bDown ? MFXPLAT_EV_KEYDOWN : MFXPLAT_EV_KEYUP, VK_SPACE);
            else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)   // B → Push/Pull (Shift)
                MfxPadPush(bDown ? MFXPLAT_EV_KEYDOWN : MFXPLAT_EV_KEYUP, VK_SHIFT);
            else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_WEST)   // X → Enter (dismiss dialogs)
                MfxPadPush(bDown ? MFXPLAT_EV_KEYDOWN : MFXPLAT_EV_KEYUP, VK_RETURN);
            else if (ev.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)   // Select → Locator map ('L')
                MfxPadPush(bDown ? MFXPLAT_EV_KEYDOWN : MFXPLAT_EV_KEYUP, 'L');
            else                                                     // D-pad → recompute 8-way
                MfxPadRecompute(SDL_GetGamepadFromID(ev.gbutton.which));
            if (MfxPadPop(pEv)) return 1;
            break;
        }
        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            s_bTouchActive = 0;
            MfxPadRecompute(SDL_GetGamepadFromID(ev.gaxis.which));
            if (MfxPadPop(pEv)) return 1;
            break;
#ifdef __ANDROID__
        case SDL_EVENT_FINGER_DOWN:
            if (MfxHandleFinger(&ev.tfinger, 0, pEv)) return 1;
            break;
        case SDL_EVENT_FINGER_MOTION:
            if (MfxHandleFinger(&ev.tfinger, 1, pEv)) return 1;
            break;
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED:
            if (MfxHandleFinger(&ev.tfinger, 2, pEv)) return 1;
            break;
#endif
        case SDL_EVENT_MOUSE_MOTION:
            pEv->nType = MFXPLAT_EV_MOUSEMOVE;
            s_bTouchActive = 0;                     // a real mouse → hide the touch overlay
            MfxScaleXY(ev.motion.x, ev.motion.y, &pEv->x, &pEv->y);
#ifdef __EMSCRIPTEN__
            s_lastX = pEv->x; s_lastY = pEv->y;
#endif
            return 1;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int bDown = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
            s_bTouchActive = 0;
            int bLeft = (ev.button.button == SDL_BUTTON_LEFT);
            if (bLeft)
                pEv->nType = bDown ? MFXPLAT_EV_LDOWN : MFXPLAT_EV_LUP;
            else if (ev.button.button == SDL_BUTTON_RIGHT)
                pEv->nType = bDown ? MFXPLAT_EV_RDOWN : MFXPLAT_EV_RUP;
            else
                break;                              // other buttons: keep polling
#ifdef __EMSCRIPTEN__
            // A release delivered here happened over the canvas; if the off-canvas hook above
            // already synthesized the up (button flag cleared), swallow this duplicate.
            if (!bDown && (bLeft ? !s_bLBtnDown : !s_bRBtnDown))
                break;
            if (bLeft) s_bLBtnDown = bDown; else s_bRBtnDown = bDown;
#endif
            // Capture the pointer for the duration of a drag so motion + the button-up keep
            // arriving even when the drag leaves the window (else a release outside strands the
            // game in its capture/walk state — most visible under wasm, where the browser stops
            // delivering events past the canvas edge). Release capture once no button remains down.
            if (bDown)
                SDL_CaptureMouse(true);
            else if (!(SDL_GetMouseState(0, 0) & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK)))
                SDL_CaptureMouse(false);
            MfxScaleXY(ev.button.x, ev.button.y, &pEv->x, &pEv->y);
#ifdef __EMSCRIPTEN__
            s_lastX = pEv->x; s_lastY = pEv->y;
#endif
            return 1;
        }
        }
    }

    // Held-direction auto-repeat: the game needs a fresh WM_KEYDOWN each tick to keep walking, so
    // re-issue the held 8-way direction on a timer (≥ the game tick). Only when SDL has no more
    // events, and at most one per drain cycle, so the pump still reaches present().
    if (s_padDirVk) {
        Uint32 now = SDL_GetTicks();
        if (now - s_padRepeatT >= 33) {
            s_padRepeatT = now;
            pEv->nType = MFXPLAT_EV_KEYDOWN;
            pEv->nVk = s_padDirVk;
            return 1;
        }
    }
    return 0;
}

// ── present ──────────────────────────────────────────────────────────────────────────────────

#ifdef __ANDROID__
// One translucent round button: soft-edged light disc + dark ring + a simple role glyph
//   glyph 0 = Push/Pull → up & down triangles (⇕, blue);  glyph 1 = Attack → a diamond (◆, red).
static SDL_Texture *MfxMakeButtonTex(int D, int nGlyph)
{
    SDL_Surface *pS = SDL_CreateSurface(D, D, SDL_PIXELFORMAT_ARGB8888);
    if (!pS) return 0;
    Uint32 *px = (Uint32 *)pS->pixels;
    int pitch = pS->pitch / 4;
    float c = (D - 1) * 0.5f, R = c;
    int gr = nGlyph ? 235 : 120, gg = nGlyph ? 90 : 150, gb = nGlyph ? 80 : 235;   // glyph colour
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = x - c, dy = y - c, d = SDL_sqrtf(dx * dx + dy * dy);
            Uint32 v = 0;
            if (d <= R) {
                int a = 200, rr = 245, gg2 = 245, bb = 245;   // light disc body
                if (d >= R - 3.0f) { rr = gg2 = bb = 40; a = 235; }               // dark rim ring
                float nx = dx / (R * 0.62f), ny = dy / (R * 0.62f);               // glyph box −1..1
                int bGlyph;
                if (nGlyph == 0) { float ay = SDL_fabsf(ny);
                                   bGlyph = (ay > 0.18f && ay < 0.92f && SDL_fabsf(nx) < ay - 0.10f); }
                else             { bGlyph = (SDL_fabsf(nx) + SDL_fabsf(ny) < 0.85f); }
                if (bGlyph) { rr = gr; gg2 = gg; bb = gb; a = 255; }
                v = ((Uint32)a << 24) | ((Uint32)rr << 16) | ((Uint32)gg2 << 8) | bb;
            }
            px[y * pitch + x] = v;
        }
    SDL_Texture *pT = SDL_CreateTextureFromSurface(s_pRen, pS);
    SDL_DestroySurface(pS);
    if (pT) { SDL_SetTextureBlendMode(pT, SDL_BLENDMODE_BLEND);
              SDL_SetTextureScaleMode(pT, SDL_SCALEMODE_LINEAR); }
    return pT;
}

// Composite the two corner buttons in OUTPUT pixels (letterbox transform off), then restore it so
// the very next frame's touch↔game mapping (SDL_RenderCoordinatesFromWindow) is correct again.
static void MfxDrawTouchOverlay(void)
{
    if (!s_bTouchActive) return;                 // hidden while a keyboard/mouse/controller is used
    static SDL_Texture *s_pTexShift = 0, *s_pTexSpace = 0;
    static int s_nBtnD = 0;
    MfxTouchBtn bShift, bSpace;
    MfxComputeButtons(&bShift, &bSpace);
    int D = (int)(bShift.r * 2.0f);
    if (D < 8) return;
    if (D != s_nBtnD) {                                       // build once / rebuild on rotation
        if (s_pTexShift) SDL_DestroyTexture(s_pTexShift);
        if (s_pTexSpace) SDL_DestroyTexture(s_pTexSpace);
        s_pTexShift = MfxMakeButtonTex(D, 0);
        s_pTexSpace = MfxMakeButtonTex(D, 1);
        s_nBtnD = D;
    }
    SDL_SetRenderLogicalPresentation(s_pRen, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    struct { SDL_Texture *t; MfxTouchBtn *b; int pressed; } aB[2] = {
        { s_pTexShift, &bShift, s_bPressShift }, { s_pTexSpace, &bSpace, s_bPressSpace } };
    for (int i = 0; i < 2; i++) {
        if (!aB[i].t) continue;
        SDL_SetTextureAlphaMod(aB[i].t, aB[i].pressed ? 245 : 120);
        SDL_FRect dst = { aB[i].b->cx - aB[i].b->r, aB[i].b->cy - aB[i].b->r,
                          aB[i].b->r * 2.0f, aB[i].b->r * 2.0f };
        SDL_RenderTexture(s_pRen, aB[i].t, 0, &dst);
    }
    SDL_SetRenderLogicalPresentation(s_pRen, s_nLogicalW, s_nLogicalH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
}
#endif // __ANDROID__

static void MfxPresentAccel(const MFXDIB *pDib, int xCur, int yCur)
{
#ifdef __ANDROID__
    // Track the presented DIB size (game screen ± the composited menu-bar chrome): keep the
    // streaming texture and the letterbox logical size in lockstep so touch↔game mapping stays
    // correct across menu-bar show/hide and orientation changes.
    if (!s_pFrameTex || s_nTexW != pDib->nWidth || s_nTexH != pDib->nHeight) {
        if (s_pFrameTex) SDL_DestroyTexture(s_pFrameTex);
        s_pFrameTex = SDL_CreateTexture(s_pRen, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING, pDib->nWidth, pDib->nHeight);
        if (s_pFrameTex) SDL_SetTextureScaleMode(s_pFrameTex, SDL_SCALEMODE_NEAREST);
        s_nTexW = pDib->nWidth; s_nTexH = pDib->nHeight;
        s_nLogicalW = pDib->nWidth; s_nLogicalH = pDib->nHeight;
        SDL_SetRenderLogicalPresentation(s_pRen, s_nLogicalW, s_nLogicalH,
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
    if (!s_pFrameTex) return;
#endif
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
            rcDst.x = (float)((xCur - s_xHot) * MfxCursorScale());
            rcDst.y = (float)((yCur - s_yHot) * MfxCursorScale());
            rcDst.w = (float)s_pCursorSurf->w; rcDst.h = (float)s_pCursorSurf->h;
            SDL_RenderTexture(s_pRen, pLastTex, 0, &rcDst);
        }
    }
#ifdef __ANDROID__
    MfxDrawTouchOverlay();          // the multitouch Push/Pull + Attack buttons, over everything
#endif
    SDL_RenderPresent(s_pRen);
}

#ifdef __EMSCRIPTEN__
// A presented frame only reaches the canvas when the browser gets control, and the game's
// busy-wait animation loops (zone transitions, palette flashes — they present via the clock
// hook, throttled) never pass through MfxPlatDelay. Yield right after each present so
// mid-handler animation frames actually display; presents are already throttled (~8ms), so
// this adds at most one browser turn per frame.
static void MfxWasmPresentYield(void) { emscripten_sleep(0); }
#else
static void MfxWasmPresentYield(void) {}
#endif

extern "C" void MfxPlatPresent(const MFXDIB *pDib, int xCur, int yCur)
{
    if (!s_pWin) return;
    if (s_pRen && s_pFrameTex) { MfxPresentAccel(pDib, xCur, yCur); MfxWasmPresentYield(); return; }

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
        if (pWinSurf->w == pSrc->w && pWinSurf->h == pSrc->h)
            SDL_BlitSurface(pSrc, 0, pWinSurf, 0);
        else
            SDL_BlitSurfaceScaled(pSrc, 0, pWinSurf, 0,   // scale to fill the window backing
                                  SDL_SCALEMODE_NEAREST);
        // software cursor: composite over the scaled frame (hardware path leaves this 0).
        // Map the game-space hotspot (xCur,yCur) to window-surface PIXELS. The mouse path
        // converts events → game via s_nScale, so the inverse is xCur*s_nScale (in SDL window
        // "event" units), then ×pixel-density to reach surface pixels (Retina backings are a
        // higher resolution than the event/point space). Do NOT use pWinSurf->w/pSrc->w: pSrc is
        // the 4-byte-row-aligned composed DIB (padded past the true game width, e.g. 525→528) and
        // an integer divide truncates a ~2× window (1050/528) to 1 — the cursor drifted to ~half
        // position, worse the further from the origin (the reported "DPI-offset" bug).
        if (s_pCursorSurf) {
            int wPt = pWinSurf->w, hPt = pWinSurf->h;
            SDL_GetWindowSize(s_pWin, &wPt, &hPt);        // window size in event/point space
            double densX = wPt > 0 ? (double)pWinSurf->w / wPt : 1.0;
            double densY = hPt > 0 ? (double)pWinSurf->h / hPt : 1.0;
            SDL_Rect rcDst;
            // hotspot in surface px, minus the hotspot's offset within the (s_nScale-sized) image
            rcDst.x = (int)(xCur * s_nScale * densX + 0.5) - s_xHot * s_nScale;
            rcDst.y = (int)(yCur * s_nScale * densY + 0.5) - s_yHot * s_nScale;
            rcDst.w = s_pCursorSurf->w; rcDst.h = s_pCursorSurf->h;
            SDL_BlitSurface(s_pCursorSurf, 0, pWinSurf, &rcDst);
        }
        SDL_UpdateWindowSurface(s_pWin);
    }
    SDL_DestroySurface(pSrc);
    MfxWasmPresentYield();
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
        aCache[nHit].pSurf = MfxMakeCursorSurface(pImg, MfxCursorScale());
        aCache[nHit].pHw = nHwMode && aCache[nHit].pSurf
            ? SDL_CreateColorCursor(aCache[nHit].pSurf, xHot * MfxCursorScale(),
                                    yHot * MfxCursorScale()) : 0;
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

#ifdef __EMSCRIPTEN__
// Explicit emscripten_sleep (not SDL_Delay) so the yield does not depend on the SDL port's
// internal ASYNCIFY handling: this is THE point where the browser's event loop gets control —
// CWinThread::Run and every modal GetMessageA wait end their spin here.
extern "C" void MfxPlatDelay(unsigned nMs) { emscripten_sleep(nMs); }
#else
extern "C" void MfxPlatDelay(unsigned nMs) { SDL_Delay(nMs); }
#endif
