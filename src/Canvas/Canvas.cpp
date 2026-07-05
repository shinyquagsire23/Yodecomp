// Canvas — DIBSection offscreen buffer.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "Canvas.h"
#include <string.h>
#pragma intrinsic(memset, memcpy)

// MMX-support flag (set once at startup); gates the accelerated blit path.
extern int Canvas_bUnk;

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

// FUNCTION: YODA 0x00408110
// 32-byte-wide (one tile) row blit. Fast path uses MMX (movq) hand-emitted as
// bytes because VC++ 4.2's inline assembler predates MMX and rejects the mnemonics.
void Canvas::BlitFast(void* src, int flags, short height,
                      unsigned short srcStride, short destX, short destY)
{
    short canvasW, canvasH;
    GetSize(&canvasW, &canvasH);
    char* dst = (char*)pData + destX + canvasW * destY;
    void* s   = src;
    if (canvasH <= height + destY - 1)
        height = canvasH - destY;
    int rows = height;
    if (Canvas_bUnk == 0) {
        do {
            memcpy(dst, s, 32);
            s = (char*)s + srcStride;
            dst += canvasW;
            rows--;
        } while (rows != 0);
        return;
    }
    int cw     = canvasW;
    int stride = (short)srcStride;
    __asm {
        _emit 0x66      // pushaw
        _emit 0x60
        mov  ecx, rows
        mov  esi, s
        mov  edi, dst
    mmxl:
        _emit 0x0f      // movq mm0, [esi]
        _emit 0x6f
        _emit 0x06
        _emit 0x0f      // movq mm1, [esi+8]
        _emit 0x6f
        _emit 0x4e
        _emit 0x08
        _emit 0x0f      // movq mm2, [esi+0x10]
        _emit 0x6f
        _emit 0x56
        _emit 0x10
        _emit 0x0f      // movq mm3, [esi+0x18]
        _emit 0x6f
        _emit 0x5e
        _emit 0x18
        _emit 0x0f      // movq [edi], mm0
        _emit 0x7f
        _emit 0x07
        _emit 0x0f      // movq [edi+8], mm1
        _emit 0x7f
        _emit 0x4f
        _emit 0x08
        _emit 0x0f      // movq [edi+0x10], mm2
        _emit 0x7f
        _emit 0x57
        _emit 0x10
        _emit 0x0f      // movq [edi+0x18], mm3
        _emit 0x7f
        _emit 0x5f
        _emit 0x18
        mov  eax, stride
        add  esi, eax
        mov  eax, cw
        add  edi, eax
        dec  ecx
        jnz  mmxl
        _emit 0x66      // popaw
        _emit 0x61
        _emit 0x0f      // emms
        _emit 0x77
    }
}
