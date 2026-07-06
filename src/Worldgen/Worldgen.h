// Worldgen TU (0x41c340–0x429000): the doc class's .dta-load + worldgen + .wld-save source file
// (a third CDeskcppDoc source file, alongside GameData and WorldDoc). All methods are World::.
// This header will accrete the doc-TU World method decls as they are transcribed; see
// docs/worldgen.md for the full algorithm map.
#ifndef WORLDGEN_H
#define WORLDGEN_H
#include <afxwin.h>
#include <afxcoll.h>
#include "../Records/RecordClasses.h"

// The static 10x10 worldgen grid-order priority table (.data 0x00456630).
extern int gWorldgenGridOrderTable[100];

// Worldgen zone-entry record (8 bytes, vtable 0x44b080, ctor Mfc::0x401390): the element type
// of the two worldgen zone-entry lists (worldgenPendingZones worklist / worldgenRefZones dedup set).
class WorldgenZoneEntry : public CObject
{
public:
    short zoneId;                    // +0x04
    short val;                       // +0x06
    WorldgenZoneEntry(short zoneId, short val);   // 0x00401390 (first app TU)
    virtual ~WorldgenZoneEntry();                 // vtable 0x44b080; delete = vcall slot +4
};                                   // sizeof 0x08

// A 10x10 world-map grid cell (0x34 bytes) — CObject-DERIVED (vftable 0x44b050; ctor 0x4010b0 /
// dtor 0x401180 live in the first app TU). Layout is ctor-proven — see src/WorldDoc/WorldDoc.h.
class MapZone : public CObject
{
public:                              // +0x00 vftable (0x44b050)
    short id;                        // +0x04  zone id; -1 = empty cell
    char  _pad06[2];                 // +0x06
    int   zoneType;                  // +0x08
    short cellQuestSlot0;            // +0x0c
    short cellQuestSlot1;            // +0x0e
    short cellItemA;                 // +0x10
    short cellItemB;                 // +0x12
    short cellItemC;                 // +0x14
    short cellQuestSlot5;            // +0x16
    short cellQuestSlot6;            // +0x18
    char  _pad1a[2];                 // +0x1a
    int   flagSolved;                // +0x1c
    int   flagC;                     // +0x20
    int   flagA;                     // +0x24
    int   flagB;                     // +0x28
    int   flagD;                     // +0x2c
    short field30;                   // +0x30
    char  _pad32[2];                 // +0x32
                                     // sizeof 0x34
    MapZone();                                            // 0x004010b0 (first app TU)
    virtual ~MapZone();                                   // 0x00401180
};

// World facade for the functions transcribed so far (offsets from the ctor-derived layout in
// src/WorldDoc/WorldDoc.h; grows toward the real CDeskcppDoc as the TU fills in).
class World : public CDocument       // sizeof(CDocument) == 0x50 in MFC 4.2
{
public:
    char        _pad50[0x30];        // +0x0050  doc scalars (totalZones@0x58, nFrameMode@0x5c, ...)
    CObArray    tiles;               // +0x0080  Tile*
    CObArray    zones;               // +0x0094  Zone*
    CObArray    inventory;           // +0x00a8  InvItem*
    CObArray    characters;          // +0x00bc  Character*
    CObArray    puzzles;             // +0x00d0  Puzzle*
    CString     soundNames[64];      // +0x00e4
    CWordArray  questItemsA;         // +0x01e4
    CWordArray  questItemsB;         // +0x01f8
    CWordArray  goalTileList;        // +0x020c
    CWordArray  placedZoneIds;       // +0x0220
    CWordArray  unk234;              // +0x0234
    CWordArray  unk248;              // +0x0248
    CObArray    worldgenPendingZones; // +0x025c  worklist (push-front)
    CObArray    worldgenRefZones;    // +0x0270  dedup set (WorldgenZoneEntry*)
    CWordArray  storyHistoryNevada;  // +0x0284
    CWordArray  storyHistoryAlaska;  // +0x0298
    CWordArray  storyHistoryOregon;  // +0x02ac
    Zone       *currentZone;         // +0x02c0
    CPalette   *pPalette;            // +0x02c4
    int         unk2c8;              // +0x02c8
    int         worldSeed;           // +0x02cc
    Zone       *apZoneGrid[100];     // +0x02d0
    Tile       *apUiTiles[20];       // +0x0460
    MapZone     mapGrid[100];        // +0x04b0  active 10x10 grid
    MapZone     mapGridBackup[100];  // +0x1900  backup grid
    MapZone     mapScratch[4];       // +0x2d50

    // ---- this TU's methods (grow one decl at a time as functions land) ----
    int  ZoneProvidesItem(short zoneId, short itemId);   // 0x0041c3b0
    int  ZoneFindInIzxList(short zoneId, short itemId, int sel); // 0x0041c490
    int  IsItemPlaced(short itemId);                     // 0x0041d670
    void WorldgenPushZoneEntry(short zoneId, short val); // 0x0041d6b0
    void RemoveZoneEntry(short zoneId);                  // 0x0041d740
    void RemoveZoneEntry2(short zoneId);                 // 0x0041d7a0
    void WorldgenAddZoneEntry(short zoneId, short val);  // 0x0041d800
    int  IsZoneUsed(short zoneId);                       // 0x0041d8d0
    void AddPlacedZoneId(short zoneId);                  // 0x0041d920
    int  CheckZoneItemsAvailable(short zoneId);          // 0x0041f830
    void WorldgenCollectZoneRefs(short zoneId);          // 0x0041f8e0
    void BackupZoneGrid();                               // 0x00421460
    void RestoreGridFromBackup();                        // 0x00421520
    int  IsTileInGoalList(unsigned int tileId);          // 0x004215e0
    int  GetZoneGridOrder(int x, int y);                 // 0x00421e50
    virtual BOOL IsModified();                           // 0x00422f40
    virtual void SetModifiedFlag(BOOL bModified = TRUE); // 0x00422f50
    void SetCurrentToIntroZone();                        // 0x00423d20

    // ---- cross-TU stubs (defined in other TUs; calls are masked relocs) ----
    Zone *GetZoneById(short id);                         // 0x00403a70 (GameData TU)
    void RefreshZone();                                  // 0x00403ae0 (GameData TU)
};

#endif
