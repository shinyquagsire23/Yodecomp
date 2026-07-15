// M2 walk oracle (docs/phase-h4-sdl.md). Fully headless, no SDL window: bootstrap the real
// game (InitInstance → OnFileNew: doc/frame/view with REAL HWNDs), deliver the first WM_PAINT
// (OnDraw → World::Load + worldgen at a pinned seed), drive the 0x1d1d game timer through
// mfxwnd.cpp's dispatch engine until the world reaches play mode, then synthesize arrow-key
// WM_KEYDOWN/WM_KEYUP into CDeskcppView's message map and assert the hero's world position
// changed. Exit 0 = the hero walked; the final screen DIB lands in walk.bmp for eyeballing.
//
//   game_walk [seed]           (default seed 0x2a; YODA_SEED is set for Randomize())

#include <afxwin.h>
#include <microfx.h>
#include "Deskcpp.h"     // CDeskcppApp
#include "Worldgen.h"    // CDeskcppDoc facade: nFrameMode, playerX/playerY
#include "../src/app/mfxwnd.h"   // the pump internals — this harness IS a headless pump
#include <stdlib.h>
#include <thread>       // portable sleep (was POSIX usleep)
#include <chrono>

static CDeskcppDoc *g_pDoc;
static CDeskcppView *g_pGameView;
static HWND g_hView;

// one headless pump iteration ≈ 10ms of game time
static void Pump(int nIters)
{
    for (int i = 0; i < nIters; i++) {
        MfxPumpTimers();
        MfxPaintIfDirty();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

static void TraceView(const char *pszWhen)
{
    printf("game_walk:   [%s] mode=%d cam=(%d,%d) dxy=(%d,%d) pend=%d cmd=0x%x kbd=%d busy=%d lock=%d\n",
           pszWhen, g_pDoc->nFrameMode, g_pDoc->cameraX, g_pDoc->cameraY,
           g_pGameView->nMoveDX, g_pGameView->nMoveDY, g_pGameView->nMovePending,
           g_pGameView->nMoveCommand, g_pGameView->bKeyboardMoveActive,
           g_pGameView->bBusy, g_pGameView->bInputLocked);
}

static void HoldKey(int vk, int nIters)
{
    g_mfxKeyState[vk] |= 0x80;
    for (int i = 0; i < nIters; i++) {
        MfxSendMsg(g_hView, WM_KEYDOWN, (WPARAM)vk, 1);   // Win32 auto-repeat shape
        if (i == 0) TraceView("first keydown");
        if (i == 20) TraceView("mid-hold");
        MfxPumpTimers();
        MfxPaintIfDirty();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    g_mfxKeyState[vk] &= (BYTE)~0x80;
    MfxSendMsg(g_hView, WM_KEYUP, (WPARAM)vk, 1);
}

int main(int argc, char **argv)
{
    setenv("YODA_SEED", argc > 1 ? argv[1] : "0x2a", 1);

    CWinApp *pApp = AfxGetApp();
    if (!pApp || !pApp->InitInstance()) {
        fprintf(stderr, "game_walk: InitInstance failed\n");
        return 2;
    }
    g_pDoc = (CDeskcppDoc *)pApp->m_pDocTemplate->m_pDoc;
    CFrameWnd *pFrame = (CFrameWnd *)pApp->m_pMainWnd;
    CView *pView = pFrame ? pFrame->GetActiveView() : 0;
    if (!g_pDoc || !pView || !MfxIsWnd(pView->m_hWnd)) {
        fprintf(stderr, "game_walk: bootstrap incomplete (doc=%p view=%p)\n",
                (void *)g_pDoc, (void *)pView);
        return 2;
    }
    g_hView = pView->m_hWnd;
    g_pGameView = (CDeskcppView *)pView;

    MfxSetDirty();
    MfxPaintIfDirty();                       // first paint → OnDraw → World::Load + worldgen
    printf("game_walk: after first paint mode=%d\n", g_pDoc->nFrameMode);

    // run the game loop until play mode (3); trace mode transitions
    int nMode = -99;
    int i = 0;
    for (; i < 3000 && g_pDoc->nFrameMode != 3; i++) {
        Pump(1);
        if (g_pDoc->nFrameMode != nMode) {
            nMode = g_pDoc->nFrameMode;
            printf("game_walk: t=%3.1fs mode=%d cam=(%d,%d)\n",
                   i * 0.01, nMode, g_pDoc->cameraX, g_pDoc->cameraY);
        }
    }
    if (g_pDoc->nFrameMode != 3) {
        printf("game_walk: never reached play mode (stuck at %d) — FAIL\n", g_pDoc->nFrameMode);
        MfxWriteDibBMP(MfxScreenDC(), "walk.bmp");
        return 1;
    }
    Pump(50);                                // settle after the entry transition

    int x0 = g_pDoc->cameraX, y0 = g_pDoc->cameraY;
    printf("game_walk: play mode reached at t=%3.1fs, cam=(%d,%d)\n", i * 0.01, x0, y0);

    // walk: a few held arrow strokes in each direction (any of them moving = pass)
    static const int aKeys[4] = { VK_DOWN, VK_RIGHT, VK_UP, VK_LEFT };
    for (int k = 0; k < 4; k++) {
        HoldKey(aKeys[k], 60);
        Pump(30);
        printf("game_walk: after vk=0x%02x cam=(%d,%d) mode=%d\n",
               aKeys[k], g_pDoc->cameraX, g_pDoc->cameraY, g_pDoc->nFrameMode);
        if (g_pDoc->cameraX != x0 || g_pDoc->cameraY != y0)
            break;
    }

    MfxWriteDibBMP(MfxScreenDC(), "walk.bmp");
    int bMoved = (g_pDoc->cameraX != x0 || g_pDoc->cameraY != y0);
    printf("game_walk: cam (%d,%d) -> (%d,%d): %s\n", x0, y0, g_pDoc->cameraX, g_pDoc->cameraY,
           bMoved ? "WALKED — M2 oracle GREEN" : "DID NOT MOVE — FAIL");
    return bMoved ? 0 : 1;
}
