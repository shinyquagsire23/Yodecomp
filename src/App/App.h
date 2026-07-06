// App TU classes (0x419730–0x419ed0): the CWinApp-derived theApp + the About dialog.
#ifndef APP_H
#define APP_H
#include <afxwin.h>

void __stdcall Log_Write(char *pszMsg);  // 0x00419cb0

// The About box (IDD_ABOUTBOX == 100). vftable 0x0044c1d8.
class CAboutDlg : public CDialog
{
public:
    CAboutDlg(CWnd *pParent);                            // 0x00419cf0
    virtual void DoDataExchange(CDataExchange *pDX);     // 0x00419dd0 (empty — no DDX_ fields)
    virtual BOOL OnInitDialog();                         // 0x00419eb0
protected:
    DECLARE_MESSAGE_MAP()                                // 0x00419de0
};

// The application object (one global instance `theApp` @0x00459d58). vftable 0x0044c130.
class CTheApp : public CWinApp
{
public:
    CString m_str;                       // +0xc0
    int     m_nFrameDelay;               // +0xc4  (0x1e on Win3.1/slow, else 0x28; -> World+0x74)

    CTheApp();                                           // 0x00419730
    virtual BOOL InitInstance();                         // 0x004198c0
    virtual BOOL OnIdle(LONG lCount);                    // 0x00419ca0
    void OnAppAbout();                                   // 0x00419df0
protected:
    DECLARE_MESSAGE_MAP()                                // 0x00419720
};

#endif
