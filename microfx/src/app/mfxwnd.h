// microfx app/ — private window-layer interface (H4 M2). Shared between mfxwnd.cpp (the pure
// C++ HWND model / message dispatch / timers) and mfxpump.cpp (the SDL event pump). NOT part
// of the public shim surface — game TUs never see this.
#ifndef MFXWND_H
#define MFXWND_H
#include <afxwin.h>
#include <microfx.h>

#define MFX_TAG_WND 0x444e5758   // 'XWND'

// Visible menu bar (H4 M5 tail, mfxmenu.cpp): a fixed-height chrome strip presented ABOVE the
// game's screen DC — never part of it, so no game coordinate math moves (docs/phase-h4-sdl.md).
#define MFX_MENUBAR_H 19

struct HWND__ {                  // a window: an MFC object + a rect in root-client coords
    unsigned nTag;
    CWnd    *pWnd;               // owning MFC object (never null while the HWND lives)
    HWND     hParent;            // 0 = the root (main frame) window
    UINT     nId;                // child id (AFX_IDW_PANE_FIRST for the view)
    RECT     rc;                 // client rect; root: {0,0,cx,cy}
    int      bVisible;
    int      bEnabled;           // EnableWindow state (M4 controls)
    int      nScrollMin;         // SB_CTL scroll state (the inventory scrollbar)
    int      nScrollMax;
    int      nScrollPos;
};

int  MfxIsWnd(HWND h);
HWND MfxRootWnd();               // the first parentless window created (the main frame), or 0
HDC  MfxScreenDC();              // the shared screen DC (8bpp DIB attached when the root exists)
HWND MfxWndFromPoint(POINT pt);  // deepest visible child under pt (mouse routing), else 0
void MfxPaintChildren();         // MfxCtlPaint every visible child control (after a view paint)

// Message delivery: walk pWnd's message-map chain, decode the AfxSig, call the handler.
// Returns the handler's result; 0 if no map entry matched (Win32 "default" behavior).
LRESULT MfxDispatchMsg(CWnd *pWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT MfxSendMsg(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

int  MfxGetPostedMsg(MSG *pMsg);  // pop one PostMessage'd MSG; 1 = got one
int  MfxNextDueTimer(MSG *pMsg);  // one due timer as a WM_TIMER MSG; 1 = got one
void MfxPumpTimers();             // fire due timers (WM_TIMER via MfxSendMsg)
void MfxSetDirty();               // request a WM_PAINT on the next MfxPaintIfDirty
void MfxPaintIfDirty();           // deliver WM_PAINT to the active view if invalidated

// CN_UPDATE_COMMAND_UI query (mfxwnd.cpp): walk the same view→frame→app chain MfxDispatchCommand
// uses for WM_COMMAND, but call the ON_UPDATE_COMMAND_UI handler for nID (real MFC's
// CFrameWnd::OnInitMenuPopup shape). pCmdUI->m_nID must be preset by the caller. Returns TRUE if
// a handler ran (pCmdUI->m_bEnabled/m_nCheck reflect it); FALSE if nID has no UI handler AND no
// ON_COMMAND handler either (a truly dead id — caller should gray it out).
BOOL MfxQueryCmdUI(CCmdUI *pCmdUI);

// Visible menu bar (mfxmenu.cpp), called from mfxpump.cpp only.
void MfxMenuInit();                                 // parse RT_MENU id 2, create the chrome DC, draw once
int  MfxMenuGetChromeDib(MFXDIB *pOut);              // sync palette + fill *pOut; 0 if not initialized
void MfxMenuHandleMouse(UINT message, int x, int y); // message: WM_MOUSEMOVE/WM_LBUTTONDOWN; bar-local
int  MfxMenuActive();                                // 1 while a popup is tracking (Escape-key gate)
void MfxMenuEscape();                                // dismiss any open popup, no command fired

// pump → USER state feeds (GetAsyncKeyState / GetCursorPos / capture read these)
extern int   g_mfxQuit;           // set by PostQuitMessage / pump hard-quit
extern int   g_mfxQuitCode;
extern BYTE  g_mfxKeyState[256];  // 0x80 = down, indexed by VK
extern POINT g_mfxCursorPos;      // last mouse position, client coords
extern HWND  g_mfxCapture;
extern HWND  g_mfxFocus;
extern HCURSOR g_mfxCursor;       // last SetCursor() — the pump applies it (SDL cursor)
extern "C" int g_mfxCursorEverSet;// 0 until the game's first SetCursor (leave the OS arrow)
                                  // (defined inside mfxwnd.cpp's extern "C" USER block)

#endif
