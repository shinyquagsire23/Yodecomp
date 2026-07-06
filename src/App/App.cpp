// App — the CWinApp-derived application TU (0x419730–0x419ed0): the theApp global object's
// ctor/dtor + InitInstance (CPUID/MMX probe, Win3.1 MIDI workaround, doc template, command
// line) + OnIdle; the CAboutDlg (IDD_ABOUTBOX=100) ctor/DoDataExchange/OnInitDialog + its
// message map; App::OnAppAbout; and the lone debug logger Log::Write (c:\yodalog.txt).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
#include "App.h"
#include <stdio.h>

// FUNCTION: YODA 0x00419cb0
// Debug logger — append one line to c:\yodalog.txt (fopen "at" + fputs + fflush + fclose per
// call). Nearly all callers compiled out under NDEBUG; only WorldgenPlacePuzzle's survives.
void __stdcall Log_Write(char *pszMsg)
{
    FILE *pFile = fopen("c:\\yodalog.txt", "at");
    fputs(pszMsg, pFile);
    fflush(pFile);
    fclose(pFile);
}

// theApp — the one global application object (0x00459d58). Defining it at file scope makes the
// compiler emit the CRT dynamic-initializer thunk cluster, all four byte-exact (reloc-masked):
//   0x00419830  _$E123  init entry (call ctor-thunk; jmp atexit-thunk)
//   0x00419840  _$E120  ctor thunk (mov ecx,&theApp; jmp CTheApp::CTheApp)
//   0x00419850  _$E122  atexit(dtor-thunk)
//   0x00419860  _$E121  dtor thunk (SEH frame + CTheApp::~CTheApp)
CTheApp theApp;

/////////////////////////////////////////////////////////////////////////////  CTheApp

// FUNCTION: YODA 0x00419730
CTheApp::CTheApp()
{
}

// FUNCTION: YODA 0x00419ca0
BOOL CTheApp::OnIdle(LONG lCount)
{
    return CWinApp::OnIdle(lCount);
}

// FUNCTION: YODA 0x00419720  (GetMessageMap)
BEGIN_MESSAGE_MAP(CTheApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419df0
// File>About: modal CAboutDlg parented to the main window.
void CTheApp::OnAppAbout()
{
    CAboutDlg dlg(AfxGetApp()->m_pMainWnd);
    dlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////  CAboutDlg

// FUNCTION: YODA 0x00419cf0
CAboutDlg::CAboutDlg(CWnd *pParent)
    : CDialog(100, pParent)
{
}

// FUNCTION: YODA 0x00419dd0
void CAboutDlg::DoDataExchange(CDataExchange *pDX)
{
}

// FUNCTION: YODA 0x00419de0  (GetMessageMap)
BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419eb0
BOOL CAboutDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    return TRUE;
}
