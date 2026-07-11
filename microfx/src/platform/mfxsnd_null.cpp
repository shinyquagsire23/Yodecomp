// microfx platform/ — the null audio backend: no output device, by design. MfxSndPlatOpen
// returns 0, so WaveMixInit reports no session and the game gracefully disables sound
// (nSoundEnabled=nMusicEnabled=0 — the pre-M3 behavior). Selected by cmake/PortableSDL.cmake
// when SDL2_mixer is absent.

#include <mfxplat.h>

extern "C" {

int   MfxSndPlatOpen(void) { return 0; }
void  MfxSndPlatClose(void) {}
void *MfxSndPlatLoadWave(const char *) { return 0; }
void  MfxSndPlatFreeWave(void *) {}
int   MfxSndPlatPlay(void *, int) { return -1; }
void  MfxSndPlatHalt(int) {}
void *MfxSndPlatMusicLoad(const char *) { return 0; }
void  MfxSndPlatMusicFree(void *) {}
int   MfxSndPlatMusicPlay(void *) { return 1; }
void  MfxSndPlatMusicHalt(void) {}

} // extern "C"
