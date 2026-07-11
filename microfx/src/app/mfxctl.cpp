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
