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
