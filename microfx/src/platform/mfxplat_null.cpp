// microfx platform/ — the null video/input backend: no display, by design. MfxPlatInit
// returns 0, so CWinThread::Run returns immediately and modal GetMessage loops auto-bail
// (dialogs cancel deterministically) — the headless contract worldgen_smoke/game_walk/
// dlg_smoke rely on. Selected by cmake/PortableSDL.cmake when SDL2 is absent.

#include <mfxplat.h>

extern "C" {

int  MfxPlatInit(const char *, int, int, int) { return 0; }   // 0 = no display by design
void MfxPlatShutdown(void) {}
int  MfxPlatPollEvent(MFXPLATEVENT *pEv) { pEv->nType = MFXPLAT_EV_NONE; return 0; }
void MfxPlatPresent(const MFXDIB *, int, int) {}
void MfxPlatSetCursor(int, const MFXIMG *, const void *, int, int) {}
void MfxPlatDelay(unsigned) {}

} // extern "C"
