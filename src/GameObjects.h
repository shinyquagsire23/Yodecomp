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
#include "GameObjectClasses.h"

class Character;

// Cross-TU stubs: layout only where offsets are touched; called methods are extern relocs.
class CDeskcppView
{
public:
    void PlaySound(int nSoundId);               // 0x00409060 (sound/feedback tick)
    void DrawZoneCell(short x, short y);        // 0x00409460 (redraw one map cell)
};

#ifdef YODA_PORTABLE
// 64-bit-safe stub view (H4): real types up to the last member this TU touches, so offsets
// equal the full declaration's on any ABI (the #else pads are 32-bit-pinned). `characters`
// stays the raw m_pData view of the characters CObArray (anonymous-union overlay of the
// microfx layout). Truncated after it — this TU never allocates or sizeof's the doc.
class CDeskcppDoc : public CDocument
{
public:
    virtual ~CDeskcppDoc();          // Itanium key-function pin -> DeskcppDoc.cpp
    int         unk50;               // +0x0050
    int         unk54;               // +0x0054
    int         totalZones;          // +0x0058
    int         nFrameMode;          // +0x005c
    int         nMapChangeReason;    // +0x0060
    int         abortFrame;          // +0x0064
    int         gameState;           // +0x0068
    int         nRequestedGoalItem;  // +0x006c
    int         score;               // +0x0070
    int         nFrameDelay;         // +0x0074
    int         timeBase;            // +0x0078
    int         timeOffset;          // +0x007c
    CObArray    tiles;               // +0x080  Tile* array (GetProjectileTile's paTiles)
    CObArray    zones;               // +0x094
    CObArray    inventory;           // +0x0a8
    union { CObArray charactersArr;  // +0x0bc
            struct { void *_vChars; Character **characters; int _nChars; int _mChars; }; };
    int FindTile(void *pTile);                  // 0x00403aa0
};
#else
class CDeskcppDoc
{
public:
    char        _pad0[0x80];         // +0x000
    CObArray    tiles;               // +0x080  Tile* array (GetProjectileTile's paTiles)
    char        _pad94[0x2c];        // +0x094
    Character **characters;          // +0x0c0
    int FindTile(void *pTile);                  // 0x00403aa0
};
#endif // YODA_PORTABLE

#endif
