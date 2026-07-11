// microfx platform-backend contract (H4 — docs/phase-h4-sdl.md "beyond SDL").
//
// Everything platform-specific in microfx funnels through the small C surfaces below, and a
// build links EXACTLY ONE video/input backend TU and EXACTLY ONE audio backend TU
// (microfx/src/platform/):
//
//   video/input   mfxplat_sdl3.cpp   (SDL3 window/present/events — the desktop default)
//                 mfxplat_sdl2.cpp   (SDL2 twin, kept as fallback + modularity proof)
//                 mfxplat_null.cpp   (headless: Init says "no display" and the pump no-ops —
//                                     worldgen_smoke/game_walk/dlg_smoke land here)
//   audio         mfxsnd_sdl3mixer.cpp (SDL3_mixer MIX_Track API + fluidsynth MIDI)
//                 mfxsnd_sdl2mixer.cpp (SDL2_mixer channels + fluidsynth MIDI)
//                 mfxsnd_null.cpp      (no device: WaveMixInit sees 0 → the game disables sound)
//
// ⚠ SDL2 and SDL3 export the same symbol names — never link both runtimes into one binary
// (CMake enforces the pairing).
//
// A new port (e.g. Nintendo DS) implements these two TUs and NOTHING else: all the hard-won
// policy — WaveMix flag semantics/LRU mixing, deferred presents + the clock-hook flush, WM_*
// synthesis, accelerator translation, modal loops — lives in the platform-NEUTRAL layer
// (mfxpump.cpp / mfxsnd.cpp) and must not be duplicated per backend.
//
// Selection is compile-time (CMake picks the TU — cmake/PortableSDL.cmake, override with
// -DYODA_MFX_VIDEO_BACKEND=<name> / -DYODA_MFX_AUDIO_BACKEND=<name> for out-of-tree backends).
// Game TUs never include this header.
#ifndef MFXPLAT_H
#define MFXPLAT_H

#include <microfx.h>   // MFXDIB (the 8bpp frame view) + MFXIMG (decoded cursor image)

extern "C" {

// ── video / input / timing ───────────────────────────────────────────────────────────────────

// Neutral pump events. Backends translate native input into these; coordinates arrive in GAME
// pixels (the backend divides its window coords by the integer scale it was given at Init).
// Key events carry Win32 VK codes — the VK mapping is the backend's job precisely because a
// port chooses it (SDL keyboard here, button/touch mapping on a handheld).
enum {
    MFXPLAT_EV_NONE = 0,
    MFXPLAT_EV_QUIT,        // window close / OS quit request
    MFXPLAT_EV_KEYDOWN,     // nVk
    MFXPLAT_EV_KEYUP,       // nVk
    MFXPLAT_EV_CHAR,        // nChar (ASCII < 0x80 — the cheat-code buffer path)
    MFXPLAT_EV_MOUSEMOVE,   // x, y
    MFXPLAT_EV_LDOWN,       // x, y
    MFXPLAT_EV_LUP,         // x, y
    MFXPLAT_EV_RDOWN,       // x, y
    MFXPLAT_EV_RUP,         // x, y (tracked for button state; the game only handles RDOWN)
    MFXPLAT_EV_EXPOSED,     // window needs a repaint
    MFXPLAT_EV_FOCUS,       // window activated
    MFXPLAT_EV_UNFOCUS      // window deactivated
};

typedef struct MFXPLATEVENT {
    int nType;              // MFXPLAT_EV_*
    int nVk;                // KEYDOWN/KEYUP
    int nChar;              // CHAR
    int x, y;               // mouse events, game pixels
} MFXPLATEVENT;

// Bring up the display. Returns 1 = up, 0 = no display by design (null backend — the pump
// returns immediately and modal GetMessage loops auto-bail, the headless contract), -1 = a
// real error (the backend logs it). nW/nH are game pixels; nScale the integer window scale.
int  MfxPlatInit(const char *pszTitle, int nW, int nH, int nScale);
void MfxPlatShutdown(void);

// Fetch the next pending input event; 0 = none left this poll. Called until it returns 0.
int  MfxPlatPollEvent(MFXPLATEVENT *pEv);

// Show the frame. pDib is the live screen DIB (8bpp + palette); (xCur, yCur) is the current
// mouse position in game pixels for compositing the backend's active soft cursor (set via
// MfxPlatSetCursor — SYSTEM/HIDDEN modes composite nothing).
void MfxPlatPresent(const MFXDIB *pDib, int xCur, int yCur);

// Cursor display policy (the DECISION — which cursor, when hidden — is neutral, mfxpump.cpp):
//   SYSTEM — show the platform's native arrow (boot state / IDC_ARROW)
//   HIDDEN — no cursor at all (the game's SetCursor(NULL) keyboard/drag modes)
//   IMAGE  — show pImg (decoded .res cursor) with the given hotspot; pKey is a stable cache
//            key (the HCURSOR) so backends can convert each image once.
enum { MFXPLAT_CURSOR_SYSTEM = 0, MFXPLAT_CURSOR_HIDDEN, MFXPLAT_CURSOR_IMAGE };
void MfxPlatSetCursor(int nMode, const MFXIMG *pImg, const void *pKey, int xHot, int yHot);

// Yield the CPU (~ms). Timing/ticks are NOT a backend concern — neutral code uses
// GetTickCount/mfx_clock (mfxcore.cpp), which a port retargets there once.
void MfxPlatDelay(unsigned nMs);

// ── audio ────────────────────────────────────────────────────────────────────────────────────

// The neutral WaveMix/MCI contract layer (mfxsnd.cpp) owns handles, channel policy (LRU
// stealing), packed-params parsing, and MCI command parsing; a backend just decodes and plays.
enum { MFXSND_CHANNELS = 16 };   // the game plays "channel 0" + LRU mixing; 16 = free headroom

int   MfxSndPlatOpen(void);      // open the output device; 1 = ok, 0 = unavailable (no retry)
void  MfxSndPlatClose(void);

void *MfxSndPlatLoadWave(const char *pszPath);      // decoded wave, or 0 (caller retries paths)
void  MfxSndPlatFreeWave(void *pWave);              // must first halt any channel playing it
int   MfxSndPlatPlay(void *pWave, int nChannel);    // nChannel -1 = any free; returns the
                                                    // channel used, or -1 (all busy/error)
void  MfxSndPlatHalt(int nChannel);                 // -1 = all channels

void *MfxSndPlatMusicLoad(const char *pszPath);     // music/MIDI file, or 0
void  MfxSndPlatMusicFree(void *pMusic);
int   MfxSndPlatMusicPlay(void *pMusic);            // one stream: halts current first; 0 = ok
void  MfxSndPlatMusicHalt(void);

} // extern "C"

#endif // MFXPLAT_H
