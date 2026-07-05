// Zone — CObject-derived map/zone class. Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG
//        /D _WINDOWS /D _MBCS  (static MFC; see toolchain/README.md).
// Functions in .text/source order so the reg-allocator's cross-function state matches the original.
#include "Zone.h"

// FUNCTION: YODA 0x00405150
Zone::Zone(short w, short h)
{
    activatedFlag = 0;
    width = w;
    height = h;
    for (int i = 0; i < width; i++)
        for (int j = 0; j < height; j++) {
            tiles[(i * 18 + j) * 3 + 0] = -1;
            tiles[(i * 18 + j) * 3 + 1] = -1;
            tiles[(i * 18 + j) * 3 + 2] = -1;
        }
    type = 1;
    tempVar = 0;
    globalVar = -1;
    randVar = -1;
    zoneUnk846 = -1;
}

// FUNCTION: YODA 0x00405330  [EFFECTIVE MATCH: DIFF(13), all reg-alloc + instr-selection --
//   loop guard `test edi,edi` vs `cmp edi,eax`(eax=result=0), an ecx/edx/esi register permutation,
//   and end-cmp operand order. Structurally identical; not source-forceable. Confirmed 2026-07-05.]
ZoneObj *Zone::FindObjectAt(int x, int y)
{
    ZoneObj *result = 0;
    for (int i = 0; i < objects.GetSize(); i++) {
        ZoneObj *obj = (ZoneObj *)objects[i];
        if (obj->x == x && obj->y == y && obj->state == 1) {
            result = obj;
            break;
        }
    }
    return result;
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

// FUNCTION: YODA 0x00405430
unsigned short Zone::GetTile(int x, int y, int layer)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        return (unsigned short)tiles[(y * 18 + x) * 3 + layer];
    return 0xffff;
}

// FUNCTION: YODA 0x00405480
void Zone::SetTile(int x, int y, int layer, unsigned short val)
{
    if (x >= 0 && y >= 0 && x < width && y < height && layer >= 0 && layer <= 2)
        tiles[(y * 18 + x) * 3 + layer] = val;
}

// FUNCTION: YODA 0x004054d0  [EFFECTIVE MATCH: DIFF(12) on 506 bytes -- pure ESI<->EDI reg-alloc
//   (count vs offset register) in the 3 element-deletion loops; the objects loop matches, iact/
//   entities drew the opposite phase. Structurally identical.]
// Destructor: delete the CObject* elements of objects/iactScripts/entities (virtual dtor via delete),
// SetSize(0,-1) each, then SetSize the 4 CDWordArray scratch lists; members auto-destruct after.
Zone::~Zone()
{
    int i, n;
    n = objects.GetSize();
    for (i = 0; i < n; i++) { CObject *p = objects[i]; if (p) delete p; }
    objects.SetSize(0, -1);
    n = iactScripts.GetSize();
    for (i = 0; i < n; i++) { CObject *p = iactScripts[i]; if (p) delete p; }
    iactScripts.SetSize(0, -1);
    n = entities.GetSize();
    for (i = 0; i < n; i++) { CObject *p = entities[i]; if (p) delete p; }
    entities.SetSize(0, -1);
    cobArray4.SetSize(0, -1);
    cobArray5.SetSize(0, -1);
    genCandidateA.SetSize(0, -1);
    genCandidateB.SetSize(0, -1);
}

// FUNCTION: YODA 0x004056d0
// Marks every "quest" object (ObjType 6..10 = item/npc/weapon/door-in/door-out) active (state=1).
void Zone::FlagQuestObjects()
{
    int n = objects.GetSize();
    for (int i = 0; i < n; i++) {
        ZoneObj *obj = (ZoneObj *)objects[i];
        if (obj->type >= 6 && obj->type <= 10)
            obj->state = 1;
    }
}
