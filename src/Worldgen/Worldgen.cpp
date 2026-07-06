// Worldgen TU (0x41c340–0x429000): worldgen + .wld save/load + .dta load (doc class source file).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "Worldgen.h"

// FUNCTION: YODA 0x00421e50
// Worldgen grid-cell traversal/placement priority (static 10x10 dword table). `this` is unused.
int World::GetZoneGridOrder(int x, int y)
{
    return gWorldgenGridOrderTable[x + y * 10];
}
