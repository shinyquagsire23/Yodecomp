// Zone — map/zone class. Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "Zone.h"

// FUNCTION: YODA 0x00405430
unsigned short Zone::GetTile(int x, int y, int layer)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        return tiles[(y * 18 + x) * 3 + layer];
    return 0xffff;
}

// FUNCTION: YODA 0x00405480
void Zone::SetTile(int x, int y, int layer, unsigned short val)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        tiles[(y * 18 + x) * 3 + layer] = val;
}

// FUNCTION: YODA 0x00405380
int Zone::GetEdgeCode(int x, int y)
{
    if (x >= 0 && x < width && y >= 0 && y < height) return 0;
    if (type == 8) return 1;
    if (x < 0 && y >= 0 && y < height) return 4;
    if (x >= width && y >= 0 && y < height) return 5;
    if (y < 0 && x >= 0 && x < width) return 2;
    if (y >= height && x >= 0 && x < width) return 3;
    return 99;
}
