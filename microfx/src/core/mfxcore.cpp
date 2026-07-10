// microfx core — no-SDL implementations: CString, CFile, collections, CObject/CRuntimeClass,
// exceptions, afx globals, rect helpers, real GetTickCount/Sleep (Iact's RNG seeds off the
// tick counter, so it must be REAL even in pure-logic milestones).
#include <afxwin.h>
#include <ctype.h>
#include <time.h>

// ── CRuntimeClass / CObject ──────────────────────────────────────────────────────────────────
CObject* CRuntimeClass::CreateObject()
{
    return m_pfnCreateObject ? (*m_pfnCreateObject)() : 0;
}

BOOL CRuntimeClass::IsDerivedFrom(const CRuntimeClass* pBaseClass) const
{
    for (const CRuntimeClass* p = this; p; p = p->m_pBaseClass)
        if (p == pBaseClass)
            return TRUE;
    return FALSE;
}

CRuntimeClass CObject::classCObject = { "CObject", sizeof(CObject), 0xFFFF, 0, 0 };
CRuntimeClass* CObject::GetRuntimeClass() const { return &CObject::classCObject; }
void CObject::Serialize(CArchive&) {}
BOOL CObject::IsKindOf(const CRuntimeClass* pClass) const
{
    return GetRuntimeClass()->IsDerivedFrom(pClass);
}

// ── exceptions ───────────────────────────────────────────────────────────────────────────────
int CException::ReportError(UINT, UINT)
{
    fprintf(stderr, "microfx: CException::ReportError\n");
    return IDOK;
}

AFX_EXCEPTION_LINK::~AFX_EXCEPTION_LINK()
{
    if (m_pException)
        m_pException->Delete();
}

void AfxThrowFileException(int cause, LONG lOsError)
{
    throw new CFileException(cause, lOsError);
}

void AfxThrowMemoryException()
{
    throw new CMemoryException();
}

// ── CString ──────────────────────────────────────────────────────────────────────────────────
// Layout: one char* pointing just past a CStringData header. The empty string is a shared
// static (never freed); every non-empty string owns its block.
static struct { CStringData hdr; char ch[4]; } _mfxEmptyString = { { -1, 0, 0 }, { 0, 0, 0, 0 } };
static char* _mfxEmptyStr() { return _mfxEmptyString.hdr.data(); }

void CString::Init() { m_pchData = _mfxEmptyStr(); }

void CString::AllocBuffer(int nLen)
{
    if (nLen <= 0) { Init(); return; }
    CStringData* pData = (CStringData*)malloc(sizeof(CStringData) + nLen + 1);
    pData->nRefs = 1;
    pData->nDataLength = nLen;
    pData->nAllocLength = nLen;
    m_pchData = pData->data();
    m_pchData[nLen] = 0;
}

void CString::Empty()
{
    if (GetData()->nRefs != -1)
        free(GetData());
    Init();
}

CString::~CString()
{
    if (GetData()->nRefs != -1)
        free(GetData());
}

CString::CString(char ch, int nRepeat)
{
    Init();
    if (nRepeat > 0)
    {
        AllocBuffer(nRepeat);
        memset(m_pchData, ch, nRepeat);
    }
}

void CString::Assign(LPCSTR p, int n)
{
    if (!p) n = 0;
    // self-assign safe: build the new block before freeing the old
    char* pOld = m_pchData;
    CStringData* pOldData = (pOld == _mfxEmptyStr()) ? 0 : GetData();
    if (n <= 0)
        m_pchData = _mfxEmptyStr();
    else
    {
        CStringData* pData = (CStringData*)malloc(sizeof(CStringData) + n + 1);
        pData->nRefs = 1;
        pData->nDataLength = n;
        pData->nAllocLength = n;
        memcpy(pData->data(), p, n);
        pData->data()[n] = 0;
        m_pchData = pData->data();
    }
    if (pOldData)
        free(pOldData);
}

void CString::Concat(LPCSTR p, int n)
{
    if (!p || n <= 0) return;
    int nOld = GetLength();
    CStringData* pData = (CStringData*)malloc(sizeof(CStringData) + nOld + n + 1);
    pData->nRefs = 1;
    pData->nDataLength = nOld + n;
    pData->nAllocLength = nOld + n;
    memcpy(pData->data(), m_pchData, nOld);
    memcpy(pData->data() + nOld, p, n);
    pData->data()[nOld + n] = 0;
    if (GetData()->nRefs != -1)
        free(GetData());
    m_pchData = pData->data();
}

CString operator+(const CString& a, const CString& b) { CString r(a); r += b; return r; }
CString operator+(const CString& a, LPCSTR b)         { CString r(a); r += b; return r; }
CString operator+(LPCSTR a, const CString& b)         { CString r(a); r += b; return r; }
CString operator+(const CString& a, char b)           { CString r(a); r += b; return r; }
CString operator+(char a, const CString& b)           { CString r(a, 1); r += b; return r; }

int CString::CompareNoCase(LPCSTR psz) const
{
    const char* a = m_pchData;
    const char* b = psz ? psz : "";
    for (;; ++a, ++b)
    {
        int ca = tolower((unsigned char)*a), cb = tolower((unsigned char)*b);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (!ca) return 0;
    }
}

CString CString::Mid(int nFirst, int nCount) const
{
    int len = GetLength();
    if (nFirst < 0) nFirst = 0;
    if (nFirst > len) nFirst = len;
    if (nCount > len - nFirst) nCount = len - nFirst;
    if (nCount < 0) nCount = 0;
    return CString(m_pchData + nFirst, nCount);
}

CString CString::Left(int nCount) const
{
    if (nCount < 0) nCount = 0;
    if (nCount > GetLength()) nCount = GetLength();
    return CString(m_pchData, nCount);
}

CString CString::Right(int nCount) const
{
    int len = GetLength();
    if (nCount < 0) nCount = 0;
    if (nCount > len) nCount = len;
    return CString(m_pchData + len - nCount, nCount);
}

int CString::Find(char ch) const
{
    const char* p = strchr(m_pchData, ch);
    return p ? (int)(p - m_pchData) : -1;
}

int CString::Find(LPCSTR psz) const
{
    if (!psz) return -1;
    const char* p = strstr(m_pchData, psz);
    return p ? (int)(p - m_pchData) : -1;
}

int CString::FindOneOf(LPCSTR pszCharSet) const
{
    if (!pszCharSet) return -1;
    size_t n = strcspn(m_pchData, pszCharSet);
    return m_pchData[n] ? (int)n : -1;
}

int CString::ReverseFind(char ch) const
{
    const char* p = strrchr(m_pchData, ch);
    return p ? (int)(p - m_pchData) : -1;
}

void CString::MakeUpper() { for (char* p = m_pchData; *p; ++p) *p = (char)toupper((unsigned char)*p); }
void CString::MakeLower() { for (char* p = m_pchData; *p; ++p) *p = (char)tolower((unsigned char)*p); }

void CString::TrimLeft()
{
    const char* p = m_pchData;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (p != m_pchData) Assign(p, (int)strlen(p));
}

void CString::TrimRight()
{
    int n = GetLength();
    while (n > 0 && isspace((unsigned char)m_pchData[n - 1])) --n;
    if (n != GetLength()) Assign(m_pchData, n);
}

void CString::Format(LPCSTR pszFormat, ...)
{
    va_list ap;
    va_start(ap, pszFormat);
    char buf[1024];
    vsnprintf(buf, sizeof buf, pszFormat, ap);
    va_end(ap);
    Assign(buf, (int)strlen(buf));
}

BOOL CString::LoadString(UINT nID)
{
    // Real string-table lookup arrives with microfx/src/res (M4).
    extern BOOL MfxLoadString(UINT nID, CString& str);   // weak hook; res/ overrides
    return MfxLoadString(nID, *this);
}

LPSTR CString::GetBuffer(int nMinBufLength)
{
    int len = GetLength();
    if (nMinBufLength < len) nMinBufLength = len;
    if (nMinBufLength > GetData()->nAllocLength || GetData()->nRefs == -1)
    {
        CStringData* pData = (CStringData*)malloc(sizeof(CStringData) + nMinBufLength + 1);
        pData->nRefs = 1;
        pData->nDataLength = len;
        pData->nAllocLength = nMinBufLength;
        memcpy(pData->data(), m_pchData, len + 1);
        if (GetData()->nRefs != -1)
            free(GetData());
        m_pchData = pData->data();
    }
    return m_pchData;
}

void CString::ReleaseBuffer(int nNewLength)
{
    if (nNewLength < 0) nNewLength = (int)strlen(m_pchData);
    if (GetData()->nRefs != -1)
    {
        m_pchData[nNewLength] = 0;
        GetData()->nDataLength = nNewLength;
    }
}

// default LoadString hook — the res/ module (M4) provides the real one reading the string table
BOOL MfxLoadString(UINT nID, CString& str)
{
    char buf[32];
    snprintf(buf, sizeof buf, "[string %u]", nID);
    str = buf;
    return FALSE;
}

// ── CFile ────────────────────────────────────────────────────────────────────────────────────
CFile::CFile(LPCSTR pszFileName, UINT nOpenFlags) : m_pStream(0)
{
    CFileException fe;
    if (!Open(pszFileName, nOpenFlags, &fe))
        throw new CFileException(fe.m_cause, fe.m_lOsError);
}

CFile::~CFile()
{
    if (m_pStream)
        Close();
}

BOOL CFile::Open(LPCSTR pszFileName, UINT nOpenFlags, CFileException* pError)
{
    ASSERT(m_pStream == 0);
    // The game builds Win32-shaped paths ("<dir>\\FILE.DTA" via _makepath + '\\' appends);
    // fopen on POSIX hosts needs forward slashes.
    char szPath[1024];
    size_t nLen = strlen(pszFileName);
    if (nLen >= sizeof(szPath)) nLen = sizeof(szPath) - 1;
    for (size_t i = 0; i < nLen; i++)
        szPath[i] = pszFileName[i] == '\\' ? '/' : pszFileName[i];
    szPath[nLen] = 0;
    const char* mode;
    if (nOpenFlags & modeCreate)
        mode = (nOpenFlags & modeNoTruncate) ? "r+b" : "w+b";
    else if (nOpenFlags & modeReadWrite)
        mode = "r+b";
    else if (nOpenFlags & modeWrite)
        mode = "r+b";                 // Win32 modeWrite w/o modeCreate opens existing
    else
        mode = "rb";
    m_pStream = fopen(szPath, mode);
    if (!m_pStream && (nOpenFlags & modeCreate) && (nOpenFlags & modeNoTruncate))
        m_pStream = fopen(szPath, "w+b");   // create-if-missing
    if (!m_pStream)
    {
        if (pError)
        {
            pError->m_cause = CFileException::fileNotFound;
            pError->m_lOsError = -1;
        }
        return FALSE;
    }
    m_strFileName = pszFileName;
    return TRUE;
}

UINT CFile::Read(void* lpBuf, UINT nCount)
{
    ASSERT(m_pStream);
    return (UINT)fread(lpBuf, 1, nCount, m_pStream);
}

void CFile::Write(const void* lpBuf, UINT nCount)
{
    ASSERT(m_pStream);
    if (fwrite(lpBuf, 1, nCount, m_pStream) != nCount)
        AfxThrowFileException(CFileException::diskFull);
}

LONG CFile::Seek(LONG lOff, UINT nFrom)
{
    ASSERT(m_pStream);
    fseek(m_pStream, lOff, nFrom == begin ? SEEK_SET : nFrom == current ? SEEK_CUR : SEEK_END);
    return (LONG)ftell(m_pStream);
}

DWORD CFile::GetPosition() const
{
    ASSERT(m_pStream);
    return (DWORD)ftell(m_pStream);
}

DWORD CFile::GetLength() const
{
    ASSERT(m_pStream);
    long cur = ftell(m_pStream);
    fseek(m_pStream, 0, SEEK_END);
    long len = ftell(m_pStream);
    fseek(m_pStream, cur, SEEK_SET);
    return (DWORD)len;
}

void CFile::Close()
{
    if (m_pStream)
    {
        fclose(m_pStream);
        m_pStream = 0;
    }
    m_strFileName.Empty();
}

void CFile::Abort()
{
    if (m_pStream)
    {
        fclose(m_pStream);
        m_pStream = 0;
    }
    m_strFileName.Empty();
}

// ── collections ──────────────────────────────────────────────────────────────────────────────
void CObArray::EnsureAlloc(int n)
{
    if (n <= m_nMaxSize) return;
    int nNew = m_nMaxSize ? m_nMaxSize : 8;
    while (nNew < n) nNew *= 2;
    m_pData = (CObject**)realloc(m_pData, nNew * sizeof(CObject*));
    m_nMaxSize = nNew;
}

void CObArray::SetSize(int nNewSize, int)
{
    EnsureAlloc(nNewSize);
    if (nNewSize > m_nSize)
        memset(m_pData + m_nSize, 0, (nNewSize - m_nSize) * sizeof(CObject*));
    m_nSize = nNewSize;
}

int CObArray::Add(CObject* p)
{
    EnsureAlloc(m_nSize + 1);
    m_pData[m_nSize] = p;
    return m_nSize++;
}

void CObArray::SetAtGrow(int i, CObject* p)
{
    if (i >= m_nSize) SetSize(i + 1);
    m_pData[i] = p;
}

void CObArray::InsertAt(int i, CObject* p, int nCount)
{
    if (i > m_nSize) { SetAtGrow(i, p); return; }
    EnsureAlloc(m_nSize + nCount);
    memmove(m_pData + i + nCount, m_pData + i, (m_nSize - i) * sizeof(CObject*));
    for (int k = 0; k < nCount; ++k)
        m_pData[i + k] = p;
    m_nSize += nCount;
}

void CObArray::RemoveAt(int i, int nCount)
{
    ASSERT(i >= 0 && i + nCount <= m_nSize);
    memmove(m_pData + i, m_pData + i + nCount, (m_nSize - i - nCount) * sizeof(CObject*));
    m_nSize -= nCount;
}

void CWordArray::EnsureAlloc(int n)
{
    if (n <= m_nMaxSize) return;
    int nNew = m_nMaxSize ? m_nMaxSize : 16;
    while (nNew < n) nNew *= 2;
    m_pData = (WORD*)realloc(m_pData, nNew * sizeof(WORD));
    m_nMaxSize = nNew;
}

void CWordArray::SetSize(int nNewSize, int)
{
    EnsureAlloc(nNewSize);
    if (nNewSize > m_nSize)
        memset(m_pData + m_nSize, 0, (nNewSize - m_nSize) * sizeof(WORD));
    m_nSize = nNewSize;
}

int CWordArray::Add(WORD w)
{
    EnsureAlloc(m_nSize + 1);
    m_pData[m_nSize] = w;
    return m_nSize++;
}

void CWordArray::SetAtGrow(int i, WORD w)
{
    if (i >= m_nSize) SetSize(i + 1);
    m_pData[i] = w;
}

void CWordArray::InsertAt(int i, WORD w, int nCount)
{
    if (i > m_nSize) { SetAtGrow(i, w); return; }
    EnsureAlloc(m_nSize + nCount);
    memmove(m_pData + i + nCount, m_pData + i, (m_nSize - i) * sizeof(WORD));
    for (int k = 0; k < nCount; ++k)
        m_pData[i + k] = w;
    m_nSize += nCount;
}

void CWordArray::RemoveAt(int i, int nCount)
{
    ASSERT(i >= 0 && i + nCount <= m_nSize);
    memmove(m_pData + i, m_pData + i + nCount, (m_nSize - i - nCount) * sizeof(WORD));
    m_nSize -= nCount;
}

// ── rect helpers (extern "C" in windows.h) ───────────────────────────────────────────────────
extern "C" {

BOOL PtInRect(const RECT* lprc, POINT pt)
{
    return pt.x >= lprc->left && pt.x < lprc->right &&
           pt.y >= lprc->top  && pt.y < lprc->bottom;
}

BOOL SetRect(LPRECT lprc, int l, int t, int r, int b)
{
    lprc->left = l; lprc->top = t; lprc->right = r; lprc->bottom = b;
    return TRUE;
}

BOOL IsRectEmpty(const RECT* lprc)
{
    return lprc->right <= lprc->left || lprc->bottom <= lprc->top;
}

BOOL OffsetRect(LPRECT lprc, int dx, int dy)
{
    lprc->left += dx; lprc->right += dx; lprc->top += dy; lprc->bottom += dy;
    return TRUE;
}

BOOL IntersectRect(LPRECT dst, const RECT* a, const RECT* b)
{
    dst->left   = a->left   > b->left   ? a->left   : b->left;
    dst->top    = a->top    > b->top    ? a->top    : b->top;
    dst->right  = a->right  < b->right  ? a->right  : b->right;
    dst->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (IsRectEmpty(dst)) { SetRect(dst, 0, 0, 0, 0); return FALSE; }
    return TRUE;
}

void _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
    if (drive) drive[0] = 0;                      // no drive letters on POSIX hosts
    const char* p = path;
    if (p[0] && p[1] == ':') { if (drive) { drive[0] = p[0]; drive[1] = ':'; drive[2] = 0; } p += 2; }
    const char* slash = 0;
    for (const char* q = p; *q; ++q)
        if (*q == '/' || *q == '\\') slash = q;
    const char* base = slash ? slash + 1 : p;
    if (dir)
    {
        size_t n = slash ? (size_t)(slash + 1 - p) : 0;
        memcpy(dir, p, n);
        dir[n] = 0;
    }
    const char* dot = strrchr(base, '.');
    if (fname)
    {
        size_t n = dot ? (size_t)(dot - base) : strlen(base);
        memcpy(fname, base, n);
        fname[n] = 0;
    }
    if (ext)
        strcpy(ext, dot ? dot : "");
}

void _makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
    path[0] = 0;
    if (drive && drive[0]) strcat(path, drive);
    if (dir && dir[0]) strcat(path, dir);
    if (fname) strcat(path, fname);
    if (ext && ext[0])
    {
        if (ext[0] != '.') strcat(path, ".");
        strcat(path, ext);
    }
}

int wsprintfA(LPSTR buf, LPCSTR fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int n = vsprintf(buf, fmt, ap);
    va_end(ap);
    return n;
}

DWORD GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

void Sleep(DWORD ms)
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
}

DWORD GetLastError(void) { return 0; }

} // extern "C"

// ── afx globals (app object arrives with microfx/src/app at M2; hooks here) ─────────────────
CWinApp* g_pMfxApp = 0;
static AFX_MODULE_STATE g_mfxModuleState = { 0, 0 };

CWinApp* AfxGetApp() { return g_pMfxApp; }
CWnd*    AfxGetMainWnd() { return g_pMfxApp ? g_pMfxApp->m_pMainWnd : 0; }
HINSTANCE AfxGetInstanceHandle() { return g_mfxModuleState.m_hCurrentInstanceHandle; }
HINSTANCE AfxGetResourceHandle() { return g_mfxModuleState.m_hCurrentResourceHandle; }
AFX_MODULE_STATE* AfxGetModuleState() { return &g_mfxModuleState; }

int AfxMessageBox(LPCSTR lpszText, UINT nType, UINT)
{
    fprintf(stderr, "microfx: AfxMessageBox: %s\n", lpszText ? lpszText : "(null)");
    return (nType & MB_YESNO) ? IDYES : IDOK;
}

int AfxMessageBox(UINT nIDPrompt, UINT nType, UINT nIDHelp)
{
    CString s;
    s.LoadString(nIDPrompt);
    return AfxMessageBox((LPCSTR)s, nType, nIDHelp);
}

void AfxAbort()
{
    fprintf(stderr, "microfx: AfxAbort\n");
    abort();
}

// ── MSVC-4.2 CRT rand()/srand() (afxwin.h redirects the game TUs here) ───────────────────────
static unsigned long g_mfxHoldrand = 1;    // MSVC CRT initial state
extern "C" void mfx_srand(unsigned int nSeed) { g_mfxHoldrand = nSeed; }
extern "C" int mfx_rand(void)
{
    g_mfxHoldrand = g_mfxHoldrand * 214013UL + 2531011UL;
    return (int)((g_mfxHoldrand >> 16) & 0x7fff);
}

// ── CCmdTarget root message map ──────────────────────────────────────────────────────────────
const AFX_MSGMAP* CCmdTarget::GetMessageMap() const { return CCmdTarget::GetThisMessageMap(); }
const AFX_MSGMAP* CCmdTarget::GetThisMessageMap()
{
    static const AFX_MSGMAP_ENTRY _entries[] = { { 0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } };
    static const AFX_MSGMAP map = { 0, _entries };
    return &map;
}
BOOL CCmdTarget::OnCmdMsg(UINT, int, void*, void*) { return FALSE; }
void CCmdTarget::BeginWaitCursor() {}
void CCmdTarget::EndWaitCursor() {}
