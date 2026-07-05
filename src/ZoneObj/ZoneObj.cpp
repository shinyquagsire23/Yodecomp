// ZoneObj — CObject-derived record. Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG
//           /D _WINDOWS /D _MBCS  (static MFC). Functions in .text/source order.
#include "ZoneObj.h"

// FUNCTION: YODA 0x00404ed0
ZoneObj::ZoneObj()
{
    y = 0;
    x = 0;
    state = 0;
    type = 0;
    visible = -1;
}

// FUNCTION: YODA 0x00404f40  (compiler-generated scalar-deleting destructor ??_GZoneObj)

// FUNCTION: YODA 0x00404f60
ZoneObj::ZoneObj(unsigned int t, unsigned short px, unsigned short py)
{
    type = t;
    x = px;
    y = py;
    visible = -1;
}

// FUNCTION: YODA 0x00404fe0
void ZoneObj::Read(CFile *pFile)
{
    unsigned short buf6[3];
    unsigned short buf2;
    pFile->Read(&type, 4);
    pFile->Read(buf6, 6);
    x = buf6[0];
    y = buf6[1];
    state = buf6[2];
    switch (type) {
    case 0: case 1: case 2: case 5:
        pFile->Read(&buf2, 2); state = 0; visible = buf2; break;
    case 3: case 4: case 9: case 12: case 14: case 15:
        pFile->Read(&buf2, 2); state = 1; visible = buf2; break;
    case 6: case 7: case 8:
        pFile->Read(&buf2, 2); state = 1; visible = buf2; break;
    default:
        pFile->Read(&buf2, 2); visible = 0xffff; state = 1; break;
    }
}

// FUNCTION: YODA 0x00405100
ZoneObj::~ZoneObj()
{
}
