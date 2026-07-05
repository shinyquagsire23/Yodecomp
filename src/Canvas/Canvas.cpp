// Canvas — DIBSection offscreen buffer.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "Canvas.h"
#include <string.h>
#pragma intrinsic(memset)

// FUNCTION: YODA 0x00407df0
Canvas* Canvas::Init(int width, int height)
{
    biHeader.biCompression = 0;
    biHeader.biPlanes = 1;
    biHeader.biSize = 0x28;
    biHeader.biXPelsPerMeter = 0;
    biHeader.biYPelsPerMeter = 0;
    biHeader.biClrImportant = 0;
    biHeader.biHeight = height;
    biHeader.biBitCount = 8;
    if (0 < height)
        biHeader.biHeight = -height;
    biHeader.biClrUsed = 0x100;
    biHeader.biWidth = width;
    biHeader.biSizeImage = height * width;
    hdc = CreateCompatibleDC(0);
    if (hdc != 0) {
        CreatePalette();
        hDib = CreateDIBSection(hdc, (BITMAPINFO*)&biHeader, 0, &pData, 0, 0);
        if (hDib != 0) {
            hOldBitmap = SelectObject(hdc, hDib);
            return this;
        }
        DeleteDC(hdc);
        if (hPalette != 0)
            DeleteObject(hPalette);
        hdc = 0;
    }
    return this;
}

// FUNCTION: YODA 0x00407eb0
void Canvas::Free()
{
    HDC dc = hdc;
    if (dc != 0 && hOldBitmap != 0) {
        SelectObject(dc, hOldBitmap);
        DeleteObject(hDib);
        DeleteDC(hdc);
        hOldBitmap = 0;
        hDib = 0;
        hdc = 0;
    }
    else if (dc != 0 && hDib != 0 && hOldBitmap == 0) {
        DeleteObject(hDib);
        DeleteDC(hdc);
        hdc = 0;
        hDib = 0;
    }
    else if (hDib != 0 && dc == 0) {
        DeleteObject(hDib);
        hDib = 0;
    }
    if (hPalette != 0)
        DeleteObject(hPalette);
}


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


// FUNCTION: YODA 0x00407f80
int Canvas::CreatePalette()
{
    HWND     hWnd = GetActiveWindow();
    HDC      dc   = GetDC(hWnd);
    HPALETTE pal  = CreateHalftonePalette(dc);
    hPalette = pal;
    if (pal != 0) {
        RGBQUAD* pe = palette;
        GetPaletteEntries(pal, 0, 0x100, (LPPALETTEENTRY)pe);
        for (int i = 0x1a; i != 0; i--) {
            BYTE t = pe->rgbBlue;
            pe->rgbBlue = pe->rgbRed;
            pe->rgbRed = t;
            pe++;
        }
        return 1;
    }
    return 0;
}


// FUNCTION: YODA 0x00407fd0
UINT Canvas::SetPalette(UINT start, UINT count, RGBQUAD* colors)
{
    if (hdc == 0)
        return 0;
    return SetDIBColorTable(hdc, start, count, colors);
}


// FUNCTION: YODA 0x00408000
int Canvas::BitBlt(CDC* dest, int destX, int destY, int width, int height, int srcX, int srcY)
{
    if (hdc == 0)
        return 0;
    return ::BitBlt(dest->m_hDC, destX, destY, width, height, hdc, srcX, srcY, SRCCOPY);
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
