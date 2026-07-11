// microfx platform/ — the SDL3_mixer audio backend. SDL_mixer 3 replaced the channel-based
// Mix_* API with MIX_Mixer/MIX_Track/MIX_Audio objects, so this TU maps the MfxSndPlat*
// contract's channel model onto a fixed MFXSND_CHANNELS-track array (+ one music track).
// ALL WaveMix/MCI semantics stay in the neutral layer (snd/mfxsnd.cpp) — this TU only decodes
// and plays. MIDI goes through SDL3_mixer's bundled fluidsynth decoder; the GM SoundFont is
// passed per-load via the "SDL_mixer.decoder.fluidsynth.soundfont_path" property (no
// SDL_SOUNDFONTS env fallback in SDL_mixer 3 — resolution: YODA_SOUNDFONT > probe list).
// Selected by cmake/PortableSDL.cmake when SDL3_mixer is found (preferred over SDL2_mixer).

#include <mfxplat.h>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_bLog;
#define SNDLOG(args) do { if (s_bLog) { fprintf args; fflush(stderr); } } while (0)

static MIX_Mixer *s_pMixer = 0;
static MIX_Track *s_apTracks[MFXSND_CHANNELS];   // the contract's "channels"
static MIX_Track *s_pMusicTrack = 0;             // one music stream (the MCI sequencer)

extern "C" int MfxSndPlatOpen(void)
{
    s_bLog = (getenv("YODA_SNDLOG") != NULL);
    if (!MIX_Init())
    {
        SNDLOG((stderr, "[snd] MIX_Init failed: %s\n", SDL_GetError()));
        return 0;
    }
    s_pMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (s_pMixer == NULL)
    {
        SNDLOG((stderr, "[snd] MIX_CreateMixerDevice failed: %s\n", SDL_GetError()));
        MIX_Quit();
        return 0;
    }
    for (int i = 0; i < MFXSND_CHANNELS; i++)
        s_apTracks[i] = MIX_CreateTrack(s_pMixer);
    s_pMusicTrack = MIX_CreateTrack(s_pMixer);
    return 1;
}

extern "C" void MfxSndPlatClose(void)
{
    if (s_pMixer == NULL)
        return;
    MIX_StopAllTracks(s_pMixer, 0);
    MIX_DestroyMixer(s_pMixer);                  // destroys its tracks with it
    s_pMixer = 0;
    s_pMusicTrack = 0;
    memset(s_apTracks, 0, sizeof s_apTracks);
    MIX_Quit();
}

extern "C" void *MfxSndPlatLoadWave(const char *pszPath)
{
    if (s_pMixer == NULL)
        return 0;
    MIX_Audio *pAudio = MIX_LoadAudio(s_pMixer, pszPath, true /*predecode: short SFX*/);
    if (pAudio == NULL)
        SNDLOG((stderr, "[snd] MIX_LoadAudio \"%s\": %s\n", pszPath, SDL_GetError()));
    return pAudio;
}

extern "C" void MfxSndPlatFreeWave(void *pWave)
{
    MIX_Audio *pAudio = (MIX_Audio *)pWave;
    for (int ch = 0; ch < MFXSND_CHANNELS; ch++)     // never free a wave mid-playback
        if (s_apTracks[ch] && MIX_TrackPlaying(s_apTracks[ch]) &&
            MIX_GetTrackAudio(s_apTracks[ch]) == pAudio)
            MIX_StopTrack(s_apTracks[ch], 0);
    MIX_DestroyAudio(pAudio);
}

extern "C" int MfxSndPlatPlay(void *pWave, int nChannel)
{
    if (s_pMixer == NULL || pWave == NULL)
        return -1;
    if (nChannel < 0)                                // -1 = any free channel
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            if (s_apTracks[ch] && !MIX_TrackPlaying(s_apTracks[ch])) { nChannel = ch; break; }
        if (nChannel < 0)
            return -1;                               // all busy — the neutral LRU steals one
    }
    if (nChannel >= MFXSND_CHANNELS || s_apTracks[nChannel] == NULL)
        return -1;
    MIX_StopTrack(s_apTracks[nChannel], 0);
    MIX_SetTrackAudio(s_apTracks[nChannel], (MIX_Audio *)pWave);
    if (!MIX_PlayTrack(s_apTracks[nChannel], 0))
        return -1;
    return nChannel;
}

extern "C" void MfxSndPlatHalt(int nChannel)
{
    if (s_pMixer == NULL)
        return;
    if (nChannel < 0)
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            if (s_apTracks[ch]) MIX_StopTrack(s_apTracks[ch], 0);
        return;
    }
    if (nChannel < MFXSND_CHANNELS && s_apTracks[nChannel])
        MIX_StopTrack(s_apTracks[nChannel], 0);
}

// ── music (MIDI) ─────────────────────────────────────────────────────────────────────────────

// fluidsynth is silent without a GM SoundFont. YODA_SOUNDFONT env > a probe list ending in
// brew fluid-synth's bundled demo font (audible but not GM-faithful — point YODA_SOUNDFONT at
// a real GM .sf2 for proper Indy music).
static const char *SndResolveSoundFont(void)
{
    const char *pszEnv = getenv("YODA_SOUNDFONT");
    if (pszEnv != NULL)
        return pszEnv;
    static const char *aszProbe[] = {
        "/opt/homebrew/share/soundfonts/default.sf2",
        "/usr/local/share/soundfonts/default.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
        "/usr/share/sounds/sf2/default-GM.sf2",
        // brew fluid-synth cellar (versioned dir — resolve via the stable opt/ symlink)
        "/opt/homebrew/opt/fluid-synth/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
        "/usr/local/opt/fluid-synth/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
    };
    for (size_t i = 0; i < sizeof aszProbe / sizeof aszProbe[0]; i++)
    {
        FILE *f = fopen(aszProbe[i], "rb");
        if (f != NULL)
        {
            fclose(f);
            SNDLOG((stderr, "[snd] soundfont: %s\n", aszProbe[i]));
            return aszProbe[i];
        }
    }
    SNDLOG((stderr, "[snd] no soundfont found — MIDI may be silent "
                    "(set YODA_SOUNDFONT=<path to a GM .sf2>)\n"));
    return NULL;
}

extern "C" void *MfxSndPlatMusicLoad(const char *pszPath)
{
    if (s_pMixer == NULL)
        return 0;
    SDL_IOStream *pIo = SDL_IOFromFile(pszPath, "rb");
    if (pIo == NULL)
    {
        SNDLOG((stderr, "[snd] music open \"%s\": %s\n", pszPath, SDL_GetError()));
        return 0;
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, pIo);
    SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
    SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, s_pMixer);
    const char *pszFont = SndResolveSoundFont();
    if (pszFont != NULL)
        SDL_SetStringProperty(props, "SDL_mixer.decoder.fluidsynth.soundfont_path", pszFont);
    MIX_Audio *pAudio = MIX_LoadAudioWithProperties(props);
    SDL_DestroyProperties(props);
    if (pAudio == NULL)
        SNDLOG((stderr, "[snd] music load \"%s\": %s\n", pszPath, SDL_GetError()));
    return pAudio;
}

extern "C" void MfxSndPlatMusicFree(void *pMusic)
{
    if (s_pMusicTrack && MIX_GetTrackAudio(s_pMusicTrack) == (MIX_Audio *)pMusic)
        MIX_StopTrack(s_pMusicTrack, 0);
    MIX_DestroyAudio((MIX_Audio *)pMusic);
}

extern "C" int MfxSndPlatMusicPlay(void *pMusic)
{
    if (s_pMusicTrack == NULL)
        return 1;
    MIX_StopTrack(s_pMusicTrack, 0);   // one stream: a new play replaces the current theme
    MIX_SetTrackAudio(s_pMusicTrack, (MIX_Audio *)pMusic);
    return MIX_PlayTrack(s_pMusicTrack, 0) ? 0 : 1;
}

extern "C" void MfxSndPlatMusicHalt(void)
{
    if (s_pMusicTrack)
        MIX_StopTrack(s_pMusicTrack, 0);
}
