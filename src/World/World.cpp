// World — game-state / score module (compile unit at 0x401450-0x401ab9).
// Uses the SHARED World class from src/Worldgen/Worldgen.h (the doc-class facade); the old
// local World.h (with its 0x34 "Zone" — really MapZone shifted by 4) was retired 2026-07-06.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "../Worldgen/Worldgen.h"

extern "C" long   time(long *);         // CRT _time  (0x0042a400)
extern "C" double difftime(long, long); // CRT        (0x0042a3e0) — returns seconds

// FUNCTION: YODA 0x00401450
void CDeskcppDoc::UpdateScore()
{
    score = 0;
    score = CalcTimeScore();
    score += CalcSolvedScore();
    score += CalcScoreFromCounter();
    score += CalcCompletionScore();
}

// FUNCTION: YODA 0x00401490  [PHASE-DISPLACED by the 2026-07-06 header consolidation: was
// byte-EXACT against the retired local World.h. The indexed mapGrid[n].field port is
// structurally identical (same lea this+0x4b4 anchor, same +0x20/+0x24 disps) — residual is
// a pure EAX/EDX/ESI rotation + one lea slot (align=12); the optimizer-made walker temp's
// register rank is not source-steerable (decl-order probe inert). Joint pass.]
int CDeskcppDoc::CalcCompletionScore()
{
    int   y, x;
    int   result = 0;
    int   n = 0;
    float count = 0.0f;

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (mapGrid[n].id > 0 && mapGrid[n].flagB == 1 && mapGrid[n].flagA == 1)
                count = count + 1.0f;
            n++;
        }
    }
    {
        float pct = (count / (float)totalZones) * 100.0f;
        if      (90.1 <= pct && pct <= 100.0) result = 300;
        else if (80.1 <= pct && pct <=  90.0) result = 270;
        else if (70.1 <= pct && pct <=  80.0) result = 240;
        else if (60.1 <= pct && pct <=  70.0) result = 210;
        else if (50.1 <= pct && pct <=  60.0) result = 180;
        else if (40.1 <= pct && pct <=  50.0) result = 150;
        else if (30.1 <= pct && pct <=  40.0) result = 120;
        else if (20.1 <= pct && pct <=  30.0) result =  90;
        else if (10.1 <= pct && pct <=  20.0) result =  60;
        else if ( 0.0 <= pct && pct <=  10.0) result =  30;
    }
    return result;
}

// FUNCTION: YODA 0x004016d0
int CDeskcppDoc::CalcScoreFromCounter()
{
    int v = counter;
    int r = 0;
    if (v >= 0x5b && v <= 0x64) return 400;
    if (v >= 0x51 && v <= 0x5a) return 0x168;
    if (v >= 0x47 && v <= 0x50) return 0x140;
    if (v >= 0x3d && v <= 0x46) return 0x118;
    if (v >= 0x33 && v <= 0x3c) return 0xf0;
    if (v >= 0x29 && v <= 0x32) return 200;
    if (v >= 0x1f && v <= 0x28) return 0xa0;
    if (v >= 0x15 && v <= 0x1e) return 0x78;
    if (v >= 0x0b && v <= 0x14) return 0x50;
    if (v >= 0    && v <= 0x0a) r = 0x28;
    return r;
}

// FUNCTION: YODA 0x00401780  [logic-complete; NOT byte-exact — x87 2-accumulator
// register/slot allocation differs (~9 bytes). Permuter-immune park (see CLAUDE.md).]
int CDeskcppDoc::CalcSolvedScore()
{
    int   x;
    int   y;
    int   n = 0;
    int   result = 0;
    float solved = 0.0f;
    float total = 0.0f;

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (mapGrid[n].id > 0 && (total = total + 1.0f, mapGrid[n].flagSolved == 1))
                solved = solved + 1.0f;
            n++;
        }
    }
    {
        float pct = (solved / total) * 100.0f;
        if      (90.1 <= pct && pct <= 100.0) result = 100;
        else if (80.1 <= pct && pct <=  90.0) result =  90;
        else if (70.1 <= pct && pct <=  80.0) result =  80;
        else if (60.1 <= pct && pct <=  70.0) result =  70;
        else if (50.1 <= pct && pct <=  60.0) result =  60;
        else if (40.1 <= pct && pct <=  50.0) result =  50;
        else if (30.1 <= pct && pct <=  40.0) result =  40;
        else if (20.1 <= pct && pct <=  30.0) result =  30;
        else if (10.1 <= pct && pct <=  20.0) result =  20;
        else if ( 0.0 <= pct && pct <=  10.0) result =  10;
    }
    return result;
}

// FUNCTION: YODA 0x004019c0  [logic-complete; NOT byte-exact — MSVC caches timeOffset
// in EDI vs the original's re-read from memory (CMP [mem],0). Optimizer CSE choice.]
int CDeskcppDoc::CalcTimeScore()
{
    int v;
    if (timeOffset != 0) {
        v = (int)difftime(timeBase, time(0));
        v += timeOffset;        // separate statement: the original re-reads [this+0x7c] here
    } else
        v = (int)difftime(timeBase, time(0));

    v = v / 60 + totalZones * -5;
    if (v <= 0) return 200;
    int t = v * 20;      // single temp: the original reuses ECX for both the compare and 200-t
    if (t >= 200) return 0;
    return 200 - t;
}

// FUNCTION: YODA 0x00401a40
// World::GetVictoryZoneIndexMaybe — demo-hardcoded: zone 76 is the victory
// screen; returns its index if it really has 13, else -1.
int CDeskcppDoc::GetVictoryZoneIndexMaybe()
{
    return ((Zone *)zones.GetAt(76))->type == ZONE_TYPE_VICTORY_SCREEN ? 76 : -1;
}

// FUNCTION: YODA 0x00401a60
// World::GetLossZoneMaybe — demo-hardcoded: zone 77 is the loss screen.
Zone *CDeskcppDoc::GetLossZoneMaybe()
{
    Zone *pZone = (Zone *)zones.GetAt(77);
    return pZone->type == ZONE_TYPE_LOSS_SCREEN ? pZone : NULL;
}

// FUNCTION: YODA 0x00401a80
unsigned short CDeskcppDoc::GetZoneCell(int x, int y)
{
    if (x >= 0 && y >= 0 && x <= 9 && y <= 9)
        return mapGrid[x + y * 10].id;
    return 0xffff;
}
