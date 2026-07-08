// Records — the DTA record-classes TU (0x4042b0–0x405ae0, one .obj):
//   Puzzle (vtable 0x44b148) · Character (0x44b160) · MapEntity (0x44b178) · Tile (0x44b190)
//   · ZoneObj (0x44b1a8) · Zone (0x44b1c0)
// All CObject-derived (consecutive vtables in one .rdata cluster prove the single TU).
// Layouts pinned from the ctor/Init/accessor disassembly — see docs/structs.md.
// NOTE: unkNN field names are physical placeholders; a naming sweep runs in parallel —
// rename here when it lands (names don't affect the byte-match).
#ifndef RECORDS_H
#define RECORDS_H
#include <afxwin.h>
#include <afxcoll.h>
#include "RecordClasses.h"

class Character;

// Cross-TU stubs: layout only where offsets are touched; called methods are extern relocs.
class CDeskcppView
{
public:
    void PlaySound(int nSoundId);               // 0x00409060 (sound/feedback tick)
    void DrawZoneCell(short x, short y);        // 0x00409460 (redraw one map cell)
};

class CDeskcppDoc
{
public:
    char        _pad0[0x80];         // +0x000
    CObArray    tiles;               // +0x080  Tile* array (GetProjectileTile's paTiles)
    char        _pad94[0x2c];        // +0x094
    Character **characters;          // +0x0c0
    int FindTile(void *pTile);                  // 0x00403aa0
};

#endif
