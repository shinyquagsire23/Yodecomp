// WorldDoc — the doc-class main source file (TU at 0x419ed0–0x41bee0): DYNCREATE +
// message map, the World (CDeskcppDoc) ctor/dtor pair, OnNewDocument/OnOpenDocument,
// and a handful of gameplay methods the dev kept in this file.
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "DeskcppDoc.h"
#include <string.h>

extern "C" int rand(void);
// The 256-entry master color table (RGBQUAD[256], .data 0x00456230), extracted from the
// original binary. WorldDoc points pSysColorTable at it; palette cycling rotates entries.
extern "C" unsigned char YodaMasterPalette[1024] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,255,255,139,0,195,207,75,0,
    139,163,27,0,87,119,0,0,139,163,27,0,195,207,75,0,
    251,251,251,0,235,231,231,0,219,211,211,0,203,195,195,0,
    187,179,179,0,171,163,163,0,155,143,143,0,139,127,127,0,
    123,111,111,0,103,91,91,0,87,75,75,0,71,59,59,0,
    51,43,43,0,35,27,27,0,19,15,15,0,0,0,0,0,
    0,199,67,0,0,183,67,0,0,171,63,0,0,159,63,0,
    0,147,63,0,0,135,59,0,0,123,55,0,0,111,51,0,
    0,99,51,0,0,83,43,0,0,71,39,0,0,59,35,0,
    0,47,27,0,0,35,19,0,0,23,15,0,0,11,7,0,
    75,123,187,0,67,115,179,0,67,107,171,0,59,99,163,0,
    59,99,155,0,51,91,147,0,51,91,139,0,43,83,131,0,
    43,75,115,0,35,75,107,0,35,67,95,0,27,59,83,0,
    27,55,71,0,27,51,67,0,19,43,59,0,11,35,43,0,
    215,255,255,0,187,239,239,0,163,223,223,0,139,207,207,0,
    119,195,195,0,99,179,179,0,83,163,163,0,67,147,147,0,
    51,135,135,0,39,119,119,0,27,103,103,0,19,91,91,0,
    11,75,75,0,7,59,59,0,0,43,43,0,0,31,31,0,
    219,235,251,0,211,227,251,0,195,219,251,0,187,211,251,0,
    179,203,251,0,163,195,251,0,155,187,251,0,143,183,251,0,
    131,179,247,0,115,167,251,0,99,155,251,0,91,147,243,0,
    91,139,235,0,83,139,219,0,83,131,211,0,75,123,203,0,
    155,199,255,0,143,183,247,0,135,179,239,0,127,167,243,0,
    115,159,239,0,83,131,207,0,59,107,179,0,47,91,163,0,
    35,79,147,0,27,67,131,0,19,59,119,0,11,47,103,0,
    7,39,87,0,0,27,71,0,0,19,55,0,0,15,43,0,
    251,251,231,0,243,243,211,0,235,231,199,0,227,223,183,0,
    219,215,167,0,211,207,151,0,203,199,139,0,195,187,127,0,
    187,179,115,0,175,167,99,0,155,147,71,0,135,123,51,0,
    111,103,31,0,91,83,15,0,71,67,0,0,55,51,0,0,
    255,247,247,0,239,223,223,0,223,199,199,0,207,179,179,0,
    191,159,159,0,179,139,139,0,163,123,123,0,147,107,107,0,
    131,87,87,0,115,75,75,0,103,59,59,0,87,47,47,0,
    71,39,39,0,55,27,27,0,39,19,19,0,27,11,11,0,
    247,179,55,0,231,147,7,0,251,83,11,0,251,0,0,0,
    203,0,0,0,159,0,0,0,111,0,0,0,67,0,0,0,
    191,187,251,0,143,139,251,0,95,91,251,0,147,187,255,0,
    95,151,247,0,59,123,239,0,35,99,195,0,19,83,179,0,
    0,0,255,0,0,0,239,0,0,0,227,0,0,0,211,0,
    0,0,195,0,0,0,183,0,0,0,167,0,0,0,155,0,
    0,0,139,0,0,0,127,0,0,0,111,0,0,0,99,0,
    0,0,83,0,0,0,71,0,0,0,55,0,0,0,43,0,
    0,255,255,0,0,227,247,0,0,207,243,0,0,183,239,0,
    0,163,235,0,0,139,231,0,0,119,223,0,0,99,219,0,
    0,79,215,0,0,63,211,0,0,47,207,0,151,255,255,0,
    131,223,239,0,115,195,223,0,95,167,207,0,83,139,195,0,
    43,43,0,0,35,35,0,0,27,27,0,0,19,19,0,0,
    255,11,0,0,255,0,75,0,255,0,163,0,255,0,255,0,
    0,255,0,0,0,75,0,0,255,255,0,0,255,51,47,0,
    0,0,255,0,0,31,151,0,223,0,255,0,115,0,119,0,
    107,123,195,0,87,87,171,0,87,71,147,0,83,55,127,0,
    79,39,103,0,71,27,79,0,59,19,59,0,39,119,119,0,
    35,115,115,0,31,111,111,0,27,107,107,0,27,103,103,0,
    27,107,107,0,31,111,111,0,35,115,115,0,39,119,119,0,
    255,255,239,0,247,247,219,0,243,239,203,0,239,235,187,0,
    243,239,203,0,231,147,7,0,231,151,15,0,235,159,23,0,
    239,163,35,0,243,171,43,0,247,179,55,0,239,167,39,0,
    235,159,27,0,231,151,15,0,11,203,251,0,11,163,251,0,
    11,115,251,0,11,75,251,0,11,35,251,0,11,115,251,0,
    0,19,147,0,0,11,211,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,0,
};

#ifdef GAME_INDY
// Indiana Jones' Desktop Adventures palette (256 BGRX entries), from DESKADV.EXE
// (via the DesktopAdventures reference indy_palette). Indy uses a distinct palette;
// like Yoda it IS cycled at runtime (see bPaletteAnimEnabled below + CyclePalette).
extern "C" unsigned char IndyMasterPalette[1024] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc1, 0xcc, 0xd9, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xd7, 0x00,
    0x00, 0x00, 0xb3, 0x00, 0x00, 0x00, 0x8b, 0x00, 0x00, 0x00, 0x67, 0x00, 0x00, 0x00, 0x43, 0x00,
    0xfb, 0xfb, 0xfb, 0x00, 0xe3, 0xe3, 0xe3, 0x00, 0xd3, 0xd3, 0xd3, 0x00, 0xc3, 0xc3, 0xc3, 0x00,
    0xb3, 0xb3, 0xb3, 0x00, 0xab, 0xab, 0xab, 0x00, 0x9b, 0x9b, 0x9b, 0x00, 0x8b, 0x8b, 0x8b, 0x00,
    0x7b, 0x7b, 0x7b, 0x00, 0x73, 0x73, 0x73, 0x00, 0x63, 0x63, 0x63, 0x00, 0x53, 0x53, 0x53, 0x00,
    0x4b, 0x4b, 0x4b, 0x00, 0x3b, 0x3b, 0x3b, 0x00, 0x2b, 0x2b, 0x2b, 0x00, 0x23, 0x23, 0x23, 0x00,
    0x00, 0xc7, 0x43, 0x00, 0x00, 0xb7, 0x3f, 0x00, 0x00, 0xab, 0x3f, 0x00, 0x00, 0x9f, 0x3b, 0x00,
    0x00, 0x93, 0x37, 0x00, 0x00, 0x87, 0x33, 0x00, 0x00, 0x7b, 0x33, 0x00, 0x00, 0x6f, 0x2f, 0x00,
    0x00, 0x63, 0x2b, 0x00, 0x00, 0x53, 0x23, 0x00, 0x00, 0x47, 0x1f, 0x00, 0x00, 0x37, 0x17, 0x00,
    0x00, 0x27, 0x0f, 0x00, 0x00, 0x1b, 0x0b, 0x00, 0x00, 0x0b, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3b, 0xfb, 0x7b, 0x00, 0x6b, 0x7b, 0xc3, 0x00, 0x5b, 0x53, 0xab, 0x00, 0x53, 0x43, 0x93, 0x00,
    0x53, 0x2b, 0x7b, 0x00, 0x4b, 0x1b, 0x63, 0x00, 0x3b, 0x13, 0x3b, 0x00, 0xab, 0xd7, 0xff, 0x00,
    0x8f, 0xc3, 0xf3, 0x00, 0x73, 0xb3, 0xe7, 0x00, 0x5b, 0xa3, 0xdb, 0x00, 0x43, 0x97, 0xcf, 0x00,
    0x2f, 0x8b, 0xc3, 0x00, 0x1b, 0x7f, 0xb7, 0x00, 0x0b, 0x73, 0xaf, 0x00, 0x00, 0x6b, 0xa3, 0x00,
    0xeb, 0xff, 0xff, 0x00, 0xd7, 0xf3, 0xf3, 0x00, 0xc7, 0xe7, 0xe7, 0x00, 0xb7, 0xdb, 0xdb, 0x00,
    0xa3, 0xcf, 0xcf, 0x00, 0x97, 0xc3, 0xc3, 0x00, 0x7f, 0xb3, 0xb3, 0x00, 0x63, 0xa3, 0xa3, 0x00,
    0x4f, 0x93, 0x93, 0x00, 0x3b, 0x83, 0x83, 0x00, 0x2b, 0x73, 0x73, 0x00, 0x1b, 0x5f, 0x5f, 0x00,
    0x0f, 0x4f, 0x4f, 0x00, 0x07, 0x3f, 0x3f, 0x00, 0x00, 0x2f, 0x2f, 0x00, 0x00, 0x1f, 0x1f, 0x00,
    0x5b, 0xfb, 0xd3, 0x00, 0x43, 0xfb, 0xc3, 0x00, 0x23, 0xfb, 0xb3, 0x00, 0x00, 0xfb, 0xa3, 0x00,
    0x00, 0xe3, 0x93, 0x00, 0x00, 0xcb, 0x83, 0x00, 0x00, 0xb3, 0x73, 0x00, 0x00, 0x9b, 0x63, 0x00,
    0x00, 0x5b, 0x8b, 0x00, 0x00, 0x4f, 0x77, 0x00, 0x00, 0x43, 0x67, 0x00, 0x00, 0x37, 0x57, 0x00,
    0x00, 0x2f, 0x47, 0x00, 0x00, 0x23, 0x37, 0x00, 0x00, 0x17, 0x27, 0x00, 0x00, 0x0f, 0x17, 0x00,
    0x00, 0xfb, 0x4f, 0x00, 0x00, 0xef, 0x4b, 0x00, 0x00, 0xdf, 0x47, 0x00, 0x00, 0xd3, 0x47, 0x00,
    0x00, 0x9f, 0x67, 0x00, 0x00, 0x7f, 0x5b, 0x00, 0x00, 0x63, 0x43, 0x00, 0x00, 0x47, 0x27, 0x00,
    0x00, 0x2b, 0x1b, 0x00, 0x23, 0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x37, 0xdb, 0x00, 0x77, 0x2b, 0xb3, 0x00,
    0xfb, 0xfb, 0xdb, 0x00, 0xfb, 0xfb, 0xbb, 0x00, 0xfb, 0xfb, 0x9b, 0x00, 0xfb, 0xfb, 0x7b, 0x00,
    0xfb, 0xfb, 0x5b, 0x00, 0xfb, 0xfb, 0x43, 0x00, 0xfb, 0xfb, 0x23, 0x00, 0xfb, 0xfb, 0x00, 0x00,
    0xe3, 0xe3, 0x00, 0x00, 0xcb, 0xcb, 0x00, 0x00, 0xb3, 0xb3, 0x00, 0x00, 0x9b, 0x9b, 0x00, 0x00,
    0x83, 0x83, 0x00, 0x00, 0x73, 0x73, 0x00, 0x00, 0x5b, 0x5b, 0x00, 0x00, 0x43, 0x43, 0x00, 0x00,
    0xff, 0xbf, 0x47, 0x00, 0xf7, 0xaf, 0x33, 0x00, 0xef, 0xa3, 0x1f, 0x00, 0xe7, 0x97, 0x0f, 0x00,
    0xe3, 0x8b, 0x00, 0x00, 0xcb, 0x7b, 0x00, 0x00, 0xb3, 0x6b, 0x00, 0x00, 0x9b, 0x5b, 0x00, 0x00,
    0x7b, 0x47, 0x00, 0x00, 0x5f, 0x37, 0x00, 0x00, 0x43, 0x27, 0x00, 0x00, 0x27, 0x17, 0x00, 0x00,
    0xfb, 0x63, 0x5b, 0x00, 0xfb, 0x43, 0x43, 0x00, 0xfb, 0x23, 0x23, 0x00, 0xfb, 0x00, 0x00, 0x00,
    0xfb, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00, 0xc3, 0x00, 0x00, 0x00, 0xab, 0x00, 0x00, 0x00,
    0x8b, 0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00,
    0xbf, 0xbb, 0xfb, 0x00, 0xaf, 0xab, 0xf7, 0x00, 0xa3, 0x9b, 0xf3, 0x00, 0x97, 0x8f, 0xef, 0x00,
    0x87, 0x7f, 0xeb, 0x00, 0x7f, 0x73, 0xe7, 0x00, 0x6b, 0x5b, 0xdf, 0x00, 0x47, 0x3b, 0xcb, 0x00,
    0xf7, 0xb3, 0x43, 0x00, 0xf7, 0xbb, 0x4f, 0x00, 0xf7, 0xc7, 0x5b, 0x00, 0xf7, 0xcf, 0x6b, 0x00,
    0xf7, 0xd7, 0x77, 0x00, 0xf7, 0xdf, 0x83, 0x00, 0xf7, 0xe7, 0x93, 0x00, 0xf7, 0xcf, 0x6b, 0x00,
    0x00, 0x43, 0xcb, 0x00, 0x00, 0x33, 0xbb, 0x00, 0x00, 0x23, 0xa3, 0x00, 0x00, 0x1b, 0x93, 0x00,
    0x00, 0x0b, 0x7b, 0x00, 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x43, 0x00,
    0x00, 0xff, 0xff, 0x00, 0x00, 0xe3, 0xf7, 0x00, 0x00, 0xcf, 0xf3, 0x00, 0x00, 0xb7, 0xef, 0x00,
    0x00, 0xa3, 0xeb, 0x00, 0x00, 0x8b, 0xe7, 0x00, 0x00, 0x77, 0xdf, 0x00, 0x00, 0x63, 0xdb, 0x00,
    0x00, 0x4f, 0xd7, 0x00, 0x00, 0x3f, 0xd3, 0x00, 0x00, 0x2f, 0xcf, 0x00, 0x77, 0xc7, 0xe3, 0x00,
    0x6b, 0xb7, 0xdb, 0x00, 0x63, 0xa7, 0xd3, 0x00, 0x5b, 0x97, 0xcb, 0x00, 0x53, 0x8b, 0xc3, 0x00,
    0xdb, 0xeb, 0xfb, 0x00, 0xd3, 0xe3, 0xfb, 0x00, 0xc3, 0xdb, 0xfb, 0x00, 0xbb, 0xd3, 0xfb, 0x00,
    0xb3, 0xcb, 0xfb, 0x00, 0xa3, 0xc3, 0xfb, 0x00, 0x9b, 0xbb, 0xfb, 0x00, 0x8f, 0xb7, 0xfb, 0x00,
    0x83, 0xb3, 0xfb, 0x00, 0x73, 0xa3, 0xfb, 0x00, 0x63, 0x9b, 0xfb, 0x00, 0x5b, 0x93, 0xf3, 0x00,
    0x5b, 0x8b, 0xeb, 0x00, 0x53, 0x8b, 0xdb, 0x00, 0x53, 0x83, 0xd3, 0x00, 0x4b, 0x7b, 0xcb, 0x00,
    0x4b, 0x7b, 0xbb, 0x00, 0x43, 0x73, 0xb3, 0x00, 0x43, 0x6b, 0xab, 0x00, 0x3b, 0x63, 0xa3, 0x00,
    0x3b, 0x63, 0x9b, 0x00, 0x33, 0x5b, 0x93, 0x00, 0x33, 0x5b, 0x8b, 0x00, 0x2b, 0x53, 0x83, 0x00,
    0x2b, 0x4b, 0x73, 0x00, 0x23, 0x4b, 0x6b, 0x00, 0x23, 0x43, 0x5b, 0x00, 0x1b, 0x3b, 0x53, 0x00,
    0x1b, 0x3b, 0x4b, 0x00, 0x1b, 0x33, 0x43, 0x00, 0x13, 0x2b, 0x3b, 0x00, 0x0b, 0x23, 0x2b, 0x00,
    0x00, 0xab, 0x6f, 0x00, 0x00, 0xa3, 0x6b, 0x00, 0x00, 0x9f, 0x67, 0x00, 0x00, 0xa3, 0x6b, 0x00,
    0x00, 0xab, 0x6f, 0x00, 0xe7, 0x93, 0x07, 0x00, 0xe7, 0x97, 0x0f, 0x00, 0xeb, 0x9f, 0x17, 0x00,
    0xef, 0xa3, 0x23, 0x00, 0xf3, 0xab, 0x2b, 0x00, 0xf7, 0xb3, 0x37, 0x00, 0xef, 0xa7, 0x27, 0x00,
    0xeb, 0x9f, 0x1b, 0x00, 0xe7, 0x97, 0x0f, 0x00, 0x0b, 0xcb, 0xfb, 0x00, 0x0b, 0xa3, 0xfb, 0x00,
    0x0b, 0x73, 0xfb, 0x00, 0x0b, 0x4b, 0xfb, 0x00, 0x0b, 0x23, 0xfb, 0x00, 0x0b, 0x73, 0xfb, 0x00,
    0x00, 0x13, 0x93, 0x00, 0x00, 0x0b, 0xd3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x99, 0xb1, 0x00,
    0x4f, 0x65, 0x7d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00,
};
#endif // GAME_INDY

CDeskcppDoc *gpWorld;                       // 0x004561dc — set by the ctor

// FUNCTION: YODA 0x00419ed0  (CreateObject)
// FUNCTION: YODA 0x00419f40  (GetRuntimeClass)
IMPLEMENT_DYNCREATE(CDeskcppDoc, CDocument)

// FUNCTION: YODA 0x00419f50  (GetMessageMap)
// The document's command/update-UI map (AFX_MSGMAP_ENTRY array @0x44c2d0, 14 entries + terminator).
// Reconstructed byte-for-byte from the binary in v45: IDs read from the array (0xe103=ID_FILE_SAVE,
// 0xe141=ID_APP_EXIT, the 0x800x are this app's menu-command IDs), handlers matched by pfn address.
// Referencing these here keeps them alive under /OPT:REF (they were dropped as unreferenced before).
BEGIN_MESSAGE_MAP(CDeskcppDoc, CDocument)
    ON_COMMAND(0x8000, OnToggleSound)
    ON_UPDATE_COMMAND_UI(0x8000, OnUpdateToggleSound)
    ON_COMMAND(0x8004, OnToggleMusic)
    ON_UPDATE_COMMAND_UI(0x8004, OnUpdateToggleMusic)
    ON_COMMAND(0x8008, OnNewWorld)
    ON_COMMAND(0xe103, OnSaveWorld)                       // ID_FILE_SAVE
    ON_COMMAND(0x800a, OnLoadWorld)
    ON_COMMAND(0x800b, OnReplayStory)
    ON_UPDATE_COMMAND_UI(0xe103, OnUpdateFileSave)        // ID_FILE_SAVE
    ON_UPDATE_COMMAND_UI(0xe141, OnUpdateAppExit)         // ID_APP_EXIT
    ON_UPDATE_COMMAND_UI(0x8001, OnUpdateHideMe)
    ON_UPDATE_COMMAND_UI(0x8008, OnUpdateNewWorld)
    ON_UPDATE_COMMAND_UI(0x800a, OnUpdateLoadWorld)
    ON_UPDATE_COMMAND_UI(0x800b, OnUpdateReplayStory)
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419f60
// dir (1=W 3=E 2=N 4=S, 0=none) of the neighbor of (x,y) holding a 0x68 gate cell.
int CDeskcppDoc::FindAdjacentGateDirMaybe(int x, int y, short *paGrid)
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
int CDeskcppDoc::ParseTilesMaybe(CFile *pFile, unsigned int nBytes)
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
unsigned int CDeskcppDoc::GetLocatorIconMaybe(int x, int y, int bAlt)
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
void CDeskcppDoc::CacheUiTilePtrsMaybe()
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
void CDeskcppDoc::DrawPlayer()
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
CDeskcppDoc::CDeskcppDoc()
{
    gpWorld = this;
    nMusicEnabled = 1;
    unk2e60 = 0;
    bWorldInvalid = 0;
    bSkipNewWorldConfirm = 0;
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
    bWeaponHitPending = 0;
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
        nFrameDelay = *(int *)((char *)pApp + 0xc4);   // TODO: name the CWinApp-derived field
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

#if defined(GAME_INDY)
    currentPlanet = -1;                   // Indy has no planets (see LoadWorld) — matches all zones
#elif !defined(YODA_FULL)
    currentPlanet = 2;                    // demo hardcode: Alaska/Hoth only (full: keep the
                                          // registry-read + planet-rotation value computed above)
#endif
    rectViewport.left = 8;
    rectViewport.top = 7;
    rectViewport.right = 0x128;
#ifndef YODA_FULL
    worldSize = 1;                        // demo hardcode: small world (full: keep the
                                          // registry-read WorldSize value, default 2)
#endif
    rectInventory.top = 6;
    rectViewport.bottom = 0x127;
    rectInventory.left = 0x133;
    rectInventory.bottom = 0xe6;
    rectInvScroll.top = 6;
    rectInventory.right = 0x1e9;
    rectInvScroll.left = 0x1f0;
    rectInvScroll.bottom = 0xe6;
    rectWeaponBox.top = 252;
    rectInvScroll.right = 0x200;
    rectWeaponBox.left = 400;
    rectWeaponBox.bottom = 284;
    rectAmmoBar.top = 0xfc;
    rectWeaponBox.right = 400+32;
#ifdef GAME_INDY
    // Indy has no ammo bar (see DrawWeaponIcon), so it centers the weapon box over the whole
    // box+ammo region — 16px left of Yoda's box (onto where Yoda's ammo bar sat) and 4px down.
    // Exact coords from DESKADV UI-rect init FUN_1010_4666: left=0x180 top=0x100 right=0x1a0
    // bottom=0x120 (vs Yoda 0x190/0xfc/0x1b0/0x11c). GAME_INDY-guarded — Yoda anchor unaffected.
    rectWeaponBox.left = 384+8;
    rectWeaponBox.top = 252; // actually 256
    rectWeaponBox.right = 384+32+8;
    rectWeaponBox.bottom = 252+32; // actually 256+32
#endif
    rectAmmoBar.left = 0x180;
    rectAmmoBar.right = 0x189;
    rectAmmoBar.bottom = 0x11c;
    rectHealthDial.bottom = 0x11c;
    rectHealthDial.left = 0x1c9;
    rectHealthDial.right = 0x1ea;
    rectHealthDial.top = 0xfb;
    rectArrowBox.left = 0x141;
    nViewTop = 0;
    nViewLeft = 0;
    nViewBottom = 0x120;
    nViewRight = 0x120;
    healthLo = 1;
    healthHi = 1;
    rectArrowBox.top = 0xf6;
    rectArrowBox.right = 0x169;
    nQueuedMoveDY = 0;
    nQueuedMoveDX = 0;
    pPlayerFrameTile = NULL;
    currentWeapon = NULL;
    bWorldReady = 0;
    bDtaLoaded = 0;
    rectArrowBox.bottom = 0x11e;
    bStateFileLoaded = 0;
    scrollDirX = 0;
    scrollDirY = 0;
    unk3368 = 0;
    unk336c = 0;
    bHidePlayer = 0;
    cameraX = 0x100;
    cameraY = 0xc0;
    unk2e34 = -1;
    playerY = 0;
    playerX = 0;
    pEquippedItem = NULL;
#ifdef GAME_INDY
    pSysColorTable = IndyMasterPalette;   // Indy has its own palette (also cycled — see below)
#else
    pSysColorTable = YodaMasterPalette;
#endif
    pPalette = new CPalette;
    pCanvas = NULL;
    currentZone = NULL;
    unk50 = 0;
    unk54 = 0;
    pPlayerChar = NULL;
    ammoTheForce = 0;
    bPaletteAnimEnabled = 0;
    palVersion = 0x300;
    palNumEntries = 0x100;
    ammoLightsaber = 0;
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
// FUNCTION: YODA 0x0041b2f0  [PHASE-DISPLACED again (v45): DIFF(6), align=0 reg_pen=8 — the same
//   esi/edi symmetric 2-cycle this function has oscillated on for versions (matched at 854fba2 and
//   after the MapZone.h de-dup; displaced here). CAUSE: v45 restored the World document message map
//   (below) — the original WorldDoc.obj was compiled WITH that map, so the map is the TRUE TU context;
//   its presence re-rotates ~World's intrinsic register choice in our partial-context build. Source is
//   proven correct (align=0 structural identity, 405/405 insns); this is the lesson-#29 reg-coloring
//   ceiling, NOT a source miss. Trade accepted: the correct map keeps 15 handlers alive under /OPT:REF
//   + byte-matches the .rdata array (see the map comment), vs this one 6-byte .text displacement.]
// World destructor: write the options back to the registry, then destroy all assets.
CDeskcppDoc::~CDeskcppDoc()
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

    n = worldgenPendingZones.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = worldgenPendingZones[i];
        if (p != NULL)
            delete p;
    }
    worldgenPendingZones.SetSize(0, -1);

    n = worldgenRefZones.GetSize();
    for (i = 0; i < n; i++) {
        CObject *p = worldgenRefZones[i];
        if (p != NULL)
            delete p;
    }
    worldgenRefZones.SetSize(0, -1);

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
BOOL CDeskcppDoc::OnOpenDocument(LPCTSTR lpszPathName)
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
BOOL CDeskcppDoc::OnNewDocument()
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
    // Enable palette cycling. BOTH games cycle: DESKADV.EXE (Indy) has a CyclePalette twin
    // (FUN_1018_8e40) with the SAME ring ranges as Yoda's, and sets this enable flag to 1 during
    // palette init (1010:506c: MOV [doc+0xc3c],1). (v59's "Indy has no cycling" was wrong — the
    // DesktopAdventures `palette_animate: if(!is_yoda) return;` is a reimplementation inaccuracy.)
    // Anchor-safe: dropping the #ifndef leaves Yoda's preprocessed tokens identical.
    bPaletteAnimEnabled = 1;
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
    bStateFileLoaded = 0;
    return TRUE;
}
