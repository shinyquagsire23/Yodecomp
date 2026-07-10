// microfx extension API — NOT part of the Win32/MFC surface. Harnesses and the (M2+) platform
// layer use these to reach behind the GDI handles (present a DC's DIB, dump it to disk).
// Game TUs must never include this header.
#ifndef MICROFX_H
#define MICROFX_H

#include <windows.h>

extern "C" {

// Snapshot view of the DIB currently selected into a memory DC. Pointers alias the live
// object — valid until the bitmap is deleted or another bitmap is selected.
typedef struct MFXDIB {
    int            nWidth;
    int            nHeight;
    unsigned char *pBits;     // 8bpp, top-down, pitch == nWidth
    const RGBQUAD *pPal;      // 256 entries (BGRA byte order, Win32 RGBQUAD)
} MFXDIB;

// Fill *pOut from hdc's selected DIB. Returns 1 on success, 0 if hdc has no DIB selected.
int MfxGetDCDib(HDC hdc, MFXDIB *pOut);

// Write hdc's selected DIB as an 8-bit indexed .bmp (palette included). 1 on success.
int MfxWriteDibBMP(HDC hdc, const char *pszPath);

} // extern "C"

#endif
