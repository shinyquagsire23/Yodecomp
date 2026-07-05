// Zone — CObject-derived map/zone class (ctor @ 0x405150, vtable 0x44b1c0, sizeof 0x848).
// Modeled as a real MFC class so the compiler emits the member-construction/destruction codegen
// (3 CObArray + 4 CDWordArray members) needed to byte-match the Ctor/Dtor. See docs/dta-format.md.
#ifndef ZONE_H
#define ZONE_H
#include <afxwin.h>
#include <afxcoll.h>

// A placed object / hotspot in a zone (0x10 bytes). Layout confirmed by byte-match of
// FindObjectAt (accesses +8/+0xa/+0xc) and FlagQuestObjects (reads type@+4, sets state@+8).
struct ZoneObj {
    void        *_vtable;            // +0x00
    unsigned int type;               // +0x04  ObjType category (unsigned; range-checked with JC/JA)
    short        state;              // +0x08  ==1 active/placed
    short        x;                  // +0x0a
    short        y;                  // +0x0c
    short        _visible;           // +0x0e
};

class Zone : public CObject
{
public:                              // +0x00 = CObject vtable
    int            type;             // +0x04  flags/areaType dword (== 8 => special/indoor)
    int            activatedFlag;    // +0x08
    short          width;            // +0x0c  (18)
    short          height;           // +0x0e  (18)
    short          tiles[18 * 18 * 3];// +0x10  flat grid (0x798 bytes, ends +0x7a8)
    CObArray       objects;          // +0x7a8
    CObArray       iactScripts;      // +0x7bc
    CObArray       entities;         // +0x7d0
    CDWordArray    cobArray4;        // +0x7e4
    CDWordArray    cobArray5;        // +0x7f8
    CDWordArray    genCandidateA;    // +0x80c  worldgen candidate-spot scratch
    CDWordArray    genCandidateB;    // +0x820
    int            tempVar;          // +0x834
    int            randVar;          // +0x838
    char           _pad83c[8];       // +0x83c
    short          globalVar;        // +0x844
    short          zoneUnk846;       // +0x846

    Zone(short w = 18, short h = 18);                       // 0x00405150
    virtual ~Zone();                                        // 0x004054d0
    unsigned short GetTile(int x, int y, int layer);        // 0x00405430  MATCH
    void           SetTile(int x, int y, int layer, unsigned short val); // 0x00405480  MATCH
    int            GetEdgeCode(int x, int y);               // 0x00405380
    ZoneObj       *FindObjectAt(int x, int y);              // 0x00405330
    void           FlagQuestObjects();                      // 0x004056d0
};

#endif
