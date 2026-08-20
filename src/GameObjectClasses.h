// RecordClasses — the six byte-match-proven DTA record classes from the Records TU
// (0x4042b0–0x405ae0, src/Records/Records.cpp): Puzzle, Character, MapEntity, Tile,
// ZoneObj, Zone. Shared by every TU that touches records (Records, GameData, ...).
// Layouts are byte-match-proven — do NOT edit offsets without re-verifying Records.cpp.
#ifndef RECORDCLASSES_H
#define RECORDCLASSES_H
#include <afxwin.h>
#include <afxcoll.h>

class Character;
class CDeskcppDoc;
class CDeskcppView;

// ── Zone grid geometry ───────────────────────────────────────────────────────
// Every zone is a fixed 18x18 cell grid, 3 stacked tile layers deep (floor /
// middle-colliding / above-player). The engine hardcodes these everywhere: the
// Zone ctor's default args, the flat `tiles` array, and the `(y * 18 + x) * 3 +
// layer` index arithmetic repeated across worldgen, IACT and the view.
// #define (not enum) so the value is textual and adds no tokens to the
// byte-matched anchor TUs.
#define ZONE_WIDTH       18
#define ZONE_HEIGHT      18
#define ZONE_LAYERS      3
#define ZONE_CELL_COUNT  (ZONE_WIDTH * ZONE_HEIGHT * ZONE_LAYERS)   // 972 shorts
// Pixel geometry of one tile and of the 9x9-tile play area blitted to the canvas.
#define TILE_PIXEL_SIZE  32                                   // tiles are 32x32 8bpp
#define TILE_PIXEL_COUNT (TILE_PIXEL_SIZE * TILE_PIXEL_SIZE)  // 1024 bytes per tile
#define VIEW_TILES       9                                    // play area is 9x9 tiles
#define VIEW_PIXEL_SIZE  (VIEW_TILES * TILE_PIXEL_SIZE)       // 288 px square
// The offscreen Canvas holds one WHOLE zone; the 288x288 view scrolls over it.
#define CANVAS_PIXEL_SIZE (ZONE_WIDTH * TILE_PIXEL_SIZE)      // 576 px square (stride too)
#define VIEW_SCROLL_MAX  (CANVAS_PIXEL_SIZE - VIEW_PIXEL_SIZE) // 288 — max camera offset
// Overview ("locator") map: the 10x10 world grid drawn as 28px cells inset 4px into the
// same 288x288 canvas, so a cell's centre is LOCATOR_MAP_INSET + LOCATOR_CELL_SIZE/2 == 18.
#define LOCATOR_CELL_SIZE 28
#define LOCATOR_MAP_INSET 4
// Zone-to-zone scroll wipe: the view slides 16px per frame; the trailing strip is
// re-read from the far edge of the canvas (576 - 16 - 1).
#define SCROLL_STEP_PIXELS 16
#define SCROLL_WRAP_SRC    (CANVAS_PIXEL_SIZE - SCROLL_STEP_PIXELS - 1)   // 559

// PUZ2 record (0x2c). 5 CStrings + item ids.
// Puzzle.nType — the puzzle's worldgen class (WorldgenSelectPuzzle maps zone types onto these:
// ZONE_TYPE_FIND_USEFUL_DROP->TRANSACTION, MAP_TO_ITEM_FOR_LOCK->TRADE, FINAL_ITEM->GOAL_PRIZE,
// and the 9999 request picks a WORLD_MISSION goal).
enum PuzzleType
{
    PUZZLE_TYPE_TRANSACTION   = 0,
    PUZZLE_TYPE_TRADE         = 1,
    PUZZLE_TYPE_GOAL_PRIZE    = 2,
    PUZZLE_TYPE_WORLD_MISSION = 3,
};

class Puzzle : public CObject
{
public:                              // +0x00 CObject vtable (0x44b148)
    int      nType;                  // +0x04  ctor: 0; a PuzzleType value (int: Records TU byte-matched)
    int      unk2;                   // +0x08  ctor: 0
    int      unk3;                   // +0x0c  ctor: 0
    short    itemA;                  // +0x10  ctor: -1
    short    itemB;                  // +0x12  ctor: -1
    short    unk14;                  // +0x14  ctor: 0
    char     _pad16[2];              // +0x16
    CString  text1;                  // +0x18
    CString  text2;                  // +0x1c
    CString  text3;                  // +0x20
    CString  text4;                  // +0x24
    CString  text5;                  // +0x28

    Puzzle();                                            // 0x004042b0
    virtual ~Puzzle();                                   // 0x004043c0 (ScalarDtor 0x004043a0)
    void Read(CFile *pFile);                             // 0x00404480
};

// CHAR record (0x4c). In-memory layout != file layout (name is read to stack, not stored).
class Character : public CObject
{
public:                              // +0x00 CObject vtable (0x44b160)
    short    frames[24];             // +0x04  3 anim banks x 8 facing dirs; -1 = none
    short    typeFlags;              // +0x34  ICHR flags low word: 1=hero 2=enemy 4=weapon
    short    moveType;               // +0x36  CharMoveType (enemy-AI switch in GameView::Tick)
    short    weaponCharId;           // +0x38  ctor via Init: -1 (none); filled by ParseChwp
    short    health;                 // +0x3a  Init: 1; filled by ParseChwp (chwp_entry)
    short    currentFrame;           // +0x3c  cached by GetFrameTile
    short    damage;                 // +0x3e  Init default: 1
    short    unk40;                  // +0x40  ICHR unk_4 (parse-only, no runtime reader)
    char     _pad42[2];              // +0x42
    int      unk44;                  // +0x44  ICHR unk_5 (parse-only); ctor 0
    short    unk48;                  // +0x48  ctor: 0
    char     _pad4a[2];              // +0x4a

    Character();                                         // 0x00404670
    virtual ~Character();                                // 0x00404700 (ScalarDtor 0x004046e0)
    void  Init(short nTypeFlags, short nMoveType, short nUnk40, int nUnk44); // 0x00404750
    void  Read(CFile *pFile);                            // 0x004047a0
    void *GetWalkFrameTile(int dx, int dy, CObArray *paTiles);            // 0x00404830
    void *GetFrameTile(int dx, int dy, CObArray *paTiles, int nAnimBank); // 0x00404850
    void *GetProjectileTile(int a, int dx, int dy, int d, CObArray *paTiles); // 0x00404910
};

// Placed entity on the map (0x64). Semantic names provisional (verification sweep in flight).
class MapEntity : public CObject
{
public:                              // +0x00 CObject vtable (0x44b178)
    short    charId;                 // +0x04  ctor: -1
    short    x;                      // +0x06  ctor: -1
    short    y;                      // +0x08  ctor: -1
    short    damageTaken;            // +0x0a  ctor: 0
    int      active;                 // +0x0c  ctor: 1
    short    unk10;                  // +0x10  ctor: 0
    short    bulletX;                // +0x12  ctor: 0; projectile pos (layer 1)
    short    bulletY;                // +0x14  ctor: 0
    short    aiStepCounter;          // +0x16  ctor: 0
    int      unk18;                  // +0x18  ctor: 0
    int      bRetreating;            // +0x1c  ctor: 0
    int      unk20;                  // +0x20  ctor: 0
    short    timer;                  // +0x24  ctor: 0
    unsigned short item;             // +0x26
    int      numItems;               // +0x28  dword (DamageEntityAt tests ==0 as int; IZAX numItems+unk3)
    int      unk2c;                  // +0x2c  ctor: 0
    short    wanderDir;              // +0x30  ctor: 1 (-1..2 dir code)
    char     _pad32[2];              // +0x32
    int      bRefreshFrame;          // +0x34  ctor: 1
    short    bulletDX;               // +0x38  ctor: 0
    short    bulletDY;               // +0x3a  ctor: 0
    short    bulletStep;             // +0x3c  ctor: 0 (travel counter, <4 = range)
    short    seqIdx;                 // +0x3e  ctor: 0 (waypoint/anim seq idx)
    int      waypoints[8];           // +0x40  4 patrol (x,y) pairs from IZAX tail
    short    unk60;                  // +0x60  ctor: 0
    char     _pad62[2];              // +0x62

    MapEntity();                                         // 0x00404c80
    virtual ~MapEntity();                                // 0x00404d50 (ScalarDtor 0x00404d30)
};

// Tile.flags bits (names from ~/workspace/DesktopAdventures src/include/tile.h).
// Bits 16+ are subtype bits whose meaning depends on the category bit (WEAPON/ITEM/CHARACTER).
enum TileFlags
{
    TILE_GAME_OBJECT                = 1 << 0,
    TILE_UNDER_PLAYER_NONCOLLIDING  = 1 << 1,
    TILE_MIDDLE_LAYER_COLLIDING     = 1 << 2,
    TILE_PUSH_PULL_BLOCK            = 1 << 3,
    TILE_ABOVE_PLAYER_NONCOLLIDING  = 1 << 4,
    TILE_MINI_MAP_TILE              = 1 << 5,
    TILE_WEAPON                     = 1 << 6,
    TILE_ITEM                       = 1 << 7,
    TILE_CHARACTER                  = 1 << 8,
    // WEAPON subtypes                        // ITEM subtypes            // CHARACTER subtypes
    TILE_LIGHT_BLASTER              = 1 << 16, // TILE_KEYCARD            // TILE_PLAYER
    TILE_HEAVY_BLASTER              = 1 << 17, // TILE_PUZZLE_ITEM_1      // TILE_ENEMY
    TILE_LIGHTSABER                 = 1 << 18, // TILE_PUZZLE_ITEM_2      // TILE_FRIENDLY
    TILE_THE_FORCE                  = 1 << 19, // TILE_PUZZLE_ITEM_SEED_END
    TILE_PLAYER                     = 1 << 16, // CHARACTER-subtype alias of TILE_LIGHT_BLASTER
    TILE_ENEMY                      = 1 << 17, // CHARACTER-subtype alias of TILE_HEAVY_BLASTER
    TILE_FRIENDLY                   = 1 << 18, // CHARACTER-subtype alias of TILE_LIGHTSABER
    TILE_LOCATOR                    = 1 << 20, // (ITEM group)
    TILE_HEALTH_PACK                = 1 << 22, // (ITEM group)
    TILE_KEYCARD                    = 1 << 16, // ITEM group: reusable key — OnDragItem does NOT
                                               // consume it when it opens a lock (alias of bit 16)
    TILE_PUZZLE_ITEM_1              = 1 << 17, // ITEM group (aliases of the WEAPON-subtype bits;
    TILE_PUZZLE_ITEM_2              = 1 << 18, //  names from DesktopAdventures tile.h — the map
    TILE_PUZZLE_ITEM_SEED_END       = 1 << 19, //  balloon text keys off them in OnLButtonDown)
    TILE_ITEM_HARMFUL_MAYBE         = 1 << 21, // ITEM group: self-applied costs 25 health (OnDragItem)
};

// ZoneObj.type (names = DesktopAdventures OBJ_TYPE, src/include/objectinfo.h).
enum ZoneObjType
{
    OBJ_QUEST_ITEM_SPOT = 0,
    OBJ_SPAWN           = 1,
    OBJ_THE_FORCE       = 2,
    OBJ_VEHICLE_TO      = 3,
    OBJ_VEHICLE_FROM    = 4,
    OBJ_LOCATOR         = 5,
    OBJ_ITEM            = 6,
    OBJ_PUZZLE_NPC      = 7,
    OBJ_WEAPON          = 8,
    OBJ_DOOR_IN         = 9,
    OBJ_DOOR_OUT        = 10,
    OBJ_UNKNOWN         = 11,
    OBJ_LOCK            = 12,
    OBJ_TELEPORTER      = 13,
    OBJ_XWING_FROM      = 14,
    OBJ_XWING_TO        = 15,
};

// Zone.type — the zone's worldgen role (DesktopAdventures "map_flags").
enum ZoneType
{
    ZONE_TYPE_EMPTY             = 0,
    ZONE_TYPE_ENEMY_TERRITORY   = 1,
    ZONE_TYPE_FINAL_DESTINATION = 2,
    ZONE_TYPE_ITEM_FOR_ITEM     = 3,
    ZONE_TYPE_FIND_USEFUL_NPC   = 4,
    ZONE_TYPE_ITEM_TO_PASS      = 5,
    ZONE_TYPE_FROM_ANOTHER_MAP  = 6,
    ZONE_TYPE_TO_ANOTHER_MAP    = 7,
    ZONE_TYPE_INDOOR            = 8,
    ZONE_TYPE_INTRO             = 9,
    ZONE_TYPE_FINAL_ITEM        = 10,
    ZONE_TYPE_MAP_START         = 11,
    ZONE_TYPE_VICTORY_SCREEN    = 13,   // demo: zones[76] (GetVictoryZoneIndexMaybe)
    ZONE_TYPE_LOSS_SCREEN       = 14,   // demo: zones[77] (GetLossZoneMaybe)
    ZONE_TYPE_MAP_TO_ITEM_FOR_LOCK = 15,
    ZONE_TYPE_FIND_USEFUL_DROP  = 16,
    ZONE_TYPE_FIND_USEFUL_BUILDING = 17,
    ZONE_TYPE_FIND_THE_FORCE    = 18,
};

// TILE record (0x40c): 32x32 8-bit pixels + flags + name.
class Tile : public CObject
{
public:                              // +0x00 CObject vtable (0x44b190)
    unsigned char pixels[TILE_PIXEL_COUNT]; // +0x004  32x32 8-bpp
    unsigned int  flags;             // +0x404  ctor: 0
    CString       name;              // +0x408  ctor: ""

    Tile();                                              // 0x00404da0
    virtual ~Tile();                                     // 0x00404e60 (ScalarDtor 0x00404e40)
};


// A placed object / hotspot in a zone (0x10 bytes).
class ZoneObj : public CObject
{
public:                          // +0x00  CObject vtable (0x44b1a8)
    unsigned int   type;         // +0x04  ObjType category
    short          state;        // +0x08  ==1 active/placed
    short          x;            // +0x0a
    short          y;            // +0x0c
    short          arg;          // +0x0e  door/vehicle target zone id (0xffff default; DA "arg")

    ZoneObj();                                                    // 0x00404ed0  default
    ZoneObj(unsigned int type, unsigned short x, unsigned short y);// 0x00404f60  spawn(type,x,y)
    virtual ~ZoneObj();                                           // 0x00405100
    void Read(CFile *pFile);                                      // 0x00404fe0  deserialize
};

// The 18x18 map zone (0x848 bytes; ctor 0x405150, vtable 0x44b1c0).
class Zone : public CObject
{
public:                              // +0x00 = CObject vtable
    int            type;             // +0x04  flags/areaType dword (== 8 => special/indoor)
    int            activatedFlag;    // +0x08
    short          width;            // +0x0c  (ZONE_WIDTH)
    short          height;           // +0x0e  (ZONE_HEIGHT)
    short          tiles[ZONE_CELL_COUNT];// +0x10  flat grid (0x798 bytes, ends +0x7a8)
    CObArray       objects;          // +0x7a8
    CObArray       iactScripts;      // +0x7bc
    CObArray       entities;         // +0x7d0
    // IZAX carries two per-zone item pools the worldgen quest builder draws from: the items this
    // zone can OFFER for the two parallel quest branches. WorldgenPickItemFromZone(sel=0) picks
    // from providedItemsA (itemA branch), sel=1 from providedItemsB (itemB branch);
    // ZoneHasIzxItemMaybe / WorldgenPlaceItemForLockChainMaybe test membership. (CWordArray proven
    // by ReadZaux's CWordArray::SetAtGrow(ushort); WORD elements, layout == CDWordArray.)
    CWordArray     providedItemsA;   // +0x7e4  IZAX item list 1 (quest branch A pool)
    CWordArray     providedItemsB;   // +0x7f8  IZAX item list 2 (quest branch B pool)
    CWordArray     genCandidateA;    // +0x80c  IZX2 list (ReadZax2: CWordArray::SetAtGrow(ushort))
    CWordArray     genCandidateB;    // +0x820  IZX3 list (ReadZax3)
    int            tempVar;          // +0x834
    int            randVar;          // +0x838
    int            doorReturnX;      // +0x83c  door return pos: where the player re-enters when exiting back (TransitionZoneDoor; also ReadSavedState)
    int            doorReturnY;      // +0x840
    short          globalVar;        // +0x844
    short          planet;           // +0x846  planet this zone belongs to (== World.currentPlanet)

    Zone(short w = ZONE_WIDTH, short h = ZONE_HEIGHT);       // 0x00405150
    virtual ~Zone();                                        // 0x004054d0
    unsigned short GetTile(int x, int y, int layer);        // 0x00405430  MATCH
    void           SetTile(int x, int y, int layer, short val); // 0x00405480  MATCH
    int            GetEdgeCode(int x, int y);               // 0x00405380
    ZoneObj       *FindObjectAt(int x, int y);              // 0x00405330
    void           FlagQuestObjects();                      // 0x004056d0
    int            DamageEntityAt(int x, int y, CObArray *paChars, short damage,
                                  CDeskcppDoc *pWorld, CDeskcppView *pView);       // 0x00405710
    int            HitEntityAt(int x, int y, CObArray *paChars, int timerVal,
                               CDeskcppDoc *pWorld, CDeskcppView *pView);          // 0x004059d0
    void           ReadSavedState(CFile *pFile, int bFull);              // 0x00405bd0 (Iact .obj)
    void           WriteSavedState(CFile *pFile, int bFull);             // 0x00405f30 (Iact .obj)
    // Iact .obj (src/Iact/Iact.cpp) — .dta chunk readers + the IACT interpreter:
    void           ReadIzon(CFile *pFile);                               // 0x00405ae0 (Iact .obj)
    void           ReadZaux(CFile *pFile);                               // 0x00406270 (Iact .obj)
    void           ReadZax2(CFile *pFile);                               // 0x00406410 (Iact .obj)
    void           ReadZax3(CFile *pFile);                               // 0x00406490 (Iact .obj)
    void           ReadZax4(CFile *pFile);                               // 0x00406510 (Iact .obj, this unused)
#ifdef GAME_INDY
    void           ReadIzaxIndy(CFile *pFile);   // Indy IZAX (6-byte entities + one item pool)
#endif
    int            IactProbeMove(int x, int y, int dx, int dy, int a5, int bForce); // 0x00406550 (Iact .obj)
    int            IactRun(int event, int x, int y, int dx, int dy, int a5,
                           CDC *pDC, CDeskcppDoc *pWorld, CDeskcppView *pView);              // 0x00406780 (Iact .obj)
    unsigned int   IactRunCommands(int scriptIdx, CDC *pDC, CDeskcppDoc *pWorld,
                                   CDeskcppView *pView);                               // 0x004070e0 (Iact .obj)
};

// ═══ Resource ids ════════════════════════════════════════════════════════════
// Symbolic names for the resource / command / control ids the engine hardcodes as
// raw hex. Recovered from YodaDemo.exe's .rsrc (string table dumped with
// tools/reslib.py) and from each id's single use site; the numbers are the ORIGINAL
// ones and must not be renumbered — our .res is built from Yoda's own .rsrc, so the
// code depends on YodaDemo's integer ids (CLAUDE.md, tools/make_res.py).
//
// They live HERE, at the tail of an already-included header, rather than in a
// Resource.h of their own: adding one more #include FILE to Worldgen.cpp's chain
// costs a byte-exact function (measured — 211 -> 210, and it happens even when the
// new file is empty, so it is the include itself, not the macros). Same dial family
// as the afxcmn.h lesson. #define, not enum, so the token stream is untouched.

// ── String table (the game's own strings live in MFC's 0xE000 band) ──────────
// (IDS_APP_TITLE / IDS_ERR_16_COLOR_VIDEO live in Deskcpp.h — the app TU cannot see
//  this header; the CTextDialog ids likewise live in TextDialog.h.)
#define IDS_CONFIRM_NEW_WORLD    0xe001  // "...Build a New World anyway?"
#define IDS_ERR_OPEN_DATA_FILE   0xe002  // "Couldn't open data file!"
#define IDS_ERR_DTA_VERSION      0xe003  // "Wrong version DTA File!"
#define IDS_LOADING_GAME_DATA    0xe004  // "Loading Game Data..."
#define IDS_BUILDING_GAME_WORLD  0xe005  // "Building Game World..."
#define IDS_FILTER_SAVE_WORLD    0xe006  // "World Files (*.wld) | *.wld"
#define IDS_FILTER_LOAD_WORLD    0xe007  // "World Files (*.wld) | *.wld"
#define IDS_ERR_NOT_A_SAVED_WORLD 0xe008 // "This file is not a Yoda Stories Saved World!"
#define IDS_CONFIRM_REPLAY       0xe009  // "...Replay anyway?"
#define IDS_WELL_DONE            0xe00b  // "Well done, Luke! "
#define IDS_PUT_MARCUS_BACK      0xe00c
#define IDS_SOLVED               0xe00d  // "...solved! "
#define IDS_REQUIRES             0xe00e  // "requires "
#define IDS_FIND                 0xe00f  // "find "
#define IDS_HINT_A_MAP           0xe010  // "a map..."
#define IDS_HINT_SOMETHING_USEFUL 0xe011 // "something useful..."
#define IDS_HINT_THE_FORCE       0xe012  // "the Force..."
#define IDS_SPACEPORT            0xe013  // "Spaceport"
#define IDS_YOU_WON              0xe014  // "You've Won!"
#define IDS_HINT_UNKNOWN         0xe015  // "unknown..."
#define IDS_HINT_A_TOOL          0xe016  // "a tool..."
#define IDS_HINT_A_PART          0xe017  // "a part..."
#define IDS_HINT_A_VALUABLE      0xe018  // "a valuable..."
#define IDS_HINT_A_KEY_CARD      0xe019  // "a key card..."
#define IDS_OPEN_SUFFIX          0xe01a  // "...open! "
#define IDS_CONFIRM_EXIT         0xe01b  // "Leave Yoda Stories?"  (Indy overrides this one)
#define IDS_ERR_NO_STORY_SAVED   0xe01c  // "Sorry, there is no story saved to replay!"
#define IDS_ERR_OUT_OF_MEMORY    0xe01d  // "There is not enough memory to run..."
#define IDS_ERR_UNRECOVERABLE    0xe01e  // "An unrecoverable error has occured..."
#define IDS_ENEMY_MILD_TEXT      0xe01f  // "Enemy mild text."

// Artoo hint balloons, indexed by ClassifyTile()'s ArtooHint result.
#define IDS_HINT_STORAGE_DEVICE  0xe020
#define IDS_HINT_XWING           0xe021
#define IDS_HINT_ENEMY           0xe022
#define IDS_HINT_DOOR            0xe023
#define IDS_HINT_PUSH_PULL       0xe024
#define IDS_HINT_CHARACTER       0xe025
#define IDS_HINT_YODA            0xe026
#define IDS_SMALLTALK_WALKING    0xe027  // the 5 rotating "nothing special here" lines
#define IDS_SMALLTALK_FINDING    0xe028
#define IDS_SMALLTALK_USING      0xe029
#define IDS_SMALLTALK_WEAPONS    0xe02a
#define IDS_SMALLTALK_HEALTH     0xe02b
#define IDS_SMALLTALK_COUNT      5
#define IDS_HINT_DARTH_VADER     0xe02c
#define IDS_HINT_VICTORY         0xe02d
#define IDS_HINT_DEFEAT          0xe02e
#define IDS_HINT_EWOK            0xe02f
#define IDS_HINT_JAWA            0xe030
#define IDS_HINT_DROID           0xe031
#define IDS_DEFAULT_SAVE_DIR     0xe032  // "C:\\Yoda"
#define IDS_HINT_LUKE            0xe033
#define IDS_HINT_TELEPORT_ACTIVE 0xe034
#define IDS_HINT_TELEPORT_IDLE   0xe035
#define IDS_HINT_MEDICAL_DROID   0xe036
#define IDS_HINT_WEAPON          0xe038

// A few messages live in the low (non-AFX) string band.
#define IDS_CONFIRM_LOAD_WORLD   3       // "...discard the current world. Load anyway?"
#define IDS_WARN_MIDI_DISABLED   4       // "...difficulty playing music (MIDI files)..."
#define IDS_ERR_OPEN_DTA         5       // "...could not open the data file YODESK.DTA."
#define IDS_ERR_DTA_SHARING      6       // "A sharing violation occurred trying to open..."
#define IDS_ERR_DISK_FULL        7       // "The disk you tried to write to is full..."
#define IDS_ERR_CANNOT_OPEN_FILE 8       // "...unable to open the file you specified."
#define IDS_ERR_CANNOT_CREATE_FILE 9     // "...unable to create the file you specified."

// ── Menu command ids (the game's own 0x8000 band; ID_FILE_*/ID_APP_* come from afxres.h) ──
#define ID_OPTIONS_SOUND         0x8000
#define ID_OPTIONS_HIDEME        0x8001  // "Hide Me!" — minimise
#define ID_OPTIONS_PAUSE         0x8002
#define ID_OPTIONS_MUSIC         0x8004
#define ID_OPTIONS_DIFFICULTY    0x8005
#define ID_FILE_NEWWORLD         0x8008
#define ID_FILE_LOADWORLD        0x800a
#define ID_FILE_REPLAYSTORY      0x800b
#define ID_OPTIONS_GAMESPEED     0x800c
#define ID_OPTIONS_WORLDSIZE     0x800d
#define ID_FILE_STATISTICS       0x800e

// ── Dialog templates ────────────────────────────────────────────────────────
#define IDD_DIFFICULTY           0x6f
#define IDD_GAMESPEED            0xd7
#define IDD_WORLDSIZE            0xda
#define IDD_STATISTICS           0xe1

// ── Control ids ─────────────────────────────────────────────────────────────
#define IDC_INV_SCROLLBAR        0x65    // the inventory scrollbar the view creates
#define IDC_DIFFICULTY_SLIDER    0x67
#define IDC_GAMESPEED_SLIDER     0x8f
#define IDC_WORLDSIZE_SLIDER     0x90
#define IDC_STATS_HIGH_SCORE     0x95    // StatsDlg DDX fields
#define IDC_STATS_LAST_SCORE     0x96
#define IDC_STATS_COMPLETIONS    0x97
#define IDC_STATS_LAST_COUNT     0x98
#define IDC_LOAD_PROGRESS        0x3e9   // worldgen CProgressCtrl
#define IDC_BUBBLE_CLOSE         0x1389  // speech-balloon CBitmapButtons + text CEdit
#define IDC_BUBBLE_DOWN          0x138a
#define IDC_BUBBLE_UP            0x138b
#define IDC_BUBBLE_TEXT          0x138c

// ── Cursors: the eight walk-direction cursors (picked by nMoveDX/nMoveDY) ────
#define IDC_CURSOR_WEST          0x6a
#define IDC_CURSOR_EAST          0x6b
#define IDC_CURSOR_NORTH         0x6c
#define IDC_CURSOR_SOUTH         0x6d
#define IDC_CURSOR_NORTHWEST     0x71
#define IDC_CURSOR_NORTHEAST     0x72
#define IDC_CURSOR_SOUTHWEST     0x73
#define IDC_CURSOR_SOUTHEAST     0x74
#define IDC_CURSOR_CENTER        0x76    // no pending direction
#define IDC_CURSOR_WAIT          0xc2    // shown while nFrameMode == 9 (script busy)

// ── Icons: the four zone-exit arrows, dim + lit ──────────────────────────────
#define IDI_ARROW_DOWN_OFF       0xc4
#define IDI_ARROW_DOWN_ON        0xc5
#define IDI_ARROW_LEFT_OFF       0xc6
#define IDI_ARROW_LEFT_ON        0xc7
#define IDI_ARROW_RIGHT_OFF      0xc8
#define IDI_ARROW_RIGHT_ON       0xc9
#define IDI_ARROW_UP_OFF         0xca
#define IDI_ARROW_UP_ON          0xcb

// ── Timers ──────────────────────────────────────────────────────────────────
#define IDT_GAME_TICK            0x1d1d  // ::SetTimer id created in OnInitialUpdate
#define IDT_ANY                  0xabcd  // OnTimer sentinel: run regardless of timer id

// ── Fixed spaceport zones ───────────────────────────────────────────────────
// DTA zone-catalog ids (hex by nature) for the 2x2 spaceport CDeskcppDoc::Populate always
// stamps at world-grid cells 44/45/54/55 — i.e. rows 4-5, cols 4-5. Quadrant names are read
// off those cell indices; ALT_NE substitutes for the NE quadrant in two of the five variants.
#define ZONE_SPACEPORT_NW        0x5e   // cell 44
#define ZONE_SPACEPORT_NE        0x5f   // cell 45
#define ZONE_SPACEPORT_SW        0x5d   // cell 54
#define ZONE_SPACEPORT_SE        0x60   // cell 55
#define ZONE_SPACEPORT_ALT_NE    0x217  // cell 45, variants 1 and 4

#endif
