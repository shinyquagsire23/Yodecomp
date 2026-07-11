// microfx res/ — the embedded-resource loader (H4 M4, docs/phase-h4-sdl.md).
//
// The SDL build embeds the SAME yoda.res the WIN32 link consumes (tools/bin2c.py →
// mfxres_blob.c). This file parses the .res container once and serves the game's resource
// consumers: CString::LoadString (all the bubble/HUD text), LoadIcon (the HUD direction
// arrows, ids 0xc4-0xcb), LoadCursor (the 11 directional/interaction cursors, ids 0x6a-0x76
// + 0xc2), and named RT_BITMAPs (the bubble CBitmapButton faces "CLOSEU" et al).
//
// .res container (tools/reslib.py emit_res): a 32-byte null entry, then per resource:
//   DWORD DataSize; DWORD HeaderSize; [type][name] (each 0xffff,WORD id | UTF-16Z string),
//   4-aligned; DWORD DataVersion; WORD MemoryFlags; WORD LanguageId; DWORD Version;
//   DWORD Characteristics; data (4-aligned).
//
// Icons/cursors/bitmaps are decoded to a uniform MfxImg (8bpp indices + own color table +
// transparency mask); gdi/mfxgdi.cpp draws them with per-pixel mapping into the DC palette
// (microfx.h MFXIMG). Pure C++, no SDL — headless harnesses keep linking.

#include <afxwin.h>
#include <microfx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
extern const unsigned char g_mfxResBlob[];
extern const unsigned int  g_mfxResBlob_size;
}

#define MFX_TAG_IMG 0x474d4958   // 'XIMG'

struct MfxImg {
    unsigned int   nTag;
    int            nWidth;
    int            nHeight;
    int            xHot, yHot;
    int            nSysCursor;   // IDC_* id for system cursors; 0 otherwise
    unsigned char *pIdx;
    unsigned char *pMask;
    RGBQUAD        aPal[256];
};

// ── RT_* type ids ────────────────────────────────────────────────────────────────────────────
#define RT_CURSOR_ID       1
#define RT_BITMAP_ID       2
#define RT_ICON_ID         3
#define RT_STRING_ID       6
#define RT_GROUP_CURSOR_ID 12
#define RT_GROUP_ICON_ID   14

// ── .res directory (parsed once) ─────────────────────────────────────────────────────────────

struct MfxResEntry {
    DWORD        nType;          // integer type id (string types don't occur in our .res)
    DWORD        nId;            // integer name id, or 0 if named
    const WORD  *pszName;        // UTF-16 name (aliases the blob), or 0 if integer id
    const BYTE  *pData;
    DWORD        nSize;
};

static MfxResEntry *g_aRes = 0;
static int          g_nRes = 0;

static DWORD MfxRd32(const BYTE *p) { DWORD v; memcpy(&v, p, 4); return v; }
static WORD  MfxRd16(const BYTE *p) { WORD  v; memcpy(&v, p, 2); return v; }

static void MfxParseRes()
{
    if (g_aRes) return;
    // count then fill (two passes over the container)
    for (int nPass = 0; nPass < 2; nPass++) {
        int n = 0;
        DWORD off = 0;
        while (off + 8 <= g_mfxResBlob_size) {
            DWORD nData = MfxRd32(g_mfxResBlob + off);
            DWORD nHdr  = MfxRd32(g_mfxResBlob + off + 4);
            if (nHdr < 16 || off + nHdr > g_mfxResBlob_size) break;
            const BYTE *p = g_mfxResBlob + off + 8;
            DWORD nTypeId = 0, nNameId = 0;
            const WORD *pszName = 0;
            // type field
            if (MfxRd16(p) == 0xffff) { nTypeId = MfxRd16(p + 2); p += 4; }
            else { while (MfxRd16(p)) p += 2; p += 2; }          // string type: skip
            // name field
            if (MfxRd16(p) == 0xffff) { nNameId = MfxRd16(p + 2); p += 4; }
            else { pszName = (const WORD *)p; while (MfxRd16(p)) p += 2; p += 2; }
            if (nData && nTypeId) {
                if (nPass == 1) {
                    g_aRes[n].nType   = nTypeId;
                    g_aRes[n].nId     = nNameId;
                    g_aRes[n].pszName = pszName;
                    g_aRes[n].pData   = g_mfxResBlob + off + nHdr;
                    g_aRes[n].nSize   = nData;
                }
                n++;
            }
            off += nHdr + nData;
            off = (off + 3) & ~3u;
        }
        if (nPass == 0)
            g_aRes = (MfxResEntry *)calloc(n ? n : 1, sizeof(MfxResEntry));
        else
            g_nRes = n;
    }
}

static const MfxResEntry *MfxFindRes(DWORD nType, DWORD nId)
{
    MfxParseRes();
    for (int i = 0; i < g_nRes; i++)
        if (g_aRes[i].nType == nType && !g_aRes[i].pszName && g_aRes[i].nId == nId)
            return &g_aRes[i];
    return 0;
}

static const MfxResEntry *MfxFindResNamed(DWORD nType, const char *pszName)
{
    MfxParseRes();
    for (int i = 0; i < g_nRes; i++) {
        if (g_aRes[i].nType != nType || !g_aRes[i].pszName) continue;
        const WORD *pW = g_aRes[i].pszName;
        const char *pC = pszName;
        while (*pW && *pC) {
            char a = (char)*pW, b = *pC;
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b) break;
            pW++; pC++;
        }
        if (!*pW && !*pC) return &g_aRes[i];
    }
    return 0;
}

// ── strings (RT_STRING: blocks of 16 length-prefixed UTF-16 strings) ─────────────────────────

BOOL MfxLoadString(UINT nID, CString &str)
{
    const MfxResEntry *pE = MfxFindRes(RT_STRING_ID, (nID >> 4) + 1);
    if (!pE) { str = ""; return FALSE; }
    const BYTE *p = pE->pData, *pEnd = pE->pData + pE->nSize;
    for (UINT i = 0; i < 16 && p + 2 <= pEnd; i++) {
        UINT nLen = MfxRd16(p);
        p += 2;
        if (i == (nID & 15)) {
            char *buf = (char *)malloc(nLen + 1);
            for (UINT k = 0; k < nLen; k++) {
                WORD c = MfxRd16(p + 2 * k);
                buf[k] = (c && c < 0x100) ? (char)c : '?';
            }
            buf[nLen] = 0;
            str = buf;
            free(buf);
            return nLen > 0;
        }
        p += 2 * nLen;
    }
    str = "";
    return FALSE;
}

// ── DIB → MfxImg decode (icons/cursors: doubled-height DIB w/ AND mask; bitmaps: plain) ─────

static MfxImg *MfxDecodeDib(const BYTE *p, DWORD nSize, int bIconCursor, int xHot, int yHot)
{
    if (nSize < 40) return 0;
    BITMAPINFOHEADER bih;
    memcpy(&bih, p, sizeof bih);
    int w = (int)bih.biWidth;
    int h = (int)bih.biHeight;
    int bpp = bih.biBitCount;
    if (bIconCursor) h /= 2;                        // XOR + AND stacked
    int bTopDown = 0;
    if (h < 0) { h = -h; bTopDown = 1; }
    if (w <= 0 || h <= 0 || w > 512 || h > 512) return 0;
    if (bpp != 1 && bpp != 4 && bpp != 8) return 0;

    int nColors = bih.biClrUsed ? (int)bih.biClrUsed : (1 << bpp);
    if (nColors > 256) nColors = 256;
    const BYTE *pPal = p + bih.biSize;
    const BYTE *pXor = pPal + nColors * 4;
    int nXorPitch = ((w * bpp + 31) / 32) * 4;
    int nAndPitch = ((w + 31) / 32) * 4;
    if ((DWORD)(pXor - p) + (DWORD)nXorPitch * h > nSize) return 0;

    MfxImg *pImg = (MfxImg *)calloc(1, sizeof(MfxImg));
    if (!pImg) return 0;
    pImg->nTag = MFX_TAG_IMG;
    pImg->nWidth = w;
    pImg->nHeight = h;
    pImg->xHot = xHot; pImg->yHot = yHot;
    pImg->pIdx  = (unsigned char *)calloc((size_t)w * h, 1);
    pImg->pMask = (unsigned char *)calloc((size_t)w * h, 1);
    memcpy(pImg->aPal, pPal, nColors * 4);

    for (int y = 0; y < h; y++) {
        int nSrcRow = bTopDown ? y : (h - 1 - y);
        const BYTE *pRow = pXor + (size_t)nSrcRow * nXorPitch;
        for (int x = 0; x < w; x++) {
            int ix = 0;
            if (bpp == 8)      ix = pRow[x];
            else if (bpp == 4) ix = (pRow[x / 2] >> (x & 1 ? 0 : 4)) & 0xf;
            else               ix = (pRow[x / 8] >> (7 - (x & 7))) & 1;
            pImg->pIdx[(size_t)y * w + x] = (unsigned char)ix;
        }
    }
    if (bIconCursor) {
        const BYTE *pAnd = pXor + (size_t)nXorPitch * h;
        if ((DWORD)(pAnd - p) + (DWORD)nAndPitch * h <= nSize) {
            for (int y = 0; y < h; y++) {
                int nSrcRow = bTopDown ? y : (h - 1 - y);
                const BYTE *pRow = pAnd + (size_t)nSrcRow * nAndPitch;
                for (int x = 0; x < w; x++)
                    pImg->pMask[(size_t)y * w + x] =
                        (pRow[x / 8] >> (7 - (x & 7))) & 1;   // AND=1 → transparent
            }
        }
    }
    return pImg;
}

// group (RT_GROUP_ICON/RT_GROUP_CURSOR) → first member id. Both directory flavors keep the
// member id in the last WORD of a 14-byte entry after the 6-byte header.
static int MfxGroupFirstId(const MfxResEntry *pGrp)
{
    if (!pGrp || pGrp->nSize < 6 + 14) return -1;
    if (MfxRd16(pGrp->pData + 4) < 1) return -1;    // idCount
    return MfxRd16(pGrp->pData + 6 + 12);
}

// loaded-image cache — LoadIcon/LoadCursor handles are shared and never freed (Win32-like)
struct MfxImgCacheEntry { DWORD nType; DWORD nId; char szName[16]; MfxImg *pImg; };
static MfxImgCacheEntry g_aImgCache[64];
static int g_nImgCache = 0;

static MfxImg *MfxCacheFind(DWORD nType, DWORD nId, const char *pszName)
{
    for (int i = 0; i < g_nImgCache; i++)
        if (g_aImgCache[i].nType == nType && g_aImgCache[i].nId == nId &&
            (!pszName || !strncmp(g_aImgCache[i].szName, pszName, 15)))
            return g_aImgCache[i].pImg;
    return 0;
}

static MfxImg *MfxCacheAdd(DWORD nType, DWORD nId, const char *pszName, MfxImg *pImg)
{
    if (pImg && g_nImgCache < 64) {
        MfxImgCacheEntry *pE = &g_aImgCache[g_nImgCache++];
        pE->nType = nType; pE->nId = nId; pE->pImg = pImg;
        if (pszName) strncpy(pE->szName, pszName, 15);
    }
    return pImg;
}

static MfxImg *MfxLoadIconById(DWORD nId)
{
    if (MfxImg *pHit = MfxCacheFind(RT_GROUP_ICON_ID, nId, 0)) return pHit;
    int nMember = MfxGroupFirstId(MfxFindRes(RT_GROUP_ICON_ID, nId));
    const MfxResEntry *pIco = nMember > 0 ? MfxFindRes(RT_ICON_ID, (DWORD)nMember) : 0;
    if (!pIco) return 0;
    return MfxCacheAdd(RT_GROUP_ICON_ID, nId, 0,
                       MfxDecodeDib(pIco->pData, pIco->nSize, 1, 0, 0));
}

static MfxImg *MfxLoadCursorById(DWORD nId)
{
    if (MfxImg *pHit = MfxCacheFind(RT_GROUP_CURSOR_ID, nId, 0)) return pHit;
    int nMember = MfxGroupFirstId(MfxFindRes(RT_GROUP_CURSOR_ID, nId));
    const MfxResEntry *pCur = nMember > 0 ? MfxFindRes(RT_CURSOR_ID, (DWORD)nMember) : 0;
    if (!pCur || pCur->nSize < 4) return 0;
    int xHot = MfxRd16(pCur->pData);                // LOCALHEADER precedes the DIB
    int yHot = MfxRd16(pCur->pData + 2);
    return MfxCacheAdd(RT_GROUP_CURSOR_ID, nId, 0,
                       MfxDecodeDib(pCur->pData + 4, pCur->nSize - 4, 1, xHot, yHot));
}

static MfxImg *MfxLoadBitmapByName(const char *pszName)
{
    if (MfxImg *pHit = MfxCacheFind(RT_BITMAP_ID, 0, pszName)) return pHit;
    const MfxResEntry *pBmp = MfxFindResNamed(RT_BITMAP_ID, pszName);
    if (!pBmp) return 0;
    return MfxCacheAdd(RT_BITMAP_ID, 0, pszName,
                       MfxDecodeDib(pBmp->pData, pBmp->nSize, 0, 0, 0));
}

// system cursors (LoadCursor(NULL, IDC_*)) — one static marker object per id
static MfxImg *MfxSystemCursor(int nId)
{
    static MfxImg aSys[4];
    int nSlot = (nId == 32514) ? 1 : 0;             // IDC_WAIT : IDC_ARROW/other
    aSys[nSlot].nTag = MFX_TAG_IMG;
    aSys[nSlot].nSysCursor = nId;
    return &aSys[nSlot];
}

// ── the public consumers ─────────────────────────────────────────────────────────────────────

extern "C" {

HICON LoadIconA(HINSTANCE, LPCSTR name)
{
    if ((ULONG_PTR)name >> 16)                       // string names: none in the game
        return 0;
    return (HICON)MfxLoadIconById((DWORD)(ULONG_PTR)name);
}

HCURSOR LoadCursorA(HINSTANCE hInst, LPCSTR name)
{
    if ((ULONG_PTR)name >> 16)
        return 0;
    DWORD nId = (DWORD)(ULONG_PTR)name;
    if (hInst == 0 && nId >= 32000)                  // IDC_ARROW / IDC_WAIT etc.
        return (HCURSOR)MfxSystemCursor((int)nId);
    return (HCURSOR)MfxLoadCursorById(nId);
}

HBITMAP LoadBitmapA(HINSTANCE, LPCSTR name)
{
    if (!((ULONG_PTR)name >> 16))
        return 0;                                    // by-id bitmaps: none in the game
    return (HBITMAP)MfxLoadBitmapByName(name);
}

int MfxGetImage(const void *hImg, MFXIMG *pOut)
{
    const MfxImg *p = (const MfxImg *)hImg;
    if (!p || p->nTag != MFX_TAG_IMG || !pOut) return 0;
    pOut->nWidth  = p->nWidth;
    pOut->nHeight = p->nHeight;
    pOut->xHot = p->xHot; pOut->yHot = p->yHot;
    pOut->nSysCursor = p->nSysCursor;
    pOut->pIdx  = p->pIdx;
    pOut->pMask = p->pMask;
    pOut->pPal  = p->aPal;
    return 1;
}

} // extern "C"
