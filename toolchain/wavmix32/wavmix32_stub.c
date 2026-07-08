/* wavmix32_stub.c — a non-copyrighted no-op stand-in for Microsoft's WAVMIX32.DLL.
 *
 * The demo imports 10 realtime-wave-mixer functions from WAVMIX32.DLL. This file provides them as
 * do-nothing stubs so `tools/link_exe.sh` can build (via wavmix32.lib) a self-contained, RUNNABLE
 * yoda.exe that behaves like the original minus sound. Because the exports use the same names and
 * stdcall signatures as the real DLL, a genuine WAVMIX32.DLL (shipped with Yoda Stories, or built
 * from the Microsoft Wavemix SDK) is a drop-in replacement that restores audio.
 *
 * Return values are the benign "no session / no wave" values so the caller's success checks fail
 * gracefully and the game runs silently instead of crashing.
 */
#include <windows.h>

/* WaveMixInit: real DLL returns an HMIXSESSION. Return a non-NULL token so the game proceeds. */
HANDLE WINAPI WaveMixInit(void)                                          { return (HANDLE)1; }
UINT   WINAPI WaveMixPump(void)                                         { return 0; }
int    WINAPI WaveMixActivate(HANDLE hMix, BOOL fActivate)             { return 0; }
HANDLE WINAPI WaveMixOpenWave(HANDLE hMix, LPSTR sz, HINSTANCE hI, DWORD f) { return (HANDLE)0; }
HANDLE WINAPI WaveMixOpenChannel(HANDLE hMix, int iChannel, DWORD f)   { return (HANDLE)0; }
int    WINAPI WaveMixPlay(LPVOID lpMixPlayParams)                       { return 0; }
int    WINAPI WaveMixFlushChannel(HANDLE hMix, int iChannel, DWORD f)  { return 0; }
int    WINAPI WaveMixCloseChannel(HANDLE hMix, int iChannel, DWORD f)  { return 0; }
int    WINAPI WaveMixFreeWave(HANDLE hMix, HANDLE hWave)               { return 0; }
int    WINAPI WaveMixCloseSession(HANDLE hMix)                          { return 0; }
