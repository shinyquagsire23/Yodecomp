// World — game-state / score module.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "World.h"

extern "C" long   time(long *);         // CRT _time  (0x0042a400)
extern "C" double difftime(long, long); // CRT        (0x0042a3e0) — returns seconds

// FUNCTION: YODA 0x00401450
void World::UpdateScore()
{
    mScore = 0;
    mScore = CalcTimeScore();
    mScore += CalcSolvedScore();
    mScore += CalcScoreFromCounter();
    mScore += CalcCompletionScore();
}

// FUNCTION: YODA 0x00401490
int World::CalcCompletionScore()
{
    int   y, x;
    Zone *pZone = mZones;
    int   score = 0;
    float count = 0.0f;

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (pZone->exists > 0 && pZone->field24 == 1 && pZone->field20 == 1)
                count = count + 1.0f;
            pZone++;
        }
    }
    {
        float pct = (count / (float)mTotalZones) * 100.0f;
        if      (90.1 <= pct && pct <= 100.0) score = 300;
        else if (80.1 <= pct && pct <=  90.0) score = 270;
        else if (70.1 <= pct && pct <=  80.0) score = 240;
        else if (60.1 <= pct && pct <=  70.0) score = 210;
        else if (50.1 <= pct && pct <=  60.0) score = 180;
        else if (40.1 <= pct && pct <=  50.0) score = 150;
        else if (30.1 <= pct && pct <=  40.0) score = 120;
        else if (20.1 <= pct && pct <=  30.0) score =  90;
        else if (10.1 <= pct && pct <=  20.0) score =  60;
        else if ( 0.0 <= pct && pct <=  10.0) score =  30;
    }
    return score;
}

// FUNCTION: YODA 0x004016d0
int World::CalcScoreFromCounter()
{
    int v = mCounter3320;
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
// register/slot allocation differs (~9 bytes). Needs a permuter-style search.]
int World::CalcSolvedScore()
{
    int   y, x;
    Zone *pZone = mZones;
    int   score = 0;
    float solved = 0.0f;
    float total = 0.0f;

    for (y = 10; y != 0; y--) {
        for (x = 10; x != 0; x--) {
            if (pZone->exists > 0 && (total = total + 1.0f, pZone->field18 == 1))
                solved = solved + 1.0f;
            pZone++;
        }
    }
    {
        float pct = (solved / total) * 100.0f;
        if      (90.1 <= pct && pct <= 100.0) score = 100;
        else if (80.1 <= pct && pct <=  90.0) score =  90;
        else if (70.1 <= pct && pct <=  80.0) score =  80;
        else if (60.1 <= pct && pct <=  70.0) score =  70;
        else if (50.1 <= pct && pct <=  60.0) score =  60;
        else if (40.1 <= pct && pct <=  50.0) score =  50;
        else if (30.1 <= pct && pct <=  40.0) score =  40;
        else if (20.1 <= pct && pct <=  30.0) score =  30;
        else if (10.1 <= pct && pct <=  20.0) score =  20;
        else if ( 0.0 <= pct && pct <=  10.0) score =  10;
    }
    return score;
}

// FUNCTION: YODA 0x004019c0  [logic-complete; NOT byte-exact — MSVC caches mField7c
// in EDI vs the original's re-read from memory (CMP [mem],0). Optimizer CSE choice.]
int World::CalcTimeScore()
{
    int v;
    if (mField7c != 0)
        v = (int)difftime(mField78, time(0)) + mField7c;
    else
        v = (int)difftime(mField78, time(0));

    v = v / 60 + mTotalZones * -5;
    if (v <= 0) return 200;
    if (v * 20 >= 200) return 0;
    return 200 - v * 20;
}

// FUNCTION: YODA 0x00401a80
unsigned short World::GetZoneCell(int x, int y)
{
    if (x >= 0 && y >= 0 && x <= 9 && y <= 9)
        return mZones[x + y * 10].exists;
    return 0xffff;
}
