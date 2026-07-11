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

// Present-on-screen-write hook: after any BitBlt whose destination is hdcScreen, gdi calls
// pfn. The pump registers its presenter here so game code that animates the screen inside a
// single handler (clock() busy-wait loops) is shown per frame, matching Win32's immediate
// screen-DC visibility. Pass (0, 0) to unregister.
void MfxSetScreenWriteHook(HDC hdcScreen, void (*pfn)(void));

// Overlay hook: fired BEFORE the present hook on every screen-DC write — the window layer
// re-composites visible child controls over the view (they're not separate surfaces here).
void MfxSetScreenOverlayHook(HDC hdcScreen, void (*pfn)(void));

// Batch many primitives into one hook fire (control painting). Hold/Release nest;
// Release at depth 0 fires the hooks for hdc.
void MfxTouchHold(void);
void MfxTouchRelease(HDC hdc);

// ── M4 resources: decoded image view of an HICON/HCURSOR/resource-bitmap handle ─────────────
// (res/mfxres.cpp decodes the .res blob into MfxImg objects; gdi/mfxgdi.cpp draws them.)
typedef struct MFXIMG {
    int                  nWidth;
    int                  nHeight;
    int                  xHot, yHot;      // cursors; 0 for icons/bitmaps
    int                  nSysCursor;      // != 0: a system cursor (IDC_* id), no pixels
    const unsigned char *pIdx;            // color-table indices, top-down, pitch == nWidth
    const unsigned char *pMask;           // 1 = transparent (0 for resource bitmaps)
    const RGBQUAD       *pPal;            // the image's OWN color table
} MFXIMG;

// Decode handle → image view. Returns 1 if the handle is a res/ image object.
int MfxGetImage(const void *hImg, MFXIMG *pOut);

// Draw an image into a DC with per-pixel color mapping into the DC palette (mask honored).
void MfxDrawImage(HDC hdc, int x, int y, const MFXIMG *pImg);

} // extern "C"

#endif
