// Canvas — DIBSection offscreen buffer.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "Canvas.h"
#include <string.h>
#pragma intrinsic(memset)

// FUNCTION: YODA 0x00407f50
void* Canvas::GetData()
{
    return pData;
}

// FUNCTION: YODA 0x00407f60
void Canvas::GetSize(short* outWidth, short* outHeight)
{
    if (hDib != 0) {
        *outWidth  = (unsigned short)biHeader.biWidth;
        *outHeight = -(short)biHeader.biHeight;
    }
}

// FUNCTION: YODA 0x00407fd0
UINT Canvas::SetPalette(UINT start, UINT count, RGBQUAD* colors)
{
    if (hdc == 0)
        return 0;
    return SetDIBColorTable(hdc, start, count, colors);
}

// FUNCTION: YODA 0x00408040
void Canvas::Clear()
{
    short width, height;
    GetSize(&width, &height);
    char* p = (char*)pData;
    for (int y = 0; y < height; y++)
        memset(p + y * width, 0, width);
}

// FUNCTION: YODA 0x004080a0
void Canvas::Fill(unsigned char value)
{
    short width, height;
    GetSize(&width, &height);
    char* p = (char*)pData;
    for (int y = 0; y < height; y++)
        memset(p + y * width, value, width);  // TODO: y/width reg-alloc flip (permuter)
}

// FUNCTION: YODA 0x00408000
int Canvas::Render_Blit(CDC* dest, int destX, int destY, int width, int height, int srcX, int srcY)
{
    if (hdc == 0)
        return 0;
    return BitBlt(dest->m_hDC, destX, destY, width, height, hdc, srcX, srcY, SRCCOPY);
}
