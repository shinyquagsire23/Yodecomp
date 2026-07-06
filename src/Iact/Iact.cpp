// Iact — the Iact .obj (0x405ae0–0x407cf4): Zone's second source file — .dta chunk readers
// (IZON/IZAX/IZX2/IZX3/IZX4), the .wld saved-state pair, and the IACT interpreter
// (IactProbeMove/IactRun/IactRunCommands — TODO, next increment).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
#include "../Records/RecordClasses.h"
#include "../IactScript/IactScriptClasses.h"
#include <string.h>
#pragma intrinsic(strcmp)

// FUNCTION: YODA 0x00405ae0
// Parses an IZON header up to the tile grid: dims/type/vars + the 18x18x3 tile rows.
// A mismatched tag rewinds the 8 header bytes (record is optional in the stream).
void Zone::ReadIzon(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;

    pFile->Read(tag, 4);
    tag[4] = 0;
    pFile->Read(&size, 4);
    if (strcmp(tag, "IZON") != 0) {
        pFile->Seek(-8, CFile::current);
        return;
    }
    pFile->Read(&width, 2);
    pFile->Read(&height, 2);
    pFile->Read(&type, 4);
    pFile->Read(&globalVar, 2);
    pFile->Read(&zoneUnk846, 2);
    for (i = 0; i < width; i++)
        pFile->Read(&tiles[i * 54], height * 6);
}

// FUNCTION: YODA 0x00405bd0
// Read this zone's runtime state from a .wld: vars + tile grid (bFull), activatedFlag,
// objects (grown with new ZoneObj as needed), entities, IACT script done-flags.
// Called by World::LoadZoneRecursive.
void Zone::ReadSavedState(CFile *pFile, int bFull)
{
    int count;
    int i;

    if (bFull) {
        pFile->Read(&tempVar, 4);
        pFile->Read(&randVar, 4);
        pFile->Read(&zoneUnk83c, 4);
        pFile->Read(&zoneUnk840, 4);
        pFile->Read(&globalVar, 2);
        pFile->Read(&zoneUnk846, 2);
        for (i = 0; i < width; i++)
            pFile->Read(&tiles[i * 54], height * 6);
    }
    pFile->Read(&activatedFlag, 4);
    pFile->Read(&count, 4);
    int n = objects.GetSize();
    if (n < count) {
        int add = count - n;
        for (i = 0; i < add; i++) {
            ZoneObj *o = new ZoneObj;
            objects.SetAtGrow(objects.GetSize(), o);
        }
    }
    for (i = 0; i < count; i++) {
        ZoneObj *o = (ZoneObj *)objects[i];
        pFile->Read(&o->state, 2);
        pFile->Read(&o->visible, 2);
        pFile->Read(&o->type, 4);
        pFile->Read(&o->x, 2);
        pFile->Read(&o->y, 2);
    }
    if (bFull) {
        pFile->Read(&count, 4);
        for (i = 0; i < count; i++) {
            MapEntity *e = (MapEntity *)entities[i];
            pFile->Read(&e->charId, 2);
            pFile->Read(&e->x, 2);
            pFile->Read(&e->y, 2);
            pFile->Read(&e->damageTaken, 2);
            pFile->Read(&e->active, 4);
            pFile->Read(&e->unk10, 2);
            pFile->Read(&e->bulletX, 2);
            pFile->Read(&e->bulletY, 2);
            pFile->Read(&e->aiStepCounter, 2);
            pFile->Read(&e->unk18, 4);
            pFile->Read(&e->bRetreating, 4);
            pFile->Read(&e->unk20, 4);
            pFile->Read(&e->bulletDX, 2);
            pFile->Read(&e->bulletDY, 2);
            pFile->Read(&e->bulletStep, 2);
            pFile->Read(&e->seqIdx, 2);
            pFile->Read(&e->unk60, 2);
            pFile->Read(&e->item, 2);
            pFile->Read(&e->unk2c, 4);
            pFile->Read(&e->bRefreshFrame, 4);
            pFile->Read(&e->numItems, 4);
            pFile->Read(&e->timer, 2);
            pFile->Read(&e->wanderDir, 2);
            int *p = e->waypoints;
            for (int k = 4; k > 0; k--) {
                pFile->Read(p, 4);
                pFile->Read(p + 1, 4);
                p += 2;
            }
        }
        pFile->Read(&count, 4);
        for (i = 0; i < count; i++)
            pFile->Read(&((IactScript *)iactScripts[i])->doneFlag, 4);
    }
}

// FUNCTION: YODA 0x00405f30
// Write mirror of ReadSavedState (counts staged through a local). Called by
// World::SaveZoneRecursive.
void Zone::WriteSavedState(CFile *pFile, int bFull)
{
    int count;
    int i;

    if (bFull) {
        pFile->Write(&tempVar, 4);
        pFile->Write(&randVar, 4);
        pFile->Write(&zoneUnk83c, 4);
        pFile->Write(&zoneUnk840, 4);
        pFile->Write(&globalVar, 2);
        pFile->Write(&zoneUnk846, 2);
        for (i = 0; i < width; i++)
            pFile->Write(&tiles[i * 54], height * 6);
    }
    pFile->Write(&activatedFlag, 4);
    count = objects.GetSize();
    pFile->Write(&count, 4);
    for (i = 0; i < count; i++) {
        ZoneObj *o = (ZoneObj *)objects[i];
        pFile->Write(&o->state, 2);
        pFile->Write(&o->visible, 2);
        pFile->Write(&o->type, 4);
        pFile->Write(&o->x, 2);
        pFile->Write(&o->y, 2);
    }
    if (bFull) {
        count = entities.GetSize();
        pFile->Write(&count, 4);
        for (i = 0; i < count; i++) {
            MapEntity *e = (MapEntity *)entities[i];
            pFile->Write(&e->charId, 2);
            pFile->Write(&e->x, 2);
            pFile->Write(&e->y, 2);
            pFile->Write(&e->damageTaken, 2);
            pFile->Write(&e->active, 4);
            pFile->Write(&e->unk10, 2);
            pFile->Write(&e->bulletX, 2);
            pFile->Write(&e->bulletY, 2);
            pFile->Write(&e->aiStepCounter, 2);
            pFile->Write(&e->unk18, 4);
            pFile->Write(&e->bRetreating, 4);
            pFile->Write(&e->unk20, 4);
            pFile->Write(&e->bulletDX, 2);
            pFile->Write(&e->bulletDY, 2);
            pFile->Write(&e->bulletStep, 2);
            pFile->Write(&e->seqIdx, 2);
            pFile->Write(&e->unk60, 2);
            pFile->Write(&e->item, 2);
            pFile->Write(&e->unk2c, 4);
            pFile->Write(&e->bRefreshFrame, 4);
            pFile->Write(&e->numItems, 4);
            pFile->Write(&e->timer, 2);
            pFile->Write(&e->wanderDir, 2);
            int *p = e->waypoints;
            for (int k = 4; k > 0; k--) {
                pFile->Write(p, 4);
                pFile->Write(p + 1, 4);
                p += 2;
            }
        }
        count = iactScripts.GetSize();
        pFile->Write(&count, 4);
        for (i = 0; i < count; i++)
            pFile->Write(&((IactScript *)iactScripts[i])->doneFlag, 4);
    }
}

// FUNCTION: YODA 0x00406270
// Parses an IZAX record: entity list (count read twice — the first is a placeholder field),
// then two word lists into cobArray4/cobArray5.
void Zone::ReadZaux(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    pFile->Read(&count, 2);
    if (count > 0) {
        n = count;
        for (i = 0; i < n; i++) {
            MapEntity *e = new MapEntity;
            pFile->Read(&e->charId, 2);
            pFile->Read(&e->x, 2);
            pFile->Read(&e->y, 2);
            pFile->Read(&e->item, 2);
            pFile->Read(&e->numItems, 4);
            pFile->Read(e->waypoints, 0x20);
            entities.SetAtGrow(entities.GetSize(), e);
        }
    }
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, see Zax2 note
        do {
            pFile->Read(&n, 2);
            cobArray4.SetAtGrow(cobArray4.GetSize(), n);
        } while (--i != 0);
    }
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, see Zax2 note
        do {
            pFile->Read(&n, 2);
            cobArray5.SetAtGrow(cobArray5.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406410
// Parses an IZX2 record: one word list into genCandidateA.
void Zone::ReadZax2(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, not source-steerable
        do {
            pFile->Read(&n, 2);
            genCandidateA.SetAtGrow(genCandidateA.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406490
// Parses an IZX3 record: one word list into genCandidateB. Source clone of ReadZax2.
void Zone::ReadZax3(CFile *pFile)
{
    char  tag[5];
    int   size;
    int   i;
    short n;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
    if (count > 0) {
        i = count;              // orig: mov ax/movsx pair (+3B) — inst-selection, not source-steerable
        do {
            pFile->Read(&n, 2);
            genCandidateB.SetAtGrow(genCandidateB.GetSize(), n);
        } while (--i != 0);
    }
}

// FUNCTION: YODA 0x00406510
// Parses an IZX4 record header and discards it (tag/size/one word). `this` is unused.
void Zone::ReadZax4(CFile *pFile)
{
    char  tag[5];
    int   size;
    short count;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&count, 2);
}

// FUNCTION: YODA 0x00406550  [WIP: +26B — found-vs-r EBP contest: orig found=EBP/r=stack,
//   ours r=EBP/found=stack (memory-form found tests cost the bytes). Control flow verified
//   line-by-line against the true disasm (take-order coord-then-flag, two-store sign forms,
//   compiler inc-ebp for the 2nd-probe found=1). n merged per orig ESI reuse. Revisit once the
//   TU is complete (IactRun/RunCommands) — ReadIzon's identical 2-cycle self-resolved then.]
// Movement sidestep probe: target = (x+dx, y+dy). Returns -1 out-of-bounds, 1 target free,
// 0 blocked, or a direction code (2=E 3=W 4=S 5=N) after sidestepping around the obstacle
// (side picked pseudo-randomly via GetTickCount parity; bForce accepts occupied sidesteps).
// a5 is unused (caller-pushed). Used by the entity AI in GameView::Tick.
int Zone::IactProbeMove(int x, int y, int dx, int dy, int a5, int bForce)
{
    int   found = 0;
    int   tx = x + dx;
    int   ty = y + dy;
    short savedX, savedY;
    int   r, n;

    if (tx < 0 || ty < 0 || tx >= width || ty >= height)
        return -1;
    r = (GetTickCount() & 1) == 0 ? 1 : -1;
    savedX = (short)tx;
    savedY = (short)ty;
    if ((short)GetTile(tx, ty, 1) < 0)
        return 1;
    if (dx == 0 && dy != 0) {
        n = tx - r;
        if (n >= 0 && ((short)GetTile(n, ty, 1) < 0 || bForce != 0)) {
            tx = n;
            found = 1;
        }
        if (!found) {
            n = r + tx;
            if (n < width && ((short)GetTile(n, ty, 1) < 0 || bForce != 0)) {
                tx = n;
                found = 1;
            }
        }
    } else if (dy == 0 && dx != 0) {
        n = ty - r;
        if (n >= 0 && ((short)GetTile(tx, n, 1) < 0 || bForce != 0)) {
            ty = n;
            found = 1;
        }
        if (!found) {
            n = r + ty;
            if (n < height && ((short)GetTile(tx, n, 1) < 0 || bForce != 0)) {
                ty = n;
                found = 1;
            }
        }
    } else {
        n = tx + 1;
        if (dx >= 0)
            n = tx - 1;
        if ((short)GetTile(n, ty, 1) < 0 || bForce != 0) {
            tx = n;
            found = 1;
        }
        if (!found) {
            n = ty + 1;
            if (dy >= 0)
                n = ty - 1;
            if ((short)GetTile(tx, n, 1) < 0 || bForce != 0) {
                ty = n;
                found = 1;
            }
        }
    }
    if (!found)
        return 0;
    if (ty < savedY)
        return 5;
    if (ty > savedY)
        return 4;
    if (tx < savedX)
        return 3;
    if (tx > savedX)
        return 2;
    return 0;
}

// TODO (next increment, .text order continues):
//   Zone::IactRun         0x00406780 (2255B — condition-opcode switch; scrdoc.txt)
//   Zone::IactRunCommands 0x004070e0 (3039B — command-opcode switch + 7 EH funclets)
