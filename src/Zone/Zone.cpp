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

// FUNCTION: YODA 0x00405380  [WIP: not byte-exact -- MSVC emits `cmp width,x;jg` for x<width; can't force from C (instruction selection). Permuter territory.]
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

// FUNCTION: YODA 0x00405330  [WIP: 7 bytes off, structure is otherwise identical (asmscore:
//   align=10, reg_pen=2, identity_miss=5). Exactly two residuals, both TU-context/instr-selection
//   (lesson #6/#7), NOT source-forceable in this partial TU:
//     (1) loop guard: orig `test edi,edi;jle`  vs mine `cmp edi,eax;jle` (eax=0 from result=0) --
//         a pure instruction-selection tie-break for the objectCount<=0 test.
//     (2) the objects[] walk pointer: orig keeps it in ECX (reusing the incoming `this` slot) and
//         puts the x-param in EDX; mine swaps them (objects->EDX, x->ECX). Clean ECX<->EDX bijection.
//   Revisit at full-Zone-TU assembly (allocation context shifts then). Confirmed via disasm 2026-07-05.]
ZoneObj *Zone::FindObjectAt(int x, int y)
{
    ZoneObj *result = 0;
    for (int i = 0; i < objectCount; i++) {
        ZoneObj *obj = objects[i];
        if (obj->x == x && obj->y == y && obj->type == 1) {
            result = obj;
            break;
        }
    }
    return result;
}
