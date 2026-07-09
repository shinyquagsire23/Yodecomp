// MapZone — one cell of the world's 10x10 zone grid (0x34 bytes) — CObject-DERIVED
// (vftable 0x44b050; ctor 0x4010b0 / dtor 0x401180 live in the first app TU). Layout is
// ctor-proven (v10 vptr-true rebuild). CANONICAL copy (struct de-dup step 2, 2026-07-07):
// included by Worldgen.h, WorldDoc.h and GameData/WorldStub.h at the position their local
// copies used to sit. World grid geography: mapGrid[100]@0x4b0, mapGridBackup[100]@0x1900,
// mapScratch[4]@0x2d50. (The old GameData "shifted-by-4" struct — id@+0, zones@0x4b4 — was
// the same layout with the vptr swallowed by the preceding pointer array; retired here.)
// No #includes on purpose: requires afxwin.h (CObject) from the including header.
#ifndef MAPZONE_H
#define MAPZONE_H

class MapZone : public CObject
{
public:                              // +0x00 vftable (0x44b050)
    short id;                        // +0x04  ctor: -1 (zone id; -1 = empty cell)
    char  _pad06[2];                 // +0x06
    int   zoneType;                  // +0x08  ctor: 0 (ZoneType; GetLocatorIcon switches on it;
                                     //         GameData StartGame clears as dword -1)
    short cellQuestSlot0;            // +0x0c  ctor: -1
    short cellQuestSlot1;            // +0x0e
    short cellItemA;                 // +0x10  ctor: -1
    short cellItemB;                 // +0x12
    short cellItemC;                 // +0x14  ctor: -1
    short cellQuestSlot5;            // +0x16
    short cellQuestSlot6;            // +0x18  ctor: -1
    char  _pad1a[2];                 // +0x1a
    int   flagSolved;                // +0x1c  ctor: 0
    int   flagC;                     // +0x20
    int   flagA;                     // +0x24  ctor: 0
    int   flagB;                     // +0x28  ctor: 0
    int   flagD;                     // +0x2c
    short field30;                   // +0x30  quest-list selector candidate (1=listA else listB
                                     //         — ShowWinMessage; rename pending proof)
    char  _pad32[2];                 // +0x32
                                     // sizeof 0x34
    MapZone();                                            // 0x004010b0 (first app TU)
    virtual ~MapZone();                                   // 0x00401180
};

#endif
