// microfx platform/ — SDL3-CORE audio backend (no SDL_mixer). Written for the WASM build
// (emscripten ships an SDL3 port but no SDL3_mixer port), and equally usable on desktop when
// SDL3_mixer isn't installed. One physical device; MFXSND_CHANNELS bound SDL_AudioStreams —
// SDL mixes bound streams itself, so the contract's channel model maps 1:1. SDL_LoadWAV lives
// in SDL3 core, and SDL_PutAudioStreamData COPIES, so waves have no cross-thread lifetime.
//
// MUSIC (MIDI) is NOT implemented here: no synth without SDL_mixer's fluidsynth. Yoda ships no
// .mid at all (its audio is the 66 sfx/*.wav), so this backend is full audio for Yoda; an
// Indy-wasm build would need a soft-synth (e.g. TinySoundFont + a GM .sf2 in the preload FS)
// before its themes are audible — MusicLoad returns 0 and the neutral MCI layer treats it as
// "sequencer unavailable", same class as a soundfont-less fluidsynth.
//
// Browser autoplay note: SDL3's emscripten audio driver resumes the suspended AudioContext on
// the first user gesture — the game starts from a click anyway, so SFX just work from then on.

#include <mfxplat.h>

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_bLog;
#define SNDLOG(args) do { if (s_bLog) { fprintf args; fflush(stderr); } } while (0)

typedef struct MFXWAVE {
    SDL_AudioSpec spec;
    Uint8 *pData;
    Uint32 nLen;
} MFXWAVE;

static SDL_AudioDeviceID s_nDev = 0;
static SDL_AudioStream *s_apStreams[MFXSND_CHANNELS];

extern "C" int MfxSndPlatOpen(void)
{
    s_bLog = (getenv("YODA_SNDLOG") != NULL);
    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        SNDLOG((stderr, "[snd] SDL_Init(AUDIO): %s\n", SDL_GetError()));
        return 0;
    }
    s_nDev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (s_nDev == 0)
    {
        SNDLOG((stderr, "[snd] SDL_OpenAudioDevice: %s\n", SDL_GetError()));
        return 0;
    }
    // per-channel streams; input format is retargeted per wave at play time and SDL converts
    SDL_AudioSpec spec; spec.format = SDL_AUDIO_S16; spec.channels = 1; spec.freq = 22050;
    for (int i = 0; i < MFXSND_CHANNELS; i++)
    {
        s_apStreams[i] = SDL_CreateAudioStream(&spec, NULL);
        if (s_apStreams[i] && !SDL_BindAudioStream(s_nDev, s_apStreams[i]))
        {
            SNDLOG((stderr, "[snd] bind ch%d: %s\n", i, SDL_GetError()));
            SDL_DestroyAudioStream(s_apStreams[i]);
            s_apStreams[i] = 0;
        }
    }
    return 1;
}

extern "C" void MfxSndPlatClose(void)
{
    for (int i = 0; i < MFXSND_CHANNELS; i++)
        if (s_apStreams[i]) { SDL_DestroyAudioStream(s_apStreams[i]); s_apStreams[i] = 0; }
    if (s_nDev) { SDL_CloseAudioDevice(s_nDev); s_nDev = 0; }
}

extern "C" void *MfxSndPlatLoadWave(const char *pszPath)
{
    if (s_nDev == 0)
        return 0;
    MFXWAVE *pWave = (MFXWAVE *)malloc(sizeof(MFXWAVE));
    if (pWave == NULL)
        return 0;
    if (!SDL_LoadWAV(pszPath, &pWave->spec, &pWave->pData, &pWave->nLen))
    {
        SNDLOG((stderr, "[snd] SDL_LoadWAV \"%s\": %s\n", pszPath, SDL_GetError()));
        free(pWave);
        return 0;
    }
    return pWave;
}

extern "C" void MfxSndPlatFreeWave(void *pWave)
{
    MFXWAVE *pW = (MFXWAVE *)pWave;
    if (pW == NULL)
        return;
    // Put copies the samples into the stream, so playback never references pData — no halt
    // sweep needed before freeing.
    SDL_free(pW->pData);
    free(pW);
}

// a channel is free when its stream has drained: nothing queued in, nothing converted waiting
static int SndChannelBusy(int ch)
{
    if (s_apStreams[ch] == NULL)
        return 1;
    return SDL_GetAudioStreamQueued(s_apStreams[ch]) > 0 ||
           SDL_GetAudioStreamAvailable(s_apStreams[ch]) > 0;
}

extern "C" int MfxSndPlatPlay(void *pWave, int nChannel)
{
    MFXWAVE *pW = (MFXWAVE *)pWave;
    if (s_nDev == 0 || pW == NULL)
        return -1;
    if (nChannel < 0)                                // -1 = any free channel
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            if (!SndChannelBusy(ch)) { nChannel = ch; break; }
        if (nChannel < 0)
            return -1;                               // all busy — the neutral LRU steals one
    }
    if (nChannel >= MFXSND_CHANNELS || s_apStreams[nChannel] == NULL)
        return -1;
    SDL_AudioStream *pStream = s_apStreams[nChannel];
    SDL_ClearAudioStream(pStream);                   // channel steal = replace outright
    SDL_SetAudioStreamFormat(pStream, &pW->spec, NULL);
    if (!SDL_PutAudioStreamData(pStream, pW->pData, (int)pW->nLen))
        return -1;
    SDL_FlushAudioStream(pStream);                   // no more data coming: let it drain fully
    return nChannel;
}

extern "C" void MfxSndPlatHalt(int nChannel)
{
    if (s_nDev == 0)
        return;
    if (nChannel < 0)
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            if (s_apStreams[ch]) SDL_ClearAudioStream(s_apStreams[ch]);
        return;
    }
    if (nChannel < MFXSND_CHANNELS && s_apStreams[nChannel])
        SDL_ClearAudioStream(s_apStreams[nChannel]);
}

// ── music (MIDI): unavailable without a synth — see the header note ─────────────────────────

extern "C" void *MfxSndPlatMusicLoad(const char *pszPath)
{
    SNDLOG((stderr, "[snd] no MIDI synth in the sdl3stream backend (\"%s\" not loaded)\n",
            pszPath));
    return 0;
}
extern "C" void MfxSndPlatMusicFree(void *) {}
extern "C" int  MfxSndPlatMusicPlay(void *) { return 1; }
extern "C" void MfxSndPlatMusicHalt(void) {}
