// IactScriptClasses — the three IACT script record classes from the Iact-script record TU
// (0x418700–0x418dd0, src/IactScript/IactScript.cpp): IactScript, IactCondition, IactCommand.
// A Records-TU clone (CObject-derived, Ctor/??_G/Dtor/Read each). Shared by every TU that
// touches zone scripts (Zone.iactScripts elements, the Iact interpreter, ...).
// Layouts are byte-match-proven — do NOT edit offsets without re-verifying IactScript.cpp.
#ifndef IACTSCRIPTCLASSES_H
#define IACTSCRIPTCLASSES_H
#include <afxwin.h>
#include <afxcoll.h>

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

#endif
