// microfx app/window layer — M0 STUBS. Every body here is a placeholder that keeps pure-logic
// milestones linking; the real implementations land with M1 (gdi/), M2 (event pump), M4 (ui).
// A stub that must not be silently hit at runtime logs once via MFX_STUB.
#include <afxwin.h>
#include <afxcmn.h>
#include <mfxplat.h>           // MfxPlatMinimize (SC_MINIMIZE / "Hide Me!")
#include <ctype.h>             // tolower (INI profile lookup)
#ifdef __APPLE__
#include <mach-o/dyld.h>       // _NSGetExecutablePath (GetModuleFileNameA)
#else
#include <unistd.h>            // readlink /proc/self/exe
#endif

#define MFX_STUB() ((void)0)   // quiet for now; flip to a log when the pump goes live

// ── CWnd family ──────────────────────────────────────────────────────────────────────────────
// (FromHandle / Create / DestroyWindow / GetParent / GetParentFrame are REAL as of M2 —
// app/mfxwnd.cpp owns the HWND object model and the message-map dispatch engine.)
// HWND__ is app-private (mfxwnd.h) — reach the visible/enabled flags via helpers there.
extern int MfxWndVisible(HWND h);
extern int MfxWndEnabled(HWND h);
CWnd::~CWnd() {}
HWND CWnd::GetSafeHwnd() const { return this ? m_hWnd : 0; }
BOOL CWnd::IsWindowVisible() const { return MfxWndVisible(m_hWnd); }
BOOL CWnd::IsWindowEnabled() const { return MfxWndEnabled(m_hWnd); }
CDC* CWnd::GetDC() { return CDC::FromHandle(::GetDC(m_hWnd)); }
int  CWnd::ReleaseDC(CDC* pDC) { return ::ReleaseDC(m_hWnd, pDC ? pDC->m_hDC : 0); }
void CWnd::MoveWindow(LPCRECT r, BOOL bRepaint)
    { ::MoveWindow(m_hWnd, r->left, r->top, r->right - r->left, r->bottom - r->top, bRepaint); }
BOOL CWnd::SetWindowPos(const CWnd* pAfter, int x, int y, int cx, int cy, UINT nFlags)
    { return ::SetWindowPos(m_hWnd, pAfter ? pAfter->m_hWnd : 0, x, y, cx, cy, nFlags); }
int  CWnd::GetWindowText(LPSTR lpsz, int nMax) const { if (nMax > 0) lpsz[0] = 0; return 0; }
void CWnd::GetWindowText(CString& rString) const { rString.Empty(); }
// CWnd::CenterWindow / CWnd::GetDlgItem are REAL as of M5 (app/mfxwnd.cpp).
void CWnd::ScreenToClient(LPRECT lpRect) const
{
    POINT tl = { lpRect->left, lpRect->top }, br = { lpRect->right, lpRect->bottom };
    ::ScreenToClient(m_hWnd, &tl); ::ScreenToClient(m_hWnd, &br);
    SetRect(lpRect, tl.x, tl.y, br.x, br.y);
}
int  CWnd::MessageBox(LPCSTR lpszText, LPCSTR lpszCaption, UINT nType)
    { return ::MessageBoxA(m_hWnd, lpszText, lpszCaption, nType); }
BOOL CWnd::PreCreateWindow(CREATESTRUCT&) { return TRUE; }
void CWnd::PostNcDestroy() {}
LRESULT CWnd::Default() { return 0; }

int  CWnd::OnCreate(LPCREATESTRUCT) { return 0; }
void CWnd::OnDestroy() {}
void CWnd::OnPaint() {}
void CWnd::OnTimer(UINT) {}
void CWnd::OnSize(UINT, int, int) {}
void CWnd::OnShowWindow(BOOL, UINT) {}
void CWnd::OnKeyDown(UINT, UINT, UINT) {}
void CWnd::OnKeyUp(UINT, UINT, UINT) {}
void CWnd::OnChar(UINT, UINT, UINT) {}
void CWnd::OnMouseMove(UINT, CPoint) {}
void CWnd::OnLButtonDown(UINT, CPoint) {}
void CWnd::OnLButtonUp(UINT, CPoint) {}
void CWnd::OnRButtonDown(UINT, CPoint) {}
void CWnd::OnHScroll(UINT, UINT, CScrollBar*) {}
void CWnd::OnVScroll(UINT, UINT, CScrollBar*) {}
void CWnd::OnActivate(UINT, CWnd*, BOOL) {}
BOOL CWnd::OnSetCursor(CWnd*, UINT, UINT) { return FALSE; }
HBRUSH CWnd::OnCtlColor(CDC*, CWnd*, UINT) { return 0; }
BOOL CWnd::OnEraseBkgnd(CDC*) { return FALSE; }
void CWnd::OnGetMinMaxInfo(MINMAXINFO*) {}
BOOL CWnd::OnQueryNewPalette() { return FALSE; }
void CWnd::OnPaletteChanged(CWnd*) {}
void CWnd::OnPaletteIsChanging(CWnd*) {}
void CWnd::OnSysCommand(UINT nID, LPARAM)
{
    if ((nID & 0xfff0) == SC_MINIMIZE) MfxPlatMinimize();   // "Hide Me!" / real Win32 title-bar minimize
}
BOOL CWnd::OnQueryEndSession() { return TRUE; }

BEGIN_MESSAGE_MAP(CWnd, CCmdTarget)
END_MESSAGE_MAP()

// ── controls — REAL as of M4 in app/mfxctl.cpp (CEdit/CButton/CBitmapButton/CScrollBar) ─────
void CButton::SetCheck(int) {}
int  CButton::GetCheck() const { return 0; }
BOOL CBitmapButton::AutoLoad(UINT, CWnd*) { MFX_STUB(); return TRUE; }
void CBitmapButton::SizeToContent() {}
BOOL CProgressCtrl::Create(DWORD, const RECT&, CWnd*, UINT) { return TRUE; }
int  CProgressCtrl::StepIt() { m_nPos += m_nStep; return m_nPos; }
int  CProgressCtrl::SetPos(int nPos) { int old = m_nPos; m_nPos = nPos; return old; }

// ── GDI wrappers (real SDL-backed C API lands at M1 in src/gdi/) ─────────────────────────────
CGdiObject::~CGdiObject() { if (m_hObject) DeleteObject(); }
BOOL CGdiObject::DeleteObject()
{
    if (!m_hObject) return FALSE;
    ::DeleteObject(m_hObject);
    m_hObject = 0;
    return TRUE;
}
HGDIOBJ CGdiObject::GetSafeHandle() const { return this ? m_hObject : 0; }
CBitmap* CBitmap::FromHandle(HBITMAP h)
    { static CBitmap wrap; wrap.Detach(); wrap.m_hObject = (HGDIOBJ)h; return &wrap; }
CPalette* CPalette::FromHandle(HPALETTE h)
    { static CPalette wrap; wrap.Detach(); wrap.m_hObject = (HGDIOBJ)h; return &wrap; }
CFont* CFont::FromHandle(HFONT h)
    { static CFont wrap; wrap.Detach(); wrap.m_hObject = (HGDIOBJ)h; return &wrap; }
BOOL CBitmap::LoadBitmap(UINT) { MFX_STUB(); return FALSE; }
BOOL CFont::CreateFontIndirect(const LOGFONT*) { MFX_STUB(); return FALSE; }

CDC::~CDC() {}
HDC CDC::GetSafeHdc() const { return this ? m_hDC : 0; }
CDC* CDC::FromHandle(HDC hDC)
    { static CDC wrap; wrap.m_hDC = hDC; return &wrap; }
BOOL CDC::DeleteDC() { BOOL b = ::DeleteDC(m_hDC); m_hDC = 0; return b; }
// A small ring of wrap objects (MFC's FromHandle temp-map, poor-man's edition): the game
// holds pOldPen AND pOldBrush from consecutive calls (DrawHealthNeedle), so one static
// would alias them and the restores would select the wrong object.
static CGdiObject* MfxWrapGdiObj(HGDIOBJ h)
{
    static CGdiObject aWrap[8];
    static int nNext = 0;
    CGdiObject* p = &aWrap[nNext];
    nNext = (nNext + 1) % 8;
    p->Detach();
    p->m_hObject = h;
    return p;
}

CGdiObject* CDC::SelectObject(CGdiObject* pObject)
{
    return MfxWrapGdiObj(::SelectObject(m_hDC, pObject ? pObject->m_hObject : 0));
}
CPalette* CDC::SelectPalette(CPalette* pPalette, BOOL bForceBackground)
{
    HPALETTE hOld = ::SelectPalette(m_hDC, pPalette ? (HPALETTE)pPalette->m_hObject : 0, bForceBackground);
    return CPalette::FromHandle(hOld);
}

CGdiObject* CDC::SelectStockObject(int nIndex)
{
    return MfxWrapGdiObj(::SelectObject(m_hDC, ::GetStockObject(nIndex)));
}

CPaintDC::CPaintDC(CWnd* pWnd) : m_pWnd(pWnd) { m_hDC = ::GetDC(pWnd ? pWnd->m_hWnd : 0); }
CPaintDC::~CPaintDC() { ::ReleaseDC(m_pWnd ? m_pWnd->m_hWnd : 0, m_hDC); m_hDC = 0; }
CClientDC::CClientDC(CWnd* pWnd) : m_pWnd(pWnd) { m_hDC = ::GetDC(pWnd ? pWnd->m_hWnd : 0); }
CClientDC::~CClientDC() { ::ReleaseDC(m_pWnd ? m_pWnd->m_hWnd : 0, m_hDC); m_hDC = 0; }

// ── doc / view / frame / dialog / app / thread ───────────────────────────────────────────────
CRuntimeClass CDocument::classCDocument = { "CDocument", sizeof(CDocument), 0xFFFF, 0, &CObject::classCObject };
CRuntimeClass* CDocument::GetRuntimeClass() const { return &CDocument::classCDocument; }
CDocument::CDocument() : m_bModified(FALSE), m_pOnlyView(0) {}
CDocument::~CDocument() {}
BOOL CDocument::OnNewDocument() { DeleteContents(); m_strPathName.Empty(); m_bModified = FALSE; return TRUE; }
BOOL CDocument::OnOpenDocument(LPCSTR lpszPathName)
{
    CFile f;
    CFileException fe;
    if (!f.Open(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &fe))
    {
        ReportSaveLoadException(lpszPathName, &fe, FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
        return FALSE;
    }
    DeleteContents();
    CArchive ar(&f, CArchive::load | CArchive::bNoFlushOnDelete);
    ar.m_pDocument = this;
    Serialize(ar);
    ar.Close();
    f.Close();
    m_bModified = FALSE;
    return TRUE;
}
BOOL CDocument::OnSaveDocument(LPCSTR) { MFX_STUB(); return TRUE; }
void CDocument::OnCloseDocument() { DeleteContents(); }
void CDocument::DeleteContents() {}
void CDocument::Serialize(CArchive&) {}
void CDocument::ReportSaveLoadException(LPCSTR lpszPathName, CException*, BOOL bSaving, UINT)
{
    fprintf(stderr, "microfx: doc %s failed: %s\n", bSaving ? "save" : "load",
            lpszPathName ? lpszPathName : "(null)");
}
POSITION CDocument::GetFirstViewPosition() const { return (POSITION)m_pOnlyView; }
CView* CDocument::GetNextView(POSITION& rPosition) const
    { CView* pView = (CView*)rPosition; rPosition = 0; return pView; }
void CDocument::UpdateAllViews(CView* pSender, LPARAM lHint, CObject* pHint)
{
    if (m_pOnlyView && m_pOnlyView != pSender)
        m_pOnlyView->OnUpdate(pSender, lHint, pHint);
}
void CDocument::AddView(CView* pView) { m_pOnlyView = pView; if (pView) pView->m_pDocument = this; }
BEGIN_MESSAGE_MAP(CDocument, CCmdTarget)
END_MESSAGE_MAP()

CRuntimeClass CView::classCView = { "CView", sizeof(CView), 0xFFFF, 0, &CObject::classCObject };
CRuntimeClass* CView::GetRuntimeClass() const { return &CView::classCView; }
CView::CView() : m_pDocument(0) {}
CView::~CView() {}
void CView::OnInitialUpdate() { OnUpdate(0, 0, 0); }
void CView::OnUpdate(CView*, LPARAM, CObject*) { Invalidate(); }
void CView::OnActivateView(BOOL, CView*, CView*) {}
BEGIN_MESSAGE_MAP(CView, CWnd)
    ON_WM_PAINT()                    // real MFC: CView::OnPaint → CPaintDC + OnDraw (mfxwnd.cpp)
END_MESSAGE_MAP()

const RECT CFrameWnd::rectDefault = { 0, 0, 640, 480 };
CRuntimeClass CFrameWnd::classCFrameWnd = { "CFrameWnd", sizeof(CFrameWnd), 0xFFFF, CFrameWnd::CreateObject, &CObject::classCObject };
CRuntimeClass* CFrameWnd::GetRuntimeClass() const { return &CFrameWnd::classCFrameWnd; }
CObject* CFrameWnd::CreateObject() { return new CFrameWnd; }
CFrameWnd::CFrameWnd() : m_pViewActive(0), m_bAutoMenuEnable(TRUE) {}
CFrameWnd::~CFrameWnd() {}
// LoadFrame / OnCreate / OnCreateClient are REAL as of M2 (mfxwnd.cpp).
CDocument* CFrameWnd::GetActiveDocument()
    { return m_pViewActive ? m_pViewActive->GetDocument() : 0; }
BEGIN_MESSAGE_MAP(CFrameWnd, CWnd)
END_MESSAGE_MAP()

CDialog::CDialog() : m_nIDTemplate(0), m_pParentWnd(0), m_nModalResult(IDCANCEL) {}
CDialog::CDialog(UINT nIDTemplate, CWnd* pParentWnd)
    : m_nIDTemplate(nIDTemplate), m_pParentWnd(pParentWnd), m_nModalResult(IDCANCEL) {}
CDialog::CDialog(LPCSTR, CWnd* pParentWnd)
    : m_nIDTemplate(0), m_pParentWnd(pParentWnd), m_nModalResult(IDCANCEL) {}
CDialog::~CDialog() {}
// CDialog::DoModal is REAL as of M5 (app/mfxdlg.cpp: parse RT_DIALOG, create controls, modal loop).
BOOL CDialog::OnInitDialog() { UpdateData(FALSE); return TRUE; }
void CDialog::OnOK() { UpdateData(TRUE); EndDialog(IDOK); }
void CDialog::OnCancel() { EndDialog(IDCANCEL); }
void CDialog::DoDataExchange(CDataExchange*) {}
BOOL CDialog::UpdateData(BOOL bSaveAndValidate)
{
    CDataExchange dx(this, bSaveAndValidate);
    DoDataExchange(&dx);
    return TRUE;
}
void CDialog::EndDialog(int nResult) { m_nModalResult = nResult; }
// standard dialog command routing: OK/Cancel buttons → OnOK/OnCancel (→ EndDialog).
BEGIN_MESSAGE_MAP(CDialog, CWnd)
    ON_COMMAND(IDOK, OnOK)
    ON_COMMAND(IDCANCEL, OnCancel)
END_MESSAGE_MAP()

// DDX_Text / DDX_Control / DDX_Check are REAL as of M5 (app/mfxdlg.cpp).

CFileDialog::CFileDialog(BOOL bOpenFileDialog, LPCSTR lpszDefExt, LPCSTR lpszFileName,
                         DWORD dwFlags, LPCSTR, CWnd* pParentWnd)
    : CDialog(0u, pParentWnd), m_bOpenFileDialog(bOpenFileDialog)
{
    memset(&m_ofn, 0, sizeof m_ofn);
    m_ofn.Flags = dwFlags;
    m_ofn.lpstrDefExt = lpszDefExt;
    if (lpszFileName) m_strPath = lpszFileName;
}
// CFileDialog::DoModal is REAL as of M5 tail (app/mfxdlg.cpp: no OS file picker on SDL, so it
// lists *.<ext> files in the save dir as clickable rows — see the comment there).
CString CFileDialog::GetPathName() const { return m_strPath; }

CWinThread::CWinThread() : m_pMainWnd(0), m_bAutoDelete(TRUE), m_hThread(0) {}
CWinThread::~CWinThread() {}
int  CWinThread::ExitInstance() { return 0; }
// CWinThread::Run is the M2 SDL event pump — app/mfxpump.cpp (headless no-op without SDL2).
BOOL CWinThread::OnIdle(LONG) { return FALSE; }

CWinThread* AfxBeginThread(AFX_THREADPROC, LPVOID, int, UINT, DWORD, void*)
{
    // Deliberately a no-thread object (M3 decision): the only caller is SoundInit's music
    // pump (MusicThreadProcMaybe = WaveMixPump loop) and SDL2_mixer mixes in its own
    // callback thread — the proc must NOT run or it would spin forever on the stub event.
    return new CWinThread;
}

CDocTemplate::CDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
                           CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
    : m_nIDResource(nIDResource), m_pDocClass(pDocClass),
      m_pFrameClass(pFrameClass), m_pViewClass(pViewClass), m_pDoc(0) {}

extern CWinApp* g_pMfxApp;
CWinApp::CWinApp(LPCSTR lpszAppName)
    : m_pszAppName(lpszAppName), m_pszProfileName(0), m_pszExeName(0),
      m_hInstance(0), m_hPrevInstance(0), m_lpCmdLine((LPSTR)""), m_nCmdShow(SW_SHOWNORMAL),
      m_pDocTemplate(0)
{
    g_pMfxApp = this;
}
CWinApp::~CWinApp() { if (g_pMfxApp == this) g_pMfxApp = 0; }
BOOL CWinApp::InitInstance() { return TRUE; }
int  CWinApp::ExitInstance() { return 0; }
BOOL CWinApp::OnIdle(LONG) { return FALSE; }
void CWinApp::AddDocTemplate(CDocTemplate* pTemplate) { m_pDocTemplate = pTemplate; }
HCURSOR CWinApp::LoadCursor(UINT nID) const { return ::LoadCursorA(AfxGetResourceHandle(), MAKEINTRESOURCE(nID)); }
HCURSOR CWinApp::LoadCursor(LPCSTR lpszName) const { return ::LoadCursorA(AfxGetResourceHandle(), lpszName); }
HCURSOR CWinApp::LoadStandardCursor(LPCSTR lpszName) const { return ::LoadCursorA(0, lpszName); }
HICON   CWinApp::LoadIcon(UINT nID) const { return ::LoadIconA(AfxGetResourceHandle(), MAKEINTRESOURCE(nID)); }

// CWinApp::OnFileNew is REAL as of M2 (mfxwnd.cpp): full MFC SDI bootstrap — doc, then
// LoadFrame (WM_CREATE → OnCreateClient → view window), then OnNewDocument + initial update.
void CWinApp::OnFileOpen() { MFX_STUB(); }
void CWinApp::OnAppExit() { MFX_STUB(); }
void CWinApp::OnHelp() {}
void CWinApp::OnHelpIndex() {}
void CWinApp::OnHelpUsing() {}
void CWinApp::OnContextHelp() {}

// profile settings — real INI store, "<exebase>.INI" next to the executable (the Win32 build
// keeps the same [OPTIONS]/[GameData] format in <exe>.INI in the Windows dir, so a bottle's
// INI can be copied over verbatim — that is how the M0 worldgen oracle aligns settings).
static void MfxProfilePath(char* szIni, size_t nCap)
{
    char szExe[1024];
    GetModuleFileNameA(0, szExe, sizeof(szExe));
    strncpy(szIni, szExe, nCap - 5);
    szIni[nCap - 5] = 0;
    char* pDot = strrchr(szIni, '.');
    char* pSlash = strrchr(szIni, '/');
    if (pDot && (!pSlash || pDot > pSlash)) *pDot = 0;
    strcat(szIni, ".INI");
}
static int MfxStrICmp(const char* a, const char* b)
{
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d || !*a) return d;
    }
}
static BOOL MfxProfileLookup(LPCSTR lpszSection, LPCSTR lpszEntry, CString& strOut)
{
    char szIni[1100];
    MfxProfilePath(szIni, sizeof(szIni));
    FILE* f = fopen(szIni, "r");
    if (!f) return FALSE;
    char line[512];
    int bInSection = 0;
    BOOL bFound = FALSE;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' ')) line[--n] = 0;
        if (line[0] == '[') {
            char* pEnd = strchr(line, ']');
            if (pEnd) { *pEnd = 0; bInSection = MfxStrICmp(line + 1, lpszSection) == 0; }
            continue;
        }
        if (!bInSection) continue;
        char* pEq = strchr(line, '=');
        if (!pEq) continue;
        *pEq = 0;
        if (MfxStrICmp(line, lpszEntry) == 0) { strOut = pEq + 1; bFound = TRUE; break; }
    }
    fclose(f);
    return bFound;
}
static BOOL MfxProfileWrite(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszValue)
{
    char szIni[1100];
    MfxProfilePath(szIni, sizeof(szIni));
    // read whole file, replace or append the key within its section, rewrite
    CString strOut;
    char line[512];
    int bInSection = 0, bWritten = 0;
    FILE* f = fopen(szIni, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            size_t n = strlen(line);
            while (n && (line[n-1] == '\n' || line[n-1] == '\r')) line[--n] = 0;
            if (line[0] == '[') {
                if (bInSection && !bWritten) {          // leaving the section: append key
                    strOut += lpszEntry; strOut += "="; strOut += lpszValue; strOut += "\n";
                    bWritten = 1;
                }
                char szName[512];
                strcpy(szName, line + 1);
                char* pEnd = strchr(szName, ']');
                if (pEnd) *pEnd = 0;
                bInSection = MfxStrICmp(szName, lpszSection) == 0;
            }
            else if (bInSection && !bWritten) {
                char szKey[512];
                strcpy(szKey, line);
                char* pEq = strchr(szKey, '=');
                if (pEq) {
                    *pEq = 0;
                    if (MfxStrICmp(szKey, lpszEntry) == 0) {
                        strOut += lpszEntry; strOut += "="; strOut += lpszValue; strOut += "\n";
                        bWritten = 1;
                        continue;
                    }
                }
            }
            strOut += line; strOut += "\n";
        }
        fclose(f);
        if (bInSection && !bWritten) {                  // section was last in the file
            strOut += lpszEntry; strOut += "="; strOut += lpszValue; strOut += "\n";
            bWritten = 1;
        }
    }
    if (!bWritten) {
        strOut += "["; strOut += lpszSection; strOut += "]\n";
        strOut += lpszEntry; strOut += "="; strOut += lpszValue; strOut += "\n";
    }
    f = fopen(szIni, "w");
    if (!f) return FALSE;
    fputs((const char*)strOut, f);
    fclose(f);
    return TRUE;
}
UINT CWinApp::GetProfileInt(LPCSTR lpszSection, LPCSTR lpszEntry, int nDefault)
{
    CString s;
    return MfxProfileLookup(lpszSection, lpszEntry, s) ? (UINT)atoi(s) : (UINT)nDefault;
}
BOOL CWinApp::WriteProfileInt(LPCSTR lpszSection, LPCSTR lpszEntry, int nValue)
{
    char szVal[32];
    sprintf(szVal, "%d", nValue);
    return MfxProfileWrite(lpszSection, lpszEntry, szVal);
}
CString CWinApp::GetProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszDefault)
{
    CString s;
    if (MfxProfileLookup(lpszSection, lpszEntry, s)) return s;
    return CString(lpszDefault ? lpszDefault : "");
}
BOOL CWinApp::WriteProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszValue)
{
    return MfxProfileWrite(lpszSection, lpszEntry, lpszValue ? lpszValue : "");
}

// CWinThread has no map of its own — CWinApp's chains straight to CCmdTarget's root.
BEGIN_MESSAGE_MAP(CWinApp, CCmdTarget)
END_MESSAGE_MAP()

// ── Win32 C stubs (replaced by src/gdi + src/app implementations at M1/M2) ───────────────────
extern "C" {

int      GetSystemMetrics(int) { return 0; }
// GetSysColor is REAL as of M4 — gdi/mfxgdi.cpp (Win95 scheme; feeds sysPalette[1..3]).
// LoadCursorA/LoadIconA/LoadBitmapA are REAL as of M4 — res/mfxres.cpp (embedded .res).
// SetCursor is REAL as of M4 — mfxwnd.cpp (stores the current cursor; the pump applies it).
int      ShowCursor(BOOL) { return 0; }
// GetCursorPos / GetAsyncKeyState / SetTimer / KillTimer / SendMessageA / PostMessageA are
// REAL as of M2 (mfxwnd.cpp — pump-fed state, timer table, message-map dispatch).
// GetMessageA/TranslateMessage/DispatchMessageA are REAL as of M4 (mfxpump.cpp — now
// platform-neutral, so they exist in EVERY build; with the null backend GetMessageA bails
// immediately → modal loops auto-dismiss, the headless contract).
int      MessageBoxA(HWND, LPCSTR text, LPCSTR, UINT type)
{
    fprintf(stderr, "microfx: MessageBox: %s\n", text ? text : "(null)");
    return (type & MB_YESNO) ? IDYES : IDOK;
}
BOOL     MessageBeep(UINT) { return TRUE; }
HWND     FindWindowA(LPCSTR, LPCSTR) { return 0; }
BOOL     BringWindowToTop(HWND) { return TRUE; }
BOOL     IsIconic(HWND) { return FALSE; }
HWND     GetLastActivePopup(HWND h) { return h; }
// GetActiveWindow / GetDC / ReleaseDC / ShowWindow / DestroyWindow / SetFocus / SetCapture /
// ReleaseCapture / GetClientRect / GetWindowRect are REAL as of M2 (mfxwnd.cpp);
// EnableWindow / MoveWindow / SetWindowPos / SetWindowTextA are REAL as of M4 (mfxwnd.cpp).
BOOL     ScreenToClient(HWND, LPPOINT) { return TRUE; }   // window client == screen (one window)
BOOL     ClientToScreen(HWND, LPPOINT) { return TRUE; }
// Get/SetScrollPos/Range + ShowScrollBar are REAL as of M4e (mfxwnd.cpp, SB_CTL state).
// DrawIcon is REAL as of M4 — gdi/mfxgdi.cpp (res/ image draw w/ palette mapping).
// InvalidateRect / UpdateWindow / RedrawWindow are REAL as of M2 (mfxwnd.cpp dirty flag).
// ::GetParent is REAL as of M4e-fix (mfxwnd.cpp) — InvScrollBar::OnVScroll finds the view
// through it to repaint the item list after a scroll (stub 0 = list never redrew).
void     FatalAppExitA(UINT, LPCSTR msg)
    { fprintf(stderr, "microfx: FatalAppExit: %s\n", msg ? msg : ""); abort(); }
BOOL     CopyRect(LPRECT dst, const RECT* src) { *dst = *src; return TRUE; }
// FillRect/PatBlt/pens/brushes/fonts/stock objects/lines/pixels/Pie/RoundRect/Polygon/
// Rectangle/SetTextColor/SetBkColor/SetBkMode/GetClipBox/GetSysColor are REAL as of M4 —
// gdi/mfxgdi.cpp (draw state on the DC + COLORREF→palette-index mapping).

// CreateCompatibleDC/DeleteDC/SelectObject/DeleteObject/CreateDIBSection/BitBlt/
// SetDIBColorTable are REAL as of M1 — gdi/mfxgdi.cpp (DIB sections + memory DCs).
int      GetObjectA(HGDIOBJ, int, LPVOID) { return 0; }
int      GetDeviceCaps(HDC, int index)
{
    // pretend to be the 8-bit palettized display the game requires
    switch (index)
    {
    case BITSPIXEL:   return 8;
    case PLANES:      return 1;
    case RASTERCAPS:  return RC_PALETTE;
    case SIZEPALETTE: return 256;
    case NUMCOLORS:   return 256;
    default:          return 0;
    }
}
// Palettes (Create/CreateHalftone/Realize/Select/Animate/Get/Set/GetSystemPaletteEntries/
// GetNearestPaletteIndex) are REAL as of M2 — gdi/mfxgdi.cpp (entries → DIB color table).
// CreateBitmap/SetBitmapBits/GetBitmapBits are REAL as of M2 tail — gdi/mfxgdi.cpp
// (drag save-under: an 8bpp DDB is just a DIB in our device).
// PostQuitMessage is REAL as of M2 (mfxwnd.cpp — sets the pump's quit flag).
BOOL     Chord(HDC, int, int, int, int, int, int, int, int) { return TRUE; }
// TextOutA/GetTextMetricsA/GetTextExtentPoint32A are REAL as of M4c — gdi/mfxgdi.cpp
// (genuine MS Sans Serif bitmap strikes, mfxfont_data.c).

DWORD    GetVersion(void) { return 0xC3B60004; }   // report Win95 (4.0 build 950)
HINSTANCE GetModuleHandleA(LPCSTR) { return 0; }
DWORD    GetModuleFileNameA(HINSTANCE, LPSTR lpFilename, DWORD nSize)
{
    // real exe path, '/'-separated; the game derives its data-file directory from this
    // (InitInstance: _splitpath/_makepath, then appends "\\<GAME>.DTA" — CFile::Open
    // normalizes the mixed separators back to '/')
    if (nSize == 0) return 0;
    char szBuf[1024];
    szBuf[0] = 0;
#ifdef __APPLE__
    uint32_t nBuf = sizeof(szBuf);
    if (_NSGetExecutablePath(szBuf, &nBuf) != 0)
        szBuf[0] = 0;
#else
    ssize_t n = readlink("/proc/self/exe", szBuf, sizeof(szBuf) - 1);
    if (n > 0) szBuf[n] = 0; else szBuf[0] = 0;
#endif
    strncpy(lpFilename, szBuf, nSize - 1);
    lpFilename[nSize - 1] = 0;
    return (DWORD)strlen(lpFilename);
}
HANDLE   CreateEventA(void*, BOOL, BOOL, LPCSTR) { return (HANDLE)1; }
BOOL     SetEvent(HANDLE) { return TRUE; }
BOOL     ResetEvent(HANDLE) { return TRUE; }
DWORD    WaitForSingleObject(HANDLE, DWORD) { return WAIT_OBJECT_0; }
DWORD    ResumeThread(HANDLE) { return 0; }
DWORD    SuspendThread(HANDLE) { return 0; }
BOOL     CloseHandle(HANDLE) { return TRUE; }

// WAVMIX32 + PlaySound/mciSendString live in snd/mfxsnd.cpp as of M3 (SDL2_mixer backend,
// with built-in silent stubs when SDL2_mixer is absent).
DWORD    timeGetTime(void) { return GetTickCount(); }

LONG     RegOpenKeyExA(HKEY, LPCSTR, DWORD, DWORD, HKEY* phk) { if (phk) *phk = 0; return 2; }
LONG     RegQueryValueExA(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD) { return 2; }
LONG     RegCloseKey(HKEY) { return 0; }
UINT     GetDriveTypeA(LPCSTR) { return DRIVE_NO_ROOT_DIR; }   // ends the A:..Z: install scan
LPSTR    lstrcpyA(LPSTR dst, LPCSTR src) { return strcpy(dst, src ? src : ""); }
LPSTR    lstrcatA(LPSTR dst, LPCSTR src) { return strcat(dst, src ? src : ""); }
int      lstrlenA(LPCSTR s) { return s ? (int)strlen(s) : 0; }

} // extern "C"
