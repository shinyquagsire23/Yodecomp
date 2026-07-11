// microfx platform/ — the SDL2_mixer audio backend (desktop default). Implements the
// MfxSndPlat* contract (mfxplat.h): wave chunks on mixing channels + one music stream
// (MIDI via fluidsynth — needs a GM SoundFont, see SndEnsureSoundFont). ALL WaveMix/MCI
// semantics live in the neutral layer (snd/mfxsnd.cpp) — this TU only decodes and plays.
// Selected by cmake/PortableSDL.cmake when SDL2_mixer is found; a port replaces this ONE file.

#include <mfxplat.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include <stdio.h>
#include <stdlib.h>

static bool s_bLog;
#define SNDLOG(args) do { if (s_bLog) { fprintf args; fflush(stderr); } } while (0)

extern "C" int MfxSndPlatOpen(void)
{
    s_bLog = (getenv("YODA_SNDLOG") != NULL);
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
    {
        SNDLOG((stderr, "[snd] SDL audio init failed: %s\n", SDL_GetError()));
        return 0;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0)
    {
        SNDLOG((stderr, "[snd] Mix_OpenAudio failed: %s\n", Mix_GetError()));
        return 0;
    }
    Mix_AllocateChannels(MFXSND_CHANNELS);
    return 1;
}

extern "C" void MfxSndPlatClose(void)
{
    Mix_CloseAudio();
}

extern "C" void *MfxSndPlatLoadWave(const char *pszPath)
{
    Mix_Chunk *pChunk = Mix_LoadWAV(pszPath);
    if (pChunk == NULL)
        SNDLOG((stderr, "[snd] Mix_LoadWAV \"%s\": %s\n", pszPath, Mix_GetError()));
    return pChunk;
}

extern "C" void MfxSndPlatFreeWave(void *pWave)
{
    Mix_Chunk *pChunk = (Mix_Chunk *)pWave;
    for (int ch = 0; ch < MFXSND_CHANNELS; ch++)     // never free a chunk mid-playback
        if (Mix_Playing(ch) && Mix_GetChunk(ch) == pChunk)
            Mix_HaltChannel(ch);
    Mix_FreeChunk(pChunk);
}

extern "C" int MfxSndPlatPlay(void *pWave, int nChannel)
{
    return Mix_PlayChannel(nChannel, (Mix_Chunk *)pWave, 0);
}

extern "C" void MfxSndPlatHalt(int nChannel)
{
    Mix_HaltChannel(nChannel);
}

// ── music (MIDI) ─────────────────────────────────────────────────────────────────────────────

// SDL2_mixer's fluidsynth MIDI backend is silent without a GM SoundFont. Resolution order:
// YODA_SOUNDFONT env > whatever Mix already has (SDL_SOUNDFONTS / built-in default) > a
// probe list ending in brew fluid-synth's bundled demo font (audible but not GM-faithful —
// point YODA_SOUNDFONT at a real GM .sf2 for proper Indy music).
static void SndEnsureSoundFont(void)
{
    const char* pszEnv = getenv("YODA_SOUNDFONT");
    if (pszEnv != NULL)
    {
        Mix_SetSoundFonts(pszEnv);
        return;
    }
    if (Mix_GetSoundFonts() != NULL)
        return;
    static const char* aszProbe[] = {
        "/opt/homebrew/share/soundfonts/default.sf2",
        "/usr/local/share/soundfonts/default.sf2",
        "/usr/share/sounds/sf2/FluidR3_GM.sf2",
        "/usr/share/sounds/sf2/default-GM.sf2",
    };
    for (size_t i = 0; i < sizeof aszProbe / sizeof aszProbe[0]; i++)
    {
        FILE* f = fopen(aszProbe[i], "rb");
        if (f != NULL)
        {
            fclose(f);
            Mix_SetSoundFonts(aszProbe[i]);
            SNDLOG((stderr, "[snd] soundfont: %s\n", aszProbe[i]));
            return;
        }
    }
    // brew fluid-synth cellar (versioned dir — resolve via the stable opt/ symlink)
    static const char* aszCellar[] = {
        "/opt/homebrew/opt/fluid-synth/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
        "/usr/local/opt/fluid-synth/share/fluid-synth/sf2/VintageDreamsWaves-v2.sf2",
    };
    for (size_t i = 0; i < sizeof aszCellar / sizeof aszCellar[0]; i++)
    {
        FILE* f = fopen(aszCellar[i], "rb");
        if (f != NULL)
        {
            fclose(f);
            Mix_SetSoundFonts(aszCellar[i]);
            SNDLOG((stderr, "[snd] soundfont (demo, set YODA_SOUNDFONT for GM): %s\n",
                    aszCellar[i]));
            return;
        }
    }
    SNDLOG((stderr, "[snd] no soundfont found — MIDI will fail to open "
                    "(set YODA_SOUNDFONT=<path to a GM .sf2>)\n"));
}

extern "C" void *MfxSndPlatMusicLoad(const char *pszPath)
{
    SndEnsureSoundFont();
    Mix_Music *pMusic = Mix_LoadMUS(pszPath);
    if (pMusic == NULL)
        SNDLOG((stderr, "[snd] Mix_LoadMUS \"%s\": %s\n", pszPath, Mix_GetError()));
    return pMusic;
}

extern "C" void MfxSndPlatMusicFree(void *pMusic)
{
    Mix_FreeMusic((Mix_Music *)pMusic);
}

extern "C" int MfxSndPlatMusicPlay(void *pMusic)
{
    Mix_HaltMusic();                   // one stream: a new play replaces the current theme
    Mix_PlayMusic((Mix_Music *)pMusic, 1);
    return 0;
}

extern "C" void MfxSndPlatMusicHalt(void)
{
    Mix_HaltMusic();
}
