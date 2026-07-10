// M1 render harness (docs/phase-h4-sdl.md). Bootstraps the real game object graph exactly like
// worldgen_smoke (theApp InitInstance → OnFileNew → CDeskcppDoc), runs Load()+worldgen at a
// pinned seed, then renders a zone into the REAL Canvas (microfx gdi DIB section) via the
// game's own CDeskcppDoc::RefreshZone, and presents it: always dumps an 8-bit .bmp (headless
// oracle), and with --show opens an SDL window wrapping the DIB as a paletted SDL_Surface.
//
//   zone_view <seed|-> [data.dta] [--zone <id>] [--dump <out.bmp>] [--show]
//
// Default zone: the intro zone (CDeskcppDoc::SetCurrentToIntroZone, map_flags 9).
#include <afxwin.h>
#include <microfx.h>
#include "Deskcpp.h"     // CDeskcppApp (m_str = data-file path)
#include "Worldgen.h"    // CDeskcppDoc facade: Load(), currentZone, pCanvas, RefreshZone()
#include <stdlib.h>

#ifdef MICROFX_HAS_SDL
#define SDL_MAIN_HANDLED
#include <SDL.h>

static int ShowDib(const MFXDIB *pDib)
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "zone_view: SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *pWin = SDL_CreateWindow("zone_view — Canvas DIB",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, pDib->nWidth, pDib->nHeight, 0);
    if (!pWin) { fprintf(stderr, "zone_view: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

    // the milestone in one line: the 8-bit DIBSection IS a paletted SDL_Surface
    SDL_Surface *pSurf = SDL_CreateRGBSurfaceWithFormatFrom(pDib->pBits,
        pDib->nWidth, pDib->nHeight, 8, pDib->nWidth, SDL_PIXELFORMAT_INDEX8);
    SDL_Color aColors[256];
    for (int i = 0; i < 256; i++) {
        aColors[i].r = pDib->pPal[i].rgbRed;
        aColors[i].g = pDib->pPal[i].rgbGreen;
        aColors[i].b = pDib->pPal[i].rgbBlue;
        aColors[i].a = 255;
    }
    SDL_SetPaletteColors(pSurf->format->palette, aColors, 0, 256);

    int bRun = 1;
    while (bRun) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT ||
                (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE))
                bRun = 0;
        SDL_Surface *pWinSurf = SDL_GetWindowSurface(pWin);
        SDL_BlitSurface(pSurf, 0, pWinSurf, 0);
        SDL_UpdateWindowSurface(pWin);
        SDL_Delay(16);
    }
    SDL_FreeSurface(pSurf);
    SDL_DestroyWindow(pWin);
    SDL_Quit();
    return 0;
}
#endif // MICROFX_HAS_SDL

int main(int argc, char **argv)
{
    const char *pszSeed = 0, *pszData = 0, *pszDump = "zone.bmp";
    int nZoneId = -1, bShow = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--zone") == 0 && i + 1 < argc) nZoneId = (int)strtol(argv[++i], 0, 0);
        else if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) pszDump = argv[++i];
        else if (strcmp(argv[i], "--show") == 0)                 bShow = 1;
        else if (!pszSeed)                                       pszSeed = argv[i];
        else                                                     pszData = argv[i];
    }
    if (!pszSeed) {
        fprintf(stderr, "usage: zone_view <seed|-> [data.dta] [--zone <id>] [--dump <out.bmp>] [--show]\n");
        return 2;
    }
    if (strcmp(pszSeed, "-") == 0)
        unsetenv("YODA_SEED");
    else
        setenv("YODA_SEED", pszSeed, 1);

    CWinApp *pApp = AfxGetApp();
    if (!pApp) { fprintf(stderr, "zone_view: no theApp\n"); return 1; }
    // ⚠ Terrain trap (docs/phase-h4-sdl.md): an invalid planet in zone_view.INI makes
    // Generate retry forever. Sanitize BEFORE InitInstance (the doc ctor reads it).
    int nTerrain = pApp->GetProfileInt("OPTIONS", "Terrain", 1);
    if (nTerrain < 1 || nTerrain > 3)
        pApp->WriteProfileInt("OPTIONS", "Terrain", 1);
    if (!pApp->InitInstance()) { fprintf(stderr, "zone_view: InitInstance failed\n"); return 1; }
    if (pszData)
        ((CDeskcppApp *)pApp)->m_str = pszData;
    if (!pApp->m_pDocTemplate || !pApp->m_pDocTemplate->m_pDoc) {
        fprintf(stderr, "zone_view: OnFileNew produced no document\n");
        return 1;
    }
    CDeskcppDoc *pDoc = (CDeskcppDoc *)pApp->m_pDocTemplate->m_pDoc;
    if (pDoc->Load() != 1) { fprintf(stderr, "zone_view: Load failed\n"); return 1; }

    // pick + render the zone through the game's own path
    if (nZoneId >= 0) {
        pDoc->currentZone = pDoc->GetZoneById((short)nZoneId);
        // slots for zones NOT on the current planet hold the engine's -1 sentinel, not NULL —
        // the game only ever looks up on-planet ids, a harness must filter both
        if (!pDoc->currentZone || pDoc->currentZone == (Zone *)-1) {
            fprintf(stderr, "zone_view: zone %d not loaded on this planet\n", nZoneId);
            pDoc->currentZone = 0;
            return 1;
        }
        pDoc->RefreshZone();
    }
    else
        pDoc->SetCurrentToIntroZone();
    if (!pDoc->currentZone) { fprintf(stderr, "zone_view: no current zone\n"); return 1; }
    printf("zone_view: seed=0x%08x zone id=%d type=%d size=%dx%d\n",
           pDoc->worldSeed, pDoc->GetZoneIndex(pDoc->currentZone),
           (int)pDoc->currentZone->type,
           (int)pDoc->currentZone->width, (int)pDoc->currentZone->height);

    Canvas *pCanvas = pDoc->pCanvas;
    if (!pCanvas || !pCanvas->hdc) { fprintf(stderr, "zone_view: no canvas\n"); return 1; }
    MFXDIB dib;
    if (!MfxGetDCDib(pCanvas->hdc, &dib)) { fprintf(stderr, "zone_view: canvas has no DIB\n"); return 1; }

    if (pszDump && *pszDump) {
        if (!MfxWriteDibBMP(pCanvas->hdc, pszDump)) {
            fprintf(stderr, "zone_view: BMP write failed: %s\n", pszDump);
            return 1;
        }
        printf("zone_view: wrote %s (%dx%d, 8bpp)\n", pszDump, dib.nWidth, dib.nHeight);
    }
    if (bShow) {
#ifdef MICROFX_HAS_SDL
        return ShowDib(&dib);
#else
        fprintf(stderr, "zone_view: built without SDL2 — --show unavailable\n");
        return 1;
#endif
    }
    return 0;
}
