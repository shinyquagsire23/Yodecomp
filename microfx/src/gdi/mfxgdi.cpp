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
#define MFX_TAG_PAL 0x4c415058   // 'XPAL'

struct HBITMAP__ {               // an 8bpp DIB section
    unsigned int   nTag;
    int            nWidth;
    int            nHeight;      // stored positive; pixels are top-down
    unsigned char *pBits;        // pitch == nWidth (all game widths are 4-aligned)
    RGBQUAD        aPal[256];
};

struct HPALETTE__ {              // a logical palette (M2: the game's screen palette + cycling)
    unsigned int nTag;
    UINT         nEntries;
    PALETTEENTRY aPal[256];
    HDC          hdcRealized;    // last DC realized into — AnimatePalette writes through to it
};

struct HDC__ {                   // a memory DC: one DIB selection slot + one palette slot
    unsigned int nTag;
    HBITMAP      hDib;           // currently selected DIB (may be 0)
    HPALETTE     hPal;           // currently selected palette (may be 0)
};

static int MfxIsDib(void *h) { return h && ((HBITMAP__ *)h)->nTag == MFX_TAG_DIB; }
static int MfxIsDc(HDC h)    { return h && h->nTag == MFX_TAG_DC; }
static int MfxIsPal(void *h) { return h && ((HPALETTE__ *)h)->nTag == MFX_TAG_PAL; }

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

// The drag save-under (CDeskcppView::UpdateDragCursor) is the one CreateBitmap caller:
// a 32x32 "device-dependent" bitmap it round-trips with Set/GetBitmapBits and BitBlts
// against the screen DC. Our device is the 8bpp DIB world, so a DDB is just a DIB with
// no color table of its own — same HBITMAP__ object, same SelectObject/BitBlt/DeleteObject
// paths. At 8bpp a DDB scanline is bytes==width (32 is WORD-aligned), matching our pitch.
HBITMAP CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void *lpBits)
{
    if (nWidth <= 0 || nHeight <= 0 || nPlanes != 1 || nBitCount != 8) return 0;
    HBITMAP hBmp = (HBITMAP)calloc(1, sizeof(HBITMAP__));
    if (!hBmp) return 0;
    hBmp->nTag    = MFX_TAG_DIB;
    hBmp->nWidth  = nWidth;
    hBmp->nHeight = nHeight;
    hBmp->pBits   = (unsigned char *)calloc((size_t)nWidth * (size_t)nHeight, 1);
    if (!hBmp->pBits) { free(hBmp); return 0; }
    if (lpBits) memcpy(hBmp->pBits, lpBits, (size_t)nWidth * (size_t)nHeight);
    return hBmp;
}

LONG SetBitmapBits(HBITMAP hbm, DWORD cb, const void *pvBits)
{
    if (!MfxIsDib(hbm) || !pvBits) return 0;
    size_t nMax = (size_t)hbm->nWidth * (size_t)hbm->nHeight;
    if (cb > nMax) cb = (DWORD)nMax;
    memcpy(hbm->pBits, pvBits, cb);
    return (LONG)cb;
}

LONG GetBitmapBits(HBITMAP hbm, LONG cb, LPVOID pvBits)
{
    if (!MfxIsDib(hbm) || !pvBits || cb < 0) return 0;
    size_t nMax = (size_t)hbm->nWidth * (size_t)hbm->nHeight;
    if ((size_t)cb > nMax) cb = (LONG)nMax;
    memcpy(pvBits, hbm->pBits, (size_t)cb);
    return cb;
}

BOOL DeleteObject(HGDIOBJ h)
{
    if (MfxIsDib(h)) {
        HBITMAP hDib = (HBITMAP)h;
        free(hDib->pBits);
        free(hDib);
    }
    else if (MfxIsPal(h)) {
        ((HPALETTE__ *)h)->nTag = 0;
        free(h);
    }
    return TRUE;                 // pens/brushes are still stub handles — nothing to free
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

// Present-on-screen-write hook (M2 tail). Win32 makes a BitBlt to the screen DC visible
// IMMEDIATELY; our pump presents only between handler returns. The game animates by blitting
// to the screen inside one handler with clock() busy-waits (ScrollZoneTransition 0x411180,
// StartGame's X-Wing STUP flight) — without this, every intermediate frame lands in the
// screen DIB unseen and the animation collapses to its final state. The pump registers its
// present function; BitBlt fires it after any write to the registered screen DC. Kept as a
// raw function pointer so gdi/ stays SDL-free (headless harnesses never register one).
static HDC   g_hdcScreenWrite = 0;
static void (*g_pfnScreenWrite)(void) = 0;

void MfxSetScreenWriteHook(HDC hdcScreen, void (*pfn)(void))
{
    g_hdcScreenWrite = hdcScreen;
    g_pfnScreenWrite = pfn;
}

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

    // Overlap-aware row order: ScrollZoneTransition (0x411180) scrolls by blitting the screen
    // OVER ITSELF (same DIB, overlapping src/dst). memmove covers horizontal overlap within a
    // row, but a downward self-blit (dst below src) must copy bottom-up or later source rows
    // are clobbered before they're read.
    if (pDst->pBits == pSrc->pBits && y > sy)
        for (int row = cy - 1; row >= 0; row--)
            memmove(pDst->pBits + (size_t)(y + row) * pDst->nWidth + x,
                    pSrc->pBits + (size_t)(sy + row) * pSrc->nWidth + sx,
                    (size_t)cx);
    else
        for (int row = 0; row < cy; row++)
            memmove(pDst->pBits + (size_t)(y + row) * pDst->nWidth + x,
                    pSrc->pBits + (size_t)(sy + row) * pSrc->nWidth + sx,
                    (size_t)cx);
    if (hdcDst == g_hdcScreenWrite && g_pfnScreenWrite)
        g_pfnScreenWrite();
    return TRUE;
}

// ── palettes (M2) ────────────────────────────────────────────────────────────────────────────
// The game builds ONE CPalette from CDeskcppDoc::sysPalette and realizes it into the screen DC
// on every paint (OnDraw / the CMainFrame WM_PALETTE* handlers); CyclePalette animates ring
// ranges each tick. Model: a palette is 256 PALETTEENTRYs; RealizePalette copies them into the
// DC's selected-DIB color table (the render truth); AnimatePalette writes through to the last
// DC the palette was realized into, so cycling shows up without a re-realize — Win32-faithful
// for the game's single-screen use.

static void MfxPalToDib(HPALETTE hPal, HDC hdc, UINT iStart, UINT nCount)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return;
    if (iStart >= 256) return;
    if (iStart + nCount > 256) nCount = 256 - iStart;
    RGBQUAD *pDst = hdc->hDib->aPal + iStart;
    const PALETTEENTRY *pSrc = hPal->aPal + iStart;
    for (UINT i = 0; i < nCount; i++) {
        pDst[i].rgbRed      = pSrc[i].peRed;
        pDst[i].rgbGreen    = pSrc[i].peGreen;
        pDst[i].rgbBlue     = pSrc[i].peBlue;
        pDst[i].rgbReserved = 0;
    }
}

HPALETTE CreatePalette(const LOGPALETTE *plpal)
{
    if (!plpal) return 0;
    HPALETTE h = (HPALETTE)calloc(1, sizeof(HPALETTE__));
    if (!h) return 0;
    h->nTag = MFX_TAG_PAL;
    h->nEntries = plpal->palNumEntries;
    if (h->nEntries > 256) h->nEntries = 256;
    memcpy(h->aPal, plpal->palPalEntry, h->nEntries * sizeof(PALETTEENTRY));
    return h;
}

HPALETTE CreateHalftonePalette(HDC)
{
    // 6x6x6 color cube + gray tail — close enough to Win32's; Canvas overwrites its DIB table
    // with the game palette (SetPalette) right after sampling this.
    HPALETTE h = (HPALETTE)calloc(1, sizeof(HPALETTE__));
    if (!h) return 0;
    h->nTag = MFX_TAG_PAL;
    h->nEntries = 256;
    for (int i = 0; i < 216; i++) {
        h->aPal[i].peRed   = (BYTE)((i / 36) * 51);
        h->aPal[i].peGreen = (BYTE)(((i / 6) % 6) * 51);
        h->aPal[i].peBlue  = (BYTE)((i % 6) * 51);
    }
    for (int i = 216; i < 256; i++)
        h->aPal[i].peRed = h->aPal[i].peGreen = h->aPal[i].peBlue = (BYTE)((i - 216) * 255 / 39);
    return h;
}

HPALETTE SelectPalette(HDC hdc, HPALETTE hPal, BOOL /*bForceBackground*/)
{
    if (!MfxIsDc(hdc)) return hPal;
    HPALETTE hOld = hdc->hPal;
    hdc->hPal = MfxIsPal(hPal) ? hPal : 0;
    return hOld;
}

UINT RealizePalette(HDC hdc)
{
    if (!MfxIsDc(hdc) || !MfxIsPal(hdc->hPal)) return 0;
    hdc->hPal->hdcRealized = hdc;
    MfxPalToDib(hdc->hPal, hdc, 0, hdc->hPal->nEntries);
    return hdc->hPal->nEntries;
}

BOOL AnimatePalette(HPALETTE hPal, UINT iStart, UINT nEntries, const PALETTEENTRY *ppe)
{
    if (!MfxIsPal(hPal) || !ppe || iStart >= 256) return FALSE;
    if (iStart + nEntries > 256) nEntries = 256 - iStart;
    memcpy(hPal->aPal + iStart, ppe, nEntries * sizeof(PALETTEENTRY));
    if (MfxIsDc(hPal->hdcRealized) && hPal->hdcRealized->hPal == hPal)
        MfxPalToDib(hPal, hPal->hdcRealized, iStart, nEntries);
    return TRUE;
}

UINT GetPaletteEntries(HPALETTE hPal, UINT iStart, UINT nEntries, LPPALETTEENTRY ppe)
{
    if (!MfxIsPal(hPal) || !ppe || iStart >= 256) return 0;
    if (iStart + nEntries > 256) nEntries = 256 - iStart;
    memcpy(ppe, hPal->aPal + iStart, nEntries * sizeof(PALETTEENTRY));
    return nEntries;
}

UINT SetPaletteEntries(HPALETTE hPal, UINT iStart, UINT nEntries, const PALETTEENTRY *ppe)
{
    if (!MfxIsPal(hPal) || !ppe || iStart >= 256) return 0;
    if (iStart + nEntries > 256) nEntries = 256 - iStart;
    memcpy(hPal->aPal + iStart, ppe, nEntries * sizeof(PALETTEENTRY));
    return nEntries;
}

UINT GetSystemPaletteEntries(HDC, UINT iStart, UINT nEntries, LPPALETTEENTRY ppe)
{
    // the Win95 8-bit static palette: 10 low + 10 high system colors, middle zeroed
    static const BYTE aStatic[20][3] = {
        {0x00,0x00,0x00},{0x80,0x00,0x00},{0x00,0x80,0x00},{0x80,0x80,0x00},{0x00,0x00,0x80},
        {0x80,0x00,0x80},{0x00,0x80,0x80},{0xc0,0xc0,0xc0},{0xc0,0xdc,0xc0},{0xa6,0xca,0xf0},
        {0xff,0xfb,0xf0},{0xa0,0xa0,0xa4},{0x80,0x80,0x80},{0xff,0x00,0x00},{0x00,0xff,0x00},
        {0xff,0xff,0x00},{0x00,0x00,0xff},{0xff,0x00,0xff},{0x00,0xff,0xff},{0xff,0xff,0xff},
    };
    if (!ppe || iStart >= 256) return 0;
    if (iStart + nEntries > 256) nEntries = 256 - iStart;
    for (UINT i = 0; i < nEntries; i++) {
        UINT n = iStart + i;
        int nStatic = (n < 10) ? (int)n : (n >= 246 ? (int)(n - 246 + 10) : -1);
        ppe[i].peRed   = nStatic >= 0 ? aStatic[nStatic][0] : 0;
        ppe[i].peGreen = nStatic >= 0 ? aStatic[nStatic][1] : 0;
        ppe[i].peBlue  = nStatic >= 0 ? aStatic[nStatic][2] : 0;
        ppe[i].peFlags = 0;
    }
    return nEntries;
}

UINT GetNearestPaletteIndex(HPALETTE hPal, COLORREF cr)
{
    if (!MfxIsPal(hPal)) return 0;
    int r = (int)(cr & 0xff), g = (int)((cr >> 8) & 0xff), b = (int)((cr >> 16) & 0xff);
    UINT nBest = 0;
    long nBestDist = 0x7fffffff;
    for (UINT i = 0; i < hPal->nEntries; i++) {
        long dr = r - hPal->aPal[i].peRed, dg = g - hPal->aPal[i].peGreen, db = b - hPal->aPal[i].peBlue;
        long d = dr * dr + dg * dg + db * db;
        if (d < nBestDist) { nBestDist = d; nBest = i; }
    }
    return nBest;
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
