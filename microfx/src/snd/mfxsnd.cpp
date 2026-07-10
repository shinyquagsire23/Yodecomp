// microfx — snd/: WAVMIX32 + mmsystem (PlaySound / MCI command strings) over SDL2_mixer.
// H4 M3 (docs/phase-h4-sdl.md). Game-side surface: src/DeskcppView.cpp — SoundInit (0x411520),
// GameView::PlaySound (0x409060), the view dtor teardown (0x408c60), the walk-sound flush
// (0x413bf0 → WaveMixFlushChannel(session, 0, 1)), and the GAME_INDY Indy_Midi* MCI strings.
//
// Contract distilled from the game code (do not "improve" it):
//  - WaveMixInit() returns a nonzero int session, 0 = audio unavailable — the game then sets
//    nSoundEnabled=nMusicEnabled=0 and never retries (the pre-M3 stub state, kept as the
//    graceful fallback whenever SDL audio can't open, and forced by YODA_NOSOUND=1).
//  - Wave handles are stored in `int g_waveHandles[64]` (32-bit) — so handles are 1-based
//    indices into an internal Mix_Chunk table, never pointers (LP64).
//  - WaveMixOpenChannel / WaveMixActivate return 0 on SUCCESS; nonzero makes SoundInit tear
//    the whole session down and disable sound.
//  - WaveMixPlay(void*) receives the PACKED 0x18-byte MIXPLAYPARAMS (WORD wSize then 4-byte
//    fields at unaligned offsets — read via memcpy). The game always plays iChannel=0 with
//    dwFlags=2 — per the MS WaveMix sample's flag values (WAVMIX32.DLL is its 32-bit port:
//    WMIX_QUEUEWAVE=0 WMIX_CLEARQUEUE=1 WMIX_USELRUCHANNEL=2 WMIX_HIPRIORITY=4) that is
//    USELRUCHANNEL: iChannel is IGNORED and the wave plays on the least-recently-used
//    channel — concurrent sounds MIX (the startup theme keeps playing under SFX; the intro
//    STUP flight isn't cut by the next effect — user-reported when this was first coded as
//    CLEARQUEUE). lpMixWave may be an UNINITIALIZED stack int for ids >= 0x40 (sic engine
//    quirk, see PlaySound) — validate handles strictly.
//  - Wave paths arrive Windows-shaped ("sfx\\ARMED.WAV"), relative to cwd.
//  - The music pump thread never runs (AfxBeginThread is a no-thread object) and WaveMixPump
//    is a no-op: SDL2_mixer mixes in its own callback thread.
//
// MCI (GAME_INDY MIDI music; Yoda ships no MIDs): exactly the four command shapes the game
// emits — "open sequencer!<file> alias <NAME>" / "play <NAME> [from 1]" / "stop <NAME>" /
// "close <NAME>" — mapped onto Mix_Music (one sequencer audible at a time, which matches how
// the game uses it: Indy_MidiPlay starts one theme, Indy_MidiStopAll stops all).
//
// Debug: YODA_SNDLOG=1 traces opens/plays/MCI to stderr (audibility itself needs a human).
// Built without SDL2_mixer (MICROFX_HAS_MIXER undefined) this TU degrades to the pre-M3
// stubs so M0-style builds still link.

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(MICROFX_HAS_SDL) && defined(MICROFX_HAS_MIXER)
#include <SDL.h>
#include <SDL_mixer.h>

#define MFX_SND_MIXER 1

static bool s_bSndLog;
#define SNDLOG(args) do { if (s_bSndLog) { fprintf args; fflush(stderr); } } while (0)

// ── session + wave table ────────────────────────────────────────────────────────────────────
static bool s_bSessionOpen;

enum { kMaxWaves = 256 };            // 64 SNDS slots + hardcoded extras; handle = index+1
static Mix_Chunk* s_apChunks[kMaxWaves];

enum { kMixChannels = 16 };          // the game only ever plays channel 0; headroom is free

// "sfx\\ARMED.WAV" → "sfx/ARMED.WAV" (same normalization CFile::Open applies to game paths)
static void SndNormalizePath(char* pszDst, size_t nDst, const char* pszSrc)
{
    size_t i = 0;
    for (; pszSrc[i] != 0 && i + 1 < nDst; i++)
        pszDst[i] = (pszSrc[i] == '\\') ? '/' : pszSrc[i];
    pszDst[i] = 0;
}

extern "C" {

UINT WaveMixPump(void) { return 0; }   // SDL2_mixer self-mixes; nothing to feed

int WaveMixInit(void)
{
    s_bSndLog = (getenv("YODA_SNDLOG") != NULL);
    if (s_bSessionOpen)
        return 1;
    if (getenv("YODA_NOSOUND") != NULL)
    {
        SNDLOG((stderr, "[snd] YODA_NOSOUND set — reporting no session\n"));
        return 0;
    }
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
    Mix_AllocateChannels(kMixChannels);
    s_bSessionOpen = true;
    SNDLOG((stderr, "[snd] session open\n"));
    return 1;                          // any nonzero int = the session handle
}

int WaveMixOpenWave(int hMixSession, char* szWaveFilename, int /*hInst*/, DWORD /*dwFlags*/)
{
    if (!s_bSessionOpen || szWaveFilename == NULL || szWaveFilename[0] == 0)
        return 0;
    char szPath[512];
    SndNormalizePath(szPath, sizeof szPath, szWaveFilename);
    Mix_Chunk* pChunk = Mix_LoadWAV(szPath);
    if (pChunk == NULL)
    {
        // case-insensitive-FS insurance for Linux (DTA names are uppercase, files often lower)
        for (char* p = szPath; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        pChunk = Mix_LoadWAV(szPath);
    }
    if (pChunk == NULL)
    {
        SNDLOG((stderr, "[snd] open FAILED \"%s\": %s\n", szWaveFilename, Mix_GetError()));
        return 0;
    }
    for (int i = 0; i < kMaxWaves; i++)
    {
        if (s_apChunks[i] == NULL)
        {
            s_apChunks[i] = pChunk;
            SNDLOG((stderr, "[snd] open \"%s\" -> handle %d\n", szWaveFilename, i + 1));
            return i + 1;
        }
    }
    Mix_FreeChunk(pChunk);
    return 0;
}

int WaveMixOpenChannel(int /*hMixSession*/, int /*iChannel*/, DWORD /*dwFlags*/)
{
    return s_bSessionOpen ? 0 : 1;     // 0 = success (channels pre-allocated in Init)
}

int WaveMixActivate(int /*hMixSession*/, BOOL fActivate)
{
    if (!s_bSessionOpen)
        return 1;
    if (!fActivate)
        Mix_HaltChannel(-1);           // deactivation (view dtor) silences everything
    return 0;                          // 0 = success
}

static Uint32 s_anChannelStart[kMixChannels];   // last play-start tick, for LRU stealing

int WaveMixPlay(void* lpMixPlayParams)
{
    if (!s_bSessionOpen || lpMixPlayParams == NULL)
        return 1;
    // packed MIXPLAYPARAMS: WORD wSize@0, session@2, iChannel@6, lpMixWave@0xa,
    // hWndNotify@0xe, dwFlags@0x12, WORD wReserved@0x16 — unaligned; memcpy each field.
    const char* p = (const char*)lpMixPlayParams;
    int nChannel, nWave, nFlags;
    memcpy(&nChannel, p + 0x06, 4);
    memcpy(&nWave,    p + 0x0a, 4);
    memcpy(&nFlags,   p + 0x12, 4);
    if (nWave < 1 || nWave > kMaxWaves || s_apChunks[nWave - 1] == NULL)
        return 1;                      // 0 = failed open; anything else = the sic garbage int
    if (nFlags & 2)                    // WMIX_USELRUCHANNEL — iChannel ignored, sounds mix
    {
        nChannel = Mix_PlayChannel(-1, s_apChunks[nWave - 1], 0);
        if (nChannel < 0)              // all channels busy: steal the longest-playing one
        {
            int nOldest = 0;
            for (int ch = 1; ch < kMixChannels; ch++)
                if (s_anChannelStart[ch] < s_anChannelStart[nOldest])
                    nOldest = ch;
            Mix_HaltChannel(nOldest);
            nChannel = Mix_PlayChannel(nOldest, s_apChunks[nWave - 1], 0);
        }
    }
    else                               // explicit channel (CLEARQUEUE halts what's playing)
    {
        if (nChannel < 0 || nChannel >= kMixChannels)
            nChannel = 0;
        if (nFlags & 1)
            Mix_HaltChannel(nChannel);
        nChannel = Mix_PlayChannel(nChannel, s_apChunks[nWave - 1], 0);
    }
    if (nChannel >= 0 && nChannel < kMixChannels)
        s_anChannelStart[nChannel] = SDL_GetTicks();
    SNDLOG((stderr, "[snd] play handle %d ch %d flags 0x%x\n", nWave, nChannel, nFlags));
    return 0;
}

int WaveMixFlushChannel(int /*hMixSession*/, int iChannel, DWORD dwFlags)
{
    if (!s_bSessionOpen)
        return 1;
    // WMIX_ALL (flags&1) flushes every channel; the game calls (session, 0, 1)
    Mix_HaltChannel((dwFlags & 1) ? -1 : iChannel);
    return 0;
}

int WaveMixFreeWave(int /*hMixSession*/, int lpMixWave)
{
    if (lpMixWave < 1 || lpMixWave > kMaxWaves || s_apChunks[lpMixWave - 1] == NULL)
        return 1;
    Mix_Chunk* pChunk = s_apChunks[lpMixWave - 1];
    for (int ch = 0; ch < kMixChannels; ch++)     // never free a chunk mid-playback
        if (Mix_Playing(ch) && Mix_GetChunk(ch) == pChunk)
            Mix_HaltChannel(ch);
    Mix_FreeChunk(pChunk);
    s_apChunks[lpMixWave - 1] = NULL;
    return 0;
}

int WaveMixCloseChannel(int /*hMixSession*/, int /*iChannel*/, DWORD /*dwFlags*/)
{
    if (s_bSessionOpen)
        Mix_HaltChannel(-1);
    return 0;
}

int WaveMixCloseSession(int /*hMixSession*/)
{
    if (!s_bSessionOpen)
        return 0;
    Mix_HaltChannel(-1);
    for (int i = 0; i < kMaxWaves; i++)
    {
        if (s_apChunks[i] != NULL)
        {
            Mix_FreeChunk(s_apChunks[i]);
            s_apChunks[i] = NULL;
        }
    }
    Mix_CloseAudio();
    s_bSessionOpen = false;
    SNDLOG((stderr, "[snd] session closed\n"));
    return 0;
}

// ── mmsystem: PlaySound + MCI sequencer strings (Indy MIDI) ─────────────────────────────────

BOOL PlaySoundA(LPCSTR, HMODULE, DWORD) { return TRUE; }   // no direct callers found; parity stub

struct MciSequencer
{
    char       szAlias[32];
    Mix_Music* pMusic;
};
enum { kMaxSequencers = 64 };          // one per possible sound id
static MciSequencer s_aSeq[kMaxSequencers];

static MciSequencer* MciFindAlias(const char* pszAlias)
{
    for (int i = 0; i < kMaxSequencers; i++)
        if (s_aSeq[i].pMusic != NULL && strcasecmp(s_aSeq[i].szAlias, pszAlias) == 0)
            return &s_aSeq[i];
    return NULL;
}

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

// read one space-delimited token; returns the char after it
static const char* MciToken(const char* p, char* pszOut, size_t nOut)
{
    while (*p == ' ') p++;
    size_t i = 0;
    while (*p != 0 && *p != ' ' && i + 1 < nOut)
        pszOut[i++] = *p++;
    pszOut[i] = 0;
    return p;
}

// The game's exact command shapes (DESKADV via src/DeskcppView.cpp Indy_Midi*):
//   "open sequencer!<file> alias <NAME>"  "play <NAME> from 1"  "stop <NAME>"  "close <NAME>"
MCIERROR mciSendStringA(LPCSTR cmd, LPSTR /*ret*/, UINT /*cchRet*/, HWND /*hwndCallback*/)
{
    if (cmd == NULL)
        return 1;
    s_bSndLog = s_bSndLog || (getenv("YODA_SNDLOG") != NULL);
    char szVerb[16], szArg[256], szAlias[64];
    const char* p = MciToken(cmd, szVerb, sizeof szVerb);

    if (strcasecmp(szVerb, "open") == 0)
    {
        p = MciToken(p, szArg, sizeof szArg);              // "sequencer!<file>" (or bare file)
        char szKw[16];
        p = MciToken(p, szKw, sizeof szKw);                // "alias"
        MciToken(p, szAlias, sizeof szAlias);
        if (strcasecmp(szKw, "alias") != 0 || szAlias[0] == 0)
            return 1;
        const char* pszFile = strchr(szArg, '!');
        pszFile = (pszFile != NULL) ? pszFile + 1 : szArg;
        char szPath[512];
        SndNormalizePath(szPath, sizeof szPath, pszFile);
        // MIDI needs SDL audio open even when WaveMix failed/was skipped (the game opens
        // the sequencers independently of the SFX session — DESKADV FUN_1018_4c54 tail)
        if (!s_bSessionOpen)
        {
            if (getenv("YODA_NOSOUND") != NULL)
                return 1;
            if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0
                || Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0)
                return 1;
            Mix_AllocateChannels(kMixChannels);
            s_bSessionOpen = true;
        }
        SndEnsureSoundFont();
        Mix_Music* pMusic = Mix_LoadMUS(szPath);
        if (pMusic == NULL)
        {
            SNDLOG((stderr, "[snd] mci open FAILED \"%s\": %s\n", szPath, Mix_GetError()));
            return 1;
        }
        for (int i = 0; i < kMaxSequencers; i++)
        {
            if (s_aSeq[i].pMusic == NULL)
            {
                strncpy(s_aSeq[i].szAlias, szAlias, sizeof s_aSeq[i].szAlias - 1);
                s_aSeq[i].szAlias[sizeof s_aSeq[i].szAlias - 1] = 0;
                s_aSeq[i].pMusic = pMusic;
                SNDLOG((stderr, "[snd] mci open \"%s\" alias %s\n", szPath, s_aSeq[i].szAlias));
                return 0;
            }
        }
        Mix_FreeMusic(pMusic);
        return 1;
    }

    if (strcasecmp(szVerb, "play") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);              // trailing "from 1" ignored (= from start)
        MciSequencer* pSeq = MciFindAlias(szAlias);
        if (pSeq == NULL)
            return 1;
        Mix_HaltMusic();
        Mix_PlayMusic(pSeq->pMusic, 1);
        SNDLOG((stderr, "[snd] mci play %s\n", szAlias));
        return 0;
    }

    if (strcasecmp(szVerb, "stop") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);
        if (MciFindAlias(szAlias) == NULL)
            return 1;
        Mix_HaltMusic();                                   // one music stream — stop is global
        return 0;
    }

    if (strcasecmp(szVerb, "close") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);
        MciSequencer* pSeq = MciFindAlias(szAlias);
        if (pSeq == NULL)
            return 1;
        Mix_HaltMusic();
        Mix_FreeMusic(pSeq->pMusic);
        pSeq->pMusic = NULL;
        pSeq->szAlias[0] = 0;
        return 0;
    }

    SNDLOG((stderr, "[snd] mci UNHANDLED \"%s\"\n", cmd));
    return 1;
}

} // extern "C"

#else // !MICROFX_HAS_MIXER — the pre-M3 silent stubs, so mixer-less builds still link

extern "C" {
UINT     WaveMixPump(void) { return 0; }
int      WaveMixInit(void) { return 0; }        // 0 = no session; SoundInit handles failure
int      WaveMixActivate(int, BOOL) { return 0; }
int      WaveMixOpenWave(int, char*, int, DWORD) { return 0; }
int      WaveMixOpenChannel(int, int, DWORD) { return 0; }
int      WaveMixPlay(void* /*MIXPLAYPARAMS*/) { return 0; }
int      WaveMixFlushChannel(int, int, DWORD) { return 0; }
int      WaveMixCloseChannel(int, int, DWORD) { return 0; }
int      WaveMixFreeWave(int, int) { return 0; }
int      WaveMixCloseSession(int) { return 0; }
BOOL     PlaySoundA(LPCSTR, HMODULE, DWORD) { return TRUE; }
MCIERROR mciSendStringA(LPCSTR, LPSTR, UINT, HWND) { return 0; }
} // extern "C"

#endif // MICROFX_HAS_MIXER
