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
// blit is SRCCOPY; the rop argument is otherwise ignored). M4 makes pens/brushes/fonts REAL
// objects with a per-DC draw state (pen/brush/font slots, current position, text/bk colors)
// and implements the HUD/bubble primitives (FillRect/PatBlt/Pie/RoundRect/Polygon/lines/
// pixels). Colors are COLORREFs mapped to the nearest entry of the target DIB's color table
// at draw time — Win32-on-8bpp behavior.

#include <windows.h>
#include <microfx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ── the objects behind the opaque handles (windows.h: HDC__ / HBITMAP__) ────────────────────
// All carry a magic tag first: SelectObject/DeleteObject take HGDIOBJ (void*) and must
// distinguish our objects from the null/fake handles the remaining stubs hand out.

#define MFX_TAG_DIB 0x42494458   // 'XDIB'
#define MFX_TAG_DC  0x00434458   // 'XDC'
#define MFX_TAG_PAL 0x4c415058   // 'XPAL'
#define MFX_TAG_PEN 0x4e455058   // 'XPEN'
#define MFX_TAG_BRU 0x55524258   // 'XBRU'
#define MFX_TAG_FNT 0x544e4658   // 'XFNT'

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

struct MfxPen {                  // CreatePen / stock pens (game: PS_SOLID width 1 only)
    unsigned int nTag;
    int          nStyle;         // PS_SOLID or PS_NULL
    COLORREF     cr;
    int          bStock;
};

struct MfxBrush {                // CreateSolidBrush / stock brushes
    unsigned int nTag;
    int          bNull;          // NULL_BRUSH: fills are no-ops
    COLORREF     cr;
    int          bStock;
};

struct MfxFont {                 // CreateFont (text drawing itself lands with mfxtext.cpp)
    unsigned int nTag;
    int          nHeight;        // as passed (negative = char height request)
    int          nWeight;        // 400 normal / 700 bold
    char         szFace[32];
};

struct HDC__ {                   // a memory DC: one DIB selection slot + full draw state
    unsigned int nTag;
    HBITMAP      hDib;           // currently selected DIB (may be 0)
    HPALETTE     hPal;           // currently selected palette (may be 0)
    MfxPen      *pPen;           // 0 = default BLACK_PEN
    MfxBrush    *pBrush;         // 0 = default WHITE_BRUSH
    MfxFont     *pFont;          // 0 = default system font
    POINT        ptCur;          // MoveTo/LineTo current position
    COLORREF     crText;         // default black
    COLORREF     crBk;           // default white
    int          nBkMode;        // default OPAQUE
};

static int MfxIsDib(void *h)   { return h && ((HBITMAP__ *)h)->nTag == MFX_TAG_DIB; }
static int MfxIsDc(HDC h)      { return h && h->nTag == MFX_TAG_DC; }
static int MfxIsPal(void *h)   { return h && ((HPALETTE__ *)h)->nTag == MFX_TAG_PAL; }
static int MfxIsPen(void *h)   { return h && ((MfxPen *)h)->nTag == MFX_TAG_PEN; }
static int MfxIsBrush(void *h) { return h && ((MfxBrush *)h)->nTag == MFX_TAG_BRU; }
static int MfxIsFont(void *h)  { return h && ((MfxFont *)h)->nTag == MFX_TAG_FNT; }

// microfx-internal accessors for the text renderer (mfxtext.cpp)
MfxFont *MfxDcFont(HDC hdc)    { return MfxIsDc(hdc) ? hdc->pFont : 0; }

// ── stock objects (created on first use; never deletable) ───────────────────────────────────
static MfxPen   g_penBlack = { MFX_TAG_PEN, PS_SOLID, RGB(0,0,0),       1 };
static MfxPen   g_penWhite = { MFX_TAG_PEN, PS_SOLID, RGB(255,255,255), 1 };
static MfxPen   g_penNull  = { MFX_TAG_PEN, PS_NULL,  0,                1 };
static MfxBrush g_brWhite  = { MFX_TAG_BRU, 0, RGB(255,255,255), 1 };
static MfxBrush g_brLtGray = { MFX_TAG_BRU, 0, RGB(192,192,192), 1 };
static MfxBrush g_brGray   = { MFX_TAG_BRU, 0, RGB(128,128,128), 1 };
static MfxBrush g_brDkGray = { MFX_TAG_BRU, 0, RGB(64,64,64),    1 };
static MfxBrush g_brBlack  = { MFX_TAG_BRU, 0, RGB(0,0,0),       1 };
static MfxBrush g_brNull   = { MFX_TAG_BRU, 1, 0,                1 };

static MfxPen   *MfxDcPen(HDC hdc)    { return hdc->pPen   ? hdc->pPen   : &g_penBlack; }
static MfxBrush *MfxDcBrush(HDC hdc)  { return hdc->pBrush ? hdc->pBrush : &g_brWhite;  }

// ── DC lifecycle ─────────────────────────────────────────────────────────────────────────────

HDC CreateCompatibleDC(HDC)
{
    HDC h = (HDC)calloc(1, sizeof(HDC__));
    if (!h) return 0;
    h->nTag    = MFX_TAG_DC;
    h->crText  = RGB(0, 0, 0);         // Win32 defaults: black text on white, OPAQUE,
    h->crBk    = RGB(255, 255, 255);   // BLACK_PEN / WHITE_BRUSH (the 0 slots)
    h->nBkMode = OPAQUE;
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
    else if ((MfxIsPen(h) && !((MfxPen *)h)->bStock) ||
             (MfxIsBrush(h) && !((MfxBrush *)h)->bStock) ||
             MfxIsFont(h)) {
        *(unsigned int *)h = 0;
        free(h);
    }
    return TRUE;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h)
{
    if (!MfxIsDc(hdc)) return h;
    if (MfxIsDib(h)) {
        HGDIOBJ hOld = (HGDIOBJ)hdc->hDib;
        hdc->hDib = (HBITMAP)h;
        return hOld;             // 0 on the first select — Canvas's dtor handles that arm
    }
    if (MfxIsPen(h)) {
        MfxPen *pOld = MfxDcPen(hdc);
        hdc->pPen = (MfxPen *)h;
        return (HGDIOBJ)pOld;
    }
    if (MfxIsBrush(h)) {
        MfxBrush *pOld = MfxDcBrush(hdc);
        hdc->pBrush = (MfxBrush *)h;
        return (HGDIOBJ)pOld;
    }
    if (MfxIsFont(h)) {
        MfxFont *pOld = hdc->pFont;
        hdc->pFont = (MfxFont *)h;
        return (HGDIOBJ)pOld;    // 0 on the first select (default font)
    }
    return h;                    // foreign/null handles: pass-through
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
static HDC   g_hdcOverlay = 0;
static void (*g_pfnOverlay)(void) = 0;

void MfxSetScreenWriteHook(HDC hdcScreen, void (*pfn)(void))
{
    g_hdcScreenWrite = hdcScreen;
    g_pfnScreenWrite = pfn;
}

void MfxSetScreenOverlayHook(HDC hdcScreen, void (*pfn)(void))
{
    g_hdcOverlay = hdcScreen;
    g_pfnOverlay = pfn;
}

// Every primitive that writes pixels calls this with its target DC. Two hooks fire, guarded
// against reentry (the overlay itself draws through these primitives):
//  1. overlay — mfxwnd re-composites visible child controls. Win32 children are separate
//     windows a screen blit can't erase; in our one-surface model a canvas blit (DrawGameArea)
//     would wipe the bubble edit/buttons, so they re-lay after every screen write.
//  2. present — the pump shows the frame (M4 chrome must present mid-handler like BitBlt).
static int g_nTouchHold = 0;

void MfxTouch(HDC hdc)
{
    static int bInTouch = 0;
    if (bInTouch || g_nTouchHold) return;
    bInTouch = 1;
    if (hdc == g_hdcOverlay && g_pfnOverlay)
        g_pfnOverlay();
    if (hdc == g_hdcScreenWrite && g_pfnScreenWrite)
        g_pfnScreenWrite();
    bInTouch = 0;
}

// Batch many primitives into ONE hook fire (control painting: a scrollbar is hundreds of
// SetPixels — presenting per primitive would be pathological). Hold/Release nest.
void MfxTouchHold(void) { g_nTouchHold++; }
void MfxTouchRelease(HDC hdc)
{
    if (--g_nTouchHold < 0) g_nTouchHold = 0;
    if (g_nTouchHold == 0) MfxTouch(hdc);
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
    MfxTouch(hdcDst);
    return TRUE;
}

// ── color mapping + drawing primitives (M4) ─────────────────────────────────────────────────
// A COLORREF is resolved against the DC's palette at draw time (exact match fast path, else
// nearest by squared RGB distance) — Win32-on-8bpp-display behavior. CRITICAL: entries with
// peFlags & PC_RESERVED (1) are SKIPPED, exactly like GDI — the game marks its palette-cycling
// ring entries reserved, and matching into a ring makes solid fills (the health dial!) change
// color as CyclePalette animates. DCs without a selected palette (the Canvas memory DCs) fall
// back to their DIB color table with no flag knowledge — the game only draws UI solids there
// in stable colors (bubble white/black).

static BYTE MfxMapColor(HDC hdc, COLORREF cr)
{
    int r = (int)(cr & 0xff), g = (int)((cr >> 8) & 0xff), b = (int)((cr >> 16) & 0xff);
    long nBestDist = 0x7fffffff;
    int  nBest = 0;
    HPALETTE hPal = MfxIsPal(hdc->hPal) ? hdc->hPal : 0;
    for (int i = 0; i < 256; i++) {
        int pr, pg, pb;
        if (hPal) {
            if (hPal->aPal[i].peFlags & 1) continue;     // PC_RESERVED: never matched
            pr = hPal->aPal[i].peRed; pg = hPal->aPal[i].peGreen; pb = hPal->aPal[i].peBlue;
        } else {
            pr = hdc->hDib->aPal[i].rgbRed; pg = hdc->hDib->aPal[i].rgbGreen;
            pb = hdc->hDib->aPal[i].rgbBlue;
        }
        long dr = r - pr, dg = g - pg, db = b - pb;
        long d = dr * dr + dg * dg + db * db;
        if (d == 0) return (BYTE)i;
        if (d < nBestDist) { nBestDist = d; nBest = i; }
    }
    return (BYTE)nBest;
}

static void MfxPutPixel(HBITMAP__ *pDib, int x, int y, BYTE ix)
{
    if (x < 0 || y < 0 || x >= pDib->nWidth || y >= pDib->nHeight) return;
    pDib->pBits[(size_t)y * pDib->nWidth + x] = ix;
}

static void MfxHLine(HBITMAP__ *pDib, int x0, int x1, int y, BYTE ix)
{
    if (y < 0 || y >= pDib->nHeight) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= pDib->nWidth) x1 = pDib->nWidth - 1;
    if (x0 > x1) return;
    memset(pDib->pBits + (size_t)y * pDib->nWidth + x0, ix, (size_t)(x1 - x0 + 1));
}

static void MfxFillRectIx(HBITMAP__ *pDib, int l, int t, int r, int b, BYTE ix)
{
    for (int y = t; y < b; y++)
        MfxHLine(pDib, l, r - 1, y, ix);
}

// Bresenham. bLast=0 = Win32 LineTo semantics (the final endpoint is NOT drawn) — the
// DrawRect bevel mitres depend on it: with an inclusive line the top/bottom bevel rows
// overshoot one pixel into the right column (user-visible off-by-one, v79 playtest).
static void MfxLineIx(HBITMAP__ *pDib, int x0, int y0, int x1, int y1, BYTE ix, int bLast)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 == x1 && y0 == y1) {
            if (bLast) MfxPutPixel(pDib, x0, y0, ix);
            break;
        }
        MfxPutPixel(pDib, x0, y0, ix);
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

extern "C" {

DWORD GetSysColor(int nIndex)
{
    switch (nIndex) {                       // classic Win95 scheme — the game palette's UI
    case 15: return RGB(192, 192, 192);     // COLOR_3DFACE   → sysPalette[1] (HUD gray)
    case 16: return RGB(128, 128, 128);     // COLOR_3DSHADOW → sysPalette[2]
    case 20: return RGB(255, 255, 255);     // COLOR_3DHILIGHT→ sysPalette[3]
    case 8:  return RGB(0, 0, 0);           // COLOR_WINDOWTEXT
    case 5:  return RGB(255, 255, 255);     // COLOR_WINDOW
    default: return RGB(192, 192, 192);
    }
}

HGDIOBJ GetStockObject(int nIndex)
{
    switch (nIndex) {
    case WHITE_PEN:    return (HGDIOBJ)&g_penWhite;
    case BLACK_PEN:    return (HGDIOBJ)&g_penBlack;
    case NULL_PEN:     return (HGDIOBJ)&g_penNull;
    case WHITE_BRUSH:  return (HGDIOBJ)&g_brWhite;
    case LTGRAY_BRUSH: return (HGDIOBJ)&g_brLtGray;
    case GRAY_BRUSH:   return (HGDIOBJ)&g_brGray;
    case DKGRAY_BRUSH: return (HGDIOBJ)&g_brDkGray;
    case BLACK_BRUSH:  return (HGDIOBJ)&g_brBlack;
    case NULL_BRUSH:   return (HGDIOBJ)&g_brNull;
    default:           return 0;
    }
}

HPEN CreatePen(int nStyle, int /*nWidth*/, COLORREF cr)
{
    MfxPen *p = (MfxPen *)calloc(1, sizeof(MfxPen));
    if (!p) return 0;
    p->nTag = MFX_TAG_PEN;
    p->nStyle = nStyle;
    p->cr = cr;
    return (HPEN)p;
}

HBRUSH CreateSolidBrush(COLORREF cr)
{
    MfxBrush *p = (MfxBrush *)calloc(1, sizeof(MfxBrush));
    if (!p) return 0;
    p->nTag = MFX_TAG_BRU;
    p->cr = cr;
    return (HBRUSH)p;
}

HFONT CreateFontA(int nHeight, int, int, int, int nWeight, DWORD, DWORD, DWORD, DWORD,
                  DWORD, DWORD, DWORD, DWORD, LPCSTR pszFace)
{
    MfxFont *p = (MfxFont *)calloc(1, sizeof(MfxFont));
    if (!p) return 0;
    p->nTag = MFX_TAG_FNT;
    p->nHeight = nHeight;
    p->nWeight = nWeight;
    if (pszFace) { strncpy(p->szFace, pszFace, sizeof(p->szFace) - 1); }
    return (HFONT)p;
}

COLORREF SetTextColor(HDC hdc, COLORREF c)
    { if (!MfxIsDc(hdc)) return c; COLORREF o = hdc->crText; hdc->crText = c; return o; }
COLORREF SetBkColor(HDC hdc, COLORREF c)
    { if (!MfxIsDc(hdc)) return c; COLORREF o = hdc->crBk; hdc->crBk = c; return o; }
int SetBkMode(HDC hdc, int m)
    { if (!MfxIsDc(hdc)) return m; int o = hdc->nBkMode; hdc->nBkMode = m; return o; }

int GetClipBox(HDC hdc, LPRECT r)
{
    if (!r) return 0;
    if (!MfxIsDc(hdc) || !hdc->hDib) { SetRect(r, 0, 0, 0, 0); return 1; }  // NULLREGION
    SetRect(r, 0, 0, hdc->hDib->nWidth, hdc->hDib->nHeight);
    return 2;                                                               // SIMPLEREGION
}

int FillRect(HDC hdc, const RECT *prc, HBRUSH hbr)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || !prc) return 0;
    MfxBrush *pBr = MfxIsBrush(hbr) ? (MfxBrush *)hbr : &g_brWhite;
    if (pBr->bNull) return 1;
    MfxFillRectIx(hdc->hDib, prc->left, prc->top, prc->right, prc->bottom,
                  MfxMapColor(hdc, pBr->cr));
    MfxTouch(hdc);
    return 1;
}

BOOL PatBlt(HDC hdc, int x, int y, int cx, int cy, DWORD rop)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    if (rop == PATCOPY) {
        MfxBrush *pBr = MfxDcBrush(hdc);
        if (!pBr->bNull)
            MfxFillRectIx(pDib, x, y, x + cx, y + cy, MfxMapColor(hdc, pBr->cr));
    }
    else if (rop == BLACKNESS)
        MfxFillRectIx(pDib, x, y, x + cx, y + cy, MfxMapColor(hdc, RGB(0, 0, 0)));
    else if (rop == WHITENESS)
        MfxFillRectIx(pDib, x, y, x + cx, y + cy, MfxMapColor(hdc, RGB(255, 255, 255)));
    MfxTouch(hdc);
    return TRUE;
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT pOld)
{
    if (!MfxIsDc(hdc)) return FALSE;
    if (pOld) *pOld = hdc->ptCur;
    hdc->ptCur.x = x; hdc->ptCur.y = y;
    return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return FALSE;
    MfxPen *pPen = MfxDcPen(hdc);
    if (pPen->nStyle != PS_NULL)
        MfxLineIx(hdc->hDib, hdc->ptCur.x, hdc->ptCur.y, x, y, MfxMapColor(hdc, pPen->cr), 0);
    hdc->ptCur.x = x; hdc->ptCur.y = y;
    MfxTouch(hdc);
    return TRUE;
}

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF c)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return c;
    MfxPutPixel(hdc->hDib, x, y, MfxMapColor(hdc, c));
    MfxTouch(hdc);
    return c;
}

COLORREF GetPixel(HDC hdc, int x, int y)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return (COLORREF)-1;
    HBITMAP__ *pDib = hdc->hDib;
    if (x < 0 || y < 0 || x >= pDib->nWidth || y >= pDib->nHeight) return (COLORREF)-1;
    RGBQUAD *pQ = &pDib->aPal[pDib->pBits[(size_t)y * pDib->nWidth + x]];
    return RGB(pQ->rgbRed, pQ->rgbGreen, pQ->rgbBlue);
}

BOOL Rectangle(HDC hdc, int l, int t, int r, int b)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    MfxBrush *pBr = MfxDcBrush(hdc);
    if (!pBr->bNull)
        MfxFillRectIx(pDib, l + 1, t + 1, r - 1, b - 1, MfxMapColor(hdc, pBr->cr));
    MfxPen *pPen = MfxDcPen(hdc);
    if (pPen->nStyle != PS_NULL) {
        BYTE ix = MfxMapColor(hdc, pPen->cr);
        MfxHLine(pDib, l, r - 1, t, ix);
        MfxHLine(pDib, l, r - 1, b - 1, ix);
        MfxLineIx(pDib, l, t, l, b - 1, ix, 1);
        MfxLineIx(pDib, r - 1, t, r - 1, b - 1, ix, 1);
    }
    MfxTouch(hdc);
    return TRUE;
}

// RoundRect via per-scanline inset from the corner ellipses (rw/rh = corner ellipse size).
// The bubble frame is a white box with a black 1px outline — pen + brush both honored.
BOOL RoundRect(HDC hdc, int l, int t, int r, int b, int rw, int rh)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    if (r <= l || b <= t) return TRUE;
    int ra = rw / 2, rb = rh / 2;                    // corner radii
    if (ra > (r - l) / 2) ra = (r - l) / 2;
    if (rb > (b - t) / 2) rb = (b - t) / 2;
    MfxBrush *pBr = MfxDcBrush(hdc);
    MfxPen  *pPen = MfxDcPen(hdc);
    BYTE ixBr  = pBr->bNull ? 0 : MfxMapColor(hdc, pBr->cr);
    BYTE ixPen = MfxMapColor(hdc, pPen->cr);
    int  bPen  = pPen->nStyle != PS_NULL;
    int aPrevL = 0, aPrevR = 0, bHavePrev = 0;
    for (int y = t; y < b; y++) {
        // horizontal inset of this scanline (elliptical corners)
        int inset = 0;
        if (rb > 0) {
            int dy = -1;
            if (y < t + rb)           dy = (t + rb - 1) - y + 1;   // top corner rows
            else if (y >= b - rb)     dy = y - (b - rb);           // bottom corner rows
            if (dy >= 0) {
                double fy = (double)dy / rb;
                double fx = 1.0 - sqrt(1.0 - fy * fy);
                inset = (int)(fx * ra + 0.5);
            }
        }
        int xl = l + inset, xr = r - 1 - inset;
        if (!pBr->bNull)
            MfxHLine(pDib, xl, xr, y, ixBr);
        if (bPen) {
            if (y == t || y == b - 1)
                MfxHLine(pDib, xl, xr, y, ixPen);
            else {
                MfxPutPixel(pDib, xl, y, ixPen);
                MfxPutPixel(pDib, xr, y, ixPen);
                if (bHavePrev) {                     // connect diagonal corner steps
                    for (int x = xl; x < aPrevL; x++) MfxPutPixel(pDib, x, y, ixPen);
                    for (int x = aPrevR + 1; x <= xr; x++) MfxPutPixel(pDib, x, y, ixPen);
                }
            }
        }
        aPrevL = xl; aPrevR = xr; bHavePrev = 1;
    }
    MfxTouch(hdc);
    return TRUE;
}

// Even-odd scanline polygon fill + pen outline (the bubble tail triangle).
BOOL Polygon(HDC hdc, const POINT *apt, int n)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || !apt || n < 2) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    MfxBrush *pBr = MfxDcBrush(hdc);
    if (!pBr->bNull && n >= 3) {
        BYTE ix = MfxMapColor(hdc, pBr->cr);
        int yMin = apt[0].y, yMax = apt[0].y;
        for (int i = 1; i < n; i++) {
            if (apt[i].y < yMin) yMin = apt[i].y;
            if (apt[i].y > yMax) yMax = apt[i].y;
        }
        for (int y = yMin; y <= yMax; y++) {
            int aX[16]; int nX = 0;
            for (int i = 0; i < n; i++) {
                const POINT *p0 = &apt[i], *p1 = &apt[(i + 1) % n];
                if ((p0->y <= y && p1->y > y) || (p1->y <= y && p0->y > y)) {
                    if (nX < 16)
                        aX[nX++] = p0->x + (int)((double)(y - p0->y) * (p1->x - p0->x)
                                                 / (p1->y - p0->y) + 0.5);
                }
            }
            for (int i = 1; i < nX; i++)             // insertion sort the crossings
                for (int j = i; j > 0 && aX[j - 1] > aX[j]; j--)
                    { int t2 = aX[j]; aX[j] = aX[j - 1]; aX[j - 1] = t2; }
            for (int i = 0; i + 1 < nX; i += 2)
                MfxHLine(pDib, aX[i], aX[i + 1], y, ix);
        }
    }
    MfxPen *pPen = MfxDcPen(hdc);
    if (pPen->nStyle != PS_NULL) {
        BYTE ix = MfxMapColor(hdc, pPen->cr);
        for (int i = 0; i < n; i++)
            MfxLineIx(pDib, apt[i].x, apt[i].y, apt[(i + 1) % n].x, apt[(i + 1) % n].y, ix, 1);
    }
    MfxTouch(hdc);
    return TRUE;
}

// Images from the res/ loader (icons/cursors/bitmap-button faces): draw with per-pixel
// mapping of the image's OWN color table into the DC palette, honoring the AND mask.
// Tiny surfaces (16x16/32x32) — the per-pixel MfxMapColor cost is irrelevant.
void MfxDrawImage(HDC hdc, int x, int y, const MFXIMG *pImg)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || !pImg || !pImg->pIdx) return;
    HBITMAP__ *pDib = hdc->hDib;
    BYTE aMap[256];
    memset(aMap, 0xff, sizeof aMap);                // lazily-mapped color cache
    for (int row = 0; row < pImg->nHeight; row++)
        for (int col = 0; col < pImg->nWidth; col++) {
            size_t n = (size_t)row * pImg->nWidth + col;
            if (pImg->pMask && pImg->pMask[n]) continue;
            BYTE ix = pImg->pIdx[n];
            if (aMap[ix] == 0xff) {
                const RGBQUAD *pQ = &pImg->pPal[ix];
                aMap[ix] = MfxMapColor(hdc, RGB(pQ->rgbRed, pQ->rgbGreen, pQ->rgbBlue));
            }
            MfxPutPixel(pDib, x + col, y + row, aMap[ix]);
        }
    MfxTouch(hdc);
}

BOOL DrawIcon(HDC hdc, int x, int y, HICON hIcon)
{
    MFXIMG img;
    if (!MfxGetImage(hIcon, &img)) return FALSE;
    MfxDrawImage(hdc, x, y, &img);
    return TRUE;
}

// ── text (M4c): the REAL MS Sans Serif bitmap strikes, rendered from raw FNT blocks ─────────
// mfxfont_data.c embeds the 13px (8pt) and 16px (10pt) strikes verbatim (tools/fon2c.py from
// a Windows SSERIFE.FON). The game's two requests map like GDI's font mapper: CreateFont(-8)
// → nearest character height (13px strike, charH 11) and CreateFont(-14) → 16px strike
// (charH 13). Weight >= 600 is synthesized bold (1px-right OR smear), as GDI does for
// bitmap fonts without a bold strike.

extern "C" {
extern const unsigned char g_mfxFnt13[];
extern const unsigned char g_mfxFnt16[];
}

struct MfxStrike {               // parsed view of one raw FNT v2 block
    const unsigned char *pFnt;
    int nPixHeight, nAscent, nLead, nAvgWidth, nMaxWidth;
    int nFirst, nLast, nDefault;
};

static const MfxStrike *MfxGetStrike(int nIndex)   // 0 = 13px, 1 = 16px
{
    static MfxStrike aStrikes[2];
    static int bInit = 0;
    if (!bInit) {
        const unsigned char *aFnt[2] = { g_mfxFnt13, g_mfxFnt16 };
        for (int i = 0; i < 2; i++) {
            MfxStrike *s = &aStrikes[i];
            s->pFnt = aFnt[i];
            s->nAscent    = aFnt[i][0x4a] | (aFnt[i][0x4b] << 8);
            s->nLead      = aFnt[i][0x4c] | (aFnt[i][0x4d] << 8);
            s->nPixHeight = aFnt[i][0x58] | (aFnt[i][0x59] << 8);
            s->nAvgWidth  = aFnt[i][0x5b] | (aFnt[i][0x5c] << 8);
            s->nMaxWidth  = aFnt[i][0x5d] | (aFnt[i][0x5e] << 8);
            s->nFirst     = aFnt[i][0x5f];
            s->nLast      = aFnt[i][0x60];
            s->nDefault   = aFnt[i][0x61];
        }
        bInit = 1;
    }
    return &aStrikes[nIndex];
}

// FNT v2 char table @0x76: {WORD width, WORD offset} per char first..last(+1 sentinel)
static int MfxGlyph(const MfxStrike *s, int ch, const unsigned char **ppBits)
{
    if (ch < s->nFirst || ch > s->nLast) ch = s->nFirst + s->nDefault;
    const unsigned char *pE = s->pFnt + 0x76 + 4 * (ch - s->nFirst);
    int nWidth = pE[0] | (pE[1] << 8);
    if (ppBits) *ppBits = s->pFnt + (pE[2] | (pE[3] << 8));
    return nWidth;
}

static const MfxStrike *MfxStrikeForDc(HDC hdc, int *pbBold)
{
    MfxFont *pFont = hdc->pFont;
    int nReq = pFont ? (pFont->nHeight < 0 ? -pFont->nHeight : pFont->nHeight) : 0;
    if (pbBold) *pbBold = pFont && pFont->nWeight >= 600;
    if (!pFont || nReq == 0) return MfxGetStrike(0);
    int nBest = 0, nBestDist = 0x7fff;
    for (int i = 0; i < 2; i++) {
        const MfxStrike *s = MfxGetStrike(i);
        int nCand = pFont->nHeight < 0 ? s->nPixHeight - s->nLead : s->nPixHeight;
        int d = nCand > nReq ? nCand - nReq : nReq - nCand;
        if (d < nBestDist) { nBestDist = d; nBest = i; }
    }
    return MfxGetStrike(nBest);
}

extern "C" {

BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRIC tm)
{
    if (!tm || !MfxIsDc(hdc)) return FALSE;
    int bBold = 0;
    const MfxStrike *s = MfxStrikeForDc(hdc, &bBold);
    memset(tm, 0, sizeof *tm);
    tm->tmHeight          = s->nPixHeight;
    tm->tmAscent          = s->nAscent;
    tm->tmDescent         = s->nPixHeight - s->nAscent;
    tm->tmInternalLeading = s->nLead;
    tm->tmAveCharWidth    = s->nAvgWidth + (bBold ? 1 : 0);
    tm->tmMaxCharWidth    = s->nMaxWidth + (bBold ? 1 : 0);
    return TRUE;
}

BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR psz, int n, LPSIZE pSize)
{
    if (!pSize || !MfxIsDc(hdc) || !psz) return FALSE;
    int bBold = 0;
    const MfxStrike *s = MfxStrikeForDc(hdc, &bBold);
    long cx = 0;
    for (int i = 0; i < n; i++)
        cx += MfxGlyph(s, (unsigned char)psz[i], 0) + (bBold ? 1 : 0);
    pSize->cx = cx;
    pSize->cy = s->nPixHeight;
    return TRUE;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR psz, int n)
{
    if (!MfxIsDc(hdc) || !hdc->hDib || !psz) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    int bBold = 0;
    const MfxStrike *s = MfxStrikeForDc(hdc, &bBold);
    BYTE ixText = MfxMapColor(hdc, hdc->crText);
    if (hdc->nBkMode == OPAQUE) {
        SIZE sz;
        GetTextExtentPoint32A(hdc, psz, n, &sz);
        MfxFillRectIx(pDib, x, y, x + (int)sz.cx, y + s->nPixHeight,
                      MfxMapColor(hdc, hdc->crBk));
    }
    int h = s->nPixHeight;
    for (int i = 0; i < n; i++) {
        const unsigned char *pBits = 0;
        int w = MfxGlyph(s, (unsigned char)psz[i], &pBits);
        int nCols = (w + 7) / 8;                    // v2 layout: byte-column-major
        for (int row = 0; row < h; row++)
            for (int j = 0; j < nCols; j++) {
                unsigned char b = pBits[j * h + row];
                if (bBold) b |= (unsigned char)(b >> 1) |
                                (j ? (unsigned char)(pBits[(j - 1) * h + row] << 7) : 0);
                for (int bit = 0; bit < 8; bit++)
                    if (b & (0x80 >> bit))
                        MfxPutPixel(pDib, x + j * 8 + bit, y + row, ixText);
            }
        x += w + (bBold ? 1 : 0);
    }
    MfxTouch(hdc);
    return TRUE;
}

} // extern "C"

// Pie: fill the elliptical sector swept COUNTERCLOCKWISE (in y-up math terms, GDI default
// arc direction) from the (x1,y1) radial to the (x2,y2) radial. Coincident radials = the
// full ellipse (the health dial's "full disc" first call). Pen == brush color in every game
// call site, so the outline is covered by the fill.
BOOL Pie(HDC hdc, int l, int t, int r, int b, int x1, int y1, int x2, int y2)
{
    if (!MfxIsDc(hdc) || !hdc->hDib) return FALSE;
    HBITMAP__ *pDib = hdc->hDib;
    MfxBrush *pBr = MfxDcBrush(hdc);
    if (pBr->bNull) return TRUE;
    BYTE ix = MfxMapColor(hdc, pBr->cr);
    double cx = (l + r - 1) / 2.0, cy = (t + b - 1) / 2.0;
    double ra = (r - l) / 2.0, rb2 = (b - t) / 2.0;
    if (ra <= 0 || rb2 <= 0) return TRUE;
    double a1 = atan2(cy - y1, x1 - cx);             // y flipped → math angles
    double a2 = atan2(cy - y2, x2 - cx);
    double sweep = a2 - a1;
    const double PI2 = 6.28318530717958647692;
    while (sweep < 0) sweep += PI2;
    if (sweep < 1e-9) sweep = PI2;                   // start == end → full ellipse
    for (int y = t; y < b; y++) {
        double fy = (y - cy) / rb2;
        if (fy < -1 || fy > 1) continue;
        double half = ra * sqrt(1.0 - fy * fy);
        int xl = (int)ceil(cx - half), xr = (int)floor(cx + half);
        for (int x = xl; x <= xr; x++) {
            double ang = atan2(cy - y, x - cx) - a1;
            while (ang < 0) ang += PI2;
            if (ang <= sweep)
                MfxPutPixel(pDib, x, y, ix);
        }
    }
    MfxTouch(hdc);
    return TRUE;
}

} // extern "C"

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
