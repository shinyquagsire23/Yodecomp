// microfx app/ — child controls (H4 M4, docs/phase-h4-sdl.md): the word-wrapping multiline
// EDIT behind the speech-bubble text (wndDialogText) and the CBitmapButton faces
// (close/up/down). Controls are HWND children of the view; they paint THEMSELVES into the
// shared screen DC — MfxPaintIfDirty calls MfxPaintChildren() after the view's WM_PAINT, so
// a full repaint re-lays them on top (Win32 z-order), and state changes repaint in place.
//
// Message flow: our dispatch engine falls through to CWnd::MfxCtlProc when no message-map
// entry matches — the control classes override it with their EM_*/BM_*/WM_SETTEXT behavior.
// TextDialog::Run's modal loop needs exactly: EM_GETLINECOUNT (wrap count at the create
// width), EM_LINESCROLL (first-visible-line step), WM_SETFONT/WM_SETREDRAW, and BM_GETSTATE
// (BST_PUSHED while a scroll button is held → auto-repeat via the view's timer 1).

#include <afxwin.h>
#include <microfx.h>
#include "mfxwnd.h"
#include <string.h>

// ── CEdit ────────────────────────────────────────────────────────────────────────────────────

BOOL CEdit::Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID)
{
    return CWnd::Create(0, 0, dwStyle, rect, pParentWnd, nID);
}

// word wrap at the window width with the control font (Win32 multiline-edit shape: break at
// spaces, hard-break words wider than the line, honor \r\n)
void CEdit::MfxRecalcLines()
{
    m_mfxLineCount = 0;
    if (!MfxIsWnd(m_hWnd)) return;
    int nWidth = m_hWnd->rc.right - m_hWnd->rc.left;
    if (nWidth <= 0) nWidth = 130;
    HDC hdc = MfxScreenDC();
    HGDIOBJ hOldFont = m_mfxFont ? ::SelectObject(hdc, (HGDIOBJ)m_mfxFont) : 0;

    const char *psz = (const char *)m_mfxText;
    int nLen = m_mfxText.GetLength();
    int nPos = 0;
    while (nPos <= nLen && m_mfxLineCount < 96) {
        if (nPos == nLen) {                          // trailing empty line only if no text
            if (m_mfxLineCount == 0) {
                m_mfxLineStart[0] = 0; m_mfxLineLen[0] = 0; m_mfxLineCount = 1;
            }
            break;
        }
        int nStart = nPos;
        int nFit = 0, nLastSpaceEnd = -1, nLastSpaceNext = -1;
        while (nPos + nFit < nLen) {
            char c = psz[nPos + nFit];
            if (c == '\r') { nLastSpaceEnd = nFit; nLastSpaceNext = nFit + (nPos + nFit + 1 < nLen && psz[nPos + nFit + 1] == '\n' ? 2 : 1); break; }
            if (c == '\n') { nLastSpaceEnd = nFit; nLastSpaceNext = nFit + 1; break; }
            SIZE sz;
            GetTextExtentPoint32A(hdc, psz + nPos, nFit + 1, &sz);
            if (sz.cx > nWidth && nFit > 0) break;
            if (c == ' ') { nLastSpaceEnd = nFit; nLastSpaceNext = nFit + 1; }
            nFit++;
        }
        int nLineLen, nNext;
        if (nPos + nFit >= nLen) {                   // rest fits
            nLineLen = nLen - nPos;
            nNext = nLen + 1;                        // terminate outer loop
        }
        else if (nLastSpaceEnd >= 0 &&
                 (psz[nPos + nLastSpaceEnd] == '\r' || psz[nPos + nLastSpaceEnd] == '\n' ||
                  nLastSpaceEnd >= nFit - 1 || psz[nPos + nFit] != ' ')) {
            // wrap at the last break char (space/newline) inside the fitted run
            nLineLen = nLastSpaceEnd;
            nNext = nPos + nLastSpaceNext;
        }
        else {                                       // no break char: hard-break the word
            nLineLen = nFit;
            nNext = nPos + nFit;
            while (nNext < nLen && psz[nNext] == ' ') nNext++;   // eat wrap spaces
        }
        m_mfxLineStart[m_mfxLineCount] = nStart;
        m_mfxLineLen[m_mfxLineCount] = nLineLen;
        m_mfxLineCount++;
        nPos = nNext;
    }
    if (hOldFont) ::SelectObject(hdc, hOldFont);
    if (m_mfxLineCount == 0) m_mfxLineCount = 1;
}

void CEdit::MfxCtlPaint()
{
    if (!MfxIsWnd(m_hWnd) || !m_hWnd->bVisible) return;
    HDC hdc = MfxScreenDC();
    MfxTouchHold();                    // one present for the fill + all visible lines
    HGDIOBJ hOldFont = m_mfxFont ? ::SelectObject(hdc, (HGDIOBJ)m_mfxFont) : 0;
    TEXTMETRIC tm;
    GetTextMetricsA(hdc, &tm);
    RECT rc = m_hWnd->rc;
    ::FillRect(hdc, &rc, (HBRUSH)::GetStockObject(WHITE_BRUSH));   // bubble interior is white
    int nVisible = tm.tmHeight > 0 ? (rc.bottom - rc.top) / (int)tm.tmHeight : 0;
    if (nVisible < 1) nVisible = 1;
    int nOldMode = ::SetBkMode(hdc, TRANSPARENT);
    COLORREF crOld = ::SetTextColor(hdc, RGB(0, 0, 0));
    for (int i = 0; i < nVisible; i++) {
        int nLine = m_mfxFirstLine + i;
        if (nLine >= m_mfxLineCount) break;
        ::TextOutA(hdc, rc.left, rc.top + i * (int)tm.tmHeight,
                   (const char *)m_mfxText + m_mfxLineStart[nLine], m_mfxLineLen[nLine]);
    }
    ::SetTextColor(hdc, crOld);
    ::SetBkMode(hdc, nOldMode);
    if (hOldFont) ::SelectObject(hdc, hOldFont);
    MfxTouchRelease(hdc);
}

LRESULT CEdit::MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SETTEXT:
        m_mfxText = lParam ? (LPCSTR)lParam : "";
        m_mfxFirstLine = 0;
        MfxRecalcLines();
        MfxCtlPaint();
        return TRUE;
    case WM_SETFONT:
        m_mfxFont = (HFONT)wParam;
        MfxRecalcLines();
        return 0;
    case WM_SETREDRAW:
        m_mfxRedraw = (int)wParam;
        if (m_mfxRedraw) MfxCtlPaint();
        return 0;
    case EM_GETLINECOUNT:
        MfxRecalcLines();
        return m_mfxLineCount;
    case EM_LINESCROLL: {
        int nNew = m_mfxFirstLine + (int)lParam;
        if (nNew < 0) nNew = 0;
        if (nNew > m_mfxLineCount - 1) nNew = m_mfxLineCount - 1;
        m_mfxFirstLine = nNew;
        MfxCtlPaint();
        return TRUE;
    }
    case WM_PAINT:
        MfxCtlPaint();
        return 0;
    }
    return 0;
}

// ── CScrollBar (M4e — the inventory scrollbar, SB_CTL) ──────────────────────────────────────
// Win95 chrome drawn with the public GDI surface: two bevelled arrow buttons, a checkered
// track, a bevelled thumb. Interaction sends WM_VSCROLL(SB_*) to the PARENT (Win32 SB_CTL
// behavior) — CDeskcppView::OnVScroll forwards to InvScrollBar's reflected handler, which
// SetScrollPos()es back into us and repaints the item list.

static const int MFX_SB_BTN = 16;      // arrow button height (Win95 metric)

BOOL CScrollBar::Create(DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID)
{
    return CWnd::Create(0, 0, dwStyle, rect, pParentWnd, nID);
}

static void MfxSbBevelRect(HDC hdc, RECT rc)
{
    CBrush brFace(GetSysColor(15));    // COLOR_3DFACE
    ::FillRect(hdc, &rc, (HBRUSH)brFace.m_hObject);
    HPEN hPenHi = ::CreatePen(PS_SOLID, 1, GetSysColor(20));   // 3DHILIGHT top/left
    HPEN hPenLo = ::CreatePen(PS_SOLID, 1, GetSysColor(16));   // 3DSHADOW bottom/right
    HGDIOBJ hOld = ::SelectObject(hdc, (HGDIOBJ)hPenHi);
    ::MoveToEx(hdc, rc.left, rc.bottom - 1, 0);
    ::LineTo(hdc, rc.left, rc.top);
    ::LineTo(hdc, rc.right - 1, rc.top);
    ::SelectObject(hdc, (HGDIOBJ)hPenLo);
    ::LineTo(hdc, rc.right - 1, rc.bottom - 1);
    ::LineTo(hdc, rc.left - 1, rc.bottom - 1);
    ::SelectObject(hdc, hOld);
    ::DeleteObject((HGDIOBJ)hPenHi);
    ::DeleteObject((HGDIOBJ)hPenLo);
}

static void MfxSbArrow(HDC hdc, RECT rc, int bDown)
{
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2 + (bDown ? 2 : -2);
    for (int i = 0; i < 4; i++) {
        int y = bDown ? cy - i : cy + i;
        for (int x = cx - i; x <= cx + i; x++)
            ::SetPixel(hdc, x, y, RGB(0, 0, 0));
    }
}

// thumb geometry: the thumb top y (control-relative) or -1 when there is no range
static int MfxSbThumbTop(HWND h)
{
    int nH = h->rc.bottom - h->rc.top;
    int nTravel = nH - 3 * MFX_SB_BTN;               // track minus the 16px thumb
    int nRange = h->nScrollMax - h->nScrollMin;
    if (nRange <= 0 || nTravel <= 0) return -1;
    return MFX_SB_BTN + (h->nScrollPos - h->nScrollMin) * nTravel / nRange;
}

void CScrollBar::MfxCtlPaint()
{
    if (!MfxIsWnd(m_hWnd) || !m_hWnd->bVisible) return;
    HDC hdc = MfxScreenDC();
    MfxTouchHold();
    RECT rc = m_hWnd->rc;
    // track: the Win95 50% checker of white / 3D-face
    COLORREF crFace = GetSysColor(15);
    for (int y = rc.top; y < rc.bottom; y++)
        for (int x = rc.left; x < rc.right; x++)
            ::SetPixel(hdc, x, y, ((x ^ y) & 1) ? RGB(255, 255, 255) : crFace);
    RECT rcUp = { rc.left, rc.top, rc.right, rc.top + MFX_SB_BTN };
    RECT rcDn = { rc.left, rc.bottom - MFX_SB_BTN, rc.right, rc.bottom };
    MfxSbBevelRect(hdc, rcUp);
    MfxSbArrow(hdc, rcUp, 0);
    MfxSbBevelRect(hdc, rcDn);
    MfxSbArrow(hdc, rcDn, 1);
    int nThumbTop = MfxSbThumbTop(m_hWnd);
    if (nThumbTop >= 0) {
        RECT rcThumb = { rc.left, rc.top + nThumbTop, rc.right, rc.top + nThumbTop + MFX_SB_BTN };
        MfxSbBevelRect(hdc, rcThumb);
    }
    MfxTouchRelease(hdc);
}

LRESULT CScrollBar::MfxCtlProc(UINT message, WPARAM, LPARAM lParam)
{
    if (!MfxIsWnd(m_hWnd)) return 0;
    HWND hParent = m_hWnd->hParent;
    RECT rc = m_hWnd->rc;
    switch (message) {
    case WM_LBUTTONDOWN: {
        int y = (int)(short)HIWORD(lParam) - rc.top;   // client → control-relative
        int nH = rc.bottom - rc.top;
        int nThumbTop = MfxSbThumbTop(m_hWnd);
        UINT nCode = (UINT)-1;
        if (y < MFX_SB_BTN) nCode = SB_LINEUP;
        else if (y >= nH - MFX_SB_BTN) nCode = SB_LINEDOWN;
        else if (nThumbTop >= 0 && y >= nThumbTop && y < nThumbTop + MFX_SB_BTN) {
            m_mfxDragging = 1;
            m_mfxDragOffset = y - nThumbTop;
            ::SetCapture(m_hWnd);
            return 0;
        }
        else if (nThumbTop >= 0) nCode = (y < nThumbTop) ? SB_PAGEUP : SB_PAGEDOWN;
        if (nCode != (UINT)-1)
            MfxSendMsg(hParent, WM_VSCROLL, MAKELONG(nCode, 0), (LPARAM)m_hWnd);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (m_mfxDragging) {
            int y = (int)(short)HIWORD(lParam) - rc.top - m_mfxDragOffset - MFX_SB_BTN;
            int nTravel = (rc.bottom - rc.top) - 3 * MFX_SB_BTN;
            int nRange = m_hWnd->nScrollMax - m_hWnd->nScrollMin;
            if (nTravel > 0 && nRange > 0) {
                int nPos = m_hWnd->nScrollMin + (y * nRange + nTravel / 2) / nTravel;
                if (nPos < m_hWnd->nScrollMin) nPos = m_hWnd->nScrollMin;
                if (nPos > m_hWnd->nScrollMax) nPos = m_hWnd->nScrollMax;
                MfxSendMsg(hParent, WM_VSCROLL, MAKELONG(SB_THUMBTRACK, nPos), (LPARAM)m_hWnd);
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_mfxDragging) {
            m_mfxDragging = 0;
            ::ReleaseCapture();
            MfxSendMsg(hParent, WM_VSCROLL,
                       MAKELONG(SB_THUMBPOSITION, m_hWnd->nScrollPos), (LPARAM)m_hWnd);
        }
        return 0;
    case WM_PAINT:
        MfxCtlPaint();
        return 0;
    }
    return 0;
}

// ── CButton / CBitmapButton ──────────────────────────────────────────────────────────────────

BOOL CButton::Create(LPCSTR, DWORD dwStyle, const RECT &rect, CWnd *pParentWnd, UINT nID)
{
    return CWnd::Create(0, 0, dwStyle, rect, pParentWnd, nID);
}

LRESULT CButton::MfxCtlProc(UINT message, WPARAM wParam, LPARAM)
{
    switch (message) {
    case BM_GETSTATE:
        return m_mfxPushed ? BST_PUSHED : 0;
    case BM_SETSTATE:
        m_mfxPushed = wParam ? 1 : 0;
        MfxCtlPaint();
        return 0;
    case WM_SETREDRAW:
        m_mfxRedraw = (int)wParam;
        if (m_mfxRedraw) MfxCtlPaint();
        return 0;
    case WM_LBUTTONDOWN:
        if (MfxIsWnd(m_hWnd) && m_hWnd->bEnabled) {
            ::SetCapture(m_hWnd);
            m_mfxPushed = 1;
            MfxCtlPaint();
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_mfxPushed) {
            m_mfxPushed = 0;
            ::ReleaseCapture();
            MfxCtlPaint();
            // click completed over the button → BN_CLICKED to the parent, via the QUEUE so
            // a modal GetMessage loop sees it (the close button's view handler sets
            // bDialogCloseClicked)
            if (MfxIsWnd(m_hWnd)) {
                POINT pt = g_mfxCursorPos;
                if (pt.x >= m_hWnd->rc.left && pt.x < m_hWnd->rc.right &&
                    pt.y >= m_hWnd->rc.top && pt.y < m_hWnd->rc.bottom)
                    ::PostMessageA(m_hWnd->hParent, WM_COMMAND,
                                   MAKELONG(m_hWnd->nId, BN_CLICKED), (LPARAM)m_hWnd);
            }
        }
        return 0;
    case WM_PAINT:
        MfxCtlPaint();
        return 0;
    }
    return 0;
}

BOOL CBitmapButton::LoadBitmaps(LPCSTR pszUp, LPCSTR pszDown, LPCSTR pszFocus, LPCSTR pszDisabled)
{
    m_bitmap.Detach();         m_bitmap.m_hObject         = (HGDIOBJ)::LoadBitmapA(0, pszUp);
    m_bitmapSel.Detach();      m_bitmapSel.m_hObject      = (HGDIOBJ)::LoadBitmapA(0, pszDown);
    m_bitmapFocus.Detach();    m_bitmapFocus.m_hObject    = (HGDIOBJ)::LoadBitmapA(0, pszFocus);
    m_bitmapDisabled.Detach(); m_bitmapDisabled.m_hObject = (HGDIOBJ)::LoadBitmapA(0, pszDisabled);
    return m_bitmap.m_hObject != 0;
}

void CBitmapButton::MfxCtlPaint()
{
    if (!MfxIsWnd(m_hWnd) || !m_hWnd->bVisible) return;
    HGDIOBJ h = m_bitmap.m_hObject;                  // U face
    if (!m_hWnd->bEnabled && m_bitmapDisabled.m_hObject) h = m_bitmapDisabled.m_hObject;  // X
    else if (m_mfxPushed && m_bitmapSel.m_hObject)       h = m_bitmapSel.m_hObject;       // D
    MFXIMG img;
    if (!MfxGetImage(h, &img)) return;
    MfxDrawImage(MfxScreenDC(), m_hWnd->rc.left, m_hWnd->rc.top, &img);
}
