// microfx app/ — the visible menu bar (H4 M5 tail, docs/phase-h4-sdl.md). Parses the game's own
// RT_MENU id=2 template (File/Options/Window/Help — the same resource the WIN32 build's real
// system menu bar reads) — no synthesized menu, so labels/command ids/shortcut hints/checkable
// items all come straight from the game's data.
//
// Two coordinate worlds, kept strictly apart so no game drawing assumption ever moves:
//   - the BAR ITSELF lives in its own small chrome HDC/DIBSection (own palette, resynced to the
//     live game palette via SetDIBColorTable on every redraw — the same nearest-match pipeline
//     mfxdlg.cpp's dialogs already rely on for GetSysColor-based chrome). mfxpump.cpp composites
//     it into a window ABOVE the game's screen DC at present time; the game's screen DC/DIB never
//     changes size, so every existing coordinate assumption in src/ stays exactly as before.
//   - a clicked POPUP lives INSIDE the game's normal screen-DC coordinate space (y=0 == right
//     under the bar once composited) and rides the exact child-window / SetCapture / overlay-
//     repaint machinery mfxdlg.cpp already proved for modal dialogs — no new compositing path.
//
// Before a popup opens, every item's command id is queried via MfxQueryCmdUI (mfxwnd.cpp) — the
// same ON_UPDATE_COMMAND_UI handlers real MFC's CFrameWnd::OnInitMenuPopup would call. The game's
// own OnUpdateToggleSound/OnUpdateFileSave/OnUpdatePauseUi/etc already exist (src/DeskcppDoc.cpp,
// src/DeskcppView.cpp) and drive check/gray state — nothing about menu semantics is invented here.

#include <afxwin.h>
#include <microfx.h>
#include "mfxwnd.h"
#include <string.h>
#include <stdlib.h>

extern "C" const unsigned char *MfxFindResourceData(unsigned nType, unsigned nId, unsigned *pnSize);
#define RT_MENU_ID 4

// ── parsed menu tree (RT_MENU id 2: File/Options/Window/Help, standard MENUITEMTEMPLATE) ──────
struct MfxMenuItem {
    CString strText;        // label, '&' mnemonic markers stripped; empty == separator
    CString strShortcut;    // "Ctrl+A" etc (from the '\t' suffix), "" if none
    UINT    nCmdId;
};
struct MfxMenuTop {
    CString strText;
    int     nFirst, nCount;
    int     nBarX, nBarW;   // measured bar-strip geometry (MfxMenuMeasureBar)
};

enum { MAX_TOP = 8, MAX_ITEMS = 40 };
static MfxMenuTop  g_aTop[MAX_TOP];
static int         g_nTop = 0;
static MfxMenuItem g_aItems[MAX_ITEMS];
static int         g_nItems = 0;
static int         g_bParsed = 0;

static WORD MfxMRdW(const BYTE *p) { WORD v; memcpy(&v, p, 2); return v; }

static CString MfxMWideToCStr(const WORD *pw, int nLen)
{
    char buf[128]; int n = 0;
    while (n < nLen && n < 127) { WORD c = pw[n]; buf[n] = (c < 128) ? (char)c : '?'; n++; }
    buf[n] = 0;
    return CString(buf);
}

// drop '&' mnemonic markers (v1 has no underline draw, so the marker itself would just show up
// as a literal ampersand otherwise); the character it marks is kept.
static CString MfxMStripAmp(const CString &strIn)
{
    CString strOut;
    int n = strIn.GetLength();
    for (int i = 0; i < n; i++) {
        char c = strIn[i];
        if (c == '&' && i + 1 < n) continue;
        strOut += c;
    }
    return strOut;
}

// "&New World\tCtrl+N" -> text="New World", shortcut="Ctrl+N"
static void MfxMSplitShortcut(const CString &strIn, CString &strText, CString &strShort)
{
    int nTab = strIn.Find('\t');
    CString strLabel = (nTab < 0) ? strIn : strIn.Left(nTab);
    strShort = (nTab < 0) ? CString("") : strIn.Mid(nTab + 1);
    strText = MfxMStripAmp(strLabel);
}

static void MfxMenuParse()
{
    if (g_bParsed) return;
    g_bParsed = 1;
    unsigned nSize = 0;
    const BYTE *pData = MfxFindResourceData(RT_MENU_ID, 2, &nSize);
    if (!pData || nSize < 4) return;
    const BYTE *p = pData + 4;                 // skip wVersion(2)+cbHeaderSize(2), both 0 here
    const BYTE *pEnd = pData + nSize;

    while (p + 2 <= pEnd && g_nTop < MAX_TOP) {
        WORD wFlags = MfxMRdW(p); p += 2;
        if (!(wFlags & 0x10)) break;            // every top-level entry in this menu is a POPUP
        const WORD *pText = (const WORD *)(const void *)p;
        int nLen = 0;
        while (p + 2 <= pEnd && MfxMRdW(p)) { p += 2; nLen++; }
        p += 2;
        MfxMenuTop &top = g_aTop[g_nTop];
        top.strText = MfxMStripAmp(MfxMWideToCStr(pText, nLen));
        top.nFirst = g_nItems;
        top.nCount = 0;
        top.nBarX = top.nBarW = 0;

        for (;;) {
            if (p + 2 > pEnd || g_nItems >= MAX_ITEMS) break;
            WORD iFlags = MfxMRdW(p); p += 2;
            UINT nCmd = 0;
            if (!(iFlags & 0x10)) { nCmd = MfxMRdW(p); p += 2; }   // plain item carries mtID
            const WORD *pItemText = (const WORD *)(const void *)p;
            int nItemLen = 0;
            while (p + 2 <= pEnd && MfxMRdW(p)) { p += 2; nItemLen++; }
            p += 2;
            MfxMenuItem &it = g_aItems[g_nItems++];
            MfxMSplitShortcut(MfxMWideToCStr(pItemText, nItemLen), it.strText, it.strShortcut);
            it.nCmdId = nCmd;
            top.nCount++;
            if (iFlags & 0x80) break;           // MF_END: last item of this popup
        }
        g_nTop++;
        if (wFlags & 0x80) break;               // MF_END on the popup header: last top-level entry
    }
}

// ── chrome bar: its own tiny DC/DIBSection, palette resynced to the live game DC every redraw ──
static int   g_bChromeReady = 0;
static HDC   g_hChromeDC = 0;
static int   g_nChromeW = 0;      // logical width (hit-testing space)
static int   g_nChromePadW = 0;   // DIBSection's actual (4-aligned) width — fill/paint use THIS,
                                   // or the 1-3 padding columns at the right edge are left
                                   // whatever they were allocated as (black) and show through.
static HFONT g_hMenuFont = 0;
static int   g_nMenuOpenTop = -1;

static void MfxMenuBevel(HDC hdc, RECT rc, int bSunken)
{
    COLORREF crHi = bSunken ? GetSysColor(COLOR_BTNSHADOW) : GetSysColor(COLOR_BTNHIGHLIGHT);
    COLORREF crLo = bSunken ? GetSysColor(COLOR_BTNHIGHLIGHT) : GetSysColor(COLOR_BTNSHADOW);
    HPEN hPenHi = ::CreatePen(PS_SOLID, 1, crHi);
    HPEN hPenLo = ::CreatePen(PS_SOLID, 1, crLo);
    HGDIOBJ hOld = ::SelectObject(hdc, (HGDIOBJ)hPenHi);
    ::MoveToEx(hdc, rc.left, rc.bottom - 1, 0);
    ::LineTo(hdc, rc.left, rc.top);
    ::LineTo(hdc, rc.right - 1, rc.top);
    ::SelectObject(hdc, (HGDIOBJ)hPenLo);
    ::MoveToEx(hdc, rc.right - 1, rc.top, 0);
    ::LineTo(hdc, rc.right - 1, rc.bottom - 1);
    ::LineTo(hdc, rc.left - 1, rc.bottom - 1);
    ::SelectObject(hdc, hOld);
    ::DeleteObject((HGDIOBJ)hPenHi);
    ::DeleteObject((HGDIOBJ)hPenLo);
}

static void MfxMenuMeasureBar()
{
    if (!g_hChromeDC || !g_hMenuFont) return;
    HGDIOBJ hOld = ::SelectObject(g_hChromeDC, (HGDIOBJ)g_hMenuFont);
    int x = 6;
    for (int i = 0; i < g_nTop; i++) {
        SIZE sz;
        ::GetTextExtentPoint32A(g_hChromeDC, (const char *)g_aTop[i].strText,
                                g_aTop[i].strText.GetLength(), &sz);
        g_aTop[i].nBarX = x;
        g_aTop[i].nBarW = (int)sz.cx + 16;
        x += g_aTop[i].nBarW;
    }
    ::SelectObject(g_hChromeDC, hOld);
}

static int MfxMenuHitBar(int x)
{
    for (int i = 0; i < g_nTop; i++)
        if (x >= g_aTop[i].nBarX - 4 && x < g_aTop[i].nBarX + g_aTop[i].nBarW - 4) return i;
    return -1;
}

static void MfxMenuPaintBar()
{
    if (!g_bChromeReady) return;
    MFXDIB dibGame;
    if (MfxGetDCDib(MfxScreenDC(), &dibGame))
        ::SetDIBColorTable(g_hChromeDC, 0, 256, dibGame.pPal);

    HDC hdc = g_hChromeDC;
    RECT rcAll = { 0, 0, g_nChromePadW, MFX_MENUBAR_H };
    CBrush brFace(GetSysColor(COLOR_3DFACE));
    ::FillRect(hdc, &rcAll, (HBRUSH)brFace.m_hObject);

    HPEN hPen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
    HGDIOBJ hOldPen = ::SelectObject(hdc, (HGDIOBJ)hPen);
    ::MoveToEx(hdc, 0, MFX_MENUBAR_H - 1, 0);
    ::LineTo(hdc, g_nChromePadW, MFX_MENUBAR_H - 1);
    ::SelectObject(hdc, hOldPen);
    ::DeleteObject((HGDIOBJ)hPen);

    HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)g_hMenuFont);
    TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
    int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < g_nTop; i++) {
        int bOpen = (i == g_nMenuOpenTop);
        RECT rcItem = { g_aTop[i].nBarX - 4, 1, g_aTop[i].nBarX + g_aTop[i].nBarW - 4, MFX_MENUBAR_H - 1 };
        if (bOpen) {
            CBrush brSel(GetSysColor(COLOR_HIGHLIGHT));
            ::FillRect(hdc, &rcItem, (HBRUSH)brSel.m_hObject);
        }
        COLORREF crOld = ::SetTextColor(hdc, bOpen ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                                    : GetSysColor(COLOR_WINDOWTEXT));
        int ty = (MFX_MENUBAR_H - 1 - tm.tmHeight) / 2;
        ::TextOutA(hdc, g_aTop[i].nBarX, ty, (const char *)g_aTop[i].strText, g_aTop[i].strText.GetLength());
        ::SetTextColor(hdc, crOld);
    }
    ::SetBkMode(hdc, nOldMode);
    if (hOldF) ::SelectObject(hdc, hOldF);
}

// ── popup (lives in game screen-DC coordinates; SetCapture-driven, same shape as mfxdlg.cpp) ───
class MfxMenuPopup : public CWnd
{
public:
    int   m_nTop, m_nHot, m_nRows;
    int   m_aY[MAX_ITEMS], m_aH[MAX_ITEMS];
    int   m_aEnabled[MAX_ITEMS], m_aChecked[MAX_ITEMS];
    HFONT m_hFont;
    MfxMenuPopup() : m_nTop(-1), m_nHot(-1), m_nRows(0), m_hFont(0) {}
    virtual void    MfxCtlPaint();
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
};

static MfxMenuPopup *g_pPopup = 0;

static int MfxMenuHitRow(MfxMenuPopup *pPopup, POINT pt)
{
    if (!MfxIsWnd(pPopup->m_hWnd)) return -1;
    RECT rc = pPopup->m_hWnd->rc;
    if (pt.x < rc.left || pt.x >= rc.right || pt.y < rc.top || pt.y >= rc.bottom) return -1;
    int ly = pt.y - rc.top;
    for (int i = 0; i < pPopup->m_nRows; i++)
        if (ly >= pPopup->m_aY[i] && ly < pPopup->m_aY[i] + pPopup->m_aH[i]) return i;
    return -1;
}

static void MfxMenuCloseAll()
{
    if (g_pPopup) {
        if (MfxIsWnd(g_pPopup->m_hWnd)) { ::ReleaseCapture(); ::DestroyWindow(g_pPopup->m_hWnd); }
        delete g_pPopup;
        g_pPopup = 0;
    }
    if (g_nMenuOpenTop >= 0) {
        g_nMenuOpenTop = -1;
        MfxSetDirty();
        // The popup was drawn directly onto the shared game screen DC (same trick as mfxdlg.cpp
        // dialogs); only a real WM_PAINT (CView::OnDraw) clears its footprint. Waiting for the
        // NEXT natural pump-loop tick left a visible one-frame flash of the selected row's
        // highlight color — repaint synchronously, right now, instead of deferring it.
        MfxPaintIfDirty();
    }
}

static void MfxMenuOpenTop(int nTop)
{
    if (nTop < 0 || nTop >= g_nTop) return;
    MfxMenuCloseAll();
    g_nMenuOpenTop = nTop;

    MfxMenuTop &top = g_aTop[nTop];
    HWND hRoot = MfxRootWnd();
    int gameW = hRoot ? hRoot->rc.right - hRoot->rc.left : 640;

    HDC hdc = MfxScreenDC();
    HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)g_hMenuFont);
    TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
    int rowH = tm.tmHeight + 6, sepH = 7;

    MfxMenuPopup *pPopup = new MfxMenuPopup;
    pPopup->m_nTop = nTop;
    pPopup->m_hFont = g_hMenuFont;
    pPopup->m_nRows = top.nCount;

    int y = 3, width = 60;
    for (int i = 0; i < top.nCount; i++) {
        MfxMenuItem &it = g_aItems[top.nFirst + i];
        int bSep = it.strText.IsEmpty();
        int h = bSep ? sepH : rowH;
        pPopup->m_aY[i] = y; pPopup->m_aH[i] = h;
        y += h;
        if (!bSep) {
            SIZE szT; ::GetTextExtentPoint32A(hdc, (const char *)it.strText, it.strText.GetLength(), &szT);
            int w = (int)szT.cx + 36;
            if (it.strShortcut.GetLength()) {
                SIZE szS;
                ::GetTextExtentPoint32A(hdc, (const char *)it.strShortcut, it.strShortcut.GetLength(), &szS);
                w += (int)szS.cx + 16;
            }
            if (w > width) width = w;
        }
    }
    int height = y + 3;
    ::SelectObject(hdc, hOldF);

    int x = top.nBarX - 4;
    if (x + width > gameW) x = gameW - width;
    if (x < 0) x = 0;
    RECT rc = { x, 0, x + width, height };
    pPopup->Create(0, 0, WS_VISIBLE, rc, AfxGetMainWnd(), 0);

    // per-item CN_UPDATE_COMMAND_UI (real MFC's CFrameWnd::OnInitMenuPopup shape, mfxwnd.cpp)
    for (int i = 0; i < top.nCount; i++) {
        MfxMenuItem &it = g_aItems[top.nFirst + i];
        pPopup->m_aEnabled[i] = 1;
        pPopup->m_aChecked[i] = 0;
        if (it.strText.IsEmpty() || !it.nCmdId) continue;
        CCmdUI cmdui; cmdui.m_nID = it.nCmdId;
        MfxQueryCmdUI(&cmdui);
        pPopup->m_aEnabled[i] = cmdui.m_bEnabled;
        pPopup->m_aChecked[i] = cmdui.m_nCheck;
    }

    g_pPopup = pPopup;
    ::SetCapture(pPopup->m_hWnd);
    MfxSetDirty();
}

void MfxMenuPopup::MfxCtlPaint()
{
    if (!MfxIsWnd(m_hWnd)) return;
    HDC hdc = MfxScreenDC();
    RECT rc = m_hWnd->rc;
    MfxTouchHold();
    CBrush brFace(GetSysColor(COLOR_3DFACE));
    ::FillRect(hdc, &rc, (HBRUSH)brFace.m_hObject);
    MfxMenuBevel(hdc, rc, 0);

    MfxMenuTop &top = g_aTop[m_nTop];
    HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)m_hFont);
    TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
    int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
    for (int i = 0; i < m_nRows; i++) {
        MfxMenuItem &it = g_aItems[top.nFirst + i];
        RECT rcRow = { rc.left + 2, rc.top + m_aY[i], rc.right - 2, rc.top + m_aY[i] + m_aH[i] };
        if (it.strText.IsEmpty()) {                        // separator: an etched line, mid-row
            HPEN hPen = ::CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
            HGDIOBJ hOldPen = ::SelectObject(hdc, (HGDIOBJ)hPen);
            int ySep = rcRow.top + (rcRow.bottom - rcRow.top) / 2;
            ::MoveToEx(hdc, rcRow.left + 2, ySep, 0);
            ::LineTo(hdc, rcRow.right - 2, ySep);
            ::SelectObject(hdc, hOldPen);
            ::DeleteObject((HGDIOBJ)hPen);
            continue;
        }
        int bHot = (i == m_nHot) && m_aEnabled[i];
        if (bHot) {
            CBrush brSel(GetSysColor(COLOR_HIGHLIGHT));
            ::FillRect(hdc, &rcRow, (HBRUSH)brSel.m_hObject);
        }
        COLORREF crText = !m_aEnabled[i] ? GetSysColor(COLOR_GRAYTEXT)
                          : bHot          ? GetSysColor(COLOR_HIGHLIGHTTEXT)
                                          : GetSysColor(COLOR_WINDOWTEXT);
        COLORREF crOld = ::SetTextColor(hdc, crText);
        int ty = rcRow.top + (m_aH[i] - tm.tmHeight) / 2;
        if (m_aChecked[i]) {                                // checkmark glyph, left gutter
            HPEN hPen = ::CreatePen(PS_SOLID, 1, crText);
            HGDIOBJ hOldPen = ::SelectObject(hdc, (HGDIOBJ)hPen);
            int cx = rcRow.left + 9, cy = rcRow.top + m_aH[i] / 2;
            ::MoveToEx(hdc, cx - 3, cy, 0);
            ::LineTo(hdc, cx - 1, cy + 3);
            ::LineTo(hdc, cx + 4, cy - 4);
            ::SelectObject(hdc, hOldPen);
            ::DeleteObject((HGDIOBJ)hPen);
        }
        ::TextOutA(hdc, rcRow.left + 20, ty, (const char *)it.strText, it.strText.GetLength());
        if (it.strShortcut.GetLength()) {
            SIZE sz;
            ::GetTextExtentPoint32A(hdc, (const char *)it.strShortcut, it.strShortcut.GetLength(), &sz);
            ::TextOutA(hdc, rcRow.right - 8 - (int)sz.cx, ty,
                      (const char *)it.strShortcut, it.strShortcut.GetLength());
        }
        ::SetTextColor(hdc, crOld);
    }
    ::SetBkMode(hdc, nOldMode);
    if (hOldF) ::SelectObject(hdc, hOldF);
    MfxTouchRelease(hdc);
}

LRESULT MfxMenuPopup::MfxCtlProc(UINT message, WPARAM, LPARAM)
{
    if (!MfxIsWnd(m_hWnd)) return 0;
    switch (message) {
    case WM_PAINT:
        MfxCtlPaint();
        return 0;
    case WM_MOUSEMOVE: {
        int nRow = MfxMenuHitRow(this, g_mfxCursorPos);
        if (nRow != m_nHot) { m_nHot = nRow; MfxCtlPaint(); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int nRow = MfxMenuHitRow(this, g_mfxCursorPos);
        UINT nCmd = 0;
        if (nRow >= 0) {
            MfxMenuTop &top = g_aTop[m_nTop];
            MfxMenuItem &it = g_aItems[top.nFirst + nRow];
            if (!it.strText.IsEmpty() && m_aEnabled[nRow]) nCmd = it.nCmdId;
        }
        HWND hRoot = MfxRootWnd();
        MfxMenuCloseAll();                    // destroys `this` — no member access after this line
        if (nCmd && hRoot) ::PostMessageA(hRoot, WM_COMMAND, MAKELONG(nCmd, 0), 0);
        return 0;
    }
    }
    return 0;
}

// ── public API (mfxwnd.h), called only from mfxpump.cpp ────────────────────────────────────────
void MfxMenuInit()
{
    MfxMenuParse();
    if (!g_nTop || g_bChromeReady) return;
    HWND hRoot = MfxRootWnd();
    g_nChromeW = hRoot ? hRoot->rc.right - hRoot->rc.left : 640;
    g_nChromePadW = (g_nChromeW + 3) & ~3;
    g_hChromeDC = ::CreateCompatibleDC(0);
    struct { BITMAPINFOHEADER h; RGBQUAD pal[256]; } bmi;
    memset(&bmi, 0, sizeof bmi);
    bmi.h.biSize = sizeof(BITMAPINFOHEADER);
    bmi.h.biWidth = g_nChromePadW;
    bmi.h.biHeight = -MFX_MENUBAR_H;
    bmi.h.biPlanes = 1;
    bmi.h.biBitCount = 8;
    bmi.h.biClrUsed = 256;
    for (int i = 0; i < 256; i++)
        bmi.pal[i].rgbRed = bmi.pal[i].rgbGreen = bmi.pal[i].rgbBlue = (BYTE)i;
    void *pBits = 0;
    HBITMAP hDib = ::CreateDIBSection(g_hChromeDC, (BITMAPINFO *)&bmi, 0, &pBits, 0, 0);
    if (hDib) ::SelectObject(g_hChromeDC, hDib);
    g_hMenuFont = ::CreateFontA(-11, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    g_bChromeReady = 1;
    MfxMenuMeasureBar();
}

int MfxMenuGetChromeDib(MFXDIB *pOut)
{
    if (!g_bChromeReady) return 0;
    MfxMenuPaintBar();
    return MfxGetDCDib(g_hChromeDC, pOut);
}

void MfxMenuHandleMouse(UINT message, int x, int /*y*/)
{
    if (!g_bChromeReady) return;
    int nHit = MfxMenuHitBar(x);
    if (message == WM_LBUTTONDOWN) {
        if (nHit < 0) MfxMenuCloseAll();
        else if (g_nMenuOpenTop == nHit) MfxMenuCloseAll();
        else MfxMenuOpenTop(nHit);
    } else if (message == WM_MOUSEMOVE) {
        if (g_pPopup && nHit >= 0 && nHit != g_nMenuOpenTop) MfxMenuOpenTop(nHit);
    }
}

int MfxMenuActive() { return g_pPopup != 0; }
void MfxMenuEscape() { MfxMenuCloseAll(); }
