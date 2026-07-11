// microfx — snd/: WAVMIX32 + mmsystem (PlaySound / MCI command strings), platform-NEUTRAL.
// H4 M3 (docs/phase-h4-sdl.md). Game-side surface: src/DeskcppView.cpp — SoundInit (0x411520),
// GameView::PlaySound (0x409060), the view dtor teardown (0x408c60), the walk-sound flush
// (0x413bf0 → WaveMixFlushChannel(session, 0, 1)), and the GAME_INDY Indy_Midi* MCI strings.
//
// This TU owns the WaveMix/MCI CONTRACT — handle tables, flag semantics, LRU channel policy,
// packed-params parsing, MCI command parsing — and plays audio only through the MfxSndPlat*
// backend ops (mfxplat.h; microfx/src/platform/mfxsnd_sdlmixer.cpp on desktop, mfxsnd_null.cpp
// without a device, a port's own TU elsewhere). The contract was distilled from the game code
// and hard-won in playtests — do NOT re-derive it per backend:
//
//  - WaveMixInit() returns a nonzero int session, 0 = audio unavailable — the game then sets
//    nSoundEnabled=nMusicEnabled=0 and never retries (the graceful fallback whenever the
//    backend can't open, and forced by YODA_NOSOUND=1).
//  - Wave handles are stored in `int g_waveHandles[64]` (32-bit) — so handles are 1-based
//    indices into an internal wave table, never pointers (LP64).
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
//  - Wave paths arrive Windows-shaped ("sfx\\ARMED.WAV"), relative to cwd; case-insensitive-FS
//    insurance = a lowercase retry (DTA names are uppercase, files often lower).
//  - The music pump thread never runs (AfxBeginThread is a no-thread object) and WaveMixPump
//    is a no-op: backends self-mix.
//
// MCI (GAME_INDY MIDI music; Yoda ships no MIDs): exactly the four command shapes the game
// emits — "open sequencer!<file> alias <NAME>" / "play <NAME> [from 1]" / "stop <NAME>" /
// "close <NAME>" — one sequencer audible at a time, which matches how the game uses it
// (Indy_MidiPlay starts one theme, Indy_MidiStopAll stops all).
//
// Debug: YODA_SNDLOG=1 traces opens/plays/MCI to stderr (audibility itself needs a human).

#include <windows.h>
#include <mmsystem.h>
#include <mfxplat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static bool s_bSndLog;
#define SNDLOG(args) do { if (s_bSndLog) { fprintf args; fflush(stderr); } } while (0)

// ── session + wave table ────────────────────────────────────────────────────────────────────
static bool s_bSessionOpen;

enum { kMaxWaves = 256 };            // 64 SNDS slots + hardcoded extras; handle = index+1
static void* s_apWaves[kMaxWaves];   // backend wave objects, owned via MfxSndPlat*

// "sfx\\ARMED.WAV" → "sfx/ARMED.WAV" (same normalization CFile::Open applies to game paths)
static void SndNormalizePath(char* pszDst, size_t nDst, const char* pszSrc)
{
    size_t i = 0;
    for (; pszSrc[i] != 0 && i + 1 < nDst; i++)
        pszDst[i] = (pszSrc[i] == '\\') ? '/' : pszSrc[i];
    pszDst[i] = 0;
}

// open the backend device once (WaveMixInit, and MCI opens sequencers independently of the
// SFX session — DESKADV FUN_1018_4c54 tail)
static bool SndEnsureSession(void)
{
    if (s_bSessionOpen)
        return true;
    s_bSndLog = s_bSndLog || (getenv("YODA_SNDLOG") != NULL);
    if (getenv("YODA_NOSOUND") != NULL)
    {
        SNDLOG((stderr, "[snd] YODA_NOSOUND set — reporting no session\n"));
        return false;
    }
    if (!MfxSndPlatOpen())
    {
        SNDLOG((stderr, "[snd] backend open failed — sound disabled\n"));
        return false;
    }
    s_bSessionOpen = true;
    SNDLOG((stderr, "[snd] session open\n"));
    return true;
}

extern "C" {

UINT WaveMixPump(void) { return 0; }   // backends self-mix; nothing to feed

int WaveMixInit(void)
{
    s_bSndLog = (getenv("YODA_SNDLOG") != NULL);
    return SndEnsureSession() ? 1 : 0;   // any nonzero int = the session handle
}

int WaveMixOpenWave(int hMixSession, char* szWaveFilename, int /*hInst*/, DWORD /*dwFlags*/)
{
    if (!s_bSessionOpen || szWaveFilename == NULL || szWaveFilename[0] == 0)
        return 0;
    char szPath[512];
    SndNormalizePath(szPath, sizeof szPath, szWaveFilename);
    void* pWave = MfxSndPlatLoadWave(szPath);
    if (pWave == NULL)
    {
        for (char* p = szPath; *p; p++)
            *p = (char)tolower((unsigned char)*p);
        pWave = MfxSndPlatLoadWave(szPath);
    }
    if (pWave == NULL)
    {
        SNDLOG((stderr, "[snd] open FAILED \"%s\"\n", szWaveFilename));
        return 0;
    }
    for (int i = 0; i < kMaxWaves; i++)
    {
        if (s_apWaves[i] == NULL)
        {
            s_apWaves[i] = pWave;
            SNDLOG((stderr, "[snd] open \"%s\" -> handle %d\n", szWaveFilename, i + 1));
            return i + 1;
        }
    }
    MfxSndPlatFreeWave(pWave);
    return 0;
}

int WaveMixOpenChannel(int /*hMixSession*/, int /*iChannel*/, DWORD /*dwFlags*/)
{
    return s_bSessionOpen ? 0 : 1;     // 0 = success (channels pre-allocated by the backend)
}

int WaveMixActivate(int /*hMixSession*/, BOOL fActivate)
{
    if (!s_bSessionOpen)
        return 1;
    if (!fActivate)
        MfxSndPlatHalt(-1);            // deactivation (view dtor) silences everything
    return 0;                          // 0 = success
}

static DWORD s_anChannelStart[MFXSND_CHANNELS];   // last play-start tick, for LRU stealing

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
    if (nWave < 1 || nWave > kMaxWaves || s_apWaves[nWave - 1] == NULL)
        return 1;                      // 0 = failed open; anything else = the sic garbage int
    if (nFlags & 2)                    // WMIX_USELRUCHANNEL — iChannel ignored, sounds mix
    {
        nChannel = MfxSndPlatPlay(s_apWaves[nWave - 1], -1);
        if (nChannel < 0)              // all channels busy: steal the longest-playing one
        {
            int nOldest = 0;
            for (int ch = 1; ch < MFXSND_CHANNELS; ch++)
                if (s_anChannelStart[ch] < s_anChannelStart[nOldest])
                    nOldest = ch;
            MfxSndPlatHalt(nOldest);
            nChannel = MfxSndPlatPlay(s_apWaves[nWave - 1], nOldest);
        }
    }
    else                               // explicit channel (CLEARQUEUE halts what's playing)
    {
        if (nChannel < 0 || nChannel >= MFXSND_CHANNELS)
            nChannel = 0;
        if (nFlags & 1)
            MfxSndPlatHalt(nChannel);
        nChannel = MfxSndPlatPlay(s_apWaves[nWave - 1], nChannel);
    }
    if (nChannel >= 0 && nChannel < MFXSND_CHANNELS)
        s_anChannelStart[nChannel] = GetTickCount();
    SNDLOG((stderr, "[snd] play handle %d ch %d flags 0x%x\n", nWave, nChannel, nFlags));
    return 0;
}

int WaveMixFlushChannel(int /*hMixSession*/, int iChannel, DWORD dwFlags)
{
    if (!s_bSessionOpen)
        return 1;
    // WMIX_ALL (flags&1) flushes every channel; the game calls (session, 0, 1)
    MfxSndPlatHalt((dwFlags & 1) ? -1 : iChannel);
    return 0;
}

int WaveMixFreeWave(int /*hMixSession*/, int lpMixWave)
{
    if (lpMixWave < 1 || lpMixWave > kMaxWaves || s_apWaves[lpMixWave - 1] == NULL)
        return 1;
    MfxSndPlatFreeWave(s_apWaves[lpMixWave - 1]);   // backend halts it first if playing
    s_apWaves[lpMixWave - 1] = NULL;
    return 0;
}

int WaveMixCloseChannel(int /*hMixSession*/, int /*iChannel*/, DWORD /*dwFlags*/)
{
    if (s_bSessionOpen)
        MfxSndPlatHalt(-1);
    return 0;
}

int WaveMixCloseSession(int /*hMixSession*/)
{
    if (!s_bSessionOpen)
        return 0;
    MfxSndPlatHalt(-1);
    for (int i = 0; i < kMaxWaves; i++)
    {
        if (s_apWaves[i] != NULL)
        {
            MfxSndPlatFreeWave(s_apWaves[i]);
            s_apWaves[i] = NULL;
        }
    }
    MfxSndPlatClose();
    s_bSessionOpen = false;
    SNDLOG((stderr, "[snd] session closed\n"));
    return 0;
}

// ── mmsystem: PlaySound + MCI sequencer strings (Indy MIDI) ─────────────────────────────────

BOOL PlaySoundA(LPCSTR, HMODULE, DWORD) { return TRUE; }   // no direct callers found; parity stub

} // extern "C"

struct MciSequencer
{
    char  szAlias[32];
    void* pMusic;                      // backend music object
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
extern "C" MCIERROR mciSendStringA(LPCSTR cmd, LPSTR /*ret*/, UINT /*cchRet*/, HWND /*hwndCallback*/)
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
        if (!SndEnsureSession())                           // MIDI opens the device even when
            return 1;                                      // WaveMix failed/was skipped
        void* pMusic = MfxSndPlatMusicLoad(szPath);
        if (pMusic == NULL)
        {
            SNDLOG((stderr, "[snd] mci open FAILED \"%s\"\n", szPath));
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
        MfxSndPlatMusicFree(pMusic);
        return 1;
    }

    if (strcasecmp(szVerb, "play") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);              // trailing "from 1" ignored (= from start)
        MciSequencer* pSeq = MciFindAlias(szAlias);
        if (pSeq == NULL)
            return 1;
        MfxSndPlatMusicPlay(pSeq->pMusic);                 // halts the current stream first
        SNDLOG((stderr, "[snd] mci play %s\n", szAlias));
        return 0;
    }

    if (strcasecmp(szVerb, "stop") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);
        if (MciFindAlias(szAlias) == NULL)
            return 1;
        MfxSndPlatMusicHalt();                             // one music stream — stop is global
        return 0;
    }

    if (strcasecmp(szVerb, "close") == 0)
    {
        MciToken(p, szAlias, sizeof szAlias);
        MciSequencer* pSeq = MciFindAlias(szAlias);
        if (pSeq == NULL)
            return 1;
        MfxSndPlatMusicHalt();
        MfxSndPlatMusicFree(pSeq->pMusic);
        pSeq->pMusic = NULL;
        pSeq->szAlias[0] = 0;
        return 0;
    }

    SNDLOG((stderr, "[snd] mci UNHANDLED \"%s\"\n", cmd));
    return 1;
}
