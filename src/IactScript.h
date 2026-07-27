// IactScriptClasses — the three IACT script record classes from the Iact-script record TU
// (0x418700–0x418dd0, src/IactScript/IactScript.cpp): IactScript, IactCondition, IactCommand.
// A Records-TU clone (CObject-derived, Ctor/??_G/Dtor/Read each). Shared by every TU that
// touches zone scripts (Zone.iactScripts elements, the Iact interpreter, ...).
// Layouts are byte-match-proven — do NOT edit offsets without re-verifying IactScript.cpp.
#ifndef IACTSCRIPTCLASSES_H
#define IACTSCRIPTCLASSES_H
#include <afxwin.h>
#include <afxcoll.h>

// Condition opcodes (Zone::IactRun switch; names per ~/workspace/DesktopAdventures/scrdoc.txt).
enum IactCondOp
{
    COND_FirstEnter,        // 0x00
    COND_Enter,             // 0x01
    COND_BumpTile,          // 0x02
    COND_DragItem,          // 0x03
    COND_Walk,              // 0x04
    COND_TempVarEq,         // 0x05
    COND_RandVarEq,         // 0x06
    COND_RandVarGt,         // 0x07
    COND_RandVarLs,         // 0x08
    COND_EnterVehicle,      // 0x09
    COND_CheckMapTile,      // 0x0a
    COND_EnemyDead,         // 0x0b
    COND_AllEnemiesDead,    // 0x0c
    COND_HasItem,           // 0x0d
    COND_CheckEndItem,      // 0x0e
    COND_CheckStartItem,    // 0x0f
    COND_ZoneSolved,        // 0x10
    COND_GameInProgress,    // 0x11
    COND_GameCompleted,     // 0x12
    COND_HealthLs,          // 0x13
    COND_HealthGt,          // 0x14
    COND_Unk15,             // 0x15 (unused; falls to default)
    COND_CheckCellItem,     // 0x16
    COND_DragWrongItem,     // 0x17
    COND_PlayerAtPos,       // 0x18
    COND_GlobalVarEq,       // 0x19
    COND_GlobalVarLs,       // 0x1a
    COND_GlobalVarGt,       // 0x1b
    COND_ExperienceEq,      // 0x1c
    COND_QuestSpotPresent,  // 0x1d
    COND_CheckCellItems,    // 0x1e
    COND_TempVarNe,         // 0x1f
    COND_RandVarNe,         // 0x20
    COND_GlobalVarNe,       // 0x21
    COND_CheckMapTileVar,   // 0x22 (same body as CheckMapTile)
    COND_ExperienceGt       // 0x23
};

// Command opcodes (Zone::IactRunCommands switch).
enum IactCmdOp
{
    CMD_SetMapTile,         // 0x00
    CMD_ClearTile,          // 0x01
    CMD_MoveMapTile,        // 0x02
    CMD_DrawOverlayTile,    // 0x03
    CMD_SayText,            // 0x04
    CMD_ShowText,           // 0x05
    CMD_RedrawTile,         // 0x06
    CMD_RedrawTiles,        // 0x07
    CMD_RenderChanges,      // 0x08
    CMD_WaitTicks,          // 0x09
    CMD_PlaySound,          // 0x0a
    CMD_TransitionIn,       // 0x0b
    CMD_Random,             // 0x0c
    CMD_SetTempVar,         // 0x0d
    CMD_AddTempVar,         // 0x0e
    CMD_SetMapTileVar,      // 0x0f (same body as SetMapTile)
    CMD_ReleaseCamera,      // 0x10
    CMD_LockCamera,         // 0x11
    CMD_SetPlayerPos,       // 0x12
    CMD_MoveCamera,         // 0x13
    CMD_FlagOnce,           // 0x14
    CMD_ShowObject,         // 0x15
    CMD_HideObject,         // 0x16
    CMD_ShowEntity,         // 0x17
    CMD_HideEntity,         // 0x18
    CMD_ShowAllEntities,    // 0x19
    CMD_HideAllEntities,    // 0x1a
    CMD_SpawnItem,          // 0x1b
    CMD_AddItemToInv,       // 0x1c
    CMD_RemoveItemFromInv,  // 0x1d
    CMD_MarkZoneSolved,     // 0x1e
    CMD_WinGame,            // 0x1f
    CMD_LoseGame,           // 0x20
    CMD_WarpToMap,          // 0x21
    CMD_SetGlobalVar,       // 0x22
    CMD_AddGlobalVar,       // 0x23
    CMD_SetRandVar,         // 0x24
    CMD_AddHealth           // 0x25
};

// One trigger condition of an IACT script (0x1c). File record = opcode + 6 short args (14 B);
// only 5 args are kept in memory. Opcodes documented in ~/workspace/DesktopAdventures/scrdoc.txt.
class IactCondition : public CObject
{
public:                          // +0x00 CObject vtable (0x44bc80)
    int opcode;                  // +0x04  pre-script condition (BumpTile/HasItem/HealthLs/...)
    int args[5];                 // +0x08

    IactCondition();                                     // 0x00418b10
    virtual ~IactCondition();                            // 0x00418b90 (ScalarDtor 0x00418b70)
    void Read(CFile *pFile);                             // 0x00418be0
};

// One action command (0x20). File record = opcode + 5 short args + length-prefixed text.
class IactCommand : public CObject
{
public:                          // +0x00 CObject vtable (0x44bc98)
    int     opcode;              // +0x04
    int     args[5];             // +0x08
    CString text;                // +0x1c  dialogue/hint text (via the shared scratch buffer)

    IactCommand();                                       // 0x00418c30
    virtual ~IactCommand();                              // 0x00418cd0 (ScalarDtor 0x00418cb0)
    void Read(CFile *pFile);                             // 0x00418d40
};

// A zone's IACT script (0x30): trigger conditions + commands (elements of Zone.iactScripts).
class IactScript : public CObject
{
public:                          // +0x00 CObject vtable (0x44bc68)
    CObArray conditions;         // +0x04  IactCondition*
    CObArray commands;           // +0x18  IactCommand*
    int      doneFlag;           // +0x2c  set once a run-once script has fired

    IactScript();                                        // 0x00418700
    virtual ~IactScript();                               // 0x004187e0 (ScalarDtor 0x004187c0)
    void Read(CFile *pFile);                             // 0x004188d0
};

// Resource id used by this TU (see the "Resource ids" block in GameObjectClasses.h,
// which this TU does not include).
#define IDS_ERR_UNRECOVERABLE    0xe01e  // "An unrecoverable error has occured..."

// ═══ IACT dirty-flags ═══════════════════════════════════════════════════════════════════
// Zone::IactRunCommands (and Zone::IactRun, which ORs its scripts' results together) returns
// this mask; callers in Iact.cpp / DeskcppView.cpp test it to decide what to redraw and
// whether the zone changed underneath them. ⚠ the view truncates it to `unsigned short`.
#define IACT_SOUND          0x001   // a sound was started
#define IACT_TEXT           0x002   // SayText/ShowText put up a balloon
#define IACT_CAMERA         0x004   // camera moved/locked/released
#define IACT_SPAWN          0x008   // an entity was spawned
#define IACT_OBJECTS        0x010   // zone objects changed
#define IACT_TILES          0x020   // map tiles changed
#define IACT_ENTITIES       0x040   // entity visibility changed
#define IACT_FULL_REDRAW    0x080   // repaint everything
#define IACT_PLAYER         0x100   // player moved/changed
#define IACT_GAME_OVER      0x200   // win/lose fired
#define IACT_INVENTORY      0x400   // inventory changed
#define IACT_ZONE_WARP      0x800   // the zone itself was swapped out
// The zone is gone from under the caller: nothing else in the mask can be trusted.
#define IACT_ZONE_INVALID   (IACT_ZONE_WARP | IACT_SPAWN)   // 0x808

#endif
