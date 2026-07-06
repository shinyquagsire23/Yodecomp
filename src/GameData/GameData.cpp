// GameData — the second doc-TU source file (0x401ac0–0x4042b0): story-history registry
// persistence, world-map helpers, menu update handlers, asset accessors.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS (static MFC).
// v1 = nops + accessors + CCmdUI handlers. TODO v2: LoadStoryHistory*/SaveStoryHistory* (registry
// sextet), RemoveEmptyZonesFromPlacedList, PlaceZoneObjectTiles, Save/LoadZoneRecursive,
// OnReplayStory, StartGame, RefreshZone, BuildQuestPath.
#include "WorldStub.h"

// Demo-limiting helper: the three permanently-grayed menu items (Save/Load/Replay) share an
// inlined disable call — the EAX staging of the pointer arg is the inlining fingerprint.
static __inline void DemoDisable(CCmdUI *p)
{
    p->Enable(0);
}

// FUNCTION: YODA 0x00402660
void World::Nop1()
{
}

// FUNCTION: YODA 0x00403060
void World::Nop2()
{
}

// FUNCTION: YODA 0x00403250  [EFFECTIVE MATCH: DIFF(16) — id/x-counter register 2-cycle
//   (orig id=DX,x=ESI; ours swapped). Decl hoisting/order inert. Allocator tie-break.]
// Locate the world-map cell holding zone `id`; outputs grid coords.
int World::FindZoneCellById(short id, int *pX, int *pY)
{
    if (id >= 0 && id < zoneCount) {
        for (int y = 0; y < 10; y++)
            for (int x = 0; x < 10; x++) {
                if (zones[y * 10 + x].id == id) {
                    *pX = x;
                    *pY = y;
                    return 1;
                }
            }
        return 0;
    }
    return 0;
}

// FUNCTION: YODA 0x004032c0
// Bitmask of map exits from the player's cell: W=8 E=4 N=1 S=2. Indoor/special zone types
// (8/9/0xd/0xe) have no map exits. Consumed by GameView::DrawDirectionArrows.
unsigned char World::GetExitDirections()
{
    if (currentZone == 0)
        return 0;
    int t = currentZone->type;
    if (t == 8 || t == 9 || t == 0xd || t == 0xe)
        return 0;
    int x = playerX;
    int y;
    if (x < 0 || (y = playerY) < 0)
        return 0;
    unsigned char dirs = 0;      // byte-width: the original accumulates flags in AL
    if (x > 0 && zones[y * 10 + x - 1].id >= 0)
        dirs = 8;
    if (x < 9 && zones[y * 10 + x + 1].id >= 0)
        dirs = dirs | 4;
    if (y > 0 && zones[y * 10 + x - 10].id >= 0)
        dirs = dirs | 1;
    if (y < 9 && zones[y * 10 + x + 10].id >= 0)
        return dirs | 2;
    return dirs;
}

// FUNCTION: YODA 0x00403510
// File>Save World: permanently grayed in the demo.
// [EFFECTIVE MATCH: DIFF(6) x3 for the grayed trio — the original stages pCmdUI through EAX
//  (mov eax,[esp+4]; mov ecx,eax; mov edx,[eax]) where ours loads ECX directly. Local-copy,
//  inline-helper, and cast forms all fold to ours. 18 bytes total; park.]
void World::OnUpdateFileSave(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403520
void World::OnUpdateAppExit(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6 && nFrameMode != 5)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403550
void World::OnUpdateHideMe(CCmdUI *pCmdUI)
{
    if (nFrameMode != 1 && nFrameMode != 6)
        pCmdUI->Enable(1);
    else
        pCmdUI->Enable(0);
}

// FUNCTION: YODA 0x00403580
void World::OnUpdateNewWorld(CCmdUI *pCmdUI)
{
    switch (nFrameMode) {
    case 1:
    case 4:
    case 5:
    case 6:
    case 9:
    case 0xb:
        pCmdUI->Enable(0);
        return;
    case 7:
        if (nMapChangeReason == 4 || nMapChangeReason == 1)
            pCmdUI->Enable(0);
        else
            pCmdUI->Enable(1);
        return;
    default:
        pCmdUI->Enable(1);
        return;
    }
}

// FUNCTION: YODA 0x00403600
// File>Load World: permanently grayed in the demo.
void World::OnUpdateLoadWorld(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403610
// File>Replay Story: permanently grayed in the demo (the handler is still linked).
void World::OnUpdateReplayStory(CCmdUI *pCmdUI)
{
    DemoDisable(pCmdUI);
}

// FUNCTION: YODA 0x00403a40
Tile *World::GetTileData(int idx)
{
    if (idx >= 0 && idx < tileCount)
        return tileArray[idx];
    return 0;
}

// FUNCTION: YODA 0x00403a70
ZoneStub *World::GetZoneById(short id)
{
    if (id >= 0 && id < zoneCount)
        return zoneObjects[id];
    return 0;
}

// FUNCTION: YODA 0x00403aa0  [EFFECTIVE MATCH: DIFF(4) — orig reuses dead ECX (this) for the
//   walk pointer; ours colors it EDX. cmp-flip inert. Allocator tie-break.]
int World::FindTile(void *pTile)
{
    int r = -1;
    int n = tileCount;
    int i = 0;
    if (n > 0) {
        Tile **p = tileArray;
        do {
            if (*p == (Tile *)pTile) {
                r = i;
                break;
            }
            p++;
            i++;
        } while (i < n);
    }
    return r;
}
