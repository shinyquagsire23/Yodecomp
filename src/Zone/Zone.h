// Zone — a map/zone object (class @ 0x405150, vtable 0x44b1c0, sizeof 0x848).
// See docs/dta-format.md for the full layout. Non-virtual here (placeholder vptr)
// so field offsets are exact; the matched methods don't use the vtable.
#ifndef ZONE_H
#define ZONE_H

struct Zone {
    void          *_vtable;          // +0x00
    int            type;             // +0x04  (== 8 => special/indoor)
    char           _pad08[4];        // +0x08
    short          width;            // +0x0c  (18)
    short          height;           // +0x0e  (18)
    unsigned short tiles[18 * 18 * 3];// +0x10  flat grid: ((y*18+x)*3 + layer)
    // ... objects CObArray @+0x7a8, etc. (not needed by the tile accessors)

    unsigned short GetTile(int x, int y, int layer);        // 0x00405430
    void           SetTile(int x, int y, int layer, unsigned short val); // 0x00405480
    int            GetEdgeCode(int x, int y);               // 0x00405380
};

#endif
