// WorldDoc — the doc-class main source file (TU at 0x419ed0–0x41bee0): DYNCREATE +
// message map, the World (CDeskcppDoc) ctor/dtor pair, OnNewDocument/OnOpenDocument,
// and a handful of gameplay methods the dev kept in this file.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "WorldDoc.h"
#include <string.h>

extern "C" int rand(void);
extern "C" unsigned char YodaMasterPalette[];   // .data 0x456230

World *gpWorld;                       // 0x004561dc — set by the ctor

// FUNCTION: YODA 0x00419ed0  (CreateObject)
// FUNCTION: YODA 0x00419f40  (GetRuntimeClass)
IMPLEMENT_DYNCREATE(World, CDocument)

// FUNCTION: YODA 0x00419f50  (GetMessageMap)
BEGIN_MESSAGE_MAP(World, CDocument)
    // TODO: reproduce the original entry list (data @0x44c2c8) — code matches regardless
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419f60
// dir (1=W 3=E 2=N 4=S, 0=none) of the neighbor of (x,y) holding a 0x68 gate cell.
int World::FindAdjacentGateDirMaybe(int x, int y, short *paGrid)
{
    int bWest = 0;
    int bNorth = 0;
    int bSouth = 0;
    int bEast = 0;
    if (x > 0)
        bWest = 1;
    if (x < 9)
        bEast = 1;
    if (y > 0)
        bNorth = 1;
    if (y < 9)
        bSouth = 1;
    if (bWest && paGrid[x + y * 10 - 1] == 0x68)
        return 1;
    if (bEast && paGrid[x + y * 10 + 1] == 0x68)
        return 3;
    if (bNorth && paGrid[x + y * 10 - 10] == 0x68)
        return 2;
    if (bSouth && paGrid[x + y * 10 + 10] == 0x68)
        return 4;
    return 0;
}

// FUNCTION: YODA 0x0041a030  [EFFECTIVE MATCH: DIFF(3) — first-loop backedge cmp direction
//   (orig cmp n,i;jg vs ours cmp i,n;jl), the GameData-loader jl/jg phase family; operand
//   flip proven inert. 119/119 insns otherwise identical.]
// TILE chunk parser: nBytes/0x404 records of (u32 flags + 0x400 pixel bytes).
int World::ParseTilesMaybe(CFile *pFile, unsigned int nBytes)
{
    Tile *pNew = NULL;
    int   n = nBytes / 0x404;
    int   i;

    tiles.SetSize(n, -1);
    for (i = 0; i < n; i++) {
        TRY {
            pNew = new Tile;
        }
        }              // closes the try block the TRY macro opened
        catch (CException *e) {                // hand-expanded CATCH_ALL(e)
            _afxExceptionLink.m_pException = e;
            THROW_LAST();
            AfxMessageBox(0xe01e, 0, (UINT)-1);    // sic: unreachable — the rethrow above
            AfxAbort();                            //      makes the OOM dialog dead code
        }                                          //      (docs/engine-bugs.md #7)
        }              // closes the TRY macro's outer (link-scope) brace
        if (pNew == NULL)
            return 0;
        tiles[i] = pNew;
    }
    for (i = 0; i < n; i++) {
        Tile *pTile = (Tile *)tiles[i];
        pFile->Read(&pTile->flags, 4);
        pFile->Read(pTile->pixels, 0x400);
    }
    return 1;
}

// FUNCTION: YODA 0x0041a1c0  [WIP: structure ~90%, align residual concentrated in block
//   LAYOUT — orig places the two head early-return bodies at the function END (0x558/0x567,
//   after all cases, before the jump table) while ours emits them inline (goto-label form
//   tried: the compiler tail-DUPLICATES the label block back to the branch site, +11 insns,
//   worse). Orig also emits the case-10 code among the case bodies. Needs the "when does
//   MSVC sink a return-body" mechanism mapped — next session. Hoisted-var set/order and all
//   case bodies verified against the disasm; per-case codegen (sbb idioms) matches.]
// Locator-map icon code for grid cell (x,y). Switches on the cell's zoneType;
// 0x11 = unvisited, 0x12/0x13 = town variants, 0xe/0x10 = gateway, etc.
unsigned int World::GetLocatorIconMaybe(int x, int y, int bAlt)
{
    int   i = x + y * 10;
    short idw = mapGrid[i].id;
    int   id = idw;
    int   zoneType = mapGrid[i].zoneType;

    if (id < 0x5d || id > 0x60) {
        if (idw >= 0) {
            short quest0 = mapGrid[i].cellQuestSlot0;
            int   solved = mapGrid[i].flagSolved;
            short itemC  = mapGrid[i].cellItemC;
            int   flagA  = mapGrid[i].flagA;
            int   flagB  = mapGrid[i].flagB;
            int   flagD  = mapGrid[i].flagD;

            switch (zoneType) {
            case 1:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                if (itemC >= 0)
                    return 3 - (flagB == 0);
                {
                    Zone *pZone = (Zone *)zones[id];
                    int nObjs = pZone->objects.GetSize();
                    for (int k = 0; k < nObjs; k++) {
                        ZoneObj *pObj = (ZoneObj *)pZone->objects[k];
                        if (pObj->type == 0xd && (unk2e60 != 0 || pObj->state == 1))
                            return (bAlt == 0) + 0x12;
                    }
                }
                return 0;
            case 2:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return (flagA == 0) ? 6 : 10;
            case 3:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return (flagA == 0) ? 7 : 0xb;
            case 4:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return (flagA == 0) ? 9 : 0xd;
            case 5:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return (flagA == 0) ? 8 : 0xc;
            case 6:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return 5 - (flagA == 0);
            case 7:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                return 5 - (flagA == 0);
            case 10:
                break;
            case 0xb:
                return 1;
            case 0xc:
                return 0;
            case 0xf:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                if (quest0 < 0)
                    return 0;
                if (flagA != 0 && flagB != 0)
                    return 3;
                return 2;
            case 0x10:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                if (quest0 < 0)
                    return 0;
                if (flagA != 0 && flagB != 0)
                    return 3;
                return 2;
            case 0x11:
                if (solved == 0 && unk2e60 == 0)
                    return 0x11;
                if (itemC >= 0)
                    return 3 - (flagB == 0);
                return 0;
            default:
                return 0xffffffff;
            }
            if (solved == 0 && unk2e60 == 0)
                return 0x11;
            if (flagB != 0 && flagD != 0)
                return 0xe;
            return 0x10;
        }
        return 0xffffffff;
    }
    return (mapGrid[i].flagSolved == 0) ? 0x11 : 0;
}

// FUNCTION: YODA 0x0041a5d0
// Caches the fixed UI tile pointers (locator icons, arrows, cursor) out of the tile array.
void World::CacheUiTilePtrsMaybe()
{
    apUiTiles[0]  = (Tile *)tiles[832];
    apUiTiles[1]  = (Tile *)tiles[829];
    apUiTiles[2]  = (Tile *)tiles[817];
    apUiTiles[3]  = (Tile *)tiles[818];
    apUiTiles[4]  = (Tile *)tiles[819];
    apUiTiles[5]  = (Tile *)tiles[820];
    apUiTiles[6]  = (Tile *)tiles[821];
    apUiTiles[7]  = (Tile *)tiles[825];
    apUiTiles[8]  = (Tile *)tiles[827];
    apUiTiles[9]  = (Tile *)tiles[823];
    apUiTiles[10] = (Tile *)tiles[822];
    apUiTiles[11] = (Tile *)tiles[826];
    apUiTiles[12] = (Tile *)tiles[828];
    apUiTiles[13] = (Tile *)tiles[824];
    apUiTiles[14] = (Tile *)tiles[830];
    apUiTiles[15] = (Tile *)tiles[837];
    apUiTiles[16] = (Tile *)tiles[831];
    apUiTiles[17] = (Tile *)tiles[835];
    apUiTiles[18] = (Tile *)tiles[834];
    apUiTiles[19] = (Tile *)tiles[833];
}

// FUNCTION: YODA 0x0041a6d0  [EFFECTIVE MATCH: DIFF(50) at exact length, 138/138 insns —
//   pure {this,camX,camY} EDI/EBX/ESI 3-cycle + one this-copy schedule slot; permuter-immune
//   (stmt/cmp/decl all inert — the parked contest family).]
// Redraws the map cell under the player onto the Canvas at (cameraX,cameraY):
// layer 0, layer 1 (masked if TILE_GAME_OBJECT), player frame, then layer 2.
void World::DrawPlayer()
{
    if (bWorldReady == 0 && pCanvas != NULL && currentZone != NULL &&
        pPlayerChar != NULL && pPlayerFrameTile != NULL) {
        int camX = cameraX;
        int camY = cameraY;
        short cx = (short)(camX / 32);
        short cy = (short)(camY / 32);
        short tileId;
        Tile *pTile;

        tileId = currentZone->GetTile(cx, cy, 0);
        if (tileId >= 0)
            pCanvas->BlitFast(((Tile *)tiles[tileId])->pixels, 0x20, 0x20, 0x20, camX, camY);
        tileId = currentZone->GetTile(cx, cy, 1);
        if (tileId >= 0) {
            pTile = (Tile *)tiles[tileId];
            if ((pTile->flags & 1) != 0)
                pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, camX, camY, 0);
            else
                pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, camX, camY);
        }
        if (bHidePlayer == 0)
            pCanvas->BlitMasked((char *)pPlayerFrameTile->pixels, 0x20, 0x20, camX, camY, 0);
        tileId = currentZone->GetTile(cx, cy, 2);
        if (tileId >= 0) {
            pTile = (Tile *)tiles[tileId];
            if ((pTile->flags & 1) != 0) {
                pCanvas->BlitMasked((char *)pTile->pixels, 0x20, 0x20, camX, camY, 0);
                return;
            }
            pCanvas->BlitFast(pTile->pixels, 0x20, 0x20, 0x20, camX, camY);
        }
    }
}

// FUNCTION: YODA 0x0041a870  [WIP: 609/609 insns after the switch() fix (planet blocks are
//   switches, compare-cascade emitted at top). Residual = the imm-vs-reg STORE-BATCHING open
//   problem (see CLAUDE.md pickup): our compiles sink `= imm` stores below `= reg(0)` runs;
//   the orig keeps them interleaved. nMusicEnabled=1 FIRST lands right; the 0x32-triple's
//   landing is order-invariant. Plus the member-ctor chain edx/ecx temp rename.]
// World (CDeskcppDoc) constructor: members in EH-state order, registry options,
// planet rotation, demo overrides, UI layout rects, palette object, install path.
World::World()
{
    gpWorld = this;
    nMusicEnabled = 1;
    unk2e60 = 0;
    bWorldInvalid = 0;
    unk2e58 = 0;
    gameState = 0;
    nSoundEnabled = 1;
    difficulty = 0x32;
    counter = 0x32;
    abortFrame = 0;
    completionCount = 0;
    highScore = 0;
    lastScore = 0;
    lastCount = 0;
    nRequestedGoalItem = -1;
    unk2e44 = 0;
    unk33a4 = -1;
    bStartingGame = 0;
    gameSpeed = 0x8c;
    worldSize = 2;
    currentPlanet = 1;
    unk33b4 = -1;

    CWinApp *pApp = AfxGetApp();
    if (pApp != NULL) {
        nSoundEnabled = pApp->GetProfileInt("OPTIONS", "PlaySound", 1);
        nMusicEnabled = pApp->GetProfileInt("OPTIONS", "PlayMusic", 1);
        difficulty = pApp->GetProfileInt("OPTIONS", "Difficulty", 0x32);
        counter = difficulty;
        gameSpeed = pApp->GetProfileInt("OPTIONS", "GameSpeed", 0x8c);
        completionCount = pApp->GetProfileInt("OPTIONS", "Count", 0);
        highScore = pApp->GetProfileInt("OPTIONS", "HScore", 0);
        lastScore = pApp->GetProfileInt("OPTIONS", "LScore", 0);
        lastCount = pApp->GetProfileInt("OPTIONS", "LCount", 0);
        worldSize = pApp->GetProfileInt("OPTIONS", "WorldSize", 2);
        currentPlanet = pApp->GetProfileInt("OPTIONS", "Terrain", 1);
        if (gameSpeed < 0x5f)
            gameSpeed = 0x5f;
        if (gameSpeed > 0xb9)
            gameSpeed = 0xb9;
        unk74 = *(int *)((char *)pApp + 0xc4);   // TODO: name the CWinApp-derived field
    }

    // planet rotation for the next game (every 5th completion forces the cycle)
    if (completionCount == 5 || completionCount == 10 || completionCount == 15) {
        switch (currentPlanet) {
        case 1:
            currentPlanet = 3;
            break;
        case 2:
            if (rand() % 2 == 0)
                currentPlanet = 3;
            else
                currentPlanet = 1;
            break;
        case 3:
            currentPlanet = 1;
            break;
        }
    }
    else {
        switch (currentPlanet) {
        case 1:
            if (rand() % 2 == 0)
                currentPlanet = 3;
            else
                currentPlanet = 2;
            break;
        case 2:
            if (rand() % 2 == 0)
                currentPlanet = 3;
            else
                currentPlanet = 1;
            break;
        case 3:
            if (rand() % 2 != 0)
                currentPlanet = 2;
            else
                currentPlanet = 1;
            break;
        }
    }
    pApp = AfxGetApp();
    if (pApp != NULL)
        pApp->WriteProfileInt("OPTIONS", "Terrain", currentPlanet);

    currentPlanet = 2;                    // demo hardcode: Alaska/Hoth only
    rectViewport.left = 8;
    rectViewport.top = 7;
    rectViewport.right = 0x128;
    worldSize = 1;                        // demo hardcode: small world
    rectInventory.top = 6;
    rectViewport.bottom = 0x127;
    rectInventory.left = 0x133;
    rectInventory.bottom = 0xe6;
    rectRightPane.top = 6;
    rectInventory.right = 0x1e9;
    rectRightPane.left = 0x1f0;
    rectRightPane.bottom = 0xe6;
    nWeaponBoxTop = 0xfc;
    rectRightPane.right = 0x200;
    nWeaponBoxLeft = 400;
    nWeaponBoxBottom = 0x11c;
    unk32b8 = 0xfc;
    nWeaponBoxRight = 0x1b0;
    unk32b4 = 0x180;
    unk32bc = 0x189;
    unk32c0 = 0x11c;
    unk32d0 = 0x11c;
    unk32c4 = 0x1c9;
    unk32cc = 0x1ea;
    unk32c8 = 0xfb;
    nArrowBoxLeft = 0x141;
    nHealthDialTop = 0;
    nHealthDialLeft = 0;
    nHealthDialBottom = 0x120;
    nHealthDialRight = 0x120;
    healthLo = 1;
    healthHi = 1;
    nArrowBoxTop = 0xf6;
    nArrowBoxRight = 0x169;
    unk333c = 0;
    unk3338 = 0;
    pPlayerFrameTile = NULL;
    currentWeapon = NULL;
    bWorldReady = 0;
    unk32f8 = 0;
    nArrowBoxBottom = 0x11e;
    unk32fc = 0;
    unk3360 = 0;
    unk3364 = 0;
    unk3368 = 0;
    unk336c = 0;
    bHidePlayer = 0;
    cameraX = 0x100;
    cameraY = 0xc0;
    unk2e34 = -1;
    playerY = 0;
    playerX = 0;
    pEquippedItem = NULL;
    pSysColorTable = YodaMasterPalette;
    pPalette = new CPalette;
    pCanvas = NULL;
    currentZone = NULL;
    unk50 = 0;
    unk54 = 0;
    pPlayerChar = NULL;
    unk3348 = 0;
    unk2e5c = 0;
    palVersion = 0x300;
    palNumEntries = 0x100;
    unk334a = 0;
    weaponState[0] = 0;
    weaponState[1] = 0;
    weaponState[2] = 0;
    weaponState[3] = 0;
    nCurrentAmmo = 0;

    // locate the installed game (registry Install Path, else scan for a fixed drive)
    CString strKey("SOFTWARE\\LucasArts Entertainment Company\\Yoda Stories\\1.0");
    HKEY  hKey;
    BYTE  path[260];
    char  drive[260];
    DWORD cb = 260;
    BOOL  bFound = FALSE;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, strKey, 0, KEY_READ, &hKey) == 0 &&
        RegQueryValueExA(hKey, "Install Path", NULL, NULL, path, &cb) == 0) {
        bFound = TRUE;
        installPath = (char *)path;
    }
    if (!bFound) {
        int nFixed = 0;
        int nTried = 0;
        lstrcpyA(drive, "A:\\");
        do {
            if (GetDriveTypeA(drive) == 3)
                nFixed++;
            else {
                nTried++;
                if (nTried < 0x1b)
                    drive[0]++;
                else {
                    nFixed++;
                    strcpy(drive, "");
                }
            }
        } while (nFixed == 0);
        if (lstrlenA(drive) >= 0)
            lstrcatA(drive, "YODA");
        installPath = drive;
    }
}

// FUNCTION: YODA 0x0041b2d0  (scalar deleting dtor ??_G — emitted by the compiler)
// FUNCTION: YODA 0x0041b2f0  [PHASE-DISPLACED: byte-matched 1441B under the dial at commit
//   854fba2; the GetLocatorIcon/DrawPlayer rewrites rotated one esi/edi 2-cycle in the tiles
//   free-loop (DIFF(6), align=0). Source proven correct; resolve at the TU joint pass.]
// World destructor: write the options back to the registry, then destroy all assets.
World::~World()
{
    int i, n;

    CWinApp *pApp = AfxGetApp();
    if (pApp != NULL) {
        pApp->WriteProfileInt("OPTIONS", "PlaySound", nSoundEnabled);
        pApp->WriteProfileInt("OPTIONS", "PlayMusic", nMusicEnabled);
        pApp->WriteProfileInt("OPTIONS", "Difficulty", difficulty);
        pApp->WriteProfileInt("OPTIONS", "GameSpeed", gameSpeed);
        pApp->WriteProfileInt("OPTIONS", "WorldSize", worldSize);
        pApp->WriteProfileInt("OPTIONS", "Count", completionCount);
        pApp->WriteProfileInt("OPTIONS", "HScore", highScore);
        pApp->WriteProfileInt("OPTIONS", "LScore", lastScore);
        pApp->WriteProfileInt("OPTIONS", "LCount", lastCount);
        pApp->WriteProfileInt("OPTIONS", "Terrain", currentPlanet);
    }

    n = inventory.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = inventory[i];
        if (p != NULL)
            delete p;
    }
    inventory.SetSize(0, -1);

    n = tiles.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = tiles[i];
        if (p != NULL)
            delete p;
    }
    tiles.SetSize(0, -1);

    n = zones.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = zones[i];
        if (p != (CObject *)-1 && p != NULL)   // sic: -1 sentinel entries in the zone array
            delete p;
    }
    zones.SetSize(0, -1);

    n = characters.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = characters[i];
        if (p != NULL)
            delete p;
    }
    characters.SetSize(0, -1);

    n = puzzles.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = puzzles[i];
        if (p != NULL)
            delete p;
    }
    puzzles.SetSize(0, -1);

    n = unk25c.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = unk25c[i];
        if (p != NULL)
            delete p;
    }
    unk25c.SetSize(0, -1);

    n = unk270.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = unk270[i];
        if (p != NULL)
            delete p;
    }
    unk270.SetSize(0, -1);

    questItemsA.SetSize(0, -1);
    questItemsB.SetSize(0, -1);
    goalTileList.SetSize(0, -1);

    if (pPalette != NULL)
        delete pPalette;
    if (pCanvas != NULL)
        delete pCanvas;
}

// FUNCTION: YODA 0x0041b8a0
// Modified copy of MFC's CDocument::OnOpenDocument (opens the CFile directly).
BOOL World::OnOpenDocument(LPCTSTR lpszPathName)
{
    IsModified();               // MFC source: if (IsModified()) TRACE0(...) — call survives

    CFile file;
    CFileException fe;
    if (!file.Open(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &fe)) {
        ReportSaveLoadException(lpszPathName, &fe, FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
        return FALSE;
    }

    DeleteContents();
    SetModifiedFlag();          // dirty during de-serialize

    CArchive loadArchive(&file, CArchive::load | CArchive::bNoFlushOnDelete);
    loadArchive.m_bForceFlat = FALSE;
    loadArchive.m_pDocument = this;
    TRY {
        BeginWaitCursor();
        Serialize(loadArchive);     // load me
        loadArchive.Close();
        file.Close();
    }
    CATCH_ALL(e) {
        file.Abort();               // will not throw an exception
        DeleteContents();           // remove failed contents
        EndWaitCursor();
        TRY {
            ReportSaveLoadException(lpszPathName, e, FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
        }
        END_TRY
        return FALSE;
    }
    END_CATCH_ALL

    EndWaitCursor();
    SetModifiedFlag(FALSE);     // start off with unmodified
    return TRUE;
}

// FUNCTION: YODA 0x0041bb10  [WIP: 286/286 insns, structure converged; residual = the
//   reg-rename/schedule family (GetSysColor byte-temp coloring al/cl, temp-store order in
//   the branch heads, zero-reg sourcing). Cracked this session: nFull=-1 init is the FIRST
//   statement; nUsed=0 per-branch (not hoisted); peFlags-inside-if + peRed/peBlue/peGreen
//   store order; pPalette loaded into a local BEFORE ::CreatePalette.]
// OnNewDocument override: zero the zone-pointer grid, build the game palette from the
// system palette + master table, create the offscreen Canvas.
BOOL World::OnNewDocument()
{
    int nFull = -1;
    if (!CDocument::OnNewDocument())
        return FALSE;

    int y, x;
    Zone **pRow = apZoneGrid;
    for (y = 10; y != 0; y--) {
        Zone **p = pRow;
        for (x = 10; x != 0; x--)
            *p++ = NULL;
        pRow += 10;
    }

    HDC hdc = ::GetDC(NULL);
    ::GetDeviceCaps(hdc, BITSPIXEL);
    unk2e5c = 1;
    int nSys = ::GetDeviceCaps(hdc, NUMCOLORS);
    if (nSys > 0x14) {
        nFull = nSys;
        nSys = 0x14;
    }
    ::GetSystemPaletteEntries(hdc, 0, 0x100, sysPalette);
    nSys /= 2;

    int k;
    int nUsed;
    if (nFull < 0) {
        // palettized device: mirror the first nSys system entries into the master table
        nUsed = 0;
        for (k = 0; k < nSys; k++) {
            sysPalette[k].peFlags = 0;
            pSysColorTable[k * 4 + 2] = sysPalette[k].peRed;
            pSysColorTable[k * 4 + 1] = sysPalette[k].peGreen;
            pSysColorTable[k * 4 + 0] = sysPalette[k].peBlue;
            nUsed = nSys;
        }
    }
    else {
        // full-color device: synthesize entries 0-3 from the system colors + white at 0xff
        nUsed = 0;
        if (nSys > 0) {
            sysPalette[0].peFlags = 0;
            nUsed = nSys;
        }
        sysPalette[0].peRed = 0;
        sysPalette[0].peBlue = 0;
        sysPalette[0].peGreen = 0;
        pSysColorTable[0] = 0;
        pSysColorTable[1] = pSysColorTable[0];
        pSysColorTable[2] = pSysColorTable[1];
        DWORD c = ::GetSysColor(0xf);
        BYTE b = (BYTE)c;
        sysPalette[1].peRed = b;
        pSysColorTable[6] = b;
        b = (BYTE)(c >> 8);
        sysPalette[1].peGreen = b;
        pSysColorTable[5] = b;
        sysPalette[1].peBlue = (BYTE)(c >> 0x10);
        pSysColorTable[4] = (BYTE)(c >> 0x10);
        c = ::GetSysColor(0x10);
        b = (BYTE)c;
        sysPalette[2].peRed = b;
        pSysColorTable[10] = b;
        b = (BYTE)(c >> 8);
        sysPalette[2].peGreen = b;
        pSysColorTable[9] = b;
        sysPalette[2].peBlue = (BYTE)(c >> 0x10);
        pSysColorTable[8] = (BYTE)(c >> 0x10);
        c = ::GetSysColor(0x14);
        WORD w = (WORD)c;
        sysPalette[3].peRed = (BYTE)w;
        pSysColorTable[0xe] = (BYTE)w;
        b = (BYTE)(w >> 8);
        sysPalette[3].peGreen = b;
        pSysColorTable[0xd] = b;
        sysPalette[3].peBlue = (BYTE)(c >> 0x10);
        pSysColorTable[0xc] = (BYTE)(c >> 0x10);
        sysPalette[0xff].peBlue = 0xff;
        sysPalette[0xff].peGreen = 0xff;
        sysPalette[0xff].peRed = 0xff;
        pSysColorTable[0x3fc] = 0xff;
        pSysColorTable[0x3fd] = pSysColorTable[0x3fc];
        pSysColorTable[0x3fe] = pSysColorTable[0x3fd];
    }

    int nEnd = 0x100 - nSys;
    if (nEnd > nUsed) {
        for (k = nUsed; k < nEnd; k++) {
            sysPalette[k].peRed = pSysColorTable[k * 4 + 2];
            sysPalette[k].peGreen = pSysColorTable[k * 4 + 1];
            sysPalette[k].peBlue = pSysColorTable[k * 4 + 0];
            sysPalette[k].peFlags = 1;
        }
    }
    if (nEnd < 0x100) {
        for (k = nEnd; k < 0x100; k++)
            sysPalette[k].peFlags = 0;
    }

    CPalette *pPal = pPalette;
    pPal->Attach(::CreatePalette((LOGPALETTE *)&palVersion));

    if (pCanvas == NULL) {
        Canvas *pNew = NULL;
        TRY {
            pNew = new Canvas(0x240, 0x240);
        } END_TRY
        pCanvas = pNew;
    }
    if (pCanvas != NULL)
        pCanvas->SetPalette(0, 0x100, (RGBQUAD *)pSysColorTable);
    pCanvas->Clear();               // sic: unguarded — crashes if the Canvas alloc failed

    Tile **p = apUiTiles;
    for (k = 0x10; k != 0; k--)
        *p++ = NULL;                // sic: only 16 of the 20 cached ptrs are cleared

    ::ReleaseDC(NULL, hdc);
    unk32fc = 0;
    return TRUE;
}
