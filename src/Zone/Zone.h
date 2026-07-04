// Zone — a map/zone object (class @ 0x405150, vtable 0x44b1c0, sizeof 0x848).
// See docs/dta-format.md for the full layout. Non-virtual here (placeholder vptr)
// so field offsets are exact; the matched methods don't use the vtable.
#ifndef ZONE_H
#define ZONE_H

// A placed object / hotspot in a zone.
struct ZoneObj {
    char  _pad00[8];
    short type;                      // +0x08  (== 1 active)
    short x;                         // +0x0a
    short y;                         // +0x0c
};

struct Zone {
    void          *_vtable;          // +0x00
    int            type;             // +0x04  (== 8 => special/indoor)
    char           _pad08[4];        // +0x08
    short          width;            // +0x0c  (18)
    short          height;           // +0x0e  (18)
    unsigned short tiles[18 * 18 * 3];// +0x10  flat grid (0x798 bytes, ends +0x7a8)
    char           _pad7a8[4];       // +0x7a8  CObArray vtable
    ZoneObj      **objects;          // +0x7ac  CObArray m_pData
    int            objectCount;      // +0x7b0  CObArray m_nSize
    char           _tail[0x848 - 0x7b4]; // pad to real sizeof == 0x848

    unsigned short GetTile(int x, int y, int layer);        // 0x00405430  MATCH
    void           SetTile(int x, int y, int layer, unsigned short val); // 0x00405480  MATCH
    int            GetEdgeCode(int x, int y);               // 0x00405380  WIP (cmp operand order)
    ZoneObj       *FindObjectAt(int x, int y);              // 0x00405330  WIP (reg alloc, ~7 bytes)
};

#endif
