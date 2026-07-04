// World — game-state / score module (compile unit at 0x401450-0x401ab9).
// Non-virtual struct so `this` == the raw pointer and field offsets are exact.
#ifndef WORLD_H
#define WORLD_H

// A map zone record. sizeof == 0x34 (52). Only touched fields are named.
struct Zone {
    short exists;        // +0x00  >0 => zone populated
    char  _pad02[0x16];
    int   field18;       // +0x18  solved flag (used by CalcSolvedScore)
    int   _pad1c;
    int   field20;       // +0x20  solved flag A
    int   field24;       // +0x24  solved flag B
    char  _pad28[0x0c];
};

struct World {
    char  _pad00[0x58];
    int   mTotalZones;   // +0x58  divisor for completion %
    char  _pad5c[0x70 - 0x5c];
    int   mScore;        // +0x70  accumulated score
    int   mField74;      // +0x74
    int   mField78;      // +0x78  (time base)
    int   mField7c;      // +0x7c  (time offset)
    char  _pad80[0x4b4 - 0x80];
    Zone  mZones[100];   // +0x4b4  10x10 grid, ends +0x1904
    char  _pad1904[0x3320 - 0x1904];
    int   mCounter3320;  // +0x3320

    // --- methods (see World.cpp) ---
    void           UpdateScore();           // 0x00401450
    int            CalcCompletionScore();   // 0x00401490
    int            CalcScoreFromCounter();  // 0x004016d0
    int            CalcSolvedScore();       // 0x00401780
    int            CalcTimeScore();         // 0x004019c0
    unsigned short GetZoneCell(int x, int y); // 0x00401a80
};

#endif
