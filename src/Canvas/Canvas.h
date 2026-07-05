// Canvas — DIBSection offscreen buffer (compile unit 0x407df0-0x4084e8).
// Non-virtual struct so `this` == the raw pointer and field offsets are exact.
#ifndef CANVAS_H
#define CANVAS_H

#include <windows.h>

// Minimal MFC CDC surface — only m_hDC (@+0x04, after the CObject vtable) is used.
struct CDC { void* _vfptr; HDC m_hDC; };

struct Canvas {
    HDC              hdc;          // +0x00  memory DC (CreateCompatibleDC)
    HBITMAP          hDib;         // +0x04  DIB section (CreateDIBSection)
    HGDIOBJ          hOldBitmap;   // +0x08  bitmap replaced by SelectObject; restored in dtor
    HPALETTE         hPalette;     // +0x0c  halftone palette
    BITMAPINFOHEADER biHeader;     // +0x10  DIB header
    RGBQUAD          palette[256]; // +0x38  color table
    void*            pData;        // +0x438 pixel bits (8bpp)

    // --- methods (see Canvas.cpp) ---
    void* GetData();                                            // 0x00407f50
    void  GetSize(short* outWidth, short* outHeight);           // 0x00407f60
    UINT  SetPalette(UINT start, UINT count, RGBQUAD* colors);  // 0x00407fd0
    void  Clear();                                              // 0x00408040
    void  Fill(unsigned char value);                           // 0x004080a0
    int   BitBlt(CDC* dest, int destX, int destY,              // 0x00408000
                      int width, int height, int srcX, int srcY);
};

#endif
