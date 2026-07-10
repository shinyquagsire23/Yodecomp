// AppData — the FIRST app translation unit (0x401000–0x401450). The app's collection/utility
// source file: it defines the small CObject-derived game-data element classes whose ctors/dtors
// the linker emits at the very start of .text (function-level COMDATs, app-objs-first link order):
//   - MapZone           (one cell of the World 10x10 zone grid; vftable 0x44b050)
//   - InvItem           (an inventory slot;                     vftable 0x44b068)
//   - WorldgenZoneEntry (a worldgen dedup entry;                vftable 0x44b080)
// The CObject Serialize/AssertValid/Dump no-op COMDATs (0x401060/70/80) and CObject's own
// ??_G/??1 (0x401130/0x401150) are pulled in for free by <afxwin.h>; this .obj emits them and
// they byte-match their folded addresses exactly (verified) — reconciled at link (Phase G2).
// The class declarations live in their canonical headers (MapZone.h, GameView.h, Worldgen.h),
// all reachable through Worldgen.h — which fixes the TU-phase dial for this file.
// It also opens with a small CWnd-derived UI class (message map @0x44b000) — see AppWnd below.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
// STATUS: 14/14 app functions EXACT. Zero unclaimed app code remains in this TU.
#include "Worldgen.h"

// ====================== AppWnd (small CWnd UI class) ======================
// The TU opens with a tiny CWnd-derived UI class (message map @0x44b000: WM_TIMER, WM_PAINT).
// Its Disable()/Enable() are EnableWindow(FALSE/TRUE) overrides that COMDAT-FOLD with the
// identical overrides in ~15 other UI classes (InvScrollBar's vtable 0x44b578 slots 36/37
// point at these folded copies) — the body only touches m_hWnd (this+0x1c), universal to every
// CWnd-derived class, so all copies are byte-identical and the linker keeps this one.
// (Exact class name / base message map 0x44c510 not yet pinned — bodies are what byte-match;
// the message-map + vtable DATA reconcile in the Phase-G whole-image build.)
class AppWnd : public CWnd
{
public:
    virtual void Disable();                  // 0x00401090
    virtual void Enable();                   // 0x004010a0
protected:
    afx_msg void OnTimer(UINT nIDEvent);     // 0x00401000
    afx_msg void OnPaint();                  // 0x00401010
    DECLARE_MESSAGE_MAP()
};

// FUNCTION: YODA 0x00401000
void AppWnd::OnTimer(UINT nIDEvent)
{
    Default();
}

// FUNCTION: YODA 0x00401010
void AppWnd::OnPaint()
{
    CPaintDC dc(this);
}

// FUNCTION: YODA 0x00401090
void AppWnd::Disable()
{
    ::EnableWindow(m_hWnd, FALSE);
}

// FUNCTION: YODA 0x004010a0
void AppWnd::Enable()
{
    ::EnableWindow(m_hWnd, TRUE);
}

#ifdef YODA_PORTABLE
// The original's map data @0x44b000 (WM_TIMER, WM_PAINT — see the TU header comment). The
// anchor build never emits AppWnd's vtable (class never instantiated; VC emits vtables lazily),
// but clang's key-function rule does, so the portable build needs the map defined.
BEGIN_MESSAGE_MAP(AppWnd, CWnd)
    ON_WM_TIMER()
    ON_WM_PAINT()
END_MESSAGE_MAP()
#endif

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
// NOTE (Phase G2 layout): the original emits ??_GWorldgenZoneEntry (0x401370) BEFORE the ctor
// (0x401390) but the ~ body AFTER it (0x401400) — the scalar-deleting dtor COMDAT is detached
// from the dtor body and pulled in early. Simple ctor/dtor source reorder does NOT reproduce it
// (swapping put the dtor BODY at 0x401370); this is a compiler-internal emission quirk. PARKED.

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
