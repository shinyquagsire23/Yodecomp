// Dta — chunk parsers. Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
// NOTE: MSVC 4.2 register allocation is TU-context-dependent. ParseZaux/ParseZax2/ParseZax3
// have IDENTICAL source but the ORIGINAL binary allocated registers differently for each
// (ESI/EDI/EBP roles rotated), based on their position in the full .obj. In this partial TU
// only the first function reproduces original 0x423190's allocation, so only ParseZax3
// byte-matches; the others are WIP until the full TU is reconstructed (or a permuter is used).
#include "Dta.h"

// FUNCTION: YODA 0x00423190
int GameDoc::ParseZax3(CFile *pFile)
{
    short count = auxCount;
    int   i = 0;
    int   off = 0;
    if (count > 0) {
        char buf[8];
        do {
            pFile->Read(buf, 8);
            void *rec = *(void **)((char *)auxArray + off);
            if (rec == 0)
                return 0;
            ((ZoneAux *)rec)->LoadZax3(pFile);
            off += 4;
            i++;
        } while (i < count);
    }
    return 1;
}

// FUNCTION: YODA 0x00423110  [WIP: identical source, but original uses a different register
// allocation (TU-context) than this partial TU produces. Needs full TU or permuter.]
int GameDoc::ParseZaux(CFile *pFile)
{
    short count = auxCount;
    int   i = 0;
    int   off = 0;
    if (count > 0) {
        char buf[8];
        do {
            pFile->Read(buf, 8);
            void *rec = *(void **)((char *)auxArray + off);
            if (rec == 0)
                return 0;
            ((ZoneAux *)rec)->LoadZaux(pFile);
            off += 4;
            i++;
        } while (i < count);
    }
    return 1;
}

// FUNCTION: YODA 0x00423210  [WIP: see ParseZaux note.]
int GameDoc::ParseZax2(CFile *pFile)
{
    short count = auxCount;
    int   i = 0;
    int   off = 0;
    if (count > 0) {
        char buf[8];
        do {
            pFile->Read(buf, 8);
            void *rec = *(void **)((char *)auxArray + off);
            if (rec == 0)
                return 0;
            ((ZoneAux *)rec)->LoadZax2(pFile);
            off += 4;
            i++;
        } while (i < count);
    }
    return 1;
}
