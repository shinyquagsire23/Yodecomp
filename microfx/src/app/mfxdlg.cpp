// microfx app/ — CDialog::DoModal (H4 M5, docs/phase-h4-sdl.md): the real modal dialog
// manager. Parses an RT_DIALOG template from the embedded .res, creates a child control per
// DLGITEMTEMPLATE (buttons / statics / group boxes / icons / horizontal scrollbars), calls the
// virtual OnInitDialog (→ DoDataExchange → DDX), then runs a Win32-shaped modal message loop
// over the same posted queue GetMessageA drains (M4 lesson 12) until EndDialog fires. Lights up
// the F8 status box (CTextDialog 0xbf), File>About (CAboutDlg 0x64), the Difficulty/
// GameSpeed/WorldSize sliders (0x6f/0xd7/0xda), and Options>Statistics (StatsDlg 0xe1 — the
// game's one DLGTEMPLATEEX resource; the old "corrupt in the .res" note was this format,
// which DoModal now parses alongside the classic shape). Missing/short template → IDCANCEL.
//
// Geometry: DLGTEMPLATE units are dialog-units; base units come from the dialog font (the
// classic MS Sans Serif 8 → 13px strike), px = MulDiv(dlu, baseUnit, 4|8). Controls carry
// ABSOLUTE root-client rects (view at 0,0 → screen == client), so they compose into the one
// shared screen DIB exactly like the M4 bubble controls; the dialog paints its frame as a
// first child (KIND_DLGFRAME), the real controls on top (MfxPaintChildren z-order).

#include <afxwin.h>
#include <microfx.h>
#include "mfxwnd.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>

// SDL3 native file picker (mfxplat.h contract); SDL2/null/DS return -1 → custom picker fallback.
extern "C" int MfxPlatShowFileDialog(int, const char *, const char *, const char *, char *, int);

extern "C" const unsigned char *MfxFindResourceData(unsigned nType, unsigned nId, unsigned *pnSize);
#define RT_DIALOG_ID 5

// standard predefined control-class atoms (DLGITEMTEMPLATE wclass ordinal, 0xffff-prefixed)
enum { ATOM_BUTTON = 0x80, ATOM_EDIT = 0x81, ATOM_STATIC = 0x82,
       ATOM_LISTBOX = 0x83, ATOM_SCROLLBAR = 0x84, ATOM_COMBOBOX = 0x85 };

enum MfxCtlKind { CK_NONE, CK_DLGFRAME, CK_LABEL, CK_FRAME, CK_ICON,
                  CK_BUTTON, CK_DEFBUTTON, CK_GROUPBOX, CK_HSCROLL };

static const int DLG_BORDER  = 3;    // WS_DLGFRAME thickness
static const int DLG_CAPTION = 18;   // caption-bar height
static const int DLG_SB_BTN  = 15;   // horizontal-scrollbar arrow/thumb width

// ── the internal control window ────────────────────────────────────────────────────────────────
class MfxDlgItem : public CWnd
{
public:
    int     m_kind;
    int     m_align;          // labels: 0 left / 1 center / 2 right
    DWORD   m_style;
    CString m_text;
    int     m_iconId;         // SS_ICON member
    HFONT   m_hFont;          // dialog font (shared; owned by DoModal)
    HFONT   m_hFontBold;      // caption font
    int     m_pushed;
    int     m_dragging, m_dragOff;
    MfxDlgItem() : m_kind(CK_NONE), m_align(0), m_style(0), m_iconId(0),
                   m_hFont(0), m_hFontBold(0), m_pushed(0), m_dragging(0), m_dragOff(0) {}
    virtual void    MfxCtlPaint();
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
};

// ── bevels (Win95 3D edges) ────────────────────────────────────────────────────────────────────
static void MfxBevel(HDC hdc, RECT rc, int bSunken)
{
    COLORREF crHi = bSunken ? GetSysColor(16) : GetSysColor(20);   // shadow : hilite (top/left)
    COLORREF crLo = bSunken ? GetSysColor(20) : GetSysColor(16);   // hilite : shadow (bot/right)
    HPEN hPenHi = ::CreatePen(PS_SOLID, 1, crHi);
    HPEN hPenLo = ::CreatePen(PS_SOLID, 1, crLo);
    HGDIOBJ hOld = ::SelectObject(hdc, (HGDIOBJ)hPenHi);
    ::MoveToEx(hdc, rc.left, rc.bottom - 1, 0);
    ::LineTo(hdc, rc.left, rc.top);
    ::LineTo(hdc, rc.right - 1, rc.top);                            // LineTo excludes endpoint
    ::SelectObject(hdc, (HGDIOBJ)hPenLo);
    ::MoveToEx(hdc, rc.right - 1, rc.top, 0);
    ::LineTo(hdc, rc.right - 1, rc.bottom - 1);
    ::LineTo(hdc, rc.left - 1, rc.bottom - 1);
    ::SelectObject(hdc, hOld);
    ::DeleteObject((HGDIOBJ)hPenHi);
    ::DeleteObject((HGDIOBJ)hPenLo);
}

static void MfxFrameRect(HDC hdc, RECT rc, COLORREF cr)
{
    HPEN hPen = ::CreatePen(PS_SOLID, 1, cr);
    HGDIOBJ hOldPen = ::SelectObject(hdc, (HGDIOBJ)hPen);
    HGDIOBJ hOldBr  = ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    ::Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
    ::SelectObject(hdc, hOldBr);
    ::SelectObject(hdc, hOldPen);
    ::DeleteObject((HGDIOBJ)hPen);
}

// draw one text line, horizontally aligned in [rc.left,rc.right]
static void MfxDrawTextLine(HDC hdc, RECT rc, int y, const char *psz, int nLen, int nAlign)
{
    if (nLen <= 0) return;
    SIZE sz; ::GetTextExtentPoint32A(hdc, psz, nLen, &sz);
    int x = rc.left;
    if (nAlign == 1) x = rc.left + ((rc.right - rc.left) - (int)sz.cx) / 2;
    else if (nAlign == 2) x = rc.right - (int)sz.cx;
    ::TextOutA(hdc, x, y, psz, nLen);
}

// ── control paint ────────────────────────────────────────────────────────────────────────────
void MfxDlgItem::MfxCtlPaint()
{
    if (!MfxIsWnd(m_hWnd) || !m_hWnd->bVisible) return;
    HDC hdc = MfxScreenDC();
    RECT rc = m_hWnd->rc;
    MfxTouchHold();

    if (m_kind == CK_DLGFRAME) {
        CBrush brFace(GetSysColor(15));
        ::FillRect(hdc, &rc, (HBRUSH)brFace.m_hObject);            // gray client
        MfxBevel(hdc, rc, 0);                                      // raised outer edge
        RECT rcIn = { rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1 };
        MfxFrameRect(hdc, rcIn, RGB(0, 0, 0));                     // 1px black frame
        if (m_style & WS_CAPTION) {
            RECT rcCap = { rc.left + DLG_BORDER, rc.top + DLG_BORDER,
                           rc.right - DLG_BORDER, rc.top + DLG_BORDER + DLG_CAPTION };
            CBrush brCap(RGB(0, 0, 128));                          // active-caption navy
            ::FillRect(hdc, &rcCap, (HBRUSH)brCap.m_hObject);
            HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)(m_hFontBold ? m_hFontBold : m_hFont));
            int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF crOld = ::SetTextColor(hdc, RGB(255, 255, 255));
            ::TextOutA(hdc, rcCap.left + 4, rcCap.top + 2,
                       (const char *)m_text, m_text.GetLength());
            ::SetTextColor(hdc, crOld);
            ::SetBkMode(hdc, nOldMode);
            if (hOldF) ::SelectObject(hdc, hOldF);
        }
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_ICON) {
        HICON hIcon = ::LoadIconA(0, (LPCSTR)(ULONG_PTR)m_iconId);
        if (hIcon) ::DrawIcon(hdc, rc.left, rc.top, hIcon);
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_FRAME) {
        MfxFrameRect(hdc, rc, RGB(0, 0, 0));                       // SS_BLACKFRAME
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_LABEL) {
        HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)m_hFont);
        TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
        int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF crOld = ::SetTextColor(hdc, RGB(0, 0, 0));
        int nW = rc.right - rc.left;
        const char *psz = (const char *)m_text;
        int nLen = m_text.GetLength(), nStart = 0, y = rc.top;
        for (int i = 0; i <= nLen; i++) {           // split on hard newlines first
            if (i != nLen && psz[i] != '\n') continue;
            int nRun = i - nStart;
            if (nRun && psz[nStart + nRun - 1] == '\r') nRun--;   // strip CR
            // word-wrap this hard line at the control width (Win32 static default)
            int nPos = nStart, nEndRun = nStart + nRun;
            while (nPos < nEndRun) {
                int nFit = 0, nLastSpace = -1;
                while (nPos + nFit < nEndRun) {
                    SIZE sz; ::GetTextExtentPoint32A(hdc, psz + nPos, nFit + 1, &sz);
                    if ((int)sz.cx > nW && nFit > 0) break;
                    if (psz[nPos + nFit] == ' ') nLastSpace = nFit;
                    nFit++;
                }
                int nLineLen, nAdvance;
                if (nPos + nFit >= nEndRun) { nLineLen = nEndRun - nPos; nAdvance = nLineLen; }
                else if (nLastSpace >= 0) { nLineLen = nLastSpace; nAdvance = nLastSpace + 1; }
                else { nLineLen = nFit; nAdvance = nFit; }
                MfxDrawTextLine(hdc, rc, y, psz + nPos, nLineLen, m_align);
                y += tm.tmHeight;
                nPos += nAdvance;
            }
            if (nRun == 0) y += tm.tmHeight;         // blank hard line
            nStart = i + 1;
        }
        ::SetTextColor(hdc, crOld);
        ::SetBkMode(hdc, nOldMode);
        if (hOldF) ::SelectObject(hdc, hOldF);
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_GROUPBOX) {
        RECT rcBox = { rc.left, rc.top + 4, rc.right, rc.bottom };
        MfxBevel(hdc, rcBox, 1);                                   // etched groove
        if (m_text.GetLength()) {
            HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)m_hFont);
            SIZE sz; ::GetTextExtentPoint32A(hdc, (const char *)m_text, m_text.GetLength(), &sz);
            RECT rcLbl = { rc.left + 6, rc.top, rc.left + 6 + (int)sz.cx + 4, rc.top + 10 };
            CBrush brFace(GetSysColor(15));
            ::FillRect(hdc, &rcLbl, (HBRUSH)brFace.m_hObject);     // gap in the groove
            int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
            COLORREF crOld = ::SetTextColor(hdc, RGB(0, 0, 0));
            ::TextOutA(hdc, rc.left + 8, rc.top, (const char *)m_text, m_text.GetLength());
            ::SetTextColor(hdc, crOld);
            ::SetBkMode(hdc, nOldMode);
            if (hOldF) ::SelectObject(hdc, hOldF);
        }
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_BUTTON || m_kind == CK_DEFBUTTON) {
        RECT rcBtn = rc;
        if (m_kind == CK_DEFBUTTON) {                             // def button: black surround
            MfxFrameRect(hdc, rcBtn, RGB(0, 0, 0));
            rcBtn.left++; rcBtn.top++; rcBtn.right--; rcBtn.bottom--;
        }
        CBrush brFace(GetSysColor(15));
        ::FillRect(hdc, &rcBtn, (HBRUSH)brFace.m_hObject);
        MfxBevel(hdc, rcBtn, m_pushed);
        HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)m_hFont);
        TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
        SIZE sz; ::GetTextExtentPoint32A(hdc, (const char *)m_text, m_text.GetLength(), &sz);
        int tx = rcBtn.left + ((rcBtn.right - rcBtn.left) - (int)sz.cx) / 2 + (m_pushed ? 1 : 0);
        int ty = rcBtn.top + ((rcBtn.bottom - rcBtn.top) - tm.tmHeight) / 2 + (m_pushed ? 1 : 0);
        int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
        COLORREF crOld = ::SetTextColor(hdc, RGB(0, 0, 0));
        ::TextOutA(hdc, tx, ty, (const char *)m_text, m_text.GetLength());
        ::SetTextColor(hdc, crOld);
        ::SetBkMode(hdc, nOldMode);
        if (hOldF) ::SelectObject(hdc, hOldF);
        MfxTouchRelease(hdc);
        return;
    }

    if (m_kind == CK_HSCROLL) {
        int nW = rc.right - rc.left;
        COLORREF crFace = GetSysColor(15);
        for (int y = rc.top; y < rc.bottom; y++)                  // Win95 checker track
            for (int x = rc.left; x < rc.right; x++)
                ::SetPixel(hdc, x, y, ((x ^ y) & 1) ? RGB(255, 255, 255) : crFace);
        RECT rcL = { rc.left, rc.top, rc.left + DLG_SB_BTN, rc.bottom };
        RECT rcR = { rc.right - DLG_SB_BTN, rc.top, rc.right, rc.bottom };
        CBrush brFace(crFace);
        ::FillRect(hdc, &rcL, (HBRUSH)brFace.m_hObject); MfxBevel(hdc, rcL, 0);
        ::FillRect(hdc, &rcR, (HBRUSH)brFace.m_hObject); MfxBevel(hdc, rcR, 0);
        int cyMid = (rc.top + rc.bottom) / 2;
        int cxL = rc.left + DLG_SB_BTN / 2, cxR = rc.right - DLG_SB_BTN / 2;
        for (int i = 0; i < 4; i++) {                             // arrow glyphs
            for (int y = cyMid - i; y <= cyMid + i; y++) {
                ::SetPixel(hdc, cxL + 2 - i, y, RGB(0, 0, 0));
                ::SetPixel(hdc, cxR - 2 + i, y, RGB(0, 0, 0));
            }
        }
        int nRange = m_hWnd->nScrollMax - m_hWnd->nScrollMin;
        int nTravel = nW - 3 * DLG_SB_BTN;
        if (nRange > 0 && nTravel > 0) {
            int nLeft = DLG_SB_BTN + (m_hWnd->nScrollPos - m_hWnd->nScrollMin) * nTravel / nRange;
            RECT rcThumb = { rc.left + nLeft, rc.top, rc.left + nLeft + DLG_SB_BTN, rc.bottom };
            ::FillRect(hdc, &rcThumb, (HBRUSH)brFace.m_hObject); MfxBevel(hdc, rcThumb, 0);
        }
        MfxTouchRelease(hdc);
        return;
    }
    MfxTouchRelease(hdc);
}

// ── control mouse / text handling ─────────────────────────────────────────────────────────────
LRESULT MfxDlgItem::MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!MfxIsWnd(m_hWnd)) return 0;

    if (m_kind == CK_BUTTON || m_kind == CK_DEFBUTTON) {
        switch (message) {
        case WM_LBUTTONDOWN:
            ::SetCapture(m_hWnd); m_pushed = 1; MfxCtlPaint(); return 0;
        case WM_LBUTTONUP:
            if (m_pushed) {
                m_pushed = 0; ::ReleaseCapture(); MfxCtlPaint();
                POINT pt = g_mfxCursorPos;
                if (pt.x >= m_hWnd->rc.left && pt.x < m_hWnd->rc.right &&
                    pt.y >= m_hWnd->rc.top && pt.y < m_hWnd->rc.bottom)
                    ::PostMessageA(m_hWnd->hParent, WM_COMMAND,
                                   MAKELONG(m_hWnd->nId, BN_CLICKED), (LPARAM)m_hWnd);
            }
            return 0;
        case WM_SETTEXT:
            m_text = lParam ? (LPCSTR)lParam : ""; MfxCtlPaint(); return TRUE;
        case WM_PAINT:
            MfxCtlPaint(); return 0;
        }
        return 0;
    }

    if (m_kind == CK_HSCROLL) {
        HWND hParent = m_hWnd->hParent;
        RECT rc = m_hWnd->rc;
        int nW = rc.right - rc.left;
        int nTravel = nW - 3 * DLG_SB_BTN;
        int nRange = m_hWnd->nScrollMax - m_hWnd->nScrollMin;
        int nThumbLeft = (nRange > 0 && nTravel > 0)
            ? DLG_SB_BTN + (m_hWnd->nScrollPos - m_hWnd->nScrollMin) * nTravel / nRange : -1;
        switch (message) {
        case WM_LBUTTONDOWN: {
            int x = (int)(short)LOWORD(lParam) - rc.left;
            UINT nCode = (UINT)-1;
            if (x < DLG_SB_BTN) nCode = SB_LINEUP;
            else if (x >= nW - DLG_SB_BTN) nCode = SB_LINEDOWN;
            else if (nThumbLeft >= 0 && x >= nThumbLeft && x < nThumbLeft + DLG_SB_BTN) {
                m_dragging = 1; m_dragOff = x - nThumbLeft; ::SetCapture(m_hWnd); return 0;
            }
            else if (nThumbLeft >= 0) nCode = (x < nThumbLeft) ? SB_PAGEUP : SB_PAGEDOWN;
            if (nCode != (UINT)-1)
                MfxSendMsg(hParent, WM_HSCROLL, MAKELONG(nCode, 0), (LPARAM)m_hWnd);
            return 0;
        }
        case WM_MOUSEMOVE:
            if (m_dragging && nTravel > 0 && nRange > 0) {
                int x = (int)(short)LOWORD(lParam) - rc.left - m_dragOff - DLG_SB_BTN;
                int nPos = m_hWnd->nScrollMin + (x * nRange + nTravel / 2) / nTravel;
                if (nPos < m_hWnd->nScrollMin) nPos = m_hWnd->nScrollMin;
                if (nPos > m_hWnd->nScrollMax) nPos = m_hWnd->nScrollMax;
                MfxSendMsg(hParent, WM_HSCROLL, MAKELONG(SB_THUMBTRACK, nPos), (LPARAM)m_hWnd);
            }
            return 0;
        case WM_LBUTTONUP:
            if (m_dragging) {
                m_dragging = 0; ::ReleaseCapture();
                MfxSendMsg(hParent, WM_HSCROLL,
                           MAKELONG(SB_THUMBPOSITION, m_hWnd->nScrollPos), (LPARAM)m_hWnd);
            }
            return 0;
        case WM_PAINT:
            MfxCtlPaint(); return 0;
        }
        return 0;
    }

    // statics / frames / group boxes: display-only, but honor WM_SETTEXT (DDX / SetDlgItemText)
    if (message == WM_SETTEXT) { m_text = lParam ? (LPCSTR)lParam : ""; MfxCtlPaint(); return TRUE; }
    if (message == WM_PAINT) { MfxCtlPaint(); return 0; }
    return 0;
}

// ── DLGTEMPLATE parse (mirrors tools/reslib.py parse_dlg32) ─────────────────────────────────────
struct MfxDlgHdr { DWORD style; int x, y, cx, cy; int nCtrl; const WORD *pCaption; };
struct MfxDlgCtl { DWORD style; int x, y, cx, cy; UINT id; UINT atom; const WORD *pClassSz;
                   int textIsOrd; UINT textOrd; const WORD *pTextSz; };

static WORD MfxRdW(const BYTE *p) { WORD v; memcpy(&v, p, 2); return v; }
static DWORD MfxRdD(const BYTE *p) { DWORD v; memcpy(&v, p, 4); return v; }

// a 0x0000/0xFFFF+ord/UTF-16Z name field: advance p, report whether it was an ordinal
static const BYTE *MfxSkipName(const BYTE *p, int *pIsOrd, UINT *pOrd, const WORD **ppSz)
{
    WORD w = MfxRdW(p);
    if (pIsOrd) *pIsOrd = 0; if (pOrd) *pOrd = 0; if (ppSz) *ppSz = 0;
    if (w == 0) return p + 2;
    if (w == 0xffff) { if (pIsOrd) *pIsOrd = 1; if (pOrd) *pOrd = MfxRdW(p + 2); return p + 4; }
    if (ppSz) *ppSz = (const WORD *)p;
    while (MfxRdW(p)) p += 2;
    return p + 2;
}

static CString MfxWideToCString(const WORD *pw)   // ASCII subset (the game's dialog strings)
{
    CString r;
    if (!pw) return r;
    int n = 0; while (pw[n]) n++;
    char *s = (char *)malloc(n + 1);
    for (int i = 0; i < n; i++) s[i] = (pw[i] && pw[i] < 0x100) ? (char)pw[i] : '?';
    s[n] = 0; r = s; free(s);
    return r;
}

// ── DDX (C++ linkage: overloaded, matches the afxwin.h declarations) ────────────────────────────
void AFXAPI DDX_Text(CDataExchange *pDX, int nIDC, CString &value)
{
    if (!pDX || !pDX->m_pDlgWnd) return;
    MfxDlgItem *p = (MfxDlgItem *)pDX->m_pDlgWnd->GetDlgItem(nIDC);
    if (!p) return;
    if (pDX->m_bSaveAndValidate) value = p->m_text;
    else { p->m_text = value; if (MfxIsWnd(p->m_hWnd) && p->m_hWnd->bVisible) p->MfxCtlPaint(); }
}
void AFXAPI DDX_Text(CDataExchange *pDX, int nIDC, int &value)
{
    if (!pDX || !pDX->m_pDlgWnd) return;
    MfxDlgItem *p = (MfxDlgItem *)pDX->m_pDlgWnd->GetDlgItem(nIDC);
    if (!p) return;
    if (pDX->m_bSaveAndValidate) value = atoi((const char *)p->m_text);
    else { char buf[32]; sprintf(buf, "%d", value); p->m_text = buf;
           if (MfxIsWnd(p->m_hWnd) && p->m_hWnd->bVisible) p->MfxCtlPaint(); }
}
void AFXAPI DDX_Control(CDataExchange *, int, CWnd &) {}
void AFXAPI DDX_Check(CDataExchange *, int, int &) {}

// ── the modal loop plumbing ─────────────────────────────────────────────────────────────────────
extern "C" {
BOOL GetMessageA(LPMSG pMsg, HWND, UINT, UINT);
LRESULT DispatchMessageA(const MSG *pMsg);
}

static int MfxIsDlgOwned(HWND h, HWND hDlg)
{
    return MfxIsWnd(h) && (h == hDlg || h->hParent == hDlg);
}

// ── DoModal ─────────────────────────────────────────────────────────────────────────────────────
int CDialog::DoModal()
{
    unsigned nSize = 0;
    const BYTE *pT = MfxFindResourceData(RT_DIALOG_ID, m_nIDTemplate, &nSize);
    if (getenv("YODA_DLGTRACE"))
        fprintf(stderr, "DLGTRACE DoModal id=0x%x tpl=%p size=%u\n", m_nIDTemplate, (void*)pT, nSize);
    if (!pT || nSize < 18) { m_nModalResult = IDCANCEL; return IDCANCEL; }

    // header — classic DLGTEMPLATE or DLGTEMPLATEEX (dlgVer=1, signature=0xFFFF; the Stats
    // dialog 0xe1 is the game's one EX template — v80's "corrupt in the shipped .res" was
    // actually this format, which real Windows parses natively)
    int bEx = (MfxRdW(pT) == 1 && MfxRdW(pT + 2) == 0xffff);
    MfxDlgHdr hdr;
    const BYTE *p;
    if (bEx) {
        hdr.style = MfxRdD(pT + 12);               // dlgVer(2)+sig(2)+helpID(4)+exStyle(4)
        hdr.nCtrl = MfxRdW(pT + 16);
        p = pT + 18;
    } else {
        hdr.style = MfxRdD(pT);
        hdr.nCtrl = MfxRdW(pT + 8);
        p = pT + 10;                               // skip style(4)+ext(4)+cdit(2)
    }
    hdr.x = (short)MfxRdW(p);       hdr.y = (short)MfxRdW(p + 2);
    hdr.cx = (short)MfxRdW(p + 4);  hdr.cy = (short)MfxRdW(p + 6); p += 8;
    p = MfxSkipName(p, 0, 0, 0);                   // menu
    p = MfxSkipName(p, 0, 0, 0);                   // class
    hdr.pCaption = (const WORD *)p;                // caption (UTF-16Z)
    while (MfxRdW(p)) p += 2; p += 2;
    if (hdr.style & 0x40) {                        // DS_SETFONT: point size [+EX: weight(2)+
        p += bEx ? 6 : 2;                          //  italic(1)+charset(1)] + face name
        while (MfxRdW(p)) p += 2; p += 2;
    }
    if (hdr.cx <= 0 || hdr.cy <= 0) { m_nModalResult = IDCANCEL; return IDCANCEL; }

    // base units from the dialog font (classic MS Sans Serif 8 → 13px strike)
    HDC hdc = MfxScreenDC();
    HFONT hFont = ::CreateFontA(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    HFONT hFontBold = ::CreateFontA(-8, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)hFont);
    SIZE szAvg; ::GetTextExtentPoint32A(hdc,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 52, &szAvg);
    TEXTMETRIC tm; ::GetTextMetricsA(hdc, &tm);
    ::SelectObject(hdc, hOldF);
    int baseX = ((int)szAvg.cx / 26 + 1) / 2; if (baseX < 1) baseX = 6;
    int baseY = tm.tmHeight;                        if (baseY < 1) baseY = 13;
#define DLU_X(v) ((int)(((long)(v) * baseX + 2) / 4))
#define DLU_Y(v) ((int)(((long)(v) * baseY + 4) / 8))

    int nCap = (hdr.style & WS_CAPTION) == WS_CAPTION ? DLG_CAPTION : 0;
    int clientW = DLU_X(hdr.cx), clientH = DLU_Y(hdr.cy);
    int outerW = clientW + 2 * DLG_BORDER;
    int outerH = clientH + 2 * DLG_BORDER + nCap;
    HWND hRoot = MfxRootWnd();
    int rootW = hRoot ? hRoot->rc.right - hRoot->rc.left : 525;
    int rootH = hRoot ? hRoot->rc.bottom - hRoot->rc.top : 310;
    int winX = (rootW - outerW) / 2; if (winX < 0) winX = 0;
    int winY = (rootH - outerH) / 2; if (winY < 0) winY = 0;
    int cliX = winX + DLG_BORDER, cliY = winY + DLG_BORDER + nCap;

    CWnd *pParent = m_pParentWnd ? m_pParentWnd : AfxGetMainWnd();
    RECT rcDlg = { winX, winY, winX + outerW, winY + outerH };
    Create(0, 0, WS_VISIBLE, rcDlg, pParent, 0);     // the dialog window (this)

    // frame item first (drawn under the controls)
    MfxDlgItem *aItems[40];
    int nItems = 0;
    {
        MfxDlgItem *pFrame = new MfxDlgItem;
        pFrame->m_kind = CK_DLGFRAME; pFrame->m_style = hdr.style;
        pFrame->m_hFont = hFont; pFrame->m_hFontBold = hFontBold;
        pFrame->m_text = MfxWideToCString(hdr.pCaption);
        pFrame->Create(0, 0, WS_VISIBLE, rcDlg, this, 0);
        aItems[nItems++] = pFrame;
    }

    // controls (EX items: helpID(4)+exStyle(4)+style(4), coords, then a DWORD id)
    for (int i = 0; i < hdr.nCtrl && nItems < 40; i++) {
        p = (const BYTE *)(((ULONG_PTR)p + 3) & ~(ULONG_PTR)3);
        DWORD cst;
        if (bEx) { cst = MfxRdD(p + 8); p += 12; }
        else     { cst = MfxRdD(p);     p += 8;  }  // style(4) + ext(4)
        int ix = (short)MfxRdW(p),     iy = (short)MfxRdW(p + 2);
        int icx = (short)MfxRdW(p + 4), icy = (short)MfxRdW(p + 6);
        UINT iid = MfxRdW(p + 8); p += bEx ? 12 : 10;
        int bClsOrd; UINT clsOrd; const WORD *pClsSz;
        p = MfxSkipName(p, &bClsOrd, &clsOrd, &pClsSz);
        int bTxtOrd; UINT txtOrd; const WORD *pTxtSz;
        p = MfxSkipName(p, &bTxtOrd, &txtOrd, &pTxtSz);
        WORD nExtra = MfxRdW(p); p += 2 + nExtra;    // creation data

        UINT atom = bClsOrd ? clsOrd : 0;
        MfxDlgItem *pC = new MfxDlgItem;
        pC->m_style = cst; pC->m_hFont = hFont; pC->m_hFontBold = hFontBold;
        pC->m_align = 0; pC->m_iconId = 0;
        int bVisible = 1;
        if (atom == ATOM_STATIC) {
            UINT ss = cst & SS_TYPEMASK;
            if (ss == SS_ICON) { pC->m_kind = CK_ICON; pC->m_iconId = bTxtOrd ? (int)txtOrd : 0; }
            else if (ss >= SS_BLACKFRAME && ss <= SS_WHITEFRAME) pC->m_kind = CK_FRAME;
            else if (ss >= SS_BLACKRECT && ss <= SS_WHITERECT) { pC->m_kind = CK_FRAME; }
            else { pC->m_kind = CK_LABEL; pC->m_align = (ss == SS_CENTER) ? 1 : (ss == SS_RIGHT) ? 2 : 0; }
            if (!bTxtOrd) pC->m_text = MfxWideToCString(pTxtSz);
        }
        else if (atom == ATOM_BUTTON) {
            UINT bs = cst & BS_TYPEMASK;
            if (bs == BS_GROUPBOX) pC->m_kind = CK_GROUPBOX;
            else if (bs == BS_DEFPUSHBUTTON) pC->m_kind = CK_DEFBUTTON;
            else pC->m_kind = CK_BUTTON;
            if (!bTxtOrd) pC->m_text = MfxWideToCString(pTxtSz);
        }
        else if (atom == ATOM_SCROLLBAR) pC->m_kind = CK_HSCROLL;
        else { pC->m_kind = CK_LABEL; if (!bTxtOrd) pC->m_text = MfxWideToCString(pTxtSz); }

        RECT rcC = { cliX + DLU_X(ix), cliY + DLU_Y(iy),
                     cliX + DLU_X(ix) + DLU_X(icx), cliY + DLU_Y(iy) + DLU_Y(icy) };
        pC->Create(0, 0, bVisible ? WS_VISIBLE : 0, rcC, this, iid);
        aItems[nItems++] = pC;
    }

    // OnInitDialog (virtual: base UpdateData(FALSE)→DDX; subclass CenterWindow + SetScrollRange)
    OnInitDialog();

    // OnInitDialog done → present the dialog immediately (before the possibly-blocking wait).
    // YODA_DLGSHOT=<path> dumps the composited screen DIB once (YODA_SHOT stalls in modal loops).
    MfxSetDirty();
    MfxPaintIfDirty();
    if (const char *pszShot = getenv("YODA_DLGSHOT")) MfxWriteDibBMP(MfxScreenDC(), pszShot);

    // modal loop over the shared queue (Win32-shaped; headless GetMessageA returns 0 → auto-cancel)
    m_nModalResult = -1;
    while (m_nModalResult == -1) {
        MSG msg;
        if (!GetMessageA(&msg, 0, 0, 0)) { m_nModalResult = IDCANCEL; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN)      { OnOK(); continue; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)      { OnCancel(); continue; }
        // modal: swallow input aimed at windows outside the dialog subtree
        if ((msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP ||
             msg.message == WM_RBUTTONDOWN || msg.message == WM_MOUSEMOVE ||
             msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_CHAR) &&
            !MfxIsDlgOwned(msg.hwnd, m_hWnd))
            continue;
        DispatchMessageA(&msg);
    }

    // teardown: destroy controls + frame + dialog, delete C++ objects, free fonts, repaint view
    for (int i = nItems - 1; i >= 0; i--) {
        if (MfxIsWnd(aItems[i]->m_hWnd)) ::DestroyWindow(aItems[i]->m_hWnd);
        delete aItems[i];
    }
    if (MfxIsWnd(m_hWnd)) ::DestroyWindow(m_hWnd);
    ::DeleteObject((HGDIOBJ)hFont);
    ::DeleteObject((HGDIOBJ)hFontBold);
    MfxSetDirty(); MfxPaintIfDirty();   // full erase+OnDraw NOW (see note at MfxShowMessageBox)
    return m_nModalResult;
}

// ── CFileDialog::DoModal (H4 M5 tail, docs/phase-h4-sdl.md) ─────────────────────────────────────
// SDL has no native common-file dialog, so this reuses the control kit above (DLGFRAME bevel +
// CK_BUTTON rows) instead of parsing a DLGTEMPLATE: it lists *.<ext> files found in the save
// directory as clickable rows — Save adds a leading "(new)" row for the caller's default
// filename, Load lists only what exists — and a click commits immediately. No text-entry
// control exists yet (M5 didn't add ATOM_EDIT), so renaming isn't offered; that's fine, the
// game only ever asks for a save SLOT, not a name.
// not static: exercised directly by the microfx/harness/dlg_smoke unit test (the modal loop
// itself can't be driven headlessly — GetMessageA bails instantly with no SDL window, M4
// lesson 12 — so the pure list/resolve logic is what a headless harness CAN verify).
int MfxFileDialogScan(const char *pszDir, const char *pszExt, CString aNames[], int nMax)
{
    int nCount = 0;
    DIR *pDir = opendir(pszDir && *pszDir ? pszDir : ".");
    if (!pDir) return 0;
    size_t nExtLen = strlen(pszExt);
    struct dirent *pEnt;
    while (nCount < nMax && (pEnt = readdir(pDir)) != 0) {
        size_t nLen = strlen(pEnt->d_name);
        if (nLen <= nExtLen + 1 || pEnt->d_name[nLen - nExtLen - 1] != '.') continue;
        if (strcasecmp(pEnt->d_name + nLen - nExtLen, pszExt) != 0) continue;
        aNames[nCount++] = pEnt->d_name;
    }
    closedir(pDir);
    return nCount;
}

// row 0..N-1 = one button per listed file (Save also gets a leading "(new)" row for the
// caller's default filename, unless that name is already one of the listed files).
int MfxFileDialogBuildRows(int bOpenFileDialog, const CString &strDefault,
                           const CString aFound[], int nFound,
                           CString aRow[], UINT aRowId[], int nMaxRows, UINT idNew, UINT idRow0)
{
    int nRows = 0;
    if (!bOpenFileDialog) {
        int bHasDefault = 0;
        for (int i = 0; i < nFound; i++) if (aFound[i] == strDefault) bHasDefault = 1;
        if (!bHasDefault && nRows < nMaxRows) {
            aRow[nRows] = strDefault + "  (new)"; aRowId[nRows] = idNew; nRows++;
        }
        for (int i = 0; i < nFound && nRows < nMaxRows; i++, nRows++) {
            aRow[nRows] = (aFound[i] == strDefault) ? aFound[i] + "  (overwrite)" : aFound[i];
            aRowId[nRows] = idRow0 + i;
        }
    } else {
        for (int i = 0; i < nFound && nRows < nMaxRows; i++, nRows++) {
            aRow[nRows] = aFound[i]; aRowId[nRows] = idRow0 + i;
        }
    }
    return nRows;
}

// clicked row id -> full path. Falls back to strDefault for an out-of-range id (defensive:
// the modal loop scopes WM_COMMAND to msg.hwnd==m_hWnd before calling this, but a stray
// accelerator-originated command sharing the BN_CLICKED=0 HIWORD must never index OOB).
CString MfxFileDialogResolve(UINT nChosenId, UINT idNew, UINT idRow0, const CString &strDefault,
                             const CString aFound[], int nFound, const char *pszDir)
{
    CString strName;
    int nIdx = (int)nChosenId - (int)idRow0;
    if (nChosenId == idNew) strName = strDefault;
    else if (nIdx >= 0 && nIdx < nFound) strName = aFound[nIdx];
    else strName = strDefault;

    CString strPath = pszDir;
    if (strPath.GetLength() && strPath[strPath.GetLength() - 1] != '/') strPath += "/";
    strPath += strName;
    return strPath;
}

int CFileDialog::DoModal()
{
    // Real Windows' common file dialog silently falls back to the working directory when
    // lpstrInitialDir names a directory that doesn't exist — which is exactly what it's handed
    // here: CDeskcppDoc::CDeskcppDoc's registry-Install-Path-else-scan-fixed-drives fallback
    // computes a real-looking guess on real Windows, but on this port RegOpenKeyExA always
    // fails and GetDriveTypeA always reports DRIVE_NO_ROOT_DIR (no real registry/drives to
    // probe), so the scan exhausts and the "directory" ends up literally "YODA". Match Windows'
    // graceful fallback here instead of silently scanning (and saving into) a directory that
    // was never going to exist — used for BOTH the row-list scan and the resolved save path
    // below, so Save and Load always agree on the same real directory.
    const char *pszDir = (m_ofn.lpstrInitialDir && *m_ofn.lpstrInitialDir) ? m_ofn.lpstrInitialDir : ".";
    if (DIR *pProbe = opendir(pszDir)) closedir(pProbe); else pszDir = ".";
    const char *pszExt = m_ofn.lpstrDefExt ? (const char *)m_ofn.lpstrDefExt : "wld";

    enum { MAX_FOUND = 8, ID_NEW = 90, ID_ROW0 = 100 };
    CString aFound[MAX_FOUND];

    CString strDefault = m_strPath;
    if (strDefault.Find('.') < 0) { strDefault += "."; strDefault += pszExt; }

    // Native OS picker first (SDL3 has one; SDL2/null/DS return -1 → fall through to the
    // in-window row-list picker below). This is the user-requested SDL3 SDL_ShowOpen/SaveFileDialog
    // path — it blocks inside the backend until the panel closes, then we're done.
    {
        char szPath[1024] = { 0 };
        int nNative = MfxPlatShowFileDialog(m_bOpenFileDialog, pszDir, pszExt,
                                            (const char *)strDefault, szPath, (int)sizeof szPath);
        if (nNative >= 0) {                       // backend HAS a native picker
            MfxSetDirty(); MfxPaintIfDirty();     // panel obscured the game window — repaint NOW
            if (nNative == 1) { m_strPath = szPath; m_nModalResult = IDOK; return IDOK; }
            m_nModalResult = IDCANCEL; return IDCANCEL;
        }
    }

    int nFound = MfxFileDialogScan(pszDir, pszExt, aFound, MAX_FOUND);

    CString aRow[MAX_FOUND + 1]; UINT aRowId[MAX_FOUND + 1];
    int nRows = MfxFileDialogBuildRows(m_bOpenFileDialog, strDefault, aFound, nFound,
                                       aRow, aRowId, MAX_FOUND + 1, ID_NEW, ID_ROW0);

    HFONT hFont = ::CreateFontA(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    HFONT hFontBold = ::CreateFontA(-8, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");

    const int ROW_H = 18, PAD = 8, BTN_W = 60, BTN_H = 16;
    int clientW = 220;
    int nRowsShown = nRows > 0 ? nRows : 1;
    int clientH = PAD + nRowsShown * ROW_H + PAD + BTN_H + PAD;
    int outerW = clientW + 2 * DLG_BORDER;
    int outerH = clientH + 2 * DLG_BORDER + DLG_CAPTION;
    HWND hRoot = MfxRootWnd();
    int rootW = hRoot ? hRoot->rc.right - hRoot->rc.left : 525;
    int rootH = hRoot ? hRoot->rc.bottom - hRoot->rc.top : 310;
    int winX = (rootW - outerW) / 2; if (winX < 0) winX = 0;
    int winY = (rootH - outerH) / 2; if (winY < 0) winY = 0;
    int cliX = winX + DLG_BORDER, cliY = winY + DLG_BORDER + DLG_CAPTION;

    CWnd *pParent = m_pParentWnd ? m_pParentWnd : AfxGetMainWnd();
    RECT rcDlg = { winX, winY, winX + outerW, winY + outerH };
    Create(0, 0, WS_VISIBLE, rcDlg, pParent, 0);

    MfxDlgItem *aItems[16]; int nItems = 0;
    MfxDlgItem *pFrame = new MfxDlgItem;
    pFrame->m_kind = CK_DLGFRAME; pFrame->m_style = WS_CAPTION;
    pFrame->m_hFont = hFont; pFrame->m_hFontBold = hFontBold;
    pFrame->m_text = m_bOpenFileDialog ? "Load World" : "Save World";
    pFrame->Create(0, 0, WS_VISIBLE, rcDlg, this, 0);
    aItems[nItems++] = pFrame;

    if (nRows == 0) {
        MfxDlgItem *pLbl = new MfxDlgItem;
        pLbl->m_kind = CK_LABEL; pLbl->m_align = 1; pLbl->m_hFont = hFont;
        pLbl->m_text = "No saved games found.";
        RECT rc = { cliX + PAD, cliY + PAD, cliX + clientW - PAD, cliY + PAD + ROW_H };
        pLbl->Create(0, 0, WS_VISIBLE, rc, this, 0);
        aItems[nItems++] = pLbl;
    }
    for (int i = 0; i < nRows && nItems < 15; i++) {
        MfxDlgItem *pBtn = new MfxDlgItem;
        pBtn->m_kind = CK_BUTTON; pBtn->m_hFont = hFont; pBtn->m_text = aRow[i];
        RECT rc = { cliX + PAD, cliY + PAD + i * ROW_H,
                    cliX + clientW - PAD, cliY + PAD + i * ROW_H + ROW_H - 2 };
        pBtn->Create(0, 0, WS_VISIBLE, rc, this, aRowId[i]);
        aItems[nItems++] = pBtn;
    }
    MfxDlgItem *pCancel = new MfxDlgItem;
    pCancel->m_kind = CK_BUTTON; pCancel->m_hFont = hFont; pCancel->m_text = "Cancel";
    RECT rcCancel = { cliX + clientW - PAD - BTN_W, cliY + clientH - PAD - BTN_H,
                       cliX + clientW - PAD, cliY + clientH - PAD };
    pCancel->Create(0, 0, WS_VISIBLE, rcCancel, this, IDCANCEL);
    aItems[nItems++] = pCancel;

    MfxSetDirty();
    MfxPaintIfDirty();
    if (const char *pszShot = getenv("YODA_DLGSHOT")) MfxWriteDibBMP(MfxScreenDC(), pszShot);

    UINT nChosenId = 0;
    m_nModalResult = -1;
    while (m_nModalResult == -1) {
        MSG msg;
        if (!GetMessageA(&msg, 0, 0, 0)) { m_nModalResult = IDCANCEL; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { m_nModalResult = IDCANCEL; break; }
        // msg.hwnd==m_hWnd scopes this to OUR row/Cancel buttons (they post WM_COMMAND to their
        // parent, i.e. this dialog) — a menu accelerator's WM_COMMAND(cmdId,0) to the FRAME
        // shares BN_CLICKED's HIWORD==0 and must NOT be mistaken for a row click (would index
        // aFound[] with a garbage id).
        if (msg.message == WM_COMMAND && msg.hwnd == m_hWnd && HIWORD(msg.wParam) == BN_CLICKED) {
            UINT id = LOWORD(msg.wParam);
            m_nModalResult = (id == IDCANCEL) ? IDCANCEL : IDOK; nChosenId = id;
            break;
        }
        if ((msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP ||
             msg.message == WM_RBUTTONDOWN || msg.message == WM_MOUSEMOVE ||
             msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_CHAR) &&
            !MfxIsDlgOwned(msg.hwnd, m_hWnd))
            continue;
        DispatchMessageA(&msg);
    }

    for (int i = nItems - 1; i >= 0; i--) {
        if (MfxIsWnd(aItems[i]->m_hWnd)) ::DestroyWindow(aItems[i]->m_hWnd);
        delete aItems[i];
    }
    if (MfxIsWnd(m_hWnd)) ::DestroyWindow(m_hWnd);
    ::DeleteObject((HGDIOBJ)hFont);
    ::DeleteObject((HGDIOBJ)hFontBold);
    MfxSetDirty(); MfxPaintIfDirty();   // full erase+OnDraw NOW (see note at MfxShowMessageBox)

    if (m_nModalResult == IDOK)
        m_strPath = MfxFileDialogResolve(nChosenId, ID_NEW, ID_ROW0, strDefault, aFound, nFound, pszDir);
    return m_nModalResult;
}

// ── AfxMessageBox / MessageBoxA backend (v86) ───────────────────────────────────────────────────
// The real in-window modal message box the ⭐ v85 pickup TODO asked for: every game confirm
// ("Leave Yoda Stories?", "…Replay anyway?", the OOM/data-file errors) was invisible while
// AfxMessageBox was the M-era stderr stub that auto-answered IDYES/IDOK. Built on the same
// control kit as CDialog::DoModal above (CK_DLGFRAME bevel + word-wrapped CK_LABEL lines +
// a CK_DEFBUTTON/CK_BUTTON row from the MB_ type), with the same GetMessageA modal loop.
// core/ reaches it through the microfx.h prototype; when the pump is down (smoke harnesses,
// null backend) it returns -1 and the callers keep the headless-safe auto-answer, so
// worldgen_smoke/game_walk/dlg_smoke stay non-blocking.

// greedy word wrap honoring embedded '\n' (hdc has the dialog font selected)
static int MfxMsgBoxWrap(HDC hdc, const char *pszIn, int nMaxW, CString aLine[], int nMaxLines)
{
    const char *p = pszIn ? pszIn : "";
    int nLines = 0;
    char szLine[240];
    while (*p && nLines < nMaxLines) {
        int nLen = 0, nLastSpace = -1;
        while (p[nLen] && p[nLen] != '\n' && nLen < (int)sizeof szLine - 1) {
            SIZE sz;
            ::GetTextExtentPoint32A(hdc, p, nLen + 1, &sz);
            if (sz.cx > nMaxW && nLen > 0) break;
            if (p[nLen] == ' ') nLastSpace = nLen;
            nLen++;
        }
        int nTake = nLen, bAteSpace = 0;
        if (p[nLen] && p[nLen] != '\n' && nLastSpace > 0) { nTake = nLastSpace; bAteSpace = 1; }
        memcpy(szLine, p, nTake); szLine[nTake] = 0;
        aLine[nLines++] = szLine;
        p += nTake;
        if (bAteSpace) p++;                    // the space becomes the line break
        else if (*p == '\n') p++;
    }
    if (nLines == 0) aLine[nLines++] = "";
    return nLines;
}

extern "C" int MfxPumpIsUp(void);

extern "C" int MfxShowMessageBox(const char *pszText, const char *pszCaption, unsigned nType)
{
    if (!MfxPumpIsUp() || !MfxRootWnd()) return -1;

    // button row from the MB_ type (the game uses MB_OK — often with an icon flag — and
    // MB_YESNO; OKCANCEL/YESNOCANCEL supported for MFC completeness, anything else → OK)
    struct { UINT id; const char *psz; } aBtn[3];
    int nBtn = 0; UINT idEsc = 0;
    switch (nType & 0xF) {
    case MB_OKCANCEL:
        aBtn[nBtn].id = IDOK;     aBtn[nBtn++].psz = "OK";
        aBtn[nBtn].id = IDCANCEL; aBtn[nBtn++].psz = "Cancel"; idEsc = IDCANCEL; break;
    case MB_YESNO:                                 // Windows: ESC does nothing on MB_YESNO
        aBtn[nBtn].id = IDYES;    aBtn[nBtn++].psz = "Yes";
        aBtn[nBtn].id = IDNO;     aBtn[nBtn++].psz = "No"; break;
    case MB_YESNOCANCEL:
        aBtn[nBtn].id = IDYES;    aBtn[nBtn++].psz = "Yes";
        aBtn[nBtn].id = IDNO;     aBtn[nBtn++].psz = "No";
        aBtn[nBtn].id = IDCANCEL; aBtn[nBtn++].psz = "Cancel"; idEsc = IDCANCEL; break;
    default:
        aBtn[nBtn].id = IDOK;     aBtn[nBtn++].psz = "OK"; idEsc = IDOK; break;
    }
    UINT idDef = aBtn[0].id;
    // what a dead pump mid-loop must yield: the M-era stub's answer, never a surprise CANCEL
    int nQuitAnswer = (aBtn[0].id == IDYES) ? IDYES : IDOK;

    HFONT hFont = ::CreateFontA(-8, 0, 0, 0, 400, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");
    HFONT hFontBold = ::CreateFontA(-8, 0, 0, 0, 700, 0, 0, 0, 0, 0, 0, 0, 0, "MS Sans Serif");

    // wrap + measure under the dialog font
    enum { MAX_LINES = 12 };
    CString aLine[MAX_LINES];
    HDC hdc = MfxScreenDC();
    HGDIOBJ hOldF = ::SelectObject(hdc, (HGDIOBJ)hFont);
    const int WRAP_W = 300;
    int nLines = MfxMsgBoxWrap(hdc, pszText, WRAP_W, aLine, MAX_LINES);
    int nTextW = 0;
    for (int i = 0; i < nLines; i++) {
        SIZE sz;
        ::GetTextExtentPoint32A(hdc, aLine[i], aLine[i].GetLength(), &sz);
        if (sz.cx > nTextW) nTextW = sz.cx;
    }
    ::SelectObject(hdc, hOldF);

    const int PAD = 10, LINE_H = 14, BTN_W = 60, BTN_H = 16, GAP = 8;
    int nBtnsW = nBtn * BTN_W + (nBtn - 1) * GAP;
    int clientW = (nTextW > nBtnsW ? nTextW : nBtnsW) + 2 * PAD;
    if (clientW < 140) clientW = 140;
    int clientH = PAD + nLines * LINE_H + PAD + BTN_H + PAD;
    int outerW = clientW + 2 * DLG_BORDER;
    int outerH = clientH + 2 * DLG_BORDER + DLG_CAPTION;
    HWND hRoot = MfxRootWnd();
    int rootW = hRoot ? hRoot->rc.right - hRoot->rc.left : 525;
    int rootH = hRoot ? hRoot->rc.bottom - hRoot->rc.top : 310;
    int winX = (rootW - outerW) / 2; if (winX < 0) winX = 0;
    int winY = (rootH - outerH) / 2; if (winY < 0) winY = 0;
    int cliX = winX + DLG_BORDER, cliY = winY + DLG_BORDER + DLG_CAPTION;

    CWnd wndDlg;                                  // plain CWnd — the loop only needs its HWND
    RECT rcDlg = { winX, winY, winX + outerW, winY + outerH };
    wndDlg.Create(0, 0, WS_VISIBLE, rcDlg, AfxGetMainWnd(), 0);

    MfxDlgItem *aItems[MAX_LINES + 5]; int nItems = 0;
    MfxDlgItem *pFrame = new MfxDlgItem;
    pFrame->m_kind = CK_DLGFRAME; pFrame->m_style = WS_CAPTION;
    pFrame->m_hFont = hFont; pFrame->m_hFontBold = hFontBold;
    pFrame->m_text = (pszCaption && *pszCaption) ? pszCaption : "Message";
    pFrame->Create(0, 0, WS_VISIBLE, rcDlg, &wndDlg, 0);
    aItems[nItems++] = pFrame;

    for (int i = 0; i < nLines; i++) {
        MfxDlgItem *pLbl = new MfxDlgItem;
        pLbl->m_kind = CK_LABEL; pLbl->m_align = 1; pLbl->m_hFont = hFont;
        pLbl->m_text = aLine[i];
        RECT rc = { cliX + PAD, cliY + PAD + i * LINE_H,
                    cliX + clientW - PAD, cliY + PAD + (i + 1) * LINE_H };
        pLbl->Create(0, 0, WS_VISIBLE, rc, &wndDlg, 0);
        aItems[nItems++] = pLbl;
    }
    int xBtn = winX + (outerW - nBtnsW) / 2;
    int yBtn = cliY + PAD + nLines * LINE_H + PAD;
    for (int i = 0; i < nBtn; i++) {
        MfxDlgItem *pBtn = new MfxDlgItem;
        pBtn->m_kind = (aBtn[i].id == idDef) ? CK_DEFBUTTON : CK_BUTTON;
        pBtn->m_hFont = hFont; pBtn->m_hFontBold = hFontBold; pBtn->m_text = aBtn[i].psz;
        RECT rc = { xBtn + i * (BTN_W + GAP), yBtn, xBtn + i * (BTN_W + GAP) + BTN_W, yBtn + BTN_H };
        pBtn->Create(0, 0, WS_VISIBLE, rc, &wndDlg, aBtn[i].id);
        aItems[nItems++] = pBtn;
    }

    MfxSetDirty();
    MfxPaintIfDirty();
    if (const char *pszShot = getenv("YODA_DLGSHOT")) MfxWriteDibBMP(MfxScreenDC(), pszShot);

    int nResult = -1;
    while (nResult == -1) {
        MSG msg;
        if (!GetMessageA(&msg, 0, 0, 0)) { nResult = nQuitAnswer; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { nResult = (int)idDef; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && idEsc) { nResult = (int)idEsc; break; }
        // Y/N accelerators when those buttons exist (VK codes for letters == ASCII uppercase)
        if (msg.message == WM_KEYDOWN && msg.wParam == 'Y' && aBtn[0].id == IDYES) { nResult = IDYES; break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == 'N' && aBtn[0].id == IDYES) { nResult = IDNO; break; }
        // clicked button → WM_COMMAND posted to the dialog window (same scoping as CFileDialog:
        // hwnd==dialog keeps accelerator-originated frame commands from faking a click)
        if (msg.message == WM_COMMAND && msg.hwnd == wndDlg.m_hWnd && HIWORD(msg.wParam) == BN_CLICKED) {
            nResult = (int)LOWORD(msg.wParam); break;
        }
        // modal: swallow input aimed at windows outside the dialog subtree
        if ((msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONUP ||
             msg.message == WM_RBUTTONDOWN || msg.message == WM_MOUSEMOVE ||
             msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_CHAR) &&
            !MfxIsDlgOwned(msg.hwnd, wndDlg.m_hWnd))
            continue;
        DispatchMessageA(&msg);
    }

    for (int i = nItems - 1; i >= 0; i--) {
        if (MfxIsWnd(aItems[i]->m_hWnd)) ::DestroyWindow(aItems[i]->m_hWnd);
        delete aItems[i];
    }
    if (MfxIsWnd(wndDlg.m_hWnd)) ::DestroyWindow(wndDlg.m_hWnd);
    ::DeleteObject((HGDIOBJ)hFont);
    ::DeleteObject((HGDIOBJ)hFontBold);
    // Force a FULL erase+OnDraw synchronously on close, rather than only marking dirty and
    // letting the next Run-loop MfxPaintIfDirty clear it (v86 redraw-residue fix). The three
    // commands that route through here — New World / Replay / Load World — return to a handler
    // that IMMEDIATELY runs a world regen + zone transition. That transition redraws only the
    // game-AREA region in a busy-wait loop (clock-hook presents, NOT a full OnDraw — the
    // "out-of-main-loop" redraw path), so the pending dirty flag's full repaint is preempted and
    // the box pixels sitting OVER the inventory panel (which a transition never touches) are
    // never erased. Painting here, while the game is still in its pre-transition drawable state,
    // clears the whole window (OnEraseBkgnd fills the clip box, OnDraw repaints incl. inventory)
    // before the caller can start the partial-redraw transition. (About/sliders never triggered
    // this because nothing redraws after them.)
    MfxSetDirty(); MfxPaintIfDirty();
    return nResult;
}
