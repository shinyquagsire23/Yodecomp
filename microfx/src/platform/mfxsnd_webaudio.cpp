// microfx platform/ — the WASM WebAudio backend (GOAL 4, v89). The sdl3stream backend routes
// through SDL's emscripten ScriptProcessorNode: deprecated, main-thread-scheduled (starves
// whenever Asyncify holds the thread), and ~1s of perceived lag on Firefox (user-reported).
// This backend hands each decoded WAV to the browser as an AudioBuffer and plays it with an
// AudioBufferSourceNode — start(0) is immediate, and the WebAudio graph renders on the
// browser's own audio thread, so game-side stalls can't starve it. Decode stays in C
// (SDL_LoadWAV + SDL_ConvertAudioSamples → float32 planar); JS only owns the graph.
//
// MUSIC (MIDI): none — same story as sdl3stream (no synth; Yoda ships no .mid; an Indy-wasm
// build needs a soft-synth before its themes are audible).
//
// Autoplay: the context starts 'suspended' until a user gesture; one-time pointer/key
// listeners resume it (the game starts from a click anyway).

#include <mfxplat.h>

#include <SDL3/SDL.h>          // SDL_LoadWAV / SDL_ConvertAudioSamples only — no audio device
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_bLog;
#define SNDLOG(args) do { if (s_bLog) { fprintf args; fflush(stderr); } } while (0)

EM_JS(int, mfx_wa_open, (void), {
  try {
    var AC = window.AudioContext || window.webkitAudioContext;
    if (!AC) return 0;
    var st = window.__mfxWA = { ctx: new AC(), bufs: {}, nextId: 1, chans: {} };
    var resume = function() {
      if (st.ctx.state !== 'running') st.ctx.resume();
    };
    window.addEventListener('pointerdown', resume, true);
    window.addEventListener('keydown', resume, true);
    return 1;
  } catch (e) { return 0; }
});

EM_JS(void, mfx_wa_close, (void), {
  var st = window.__mfxWA;
  if (!st) return;
  Object.values(st.chans).forEach(function(src) { try { src.stop(); } catch (e) {} });
  st.ctx.close();
  window.__mfxWA = null;
});

// pPlanar = nCh consecutive float32 planes of nFrames each
EM_JS(int, mfx_wa_load, (const float *pPlanar, int nCh, int nFrames, int nRate), {
  var st = window.__mfxWA;
  if (!st) return 0;
  var buf = st.ctx.createBuffer(nCh, nFrames, nRate);
  for (var c = 0; c < nCh; c++) {
    var plane = HEAPF32.subarray((pPlanar >> 2) + c * nFrames, (pPlanar >> 2) + (c + 1) * nFrames);
    buf.copyToChannel(new Float32Array(plane), c);
  }
  var id = st.nextId++;
  st.bufs[id] = buf;
  return id;
});

EM_JS(void, mfx_wa_free, (int id), {
  var st = window.__mfxWA;
  if (st) delete st.bufs[id];
});

EM_JS(int, mfx_wa_play, (int id, int ch), {
  var st = window.__mfxWA;
  if (!st || !st.bufs[id]) return 0;
  // Suspended context (no user gesture yet): DROP the sound instead of queueing it. Queued
  // audio is the "later click = more lag" bug the ScriptProcessor path had — a sound that
  // couldn't be heard at its moment must not play late. Report success so the neutral layer
  // treats it as played, not as a busy channel to steal.
  if (st.ctx.state !== 'running') return 1;
  var old = st.chans[ch];
  if (old) { try { old.stop(); } catch (e) {} }
  var src = st.ctx.createBufferSource();
  src.buffer = st.bufs[id];
  src.connect(st.ctx.destination);
  src.__done = false;
  src.onended = function() { src.__done = true; };
  src.start(0);
  st.chans[ch] = src;
  return 1;
});

EM_JS(int, mfx_wa_busy, (int ch), {
  var st = window.__mfxWA;
  if (!st || !st.chans[ch]) return 0;
  return st.chans[ch].__done ? 0 : 1;
});

EM_JS(void, mfx_wa_halt, (int ch), {
  var st = window.__mfxWA;
  if (st && st.chans[ch]) { try { st.chans[ch].stop(); } catch (e) {} st.chans[ch] = null; }
});

// ── the MfxSndPlat contract ──────────────────────────────────────────────────────────────────

typedef struct MFXWAWAVE { int nId; } MFXWAWAVE;   // wave handle = JS buffer id

static int s_bOpen = 0;

extern "C" int MfxSndPlatOpen(void)
{
    s_bLog = (getenv("YODA_SNDLOG") != NULL);
    s_bOpen = mfx_wa_open();
    if (!s_bOpen)
        SNDLOG((stderr, "[snd] WebAudio unavailable\n"));
    return s_bOpen;
}

extern "C" void MfxSndPlatClose(void)
{
    if (s_bOpen) { mfx_wa_close(); s_bOpen = 0; }
}

extern "C" void *MfxSndPlatLoadWave(const char *pszPath)
{
    if (!s_bOpen)
        return 0;
    SDL_AudioSpec spec;
    Uint8 *pData = 0;
    Uint32 nLen = 0;
    if (!SDL_LoadWAV(pszPath, &spec, &pData, &nLen))
    {
        SNDLOG((stderr, "[snd] SDL_LoadWAV \"%s\": %s\n", pszPath, SDL_GetError()));
        return 0;
    }
    // convert to float32 (interleaved), then de-interleave to the planar layout WebAudio wants
    SDL_AudioSpec dst = spec;
    dst.format = SDL_AUDIO_F32;
    Uint8 *pF32 = 0;
    int nF32 = 0;
    if (!SDL_ConvertAudioSamples(&spec, pData, (int)nLen, &dst, &pF32, &nF32))
    {
        SNDLOG((stderr, "[snd] convert \"%s\": %s\n", pszPath, SDL_GetError()));
        SDL_free(pData);
        return 0;
    }
    SDL_free(pData);
    int nCh = dst.channels > 0 ? dst.channels : 1;
    int nFrames = nF32 / (int)(sizeof(float) * nCh);
    float *pPlanar = (float *)malloc(sizeof(float) * (size_t)nCh * nFrames);
    const float *pSrc = (const float *)pF32;
    for (int c = 0; c < nCh; c++)
        for (int i = 0; i < nFrames; i++)
            pPlanar[(size_t)c * nFrames + i] = pSrc[(size_t)i * nCh + c];
    SDL_free(pF32);

    int nId = mfx_wa_load(pPlanar, nCh, nFrames, dst.freq);
    free(pPlanar);
    if (nId == 0)
        return 0;
    MFXWAWAVE *pWave = (MFXWAWAVE *)malloc(sizeof(MFXWAWAVE));
    pWave->nId = nId;
    return pWave;
}

extern "C" void MfxSndPlatFreeWave(void *pWave)
{
    MFXWAWAVE *pW = (MFXWAWAVE *)pWave;
    if (pW == NULL)
        return;
    mfx_wa_free(pW->nId);
    free(pW);
}

extern "C" int MfxSndPlatPlay(void *pWave, int nChannel)
{
    MFXWAWAVE *pW = (MFXWAWAVE *)pWave;
    if (!s_bOpen || pW == NULL)
        return -1;
    if (nChannel < 0)                                // -1 = any free channel
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            if (!mfx_wa_busy(ch)) { nChannel = ch; break; }
        if (nChannel < 0)
            return -1;                               // all busy — the neutral LRU steals one
    }
    if (nChannel >= MFXSND_CHANNELS)
        return -1;
    return mfx_wa_play(pW->nId, nChannel) ? nChannel : -1;
}

extern "C" void MfxSndPlatHalt(int nChannel)
{
    if (!s_bOpen)
        return;
    if (nChannel < 0)
    {
        for (int ch = 0; ch < MFXSND_CHANNELS; ch++)
            mfx_wa_halt(ch);
        return;
    }
    if (nChannel < MFXSND_CHANNELS)
        mfx_wa_halt(nChannel);
}

// ── music (MIDI): unavailable — no synth (see header) ───────────────────────────────────────

extern "C" void *MfxSndPlatMusicLoad(const char *pszPath)
{
    SNDLOG((stderr, "[snd] no MIDI synth in the webaudio backend (\"%s\" not loaded)\n", pszPath));
    return 0;
}
extern "C" void MfxSndPlatMusicFree(void *) {}
extern "C" int  MfxSndPlatMusicPlay(void *) { return 1; }
extern "C" void MfxSndPlatMusicHalt(void) {}
