// microfx gdi/ — real GDI object layer (H4 M1, docs/phase-h4-sdl.md).
//
// The game's whole render pipeline is 8bpp DIB sections composited in memory DCs (Canvas.cpp),
// then BitBlt'd to the screen DC. This file makes those objects REAL — pure C++, no SDL
// dependency (worldgen_smoke stays headless-buildable): a DIB is pixels + a 256-entry color
// table; a memory DC is a selection slot for one DIB. Presentation (SDL window / BMP dump)
// reads the DIB back out through the microfx.h extension API. The M2 screen DC will be one of
// these DIBs too — BitBlt-to-screen lands in it, and the pump presents it per frame.
//
// Only what the game exercises: 8bpp, DIB_RGB_COLORS, SRCCOPY (every Canvas::BitBlt / OnDraw
// blit is SRCCOPY; the rop argument is otherwise ignored). Pens/brushes/fonts stay stub
// handles in mfxstubs.cpp — SelectObject passes anything that is not one of OUR objects
// straight through.

#include <windows.h>
#include <microfx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ── the objects behind the opaque handles (windows.h: HDC__ / HBITMAP__) ────────────────────
// Both carry a magic tag first: SelectObject/DeleteObject take HGDIOBJ (void*) and must
// distinguish our objects from the null/fake handles the remaining stubs hand out.

#define MFX_TAG_DIB 0x42494458   // 'XDIB'
#define MFX_TAG_DC  0x00434458   // 'XDC'

struct HBITMAP__ {               // an 8bpp DIB section
    unsigned int   nTag;
    int            nWidth;
    int            nHeight;      // stored positive; pixels are top-down
    unsigned char *pBits;        // pitch == nWidth (all game widths are 4-aligned)
    RGBQUAD        aPal[256];
};

struct HDC__ {                   // a memory DC: one DIB selection slot
    unsigned int nTag;
    HBITMAP      hDib;           // currently selected DIB (may be 0)
};

static int MfxIsDib(void *h) { return h && ((HBITMAP__ *)h)->nTag == MFX_TAG_DIB; }
static int MfxIsDc(HDC h)    { return h && h->nTag == MFX_TAG_DC; }

// ── DC lifecycle ─────────────────────────────────────────────────────────────────────────────

HDC CreateCompatibleDC(HDC)
{
    HDC h = (HDC)calloc(1, sizeof(HDC__));
    if (h) h->nTag = MFX_TAG_DC;
    return h;
}

BOOL DeleteDC(HDC hdc)
{
    if (!MfxIsDc(hdc)) return FALSE;
    free(hdc);                   // selected DIB is NOT owned (Win32 semantics: caller deletes)
    return TRUE;
}

// ── DIB sections ─────────────────────────────────────────────────────────────────────────────

HBITMAP CreateDIBSection(HDC, const BITMAPINFO *pbmi, UINT /*DIB_RGB_COLORS*/,
                         void **ppvBits, HANDLE, DWORD)
{
    long w = pbmi->bmiHeader.biWidth;
    long h = pbmi->bmiHeader.biHeight;
    if (h < 0) h = -h;           // the game always passes top-down (negative height)
    if (w <= 0 || h <= 0 || pbmi->bmiHeader.biBitCount != 8) {
        if (ppvBits) *ppvBits = 0;
        return 0;
    }
    HBITMAP hDib = (HBITMAP)calloc(1, sizeof(HBITMAP__));
    if (!hDib) { if (ppvBits) *ppvBits = 0; return 0; }
    hDib->nTag    = MFX_TAG_DIB;
    hDib->nWidth  = (int)w;
    hDib->nHeight = (int)h;
    hDib->pBits   = (unsigned char *)calloc((size_t)w * (size_t)h, 1);
    if (!hDib->pBits) { free(hDib); if (ppvBits) *ppvBits = 0; return 0; }
    // Win32 initializes the DIB color table from bmiColors (Canvas passes its palette[256],
    // which sits right after the header in the object — the cast is load-bearing there)
    DWORD nClr = pbmi->bmiHeader.biClrUsed;
    if (nClr > 256) nClr = 256;
    memcpy(hDib->aPal, pbmi->bmiColors, nClr * sizeof(RGBQUAD));
    if (ppvBits) *ppvBits = hDib->pBits;
    return hDib;
}

BOOL DeleteObject(HGDIOBJ h)
{
    if (MfxIsDib(h)) {
        HBITMAP hDib = (HBITMAP)h;
        free(hDib->pBits);
        free(hDib);
    }
    return TRUE;                 // pens/brushes/palettes are still stub handles — nothing to free
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h)
{
    if (MfxIsDc(hdc) && MfxIsDib(h)) {
        HGDIOBJ hOld = (HGDIOBJ)hdc->hDib;
        hdc->hDib = (HBITMAP)h;
        return hOld;             // 0 on the first select — Canvas's dtor handles that arm
    }
    return h;                    // non-bitmap objects: pass-through (stub behavior)
}

// ── color table ──────────────────────────────────────────────────────────────────────────────

UINT SetDIBColorTable(HDC hdc, UINT iStart, UINT cEntries, const RGBQUAD *prgbq)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || iStart >= 256) return 0;
    if (iStart + cEntries > 256) cEntries = 256 - iStart;
    memcpy(hdc->hDib->aPal + iStart, prgbq, cEntries * sizeof(RGBQUAD));
    return cEntries;
}

UINT GetDIBColorTable(HDC hdc, UINT iStart, UINT cEntries, RGBQUAD *prgbq)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || iStart >= 256) return 0;
    if (iStart + cEntries > 256) cEntries = 256 - iStart;
    memcpy(prgbq, hdc->hDib->aPal + iStart, cEntries * sizeof(RGBQUAD));
    return cEntries;
}

// ── blitting ─────────────────────────────────────────────────────────────────────────────────

BOOL BitBlt(HDC hdcDst, int x, int y, int cx, int cy, HDC hdcSrc, int sx, int sy, DWORD /*rop*/)
{
    if (!MfxIsDc(hdcDst) || !hdcDst->hDib) return FALSE;
    if (!MfxIsDc(hdcSrc) || !hdcSrc->hDib) return FALSE;
    HBITMAP__ *pDst = hdcDst->hDib;
    HBITMAP__ *pSrc = hdcSrc->hDib;

    // clip against both surfaces (the game blits partially-offscreen during scroll transitions)
    if (x < 0)  { cx += x;  sx -= x;  x = 0; }
    if (y < 0)  { cy += y;  sy -= y;  y = 0; }
    if (sx < 0) { cx += sx; x  -= sx; sx = 0; }
    if (sy < 0) { cy += sy; y  -= sy; sy = 0; }
    if (cx > pDst->nWidth  - x)  cx = pDst->nWidth  - x;
    if (cy > pDst->nHeight - y)  cy = pDst->nHeight - y;
    if (cx > pSrc->nWidth  - sx) cx = pSrc->nWidth  - sx;
    if (cy > pSrc->nHeight - sy) cy = pSrc->nHeight - sy;
    if (cx <= 0 || cy <= 0) return TRUE;

    for (int row = 0; row < cy; row++)
        memmove(pDst->pBits + (size_t)(y + row) * pDst->nWidth + x,
                pSrc->pBits + (size_t)(sy + row) * pSrc->nWidth + sx,
                (size_t)cx);
    return TRUE;
}

// ── extension API (microfx.h) — presentation reads the DIB back out ────────────────────────

extern "C" int MfxGetDCDib(HDC hdc, MFXDIB *pOut)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || !pOut) return 0;
    pOut->nWidth  = hdc->hDib->nWidth;
    pOut->nHeight = hdc->hDib->nHeight;
    pOut->pBits   = hdc->hDib->pBits;
    pOut->pPal    = hdc->hDib->aPal;
    return 1;
}

extern "C" int MfxWriteDibBMP(HDC hdc, const char *pszPath)
{
    MFXDIB dib;
    if (!MfxGetDCDib(hdc, &dib)) return 0;
    FILE *f = fopen(pszPath, "wb");
    if (!f) return 0;

    const unsigned nPalBytes = 256 * 4;
    const unsigned nOffBits  = 14 + 40 + nPalBytes;
    const unsigned nImage    = (unsigned)dib.nWidth * (unsigned)dib.nHeight; // width is 4-aligned
    unsigned char hdr[14 + 40] = { 'B', 'M' };
    unsigned nSize = nOffBits + nImage;
    memcpy(hdr + 2,  &nSize,   4);
    memcpy(hdr + 10, &nOffBits, 4);
    unsigned n40 = 40;               memcpy(hdr + 14, &n40, 4);
    int w = dib.nWidth;              memcpy(hdr + 18, &w, 4);
    int h = dib.nHeight;             memcpy(hdr + 22, &h, 4);   // positive = bottom-up
    unsigned short one = 1, bpp = 8; memcpy(hdr + 26, &one, 2); memcpy(hdr + 28, &bpp, 2);
    unsigned nClr = 256;             memcpy(hdr + 34, &nImage, 4); memcpy(hdr + 46, &nClr, 4);
    fwrite(hdr, 1, sizeof(hdr), f);
    fwrite(dib.pPal, 1, nPalBytes, f);              // RGBQUAD is already BMP palette order
    for (int row = dib.nHeight - 1; row >= 0; row--) // our bits are top-down; BMP wants bottom-up
        fwrite(dib.pBits + (size_t)row * dib.nWidth, 1, (size_t)dib.nWidth, f);
    fclose(f);
    return 1;
}
