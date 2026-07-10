// microfx — <mmsystem.h> drop-in (YODA_PORTABLE): PlaySound + MCI command strings (Indy MIDI),
// implemented over SDL2_mixer in microfx/src/snd/.
#ifndef MICROFX_MMSYSTEM_H
#define MICROFX_MMSYSTEM_H
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// PlaySound flags (the game uses SND_ASYNC|SND_MEMORY-era calls; audit at M3)
#define SND_SYNC      0x0000
#define SND_ASYNC     0x0001
#define SND_NODEFAULT 0x0002
#define SND_MEMORY    0x0004
#define SND_LOOP      0x0008
#define SND_NOSTOP    0x0010
#define SND_PURGE     0x0040
#define SND_FILENAME  0x00020000
#define SND_RESOURCE  0x00040004

BOOL  PlaySoundA(LPCSTR pszSound, HMODULE hmod, DWORD fdwSound);
// NOTE: real mmsystem.h #defines PlaySound -> PlaySoundA too (the v72 gotcha); keep parity so
// shared code behaves identically. src does #undef where it collided.
#define PlaySound PlaySoundA

typedef DWORD MCIERROR;
MCIERROR mciSendStringA(LPCSTR cmd, LPSTR ret, UINT cchRet, HWND hwndCallback);
#define mciSendString mciSendStringA

DWORD timeGetTime(void);

#ifdef __cplusplus
} // extern "C"
#endif
#endif // MICROFX_MMSYSTEM_H
