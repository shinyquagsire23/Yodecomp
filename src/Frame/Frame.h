// Frame TU (0x419000–0x419720): CMainFrame, the CFrameWnd-derived main window.
// Pauses/resumes the game on activate & sys-command, tracks the game's saved frame-mode,
// and manages the game palette (realize on paint/palette messages).
#ifndef FRAME_H
#define FRAME_H
#include <afxwin.h>

// --- minimal stubs for the doc/view the frame drives (real classes elsewhere) ---
class FrameWorld;

// GameView (CDeskcppView) — only the fields/methods the frame touches. vftable elsewhere.
class FrameView
{
public:
    char        _pad00[0x44];
    FrameWorld *pWorld;              // +0x44
    int         _pad48;             // +0x48
    int         bBusy;              // +0x4c  (game paused while the window is inactive/moving)
    char        _pad50[0xf4];       // +0x50
    short       nDragSlot;          // +0x144
    char        _pad146[2];         // +0x146
    int         bDragActive;        // +0x148
    char        _pad14c[0x1b0];     // +0x14c
    struct MusicThread *pMusicThread; // +0x2fc  (+0x28 = the HANDLE)

    void DrawText(CDC *pDC);                 // 0x0040f060 (repaint status/frame text)
    void UpdateDragCursor(int a);            // 0x00412cc0
    void ConfirmExit();                      // 0x00416030
};

// The music playback thread object — only the HANDLE the frame resumes/suspends.
struct MusicThread
{
    char   _pad00[0x28];
    HANDLE hThread;                 // +0x28
};

class FrameWorld
{
public:
    char       _pad00[0x5c];
    int        nFrameMode;          // +0x5c  (1..9,0xb play states; 0xc = shutting down)
    char       _pad60[0x18];
    int        timeBase;            // +0x78
    int        timeOffset;          // +0x7c
    char       _pad80[0x244];
    CPalette  *pPalette;            // +0x2c4
};

extern CString g_strReplayPath;      // 0x00459e20 — defined in this TU (its init thunks live here)

class CMainFrame : public CFrameWnd
{
public:
    char       _pad_bc[0x10];       // +0xbc  (CFrameWnd base ends at 0xbc)
    CPalette  *m_pOldPalette;       // +0xcc  saved palette across realize
    int        _pad_d0;             // +0xd0
    int        m_nSavedFrameMode;   // +0xd4  ctor: -1  (World.nFrameMode parked while inactive)
                                    // sizeof 0xd8

    CMainFrame();                                        // 0x00419090
    virtual ~CMainFrame();                               // 0x00419120

    virtual BOOL PreCreateWindow(CREATESTRUCT &cs);      // 0x00419210
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext *pContext); // 0x00419370

    afx_msg void OnGetMinMaxInfo(MINMAXINFO *lpMMI);     // 0x004192d0
    afx_msg void OnSize(UINT nType, int cx, int cy);     // 0x00419320
    afx_msg int  OnCreate(LPCREATESTRUCT lpcs);          // 0x00419340
    afx_msg void OnPaletteChanged(CWnd *pFocusWnd);      // 0x004193f0
    afx_msg void OnPaletteIsChanging(CWnd *pRealizeWnd); // 0x00419460
    afx_msg BOOL OnQueryNewPalette();                    // 0x004194d0
    afx_msg void OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized); // 0x00419540
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus); // 0x004196a0
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);  // 0x00419170
    afx_msg BOOL OnQueryEndSession();                    // 0x004196c0

protected:
    DECLARE_DYNCREATE(CMainFrame)                        // CreateObject 0x00419000 / GetRuntimeClass 0x00419070
    DECLARE_MESSAGE_MAP()                                // 0x00419080
};

#endif
