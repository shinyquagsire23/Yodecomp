// IactScript — the IACT-script record TU (0x418700–0x418dd0, one .obj):
//   IactScript (vtable 0x44bc68) · IactCondition (0x44bc80) · IactCommand (0x44bc98)
// A Records-TU clone: three CObject-derived record classes, Ctor/??_G/Dtor/Read each.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS  (static MFC).
#include "IactScript.h"

// Shared scratch for IACT command text (.data 0x459558). Only real reader/writer is
// IactCommand::Read — the SoundInit/GameView::Dtor "refs" are the END bound of the
// wave-handle array at 0x459458.
// IACT command text scratch buffer (.bss 0x00459558). IactCommand::Read fills it with the
// command's inline text (len is a WORD from the .dta). Size is the reservation to the next
// global (0x459e28); the exact figure is a G2 layout detail — 2048 comfortably holds any text.
char Iact_szCmdTextBuf[2048];

#ifdef GAME_INDY
// Indy renumbers the IACT condition/command opcodes vs Yoda (same engine, different enums —
// RE'd from DESKADV.EXE: runner FUN_1010_2910, executor FUN_1010_2eb6). Record sizes, arg
// offsets, the tile formula tiles[(y*18+x)*3+layer], event numbers (1=Walk 2=Bump 3=Drag
// 4/5=Enter) and every field offset are IDENTICAL — ONLY the opcode numbering differs. So we
// translate each Indy opcode to its Yoda equivalent at parse time and let the byte-matched Yoda
// interpreter run unchanged. 0xff = an opcode with no Yoda case (interpreter default: commands
// no-op, conditions pass). ⚠ A few are best-guess/imperfect (marked): the rare condition specials
// (Indy 0/8/9/0xb/0x14..0x16) and the arg-order of DrawOverlay (Indy 0x10). The high-impact,
// jump-table-confirmed ones (entry gates, walk/bump/drag, randvar, the 1<->2 tile-cmd swap, the
// SayText/ShowText moves) are exact — those fix the crash + silent NPCs + un-gated building entry.
// ⭐ v69: re-derived the COMMAND table case-for-case from the FUN_1010_2eb6 jump table (1010:2f85).
// Found an off-by-one cluster 0x0b–0x14 that was shifted vs the true behavior (see per-entry notes
// below). The load-bearing fix: Indy cmd 0x11 is RedrawTile, not SetPlayerPos — the mis-map made
// the house-door script teleport the player onto the door cell instead of repainting it, which is
// what caused the "walk back then forward to actually enter" door bug.
// ⭐ v85: conditions 0x00/0x08/0x09/0x0a/0x0b/0x0f/0x14/0x15/0x16 re-derived case-for-case from
// DESKADV's REAL condition switch — it is INLINED in the IACT runner FUN_1010_2910 (jump table
// 1010:2a28, cases 0x00-0x16; there is no separate condition function). The old 0x08/0x09
// guesses were SWAPPED, and 0x0b/0x14/0x15/0x16 all have exact Yoda twins after all.
static const unsigned char kIndyCondToYoda[0x17] = {
    /*0x00*/ 0x15,  // NO Yoda twin: passes iff click-walking (view bMouseCaptured) toward
                    //   exactly (a0,a1) (doc nWalkTarget, x32 fixed-pt) AND tile check a2/a3,
                    //   event not in {2,3,4}. Mapped always-pass — SAFE: a v85 scan of all 2825
                    //   IACT scripts in the shipped DESKTOP.DAW found ZERO uses of cond 0x00
                    //   (0x08 and 0x0b are also unused; 0x14 has 142 uses, 0x16 has 4).
    /*0x01*/ 0x04,  // Walk
    /*0x02*/ 0x02,  // BumpTile
    /*0x03*/ 0x03,  // DragItem
    /*0x04*/ 0x00,  // FirstEnter
    /*0x05*/ 0x01,  // Enter
    /*0x06*/ 0x05,  // TempVarEq (zone counter)
    /*0x07*/ 0x15,  // always-pass
    /*0x08*/ 0x11,  // gameState==-1 (game LOST; v85 — was 0x12, swapped. Yoda 0x11's CODE is
                    //   gameState==-1; its COND_GameInProgress enum name is the misleading part)
    /*0x09*/ 0x12,  // gameState== 1 (game WON; v85 — was 0x11, swapped)
    /*0x0a*/ 0x0d,  // HasItem (v85 CONFIRMED vs 1010:2a28: pass iff item IS in inventory —
                    //   the old "sense may be inverted" flag was wrong; a0==-1 -> cellItemC)
    /*0x0b*/ 0x20,  // randVar >= 0 ("a Random was rolled"; v85 — exact twin: Yoda 0x20
                    //   COND_RandVarNe with arg0 forced to -1 at remap time, see Read())
    /*0x0c*/ 0x08,  // RandVarLs
    /*0x0d*/ 0x06,  // RandVarEq
    /*0x0e*/ 0x07,  // RandVarGt
    /*0x0f*/ 0x10,  // ZoneSolved/flagB!=0 (v85 CONFIRMED positive sense — no negation; the
                    //   field Indy cmd 0x21 MarkZoneSolved sets is the same one this reads)
    /*0x10*/ 0x09,  // EnterVehicle
    /*0x11*/ 0x0b,  // EnemyDead
    /*0x12*/ 0x0c,  // AllEnemiesDead
    /*0x13*/ 0x0a,  // CheckMapTile (args a0=val,a1=x,a2=y,a3=layer — same as Yoda)
    /*0x14*/ 0x0e,  // cellItemA==a0 (v85 — was 0x1c guess; Yoda 0x0e COND_CheckEndItem is the
                    //   closest twin: passes on cellItemA==a0 OR cellItemB==a0, a superset —
                    //   Indy has no itemB clause)
    /*0x15*/ 0x0f,  // startItem==a0 (v85 — was 0x19 guess; Yoda 0x0f COND_CheckStartItem is
                    //   EXACT: Indy doc+0xc36 == our startItem, same arg slot)
    /*0x16*/ 0x13   // HealthLs (v85 — was always-pass guess; (healthHi*-100-healthLo)+0x191<a0
                    //   is Yoda 0x13's formula verbatim)
};
static const unsigned char kIndyCmdToYoda[0x24] = {
    /*0x00*/ 0x00,  // SetMapTile (identical formula + arg order)
    /*0x01*/ 0x02,  // MoveMapTile   <-- Indy 1/2 are SWAPPED vs Yoda
    /*0x02*/ 0x01,  // ClearTile     <-- (this swap is the door-entry crash)
    /*0x03*/ 0x15,  // ShowObject
    /*0x04*/ 0x16,  // HideObject
    /*0x05*/ 0x04,  // SayText        <-- text moved (silent NPCs)
    /*0x06*/ 0x09,  // WaitTicks
    /*0x07*/ 0xff,  // no-op
    /*0x08*/ 0xff,  // no-op
    /*0x09*/ 0x0d,  // SetTempVar (zone counter)
    /*0x0a*/ 0x0e,  // AddTempVar
    /*0x0b*/ 0x0a,  // PlaySound   (v69: DESKADV case 0xb = FUN_1010_e43c WaveMix/MCI + result bit 0x1)
    /*0x0c*/ 0x08,  // RenderChanges (v69: case 0xc = FUN_1018_0670 full redraw + result bit 0x80)
    /*0x0d*/ 0x0c,  // Random (-> RandVar)
    /*0x0e*/ 0x10,  // ReleaseCamera/hide player (v69: case 0xe sets doc+0xc38=1 = bHidePlayer=1)
    /*0x0f*/ 0x11,  // LockCamera/show player     (v69: case 0xf sets doc+0xc38=0 = bHidePlayer=0)
    /*0x10*/ 0x03,  // DrawOverlayTile (⚠ Indy swaps a0/a1 vs Yoda — TODO position)
    /*0x11*/ 0x06,  // RedrawTile  (v69 ⭐ THE DOOR FIX: case 0x11 = DrawZoneCell(x,y)+DrawPlayer, NOT
                    //             SetPlayerPos. Mis-mapping teleported the player onto the door cell,
                    //             bypassing the walk-into-DOOR_IN warp -> "step off + back on" quirk.)
    /*0x12*/ 0x12,  // SetPlayerPos (v69: case 0x12 writes playerX/Y<<5 + camera clamp + result bit 0x4)
    /*0x13*/ 0x07,  // RedrawTiles rect (v69: case 0x13 = FUN_1010_eade rect redraw + DrawPlayer;
                    //             v85 CONFIRMED: eade(view,a3,a2,a1,a0) loops y=a1..a3/x=a0..a2
                    //             == Yoda DrawZoneCellRect(a0..a3) + DrawPlayer, same arg order)
    /*0x14*/ 0x08,  // full-zone redraw (v69: case 0x14 = FUN_1010_eb1c whole-zone repaint; no exact
                    //             Yoda twin, RenderChanges is the closest no-arg full redraw)
    /*0x15*/ 0x14,  // FlagOnce
    /*0x16*/ 0x17,  // ShowEntity
    /*0x17*/ 0x18,  // HideEntity
    /*0x18*/ 0x19,  // ShowAllEntities
    /*0x19*/ 0x1a,  // HideAllEntities
    /*0x1a*/ 0x13,  // MoveCamera (timed)
    /*0x1b*/ 0x1f,  // WinGame/end
    /*0x1c*/ 0x05,  // ShowText        <-- text moved (silent NPCs)
    /*0x1d*/ 0x1c,  // AddItemToInv
    /*0x1e*/ 0x1b,  // SpawnItem (sets frame-mode 9 — guess)
    /*0x1f*/ 0x20,  // LoseGame/end
    /*0x20*/ 0x1d,  // RemoveItemFromInv
    /*0x21*/ 0x1e,  // MarkZoneSolved
    /*0x22*/ 0x21,  // WarpToMap
    /*0x23*/ 0xff   // no-op
};
#endif

// ============================== IactScript ==============================

// FUNCTION: YODA 0x00418700
IactScript::IactScript()
{
    doneFlag = 0;
    conditions.SetSize(0, 1);
    commands.SetSize(0, 1);
}

// FUNCTION: YODA 0x004187c0  (compiler-generated scalar-deleting destructor ??_GIactScript)

// FUNCTION: YODA 0x004187e0  [EFFECTIVE MATCH: DIFF(7) — loop-1 walker/counter ESI<->EDI 2-cycle.
//   The ORIGINAL's own two (source-identical) loops use OPPOSITE allocations (loop1 walker=ESI,
//   loop2 walker=EDI); ours emits loop2's allocation twice. Same phase-drift class as the
//   GameData loader jg/jl/jg triple. Probes inert: decl order, i=0-before-n. Dial/endgame.]
// Delete the owned condition/command objects (virtual dtor via delete), then empty both arrays.
IactScript::~IactScript()
{
    int n, i;

    n = conditions.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = conditions[i];
        if (p)
            delete p;
    }
    conditions.SetSize(0, -1);
    n = commands.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = commands[i];
        if (p)
            delete p;
    }
    commands.SetSize(0, -1);
}

// FUNCTION: YODA 0x004188d0
// Parses one IACT record: tag + size (both discarded), then count-prefixed condition and
// command lists. Each element is TRY-new'd (MFC memory-exception guard), stored, and Read.
void IactScript::Read(CFile *pFile)
{
    char  tag[5];       // [5] (house style of the record readers) — only 4 bytes are read
    int   size;
    short numConditions;
    short numCommands;
    int   i;
    int   j;

    pFile->Read(tag, 4);
    pFile->Read(&size, 4);
    pFile->Read(&numConditions, 2);
    if (numConditions > 0)
        conditions.SetSize(numConditions, 1);
    for (i = 0; i < numConditions; i++) {
        IactCondition *pCond;
        TRY {
            pCond = new IactCondition;
        } CATCH (CMemoryException, e) {
            AfxMessageBox(0xe01e);      // app string 57374 (out-of-memory)
            AfxAbort();
        } END_CATCH
        if (pCond != NULL) {
            conditions[i] = pCond;
            pCond->Read(pFile);
        }
    }
    pFile->Read(&numCommands, 2);
    if (numCommands > 0)
        commands.SetSize(numCommands, 1);
    for (j = 0; j < numCommands; j++) {
        IactCommand *pCmd;
        TRY {
            pCmd = new IactCommand;
        } CATCH (CMemoryException, e) {
            AfxMessageBox(0xe01e);      // app string 57374 (out-of-memory)
            AfxAbort();
        } END_CATCH
        if (pCmd != NULL) {
            commands[j] = pCmd;
            pCmd->Read(pFile);
        }
    }
    doneFlag = 0;
}

// ============================== IactCondition ==============================

// FUNCTION: YODA 0x00418b10
IactCondition::IactCondition()
{
    opcode = 0;
}

// FUNCTION: YODA 0x00418b70  (compiler-generated scalar-deleting destructor ??_GIactCondition)

// FUNCTION: YODA 0x00418b90
IactCondition::~IactCondition()
{
}

// FUNCTION: YODA 0x00418be0
// File record is opcode + 6 args (14 bytes); the 6th arg is discarded.
void IactCondition::Read(CFile *pFile)
{
    short buf[7];

    pFile->Read(buf, 0xe);
    opcode  = buf[0];
#ifdef GAME_INDY
    if ((unsigned)opcode < 0x17) {   // translate Indy condition opcode -> Yoda equivalent
        // Indy cond 0x0b is "randVar >= 0" (a Random has been rolled); Yoda has no such
        // opcode, but 0x20 COND_RandVarNe with arg0=-1 is exactly equivalent (randVar
        // ctor-inits to -1). Rewrite the arg at remap time (v85, DESKADV 1010:2a28 case 0xb).
        if (opcode == 0x0b)
            buf[1] = -1;
        opcode = kIndyCondToYoda[opcode];
    }
#endif
    args[0] = buf[1];
    args[1] = buf[2];
    args[2] = buf[3];
    args[3] = buf[4];
    args[4] = buf[5];
}

// ============================== IactCommand ==============================

// FUNCTION: YODA 0x00418c30
IactCommand::IactCommand()
{
    opcode = 0;
}

// FUNCTION: YODA 0x00418cb0  (compiler-generated scalar-deleting destructor ??_GIactCommand)

// FUNCTION: YODA 0x00418cd0
IactCommand::~IactCommand()
{
}

// FUNCTION: YODA 0x00418d40
// File record is opcode + 5 args (12 bytes) + length-prefixed text (into the shared scratch,
// NUL-terminated there, then copied into the CString).
void IactCommand::Read(CFile *pFile)
{
    short buf[6];
    short len;

    pFile->Read(buf, 0xc);
    opcode  = buf[0];
#ifdef GAME_INDY
    if ((unsigned)opcode < 0x24)     // translate Indy command opcode -> Yoda equivalent
        opcode = kIndyCmdToYoda[opcode];
#endif
    args[0] = buf[1];
    args[1] = buf[2];
    args[2] = buf[3];
    args[3] = buf[4];
    args[4] = buf[5];
    pFile->Read(&len, 2);
    if (len > 0) {
        pFile->Read(Iact_szCmdTextBuf, len);
        Iact_szCmdTextBuf[len] = 0;
        text = Iact_szCmdTextBuf;
    }
}
