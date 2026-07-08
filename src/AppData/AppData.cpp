// AppData — the FIRST app translation unit (0x401000–0x401450). The app's collection/utility
// source file: it defines the small CObject-derived game-data element classes whose ctors/dtors
// the linker emits at the very start of .text (function-level COMDATs, app-objs-first link order):
//   - MapZone           (one cell of the World 10x10 zone grid; vftable 0x44b050)
//   - InvItem           (an inventory slot;                     vftable 0x44b068)
//   - WorldgenZoneEntry (a worldgen dedup entry;                vftable 0x44b080)
// The CObject Serialize/AssertValid/Dump no-op COMDATs (0x401060/70/80) and CObject's own
// ??_G/??1 (0x401130/0x401150) are pulled in for free by <afxwin.h> and fold at link (Phase G2).
// The class declarations live in their canonical headers (MapZone.h, GameView.h, Worldgen.h),
// all reachable through Worldgen.h — which fixes the TU-phase dial for this file.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
//
// Still un-transcribed in this TU (next session): the small CWnd class whose message map is at
// 0x44b000 (WM_TIMER 0x401000 -> Default(), WM_PAINT 0x401010 -> CPaintDC) and the two adjacent
// virtual overrides 0x401090/0x4010a0 (EnableWindow(m_hWnd, FALSE/TRUE)) that appear in many
// derived vtables (0x44b5f0 …). Their owning class is not yet identified.
#include "../Worldgen/Worldgen.h"

// ============================== MapZone ==============================

// FUNCTION: YODA 0x004010b0
// Empty-cell defaults: ids/item slots = -1, flags/type = 0. (The compiler groups the same-value
// stores: the 0-writes descend 0x28->0x08, then the -1-writes descend 0x18->0x04.)
MapZone::MapZone()
{
    flagB          = 0;
    flagA          = 0;
    flagSolved     = 0;
    cellQuestSlot6 = -1;
    cellItemC      = -1;
    cellItemA      = -1;
    cellQuestSlot0 = -1;
    zoneType       = 0;
    id             = -1;
}

// FUNCTION: YODA 0x00401160  (compiler-generated scalar-deleting destructor ??_GMapZone)
// FUNCTION: YODA 0x00401180
MapZone::~MapZone()
{
}

// ============================== InvItem ==============================

// FUNCTION: YODA 0x004011d0
InvItem::InvItem()
{
    pTile = NULL;
}

// FUNCTION: YODA 0x00401250  (compiler-generated scalar-deleting destructor ??_GInvItem)
// FUNCTION: YODA 0x00401270
InvItem::InvItem(Tile *pTile, const char *pszName)
{
    this->pTile = pTile;
    name = pszName;
}

// FUNCTION: YODA 0x00401300
// Explicit (out-of-line) dtor: the CString member is destroyed here + CObject::~CObject chained.
// Declaring it (vs. leaving it implicit) keeps ??_GInvItem a thin 28-byte call-through instead of
// inlining the member destruction (implicit-vs-explicit dtor shape lesson).
InvItem::~InvItem()
{
}

// ============================== WorldgenZoneEntry ==============================

// FUNCTION: YODA 0x00401390
WorldgenZoneEntry::WorldgenZoneEntry(short zoneId, short val)
{
    this->zoneId = zoneId;
    this->val    = val;
}

// FUNCTION: YODA 0x00401370  (compiler-generated scalar-deleting destructor ??_GWorldgenZoneEntry)
// FUNCTION: YODA 0x00401400
WorldgenZoneEntry::~WorldgenZoneEntry()
{
}
