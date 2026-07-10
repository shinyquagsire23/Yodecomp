// App — the CWinApp-derived application TU (0x419730–0x419ed0): the theApp global object's
// ctor/dtor + InitInstance (CPUID/MMX probe, Win3.1 MIDI workaround, doc template, command
// line) + OnIdle; the CAboutDlg (IDD_ABOUTBOX=100) ctor/DoDataExchange/OnInitDialog + its
// message map; App::OnAppAbout; and the lone debug logger Log::Write (c:\yodalog.txt).
// Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
//
// v44 (G2): function definitions arranged in the ORIGINAL emission order (ascending .text
// address within the TU) so the linked image lays this obj's COMDATs out to match the
// original: msgmap → ctor → theApp → InitInstance → OnIdle → LogWrite → CAboutDlg ctor →
// DoDataExchange → CAboutDlg msgmap → OnAppAbout → OnInitDialog. Codegen-neutral (212 stands).
#include "Deskcpp.h"
#include <stdio.h>
#include <stdlib.h>

extern "C" { int App_bCpuHasMMX; }       // 0x00459e28  definition (set by InitInstance CPUID probe)
int         g_bReplayMode;               // 0x00459e2c
extern CString g_strReplayPath;          // 0x00459e20 — defined in the Frame TU (its thunks live there)
CWnd    *g_pExistingInstance;            // 0x00459e24
BYTE     g_bInstanceChecked;             // 0x00459e30  bit0: FindWindow done once

// The doc-template CRuntimeClasses. IMPLEMENT_DYNCREATE(World/CMainFrame/GameView) in their own
// TUs emits `World::classWorld` etc.; forward-declare just that static member so RUNTIME_CLASS
// resolves to it (the addresses are masked relocations at the CSingleDocTemplate call site).
class CDeskcppDoc      { public: static const CRuntimeClass classCDeskcppDoc; };
class CMainFrame { public: static const CRuntimeClass classCMainFrame; };
class CDeskcppView   { public: static const CRuntimeClass classCDeskcppView; };

/////////////////////////////////////////////////////////////////////////////  CDeskcppApp

// FUNCTION: YODA 0x00419720  (GetMessageMap)
// The AppWizard standard CWinApp map (entries @0x44b??? — msgcheck-verified against the original's 8
// AFX_MSGMAP_ENTRY records): OnAppAbout + the standard file/help commands routed to CWinApp's handlers.
// v49: was just the About entry; the other 7 were missing (File>New/Open + the context-help block would
// not dispatch in the recompiled app). IDs/order read from the binary; handlers are CWinApp lib methods.
BEGIN_MESSAGE_MAP(CDeskcppApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)                    // 0xe140
    // Standard file based document commands
    ON_COMMAND(ID_FILE_NEW, CWinApp::OnFileNew)             // 0xe100
    ON_COMMAND(ID_FILE_OPEN, CWinApp::OnFileOpen)           // 0xe101
    // Standard context-sensitive help block (AppWizard order)
    ON_COMMAND(ID_HELP_INDEX, CWinApp::OnHelpIndex)         // 0xe142
    ON_COMMAND(ID_HELP_USING, CWinApp::OnHelpUsing)         // 0xe144
    ON_COMMAND(ID_HELP, CWinApp::OnHelp)                    // 0xe146
    ON_COMMAND(ID_CONTEXT_HELP, CWinApp::OnContextHelp)     // 0xe145 (this afxres.h: CONTEXT_HELP=0xe145)
    ON_COMMAND(ID_DEFAULT_HELP, CWinApp::OnHelpIndex)       // 0xe147 (DEFAULT_HELP=0xe147)
END_MESSAGE_MAP()

// FUNCTION: YODA 0x00419730
// FUNCTION: YODA 0x004197b0  (??_GCDeskcppApp scalar-deleting dtor — compiler-generated)
CDeskcppApp::CDeskcppApp()
{
}

// theApp — the one global application object (0x00459d58). Defining it at file scope makes the
// compiler emit the CRT dynamic-initializer thunk cluster, all four byte-exact (reloc-masked):
//   0x00419830  _$E123  init entry (call ctor-thunk; jmp atexit-thunk)
//   0x00419840  _$E120  ctor thunk (mov ecx,&theApp; jmp CDeskcppApp::CDeskcppApp)
//   0x00419850  _$E122  atexit(dtor-thunk)
//   0x00419860  _$E121  dtor thunk (SEH frame + CDeskcppApp::~CDeskcppApp)
CDeskcppApp theApp;

// FUNCTION: YODA 0x004198c0  [EFFECTIVE MATCH: main body structurally identical (asmscore
//   align=16 = one cmp operand-order on the g_pExistingInstance null test — the inert
//   mirror/cmp family — plus a boundary artifact where OnIdle bleeds past the extent). The
//   329 positional byte diffs are pure EH-funclet LAYOUT: our COMDAT is 6B longer and orders
//   the cleanup funclets differently (endgame/joint-TU territory, like the WorldDoc dtor).
//   Cracks landed: (BYTE)dwVer major==3 stays AL but (int)(BYTE)(dwVer>>8) widens to the
//   signed movzx compare; short nBpp keeps the 16-bit DI store; default-ctor-THEN-assign for
//   strCmd (copy-ctor form emitted the wrong CString shape); CPUID via _emit 0F A2 (VC4.2
//   predates the mnemonic, same trick as the Canvas MMX blits).]
// App startup: single-instance guard (activate an existing window instead of a 2nd copy),
// CPUID/MMX probe, Win3.1 MIDI workaround + frame-delay pick, build the YODADEMO.DTA path,
// require an 8bpp display, register the doc template, then parse the command line.
BOOL CDeskcppApp::InitInstance()
{
    if (m_hPrevInstance != NULL)
        return FALSE;

    g_bReplayMode = 0;
    g_strReplayPath = "";
    CString strTitle;
    strTitle.LoadString(0xe000);
    if ((g_bInstanceChecked & 1) == 0) {
        g_bInstanceChecked |= 1;
        g_pExistingInstance = CWnd::FromHandle(::FindWindow(NULL, strTitle));
    }
    if (NULL != g_pExistingInstance) {
        // another instance is running — surface it and bail
        CWnd *pPopup = CWnd::FromHandle(::GetLastActivePopup(g_pExistingInstance->m_hWnd));
        ::BringWindowToTop(g_pExistingInstance->m_hWnd);
        if (::IsIconic(g_pExistingInstance->m_hWnd))
            g_pExistingInstance->ShowWindow(SW_RESTORE);
        if (pPopup != g_pExistingInstance)
            ::BringWindowToTop(pPopup->m_hWnd);
        return FALSE;
    }

    // CPUID feature probe -> App_bCpuHasMMX (VC4.2 predates the CPUID mnemonic; emit 0F A2).
    App_bCpuHasMMX = 0;
    __asm {
        pushad
        pushfd
        pop  eax
        mov  ecx, eax
        xor  eax, 0x200000
        push eax
        popfd
        pushfd
        pop  eax
        xor  eax, ecx
        jz   no_cpuid
        mov  eax, 1
        _emit 0x0f
        _emit 0xa2                       // cpuid
        test edx, 0x800000               // MMX feature bit
        jnz  has_mmx
    no_cpuid:
        mov  eax, 0
        mov  App_bCpuHasMMX, eax
        jmp  cpu_done
    has_mmx:
        mov  eax, 1
        mov  App_bCpuHasMMX, eax
    cpu_done:
        popad
    }

    // Win3.1 (Win32s major 3, minor < 0x14): slower frame delay + one-time MIDI-disable nag.
    DWORD dwVer = ::GetVersion();
    int nMajor = (BYTE)dwVer;
    int nMinor = (BYTE)(dwVer >> 8);
    if (nMajor == 3 && nMinor < 0x14) {
        m_nFrameDelay = 0x1e;
        UINT bMusic = GetProfileInt("OPTIONS", "PlayMusic", -1);
        UINT bMidiLoad = GetProfileInt("OPTIONS", "MIDILoad", -1);
        if (bMusic == 1 && bMidiLoad == (UINT)-1) {
            AfxMessageBox(4, 0, (UINT)-1);
            WriteProfileInt("OPTIONS", "PlayMusic", 0);
        }
        else if (bMidiLoad == 1) {
            WriteProfileInt("OPTIONS", "MIDILoad", -1);
        }
    }
    else {
        m_nFrameDelay = 0x28;
    }

    // asset directory = the exe's folder + YODADEMO.DTA
    char szPath[300];
    char szDrive[300];
    char szDir[300];
    ::GetModuleFileName(NULL, szPath, 300);
    _splitpath(szPath, szDrive, szDir, NULL, NULL);
    _makepath(szPath, szDrive, szDir, NULL, NULL);
    m_str = szPath;
    if (m_str[m_str.GetLength() - 1] != '\\')
        m_str += "\\";
#if defined(GAME_INDY)
    m_str += "DESKTOP.DAW";       // Indiana Jones' Desktop Adventures data (GAME_INDY)
#elif defined(YODA_FULL)
    m_str += "YODESK.DTA";        // full retail data (STUP + all-planet tiles); YODA_VARIANT=FULL
#else
    m_str += "YODADEMO.DTA";
#endif

    // needs a palettized (>=8bpp) display
    HDC hdc = ::GetDC(NULL);
    short nBpp = (short)::GetDeviceCaps(hdc, BITSPIXEL);
    ::ReleaseDC(NULL, hdc);
    if (nBpp < 8) {
        AfxMessageBox(0xe00a, 0, (UINT)-1);
        return FALSE;
    }

    CSingleDocTemplate *pDocTemplate = new CSingleDocTemplate(
        2, RUNTIME_CLASS(CDeskcppDoc), RUNTIME_CLASS(CMainFrame), RUNTIME_CLASS(CDeskcppView));
    AddDocTemplate(pDocTemplate);
    OnFileNew();
    // The window title + app icon come from resource IDR_MAINFRAME (id 2): the doc-template string
    // and GROUP_ICON. For GAME_INDY, tools/make_indy_res.py swaps both for Indy's ("Desktop
    // Adventures" + the DESKADV.EXE icon), so no runtime title override is needed here.

    // command line: "[OPEN ]<path>[.WLD]" — a .WLD path switches on replay mode
    if (m_lpCmdLine[0] != '\0') {
        CString strCmd;
        strCmd = m_lpCmdLine;
        strCmd.MakeUpper();
        if (strCmd.Find("OPEN ") >= 0)
            strCmd = strCmd.Right(strCmd.GetLength() - 5);
        if (strCmd.Find(".WLD") > 0) {
            g_bReplayMode = 1;
            g_strReplayPath = strCmd;
        }
    }
    return TRUE;
}

// FUNCTION: YODA 0x00419ca0
BOOL CDeskcppApp::OnIdle(LONG lCount)
{
    return CWinApp::OnIdle(lCount);
}

// FUNCTION: YODA 0x00419cb0
// Debug logger — append one line to c:\yodalog.txt (fopen "at" + fputs + fflush + fclose per
// call). Nearly all callers compiled out under NDEBUG; only WorldgenPlacePuzzle's survives.
// The original is a CDeskcppApp member (call sites load ECX = AfxGetApp()); the body never reads
// `this`, so the thiscall codegen is byte-identical to a __stdcall free function (RET 4).
void CDeskcppApp::LogWrite(char *pszMsg)
{
    FILE *pFile = fopen("c:\\yodalog.txt", "at");
    fputs(pszMsg, pFile);
    fflush(pFile);
    fclose(pFile);
}

/////////////////////////////////////////////////////////////////////////////  CAboutDlg

// FUNCTION: YODA 0x00419cf0
// FUNCTION: YODA 0x00419d60  (??_GCAboutDlg scalar-deleting dtor — compiler-generated)
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

// FUNCTION: YODA 0x00419df0
// File>About: modal CAboutDlg parented to the main window.
void CDeskcppApp::OnAppAbout()
{
    CAboutDlg dlg(AfxGetApp()->m_pMainWnd);
    dlg.DoModal();
}

// FUNCTION: YODA 0x00419eb0
BOOL CAboutDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    CenterWindow();
    return TRUE;
}
