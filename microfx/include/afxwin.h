// microfx — <afxwin.h> drop-in for the YODA_PORTABLE (SDL) build.        docs/phase-h4-sdl.md
//
// A source-compatible subset of MFC 4.2 — exactly the classes/methods/macros the game TUs use
// (measured inventory, v73) — so src/*.cpp compile UNMODIFIED. Semantics follow MFC where the
// game can observe them. Struct layout does NOT mirror MFC except where noted (CString is a
// single pointer, like real MFC — it is embedded in game structs at documented offsets).
//
// Message maps use the MODERN MFC shape (function-local statics + `typedef theClass ThisClass`)
// because it is standard C++; the user-facing macros are identical to MFC 4.2's. Note the
// `&ThisClass::memberFxn` qualification also accepts `ON_COMMAND(id, CWinApp::OnFileNew)`
// (base-qualified member names resolve via the injected-class-name).
#ifndef MICROFX_AFXWIN_H
#define MICROFX_AFXWIN_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

// ── diagnostics ──────────────────────────────────────────────────────────────────────────────
#ifdef NDEBUG
#define ASSERT(f)        ((void)0)
#define VERIFY(f)        ((void)(f))
#define ASSERT_VALID(p)  ((void)0)
#define TRACE(...)       ((void)0)
#define TRACE0(s)        ((void)0)
#define TRACE1(s,a)      ((void)0)
#define TRACE2(s,a,b)    ((void)0)
#define TRACE3(s,a,b,c)  ((void)0)
#else
#define ASSERT(f)        assert(f)
#define VERIFY(f)        ASSERT(f)
#define ASSERT_VALID(p)  ((void)0)
#define TRACE(...)       fprintf(stderr, __VA_ARGS__)
#define TRACE0(s)        fprintf(stderr, "%s", s)
#define TRACE1(s,a)      fprintf(stderr, s, a)
#define TRACE2(s,a,b)    fprintf(stderr, s, a, b)
#define TRACE3(s,a,b,c)  fprintf(stderr, s, a, b, c)
#endif
#define afx_msg
#define AFX_CDECL
#define AFX_DATA
#define DEBUG_NEW new

// diagnostic-only OOB trap for CObArray/CWordArray GetAt/SetAt (2026-07-11, engine-bugs.md #15
// hunt): prints the array/index/size and a backtrace to stderr BEFORE the ASSERT aborts, so a
// live crash pinpoints the exact call site instead of just "GetAt, line 324". Remove once #15's
// real trigger is confirmed and permanently guarded.
#if !defined(NDEBUG) && (defined(__APPLE__) || defined(__linux__))
#include <execinfo.h>
static inline void MfxArrayOOBTrap(int i, int n, const char *pszWhat)
{
    if (i >= 0 && i < n) return;
    void *frames[32];
    int nFrames = backtrace(frames, 32);
    fprintf(stderr, "MfxArrayOOBTrap: %s(i=%d) n=%d — OUT OF BOUNDS\n", pszWhat, i, n);
    backtrace_symbols_fd(frames, nFrames, 2);
    FILE *fLog = fopen("yoda_crash.log", "a");
    if (fLog) {
        fprintf(fLog, "MfxArrayOOBTrap: %s(i=%d) n=%d — OUT OF BOUNDS\n", pszWhat, i, n);
        char **ppszSyms = backtrace_symbols(frames, nFrames);
        if (ppszSyms) { for (int k = 0; k < nFrames; k++) fprintf(fLog, "%s\n", ppszSyms[k]); free(ppszSyms); }
        fclose(fLog);
    }
}
#else
static inline void MfxArrayOOBTrap(int, int, const char *) {}
#endif

class CObject;
class CArchive;
class CFile;
class CDC;
class CWnd;
class CView;
class CFrameWnd;
class CDialog;
class CScrollBar;
class CDataExchange;
class CDocument;
class CWinApp;
class CException;
struct CCreateContext;

struct __POSITION;
typedef __POSITION* POSITION;
#define BEFORE_START_POSITION ((POSITION)-1)

// ── CRuntimeClass + dynamic-creation macros ─────────────────────────────────────────────────
struct CRuntimeClass
{
    LPCSTR         m_lpszClassName;
    int            m_nObjectSize;
    UINT           m_wSchema;
    CObject*     (*m_pfnCreateObject)();
    CRuntimeClass* m_pBaseClass;         // MFC 4.x: direct pointer

    CObject* CreateObject();
    BOOL IsDerivedFrom(const CRuntimeClass* pBaseClass) const;
};
#define RUNTIME_CLASS(class_name) ((CRuntimeClass*)(&class_name::class##class_name))

// classXxx is `const` to match real MFC (and the game's own `static const CRuntimeClass`
// forward-decls in Deskcpp.cpp): MSVC encodes const into the symbol name, so a const decl vs a
// non-const definition would not link (clang's Itanium mangling ignores it, hence it built there).
#define DECLARE_DYNAMIC(class_name) \
public: \
    static const CRuntimeClass class##class_name; \
    virtual CRuntimeClass* GetRuntimeClass() const;

#define DECLARE_DYNCREATE(class_name) \
    DECLARE_DYNAMIC(class_name) \
    static CObject* CreateObject();

#define IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, wSchema, pfnNew) \
    const CRuntimeClass class_name::class##class_name = { \
        #class_name, sizeof(class_name), wSchema, pfnNew, \
        RUNTIME_CLASS(base_class_name) }; \
    CRuntimeClass* class_name::GetRuntimeClass() const \
        { return RUNTIME_CLASS(class_name); }

#define IMPLEMENT_DYNAMIC(class_name, base_class_name) \
    IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, 0xFFFF, 0)

#define IMPLEMENT_DYNCREATE(class_name, base_class_name) \
    CObject* class_name::CreateObject() { return new class_name; } \
    IMPLEMENT_RUNTIMECLASS(class_name, base_class_name, 0xFFFF, class_name::CreateObject)

// ── CObject ──────────────────────────────────────────────────────────────────────────────────
class CObject
{
public:
    CObject() {}
    virtual ~CObject() {}
    virtual CRuntimeClass* GetRuntimeClass() const;
    virtual void Serialize(CArchive& ar);
    virtual void AssertValid() const {}
    virtual void Dump() const {}
    BOOL IsKindOf(const CRuntimeClass* pClass) const;

    static CRuntimeClass classCObject;
private:
    CObject(const CObject&);
    void operator=(const CObject&);
};

// ── exceptions ───────────────────────────────────────────────────────────────────────────────
class CException : public CObject
{
public:
    CException(BOOL bAutoDelete = TRUE) : m_bAutoDelete(bAutoDelete) {}
    virtual ~CException() {}
    void Delete() { if (m_bAutoDelete) delete this; }
    virtual int ReportError(UINT nType = MB_OK, UINT nError = 0);
    BOOL m_bAutoDelete;
};

class CFileException : public CException
{
public:
    enum {
        none, generic, fileNotFound, badPath, tooManyOpenFiles, accessDenied,
        invalidFile, removeCurrentDir, directoryFull, badSeek, hardIO,
        sharingViolation, lockViolation, diskFull, endOfFile
    };
    CFileException(int cause = none, LONG lOsError = -1)
        : CException(FALSE), m_cause(cause), m_lOsError(lOsError) {}
    int  m_cause;
    LONG m_lOsError;
};

class CMemoryException : public CException {};

void AfxThrowFileException(int cause, LONG lOsError = -1);
void AfxThrowMemoryException();

// MFC exception-frame link: TRY creates a local `_afxExceptionLink`; CATCH records the caught
// exception in it. The game HAND-EXPANDS this pattern in places (Worldgen.cpp), so the names
// and fields must match MFC's exactly.
struct AFX_EXCEPTION_LINK
{
    CException* m_pException;
    AFX_EXCEPTION_LINK() : m_pException(0) {}
    ~AFX_EXCEPTION_LINK();               // deletes m_pException if still owned (MFC cleanup)
};

#define TRY                       { AFX_EXCEPTION_LINK _afxExceptionLink; try {
#define CATCH(class_name, e)      } catch (class_name* e) { _afxExceptionLink.m_pException = e;
#define AND_CATCH(class_name, e)  } catch (class_name* e) { _afxExceptionLink.m_pException = e;
#define CATCH_ALL(e)              } catch (CException* e) { _afxExceptionLink.m_pException = e;
#define AND_CATCH_ALL(e)          } catch (CException* e) { _afxExceptionLink.m_pException = e;
#define END_CATCH                 } }
#define END_CATCH_ALL             } }
// TRY{...}END_TRY with no CATCH: real MFC swallows the CException* (the link dtor deletes it)
#define END_TRY                   } catch (CException* _mfxSwallowed) \
                                      { _afxExceptionLink.m_pException = _mfxSwallowed; } }
#define THROW(e)                  throw e
#define THROW_LAST()              (_afxExceptionLink.m_pException = 0, throw)

// ── CString — real-MFC layout: ONE pointer, length/alloc live in a header before the chars ──
struct CStringData
{
    int nRefs;        // unused (no ref-counting) but keeps header shape MFC-like
    int nDataLength;
    int nAllocLength;
    char* data() { return (char*)(this + 1); }
};

class CString
{
public:
    CString() { Init(); }
    CString(const CString& s) { Init(); Assign((LPCSTR)s.m_pchData, s.GetLength()); }
    CString(LPCSTR psz) { Init(); if (psz) Assign(psz, (int)strlen(psz)); }
    CString(LPCSTR psz, int nLen) { Init(); Assign(psz, nLen); }
    CString(char ch, int nRepeat = 1);
    ~CString();

    int  GetLength() const { return GetData()->nDataLength; }
    BOOL IsEmpty() const { return GetLength() == 0; }
    void Empty();
    operator LPCSTR() const { return m_pchData; }
    char GetAt(int i) const { return m_pchData[i]; }
    char operator[](int i) const { return m_pchData[i]; }
    void SetAt(int i, char ch) { m_pchData[i] = ch; }

    const CString& operator=(const CString& s) { Assign(s.m_pchData, s.GetLength()); return *this; }
    const CString& operator=(LPCSTR psz) { Assign(psz, psz ? (int)strlen(psz) : 0); return *this; }
    const CString& operator=(char ch) { Assign(&ch, 1); return *this; }
    const CString& operator+=(const CString& s) { Concat(s.m_pchData, s.GetLength()); return *this; }
    const CString& operator+=(LPCSTR psz) { if (psz) Concat(psz, (int)strlen(psz)); return *this; }
    const CString& operator+=(char ch) { Concat(&ch, 1); return *this; }

    friend CString operator+(const CString& a, const CString& b);
    friend CString operator+(const CString& a, LPCSTR b);
    friend CString operator+(LPCSTR a, const CString& b);
    friend CString operator+(const CString& a, char b);
    friend CString operator+(char a, const CString& b);

    int Compare(LPCSTR psz) const { return strcmp(m_pchData, psz ? psz : ""); }
    int CompareNoCase(LPCSTR psz) const;

    CString Mid(int nFirst) const { return Mid(nFirst, GetLength() - nFirst); }
    CString Mid(int nFirst, int nCount) const;
    CString Left(int nCount) const;
    CString Right(int nCount) const;

    int  Find(char ch) const;
    int  Find(LPCSTR psz) const;
    int  FindOneOf(LPCSTR pszCharSet) const;
    int  ReverseFind(char ch) const;
    void MakeUpper();
    void MakeLower();
    void TrimLeft();
    void TrimRight();

    void Format(LPCSTR pszFormat, ...);
    BOOL LoadString(UINT nID);                   // resource strings — microfx/src/res (M4)

    LPSTR GetBuffer(int nMinBufLength);
    void  ReleaseBuffer(int nNewLength = -1);

private:
    char* m_pchData;                             // the ONLY member — sizeof(CString)==sizeof(char*)
    CStringData* GetData() const { return ((CStringData*)m_pchData) - 1; }
    void Init();
    void Assign(LPCSTR p, int n);
    void Concat(LPCSTR p, int n);
    void AllocBuffer(int nLen);
};

inline BOOL operator==(const CString& a, const CString& b) { return a.Compare(b) == 0; }
inline BOOL operator==(const CString& a, LPCSTR b) { return a.Compare(b) == 0; }
inline BOOL operator==(LPCSTR a, const CString& b) { return b.Compare(a) == 0; }
inline BOOL operator!=(const CString& a, const CString& b) { return a.Compare(b) != 0; }
inline BOOL operator!=(const CString& a, LPCSTR b) { return a.Compare(b) != 0; }
inline BOOL operator!=(LPCSTR a, const CString& b) { return b.Compare(a) != 0; }

// ── CFile (stdio-backed) ─────────────────────────────────────────────────────────────────────
class CFile : public CObject
{
public:
    enum OpenFlags {
        modeRead        = 0x0000,
        modeWrite       = 0x0001,
        modeReadWrite   = 0x0002,
        shareCompat     = 0x0000,
        shareExclusive  = 0x0010,
        shareDenyWrite  = 0x0020,
        shareDenyRead   = 0x0030,
        shareDenyNone   = 0x0040,
        modeNoInherit   = 0x0080,
        modeCreate      = 0x1000,
        modeNoTruncate  = 0x2000,
        typeText        = 0x4000,
        typeBinary      = 0x8000
    };
    enum SeekPosition { begin = 0, current = 1, end = 2 };

    CFile() : m_pStream(0) {}
    CFile(LPCSTR pszFileName, UINT nOpenFlags);      // throws CFileException* on failure
    virtual ~CFile();

    virtual BOOL  Open(LPCSTR pszFileName, UINT nOpenFlags, CFileException* pError = 0);
    virtual UINT  Read(void* lpBuf, UINT nCount);
    virtual void  Write(const void* lpBuf, UINT nCount);
    virtual LONG  Seek(LONG lOff, UINT nFrom);
    void          SeekToBegin() { Seek(0, begin); }
    DWORD         SeekToEnd() { return (DWORD)Seek(0, end); }
    virtual DWORD GetPosition() const;
    virtual DWORD GetLength() const;
    virtual void  Close();
    virtual void  Abort();

    CString m_strFileName;
protected:
    FILE* m_pStream;
};

// ── CArchive (a CFile carrier here — the game's Serialize does raw chunk I/O) ────────────────
class CArchive
{
public:
    enum Mode { store = 0, load = 1, bNoFlushOnDelete = 2, bNoByteSwap = 4 };
    CArchive(CFile* pFile, UINT nMode, int nBufSize = 4096, void* lpBuf = 0)
        : m_pFile(pFile), m_nMode(nMode), m_pDocument(0), m_bForceFlat(TRUE) {}
    ~CArchive() {}
    BOOL IsStoring() const { return (m_nMode & load) == 0; }
    BOOL IsLoading() const { return (m_nMode & load) != 0; }
    CFile* GetFile() const { return m_pFile; }
    void Close() {}
    UINT Read(void* lpBuf, UINT nMax) { return m_pFile->Read(lpBuf, nMax); }
    void Write(const void* lpBuf, UINT nMax) { m_pFile->Write(lpBuf, nMax); }

    CDocument* m_pDocument;
    BOOL       m_bForceFlat;
protected:
    CFile* m_pFile;
    UINT   m_nMode;
};

// ── collections ──────────────────────────────────────────────────────────────────────────────
class CObArray : public CObject
{
public:
    CObArray() : m_pData(0), m_nSize(0), m_nMaxSize(0) {}
    ~CObArray() { free(m_pData); }
    int  GetSize() const { return m_nSize; }
    int  GetUpperBound() const { return m_nSize - 1; }
    void SetSize(int nNewSize, int nGrowBy = -1);
    CObject* GetAt(int i) const { MfxArrayOOBTrap(i, m_nSize, "CObArray::GetAt"); ASSERT(i >= 0 && i < m_nSize); return m_pData[i]; }
    void SetAt(int i, CObject* p) { MfxArrayOOBTrap(i, m_nSize, "CObArray::SetAt"); ASSERT(i >= 0 && i < m_nSize); m_pData[i] = p; }
    CObject*& ElementAt(int i) { return m_pData[i]; }
    CObject* operator[](int i) const { return GetAt(i); }
    CObject*& operator[](int i) { return ElementAt(i); }
    int  Add(CObject* p);
    void SetAtGrow(int i, CObject* p);
    void InsertAt(int i, CObject* p, int nCount = 1);
    void RemoveAt(int i, int nCount = 1);
    void RemoveAll() { m_nSize = 0; }
    CObject** GetData() { return m_pData; }
    const CObject** GetData() const { return (const CObject**)m_pData; }
protected:
    CObject** m_pData;
    int m_nSize, m_nMaxSize;
    void EnsureAlloc(int n);
};

class CWordArray : public CObject
{
public:
    CWordArray() : m_pData(0), m_nSize(0), m_nMaxSize(0) {}
    ~CWordArray() { free(m_pData); }
    int  GetSize() const { return m_nSize; }
    int  GetUpperBound() const { return m_nSize - 1; }
    void SetSize(int nNewSize, int nGrowBy = -1);
    WORD GetAt(int i) const { MfxArrayOOBTrap(i, m_nSize, "CWordArray::GetAt"); ASSERT(i >= 0 && i < m_nSize); return m_pData[i]; }
    void SetAt(int i, WORD w) { MfxArrayOOBTrap(i, m_nSize, "CWordArray::SetAt"); ASSERT(i >= 0 && i < m_nSize); m_pData[i] = w; }
    WORD& ElementAt(int i) { return m_pData[i]; }
    WORD operator[](int i) const { return GetAt(i); }
    WORD& operator[](int i) { return ElementAt(i); }
    int  Add(WORD w);
    void SetAtGrow(int i, WORD w);
    void InsertAt(int i, WORD w, int nCount = 1);
    void RemoveAt(int i, int nCount = 1);
    void RemoveAll() { m_nSize = 0; }
    WORD* GetData() { return m_pData; }
protected:
    WORD* m_pData;
    int m_nSize, m_nMaxSize;
    void EnsureAlloc(int n);
};

// ── geometry wrappers ────────────────────────────────────────────────────────────────────────
class CPoint : public tagPOINT
{
public:
    CPoint() { x = y = 0; }
    CPoint(int ix, int iy) { x = ix; y = iy; }
    CPoint(POINT pt) { x = pt.x; y = pt.y; }
    CPoint(DWORD dw) { x = (SHORT)LOWORD(dw); y = (SHORT)HIWORD(dw); }
    void Offset(int dx, int dy) { x += dx; y += dy; }
    BOOL operator==(POINT pt) const { return x == pt.x && y == pt.y; }
    BOOL operator!=(POINT pt) const { return !(x == pt.x && y == pt.y); }
};

class CSize : public tagSIZE
{
public:
    CSize() { cx = cy = 0; }
    CSize(int icx, int icy) { cx = icx; cy = icy; }
};

class CRect : public tagRECT
{
public:
    CRect() { left = top = right = bottom = 0; }
    CRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& rc) { *(RECT*)this = rc; }
    CRect(POINT topLeft, POINT bottomRight)
        { left = topLeft.x; top = topLeft.y; right = bottomRight.x; bottom = bottomRight.y; }
    int  Width() const { return right - left; }
    int  Height() const { return bottom - top; }
    BOOL PtInRect(POINT pt) const { return ::PtInRect(this, pt); }
    void SetRect(int l, int t, int r, int b) { ::SetRect(this, l, t, r, b); }
    void SetRectEmpty() { left = top = right = bottom = 0; }
    BOOL IsRectEmpty() const { return ::IsRectEmpty(this); }
    void OffsetRect(int dx, int dy) { ::OffsetRect(this, dx, dy); }
    void InflateRect(int dx, int dy) { left -= dx; top -= dy; right += dx; bottom += dy; }
    CPoint TopLeft() const { return CPoint(left, top); }
    CPoint BottomRight() const { return CPoint(right, bottom); }
    operator LPRECT() { return this; }
    operator LPCRECT() const { return this; }
};

// ── GDI object wrappers ──────────────────────────────────────────────────────────────────────
class CGdiObject : public CObject
{
public:
    CGdiObject() : m_hObject(0) {}
    virtual ~CGdiObject();
    BOOL Attach(HGDIOBJ h) { m_hObject = h; return h != 0; }
    HGDIOBJ Detach() { HGDIOBJ h = m_hObject; m_hObject = 0; return h; }
    BOOL DeleteObject();
    HGDIOBJ GetSafeHandle() const;
    HGDIOBJ m_hObject;
};

class CBitmap : public CGdiObject
{
public:
    static CBitmap* FromHandle(HBITMAP h);
    BOOL LoadBitmap(UINT nIDResource);
    LONG SetBitmapBits(DWORD dwCount, const void* lpBits)
        { return ::SetBitmapBits((HBITMAP)m_hObject, dwCount, lpBits); }
    int  GetObject(int nCount, LPVOID lpObject) const
        { return ::GetObjectA(m_hObject, nCount, lpObject); }
    operator HBITMAP() const { return (HBITMAP)m_hObject; }
};

class CPalette : public CGdiObject
{
public:
    static CPalette* FromHandle(HPALETTE h);
    BOOL CreatePalette(LPLOGPALETTE lpLogPalette)
        { return Attach((HGDIOBJ)::CreatePalette(lpLogPalette)); }
    UINT GetPaletteEntries(UINT iStart, UINT nEntries, LPPALETTEENTRY lpPaletteColors) const
        { return ::GetPaletteEntries((HPALETTE)m_hObject, iStart, nEntries, lpPaletteColors); }
    UINT SetPaletteEntries(UINT iStart, UINT nEntries, LPPALETTEENTRY lpPaletteColors)
        { return ::SetPaletteEntries((HPALETTE)m_hObject, iStart, nEntries, lpPaletteColors); }
    void AnimatePalette(UINT iStart, UINT nEntries, LPPALETTEENTRY lpPaletteColors)
        { ::AnimatePalette((HPALETTE)m_hObject, iStart, nEntries, lpPaletteColors); }
    operator HPALETTE() const { return (HPALETTE)m_hObject; }
};

class CBrush : public CGdiObject
{
public:
    CBrush() {}
    CBrush(COLORREF cr) { CreateSolidBrush(cr); }
    BOOL CreateSolidBrush(COLORREF cr) { return Attach((HGDIOBJ)::CreateSolidBrush(cr)); }
    operator HBRUSH() const { return (HBRUSH)m_hObject; }
};

class CPen : public CGdiObject
{
public:
    CPen() {}
    CPen(int nPenStyle, int nWidth, COLORREF cr) { CreatePen(nPenStyle, nWidth, cr); }
    BOOL CreatePen(int nPenStyle, int nWidth, COLORREF cr)
        { return Attach((HGDIOBJ)::CreatePen(nPenStyle, nWidth, cr)); }
    operator HPEN() const { return (HPEN)m_hObject; }
};

class CFont : public CGdiObject
{
public:
    static CFont* FromHandle(HFONT h);
    BOOL CreateFontIndirect(const LOGFONT* lpLogFont);
    operator HFONT() const { return (HFONT)m_hObject; }
};

// ── CDC ──────────────────────────────────────────────────────────────────────────────────────
class CDC : public CObject
{
public:
    CDC() : m_hDC(0) {}
    virtual ~CDC();
    static CDC* FromHandle(HDC hDC);
    BOOL Attach(HDC hDC) { m_hDC = hDC; return hDC != 0; }
    HDC  Detach() { HDC h = m_hDC; m_hDC = 0; return h; }
    HDC  GetSafeHdc() const;
    operator HDC() const { return m_hDC; }

    BOOL CreateCompatibleDC(CDC* pDC)
        { return Attach(::CreateCompatibleDC(pDC ? pDC->m_hDC : 0)); }
    BOOL DeleteDC();

    CGdiObject* SelectObject(CGdiObject* pObject);
    CPen*     SelectObject(CPen* p)    { return (CPen*)SelectObject((CGdiObject*)p); }
    CBrush*   SelectObject(CBrush* p)  { return (CBrush*)SelectObject((CGdiObject*)p); }
    CFont*    SelectObject(CFont* p)   { return (CFont*)SelectObject((CGdiObject*)p); }
    CBitmap*  SelectObject(CBitmap* p) { return (CBitmap*)SelectObject((CGdiObject*)p); }
    CPalette* SelectPalette(CPalette* pPalette, BOOL bForceBackground);
    UINT      RealizePalette() { return ::RealizePalette(m_hDC); }

    BOOL BitBlt(int x, int y, int nWidth, int nHeight, CDC* pSrcDC, int xSrc, int ySrc, DWORD dwRop)
        { return ::BitBlt(m_hDC, x, y, nWidth, nHeight, pSrcDC ? pSrcDC->m_hDC : 0, xSrc, ySrc, dwRop); }
    int  GetDeviceCaps(int nIndex) const { return ::GetDeviceCaps(m_hDC, nIndex); }
    int  GetClipBox(LPRECT lpRect) const { return ::GetClipBox(m_hDC, lpRect); }
    CGdiObject* SelectStockObject(int nIndex);

    COLORREF SetTextColor(COLORREF cr) { return ::SetTextColor(m_hDC, cr); }
    COLORREF SetBkColor(COLORREF cr) { return ::SetBkColor(m_hDC, cr); }
    int      SetBkMode(int nMode) { return ::SetBkMode(m_hDC, nMode); }
    BOOL     Rectangle(int x1, int y1, int x2, int y2) { return ::Rectangle(m_hDC, x1, y1, x2, y2); }
    BOOL     Pie(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
        { return ::Pie(m_hDC, x1, y1, x2, y2, x3, y3, x4, y4); }
    CPoint   MoveTo(int x, int y) { POINT p = {0,0}; ::MoveToEx(m_hDC, x, y, &p); return CPoint(p); }
    BOOL     LineTo(int x, int y) { return ::LineTo(m_hDC, x, y); }
    BOOL     DrawIcon(int x, int y, HICON hIcon) { return ::DrawIcon(m_hDC, x, y, hIcon); }
    BOOL     FillRect(LPCRECT lpRect, CBrush* pBrush)
        { return ::FillRect(m_hDC, lpRect, pBrush ? (HBRUSH)pBrush->m_hObject : 0) != 0; }
    BOOL     TextOut(int x, int y, const CString& str)
        { return ::TextOutA(m_hDC, x, y, str, str.GetLength()); }
    BOOL     TextOut(int x, int y, LPCSTR lpsz, int nCount)
        { return ::TextOutA(m_hDC, x, y, lpsz, nCount); }

    HDC m_hDC;
};

class CPaintDC : public CDC
{
public:
    CPaintDC(CWnd* pWnd);
    virtual ~CPaintDC();
protected:
    CWnd* m_pWnd;
};

class CClientDC : public CDC
{
public:
    CClientDC(CWnd* pWnd);
    virtual ~CClientDC();
protected:
    CWnd* m_pWnd;
};

// ── message maps (modern-MFC shape: standard C++, identical user-facing macros) ─────────────
class CCmdTarget;
class CCmdUI;
typedef void (CCmdTarget::*AFX_PMSG)(void);

struct AFX_MSGMAP_ENTRY
{
    UINT     nMessage;
    UINT     nCode;      // control/notify code (CN_*)
    UINT     nID;        // command/control ID (0 for window messages)
    UINT     nLastID;
    UINT     nSig;       // AfxSig_* — how to cast pfn when dispatching
    AFX_PMSG pfn;
};
struct AFX_MSGMAP
{
    const AFX_MSGMAP* (*pfnGetBaseMap)();
    const AFX_MSGMAP_ENTRY* lpEntries;
};

#define CN_COMMAND            0
#define CN_UPDATE_COMMAND_UI  ((UINT)-1)
#define BN_CLICKED            0

enum AfxSig
{
    AfxSig_end = 0,
    AfxSig_vv,          // void ()
    AfxSig_vw,          // void (UINT)                    OnTimer
    AfxSig_vwl,         // void (UINT, LPARAM)            OnSysCommand
    AfxSig_vwww,        // void (UINT, UINT, UINT)        OnKeyDown/Up/OnChar
    AfxSig_vwp,         // void (UINT, CPoint)            mouse
    AfxSig_vwii,        // void (UINT, int, int)          OnSize
    AfxSig_vbw,         // void (BOOL, UINT)              OnShowWindow
    AfxSig_vwwx,        // void (UINT, UINT, CScrollBar*) OnHScroll/OnVScroll
    AfxSig_vwWb,        // void (UINT, CWnd*, BOOL)       OnActivate
    AfxSig_vW,          // void (CWnd*)                   OnPaletteChanged/IsChanging
    AfxSig_vM,          // void (MINMAXINFO*)             OnGetMinMaxInfo
    AfxSig_is,          // int  (LPCREATESTRUCT)          OnCreate
    AfxSig_bv,          // BOOL ()                        OnQueryNewPalette/OnQueryEndSession
    AfxSig_bD,          // BOOL (CDC*)                    OnEraseBkgnd
    AfxSig_bWww,        // BOOL (CWnd*, UINT, UINT)       OnSetCursor
    AfxSig_hDWw,        // HBRUSH (CDC*, CWnd*, UINT)     OnCtlColor
    AfxSig_cmdui        // void (CCmdUI*)                 ON_UPDATE_COMMAND_UI
};

#define DECLARE_MESSAGE_MAP() \
protected: \
    static const AFX_MSGMAP* GetThisMessageMap(); \
    virtual const AFX_MSGMAP* GetMessageMap() const;

#define BEGIN_MESSAGE_MAP(theClass, baseClass) \
    const AFX_MSGMAP* theClass::GetMessageMap() const \
        { return theClass::GetThisMessageMap(); } \
    const AFX_MSGMAP* theClass::GetThisMessageMap() \
    { \
        typedef theClass ThisClass; \
        typedef baseClass TheBaseClass; \
        static const AFX_MSGMAP_ENTRY _messageEntries[] = \
        {

#define END_MESSAGE_MAP() \
            { 0, 0, 0, 0, AfxSig_end, (AFX_PMSG)0 } \
        }; \
        static const AFX_MSGMAP messageMap = \
            { &TheBaseClass::GetThisMessageMap, &_messageEntries[0] }; \
        return &messageMap; \
    }

#define ON_COMMAND(id, memberFxn) \
    { WM_COMMAND, CN_COMMAND, (UINT)(id), (UINT)(id), AfxSig_vv, \
      (AFX_PMSG)(void (ThisClass::*)(void))&ThisClass::memberFxn },
#define ON_COMMAND_RANGE(idFirst, idLast, memberFxn) \
    { WM_COMMAND, CN_COMMAND, (UINT)(idFirst), (UINT)(idLast), AfxSig_vw, \
      (AFX_PMSG)(void (ThisClass::*)(UINT))&ThisClass::memberFxn },
#define ON_UPDATE_COMMAND_UI(id, memberFxn) \
    { WM_COMMAND, CN_UPDATE_COMMAND_UI, (UINT)(id), (UINT)(id), AfxSig_cmdui, \
      (AFX_PMSG)(void (ThisClass::*)(CCmdUI*))&ThisClass::memberFxn },
#define ON_BN_CLICKED(id, memberFxn) \
    { WM_COMMAND, BN_CLICKED, (UINT)(id), (UINT)(id), AfxSig_vv, \
      (AFX_PMSG)(void (ThisClass::*)(void))&ThisClass::memberFxn },

#define ON_WM_CREATE() \
    { WM_CREATE, 0, 0, 0, AfxSig_is, \
      (AFX_PMSG)(int (ThisClass::*)(LPCREATESTRUCT))&ThisClass::OnCreate },
#define ON_WM_DESTROY() \
    { WM_DESTROY, 0, 0, 0, AfxSig_vv, \
      (AFX_PMSG)(void (ThisClass::*)(void))&ThisClass::OnDestroy },
#define ON_WM_PAINT() \
    { WM_PAINT, 0, 0, 0, AfxSig_vv, \
      (AFX_PMSG)(void (ThisClass::*)(void))&ThisClass::OnPaint },
#define ON_WM_TIMER() \
    { WM_TIMER, 0, 0, 0, AfxSig_vw, \
      (AFX_PMSG)(void (ThisClass::*)(UINT))&ThisClass::OnTimer },
#define ON_WM_SIZE() \
    { WM_SIZE, 0, 0, 0, AfxSig_vwii, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, int, int))&ThisClass::OnSize },
#define ON_WM_SHOWWINDOW() \
    { WM_SHOWWINDOW, 0, 0, 0, AfxSig_vbw, \
      (AFX_PMSG)(void (ThisClass::*)(BOOL, UINT))&ThisClass::OnShowWindow },
#define ON_WM_KEYDOWN() \
    { WM_KEYDOWN, 0, 0, 0, AfxSig_vwww, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, UINT, UINT))&ThisClass::OnKeyDown },
#define ON_WM_KEYUP() \
    { WM_KEYUP, 0, 0, 0, AfxSig_vwww, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, UINT, UINT))&ThisClass::OnKeyUp },
#define ON_WM_CHAR() \
    { WM_CHAR, 0, 0, 0, AfxSig_vwww, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, UINT, UINT))&ThisClass::OnChar },
#define ON_WM_MOUSEMOVE() \
    { WM_MOUSEMOVE, 0, 0, 0, AfxSig_vwp, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, CPoint))&ThisClass::OnMouseMove },
#define ON_WM_LBUTTONDOWN() \
    { WM_LBUTTONDOWN, 0, 0, 0, AfxSig_vwp, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, CPoint))&ThisClass::OnLButtonDown },
#define ON_WM_LBUTTONUP() \
    { WM_LBUTTONUP, 0, 0, 0, AfxSig_vwp, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, CPoint))&ThisClass::OnLButtonUp },
#define ON_WM_RBUTTONDOWN() \
    { WM_RBUTTONDOWN, 0, 0, 0, AfxSig_vwp, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, CPoint))&ThisClass::OnRButtonDown },
#define ON_WM_HSCROLL() \
    { WM_HSCROLL, 0, 0, 0, AfxSig_vwwx, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, UINT, CScrollBar*))&ThisClass::OnHScroll },
#define ON_WM_VSCROLL() \
    { WM_VSCROLL, 0, 0, 0, AfxSig_vwwx, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, UINT, CScrollBar*))&ThisClass::OnVScroll },
#define ON_WM_ACTIVATE() \
    { WM_ACTIVATE, 0, 0, 0, AfxSig_vwWb, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, CWnd*, BOOL))&ThisClass::OnActivate },
#define ON_WM_SETCURSOR() \
    { WM_SETCURSOR, 0, 0, 0, AfxSig_bWww, \
      (AFX_PMSG)(BOOL (ThisClass::*)(CWnd*, UINT, UINT))&ThisClass::OnSetCursor },
#define ON_WM_CTLCOLOR() \
    { WM_CTLCOLOR, 0, 0, 0, AfxSig_hDWw, \
      (AFX_PMSG)(HBRUSH (ThisClass::*)(CDC*, CWnd*, UINT))&ThisClass::OnCtlColor },
#define ON_WM_ERASEBKGND() \
    { WM_ERASEBKGND, 0, 0, 0, AfxSig_bD, \
      (AFX_PMSG)(BOOL (ThisClass::*)(CDC*))&ThisClass::OnEraseBkgnd },
#define ON_WM_GETMINMAXINFO() \
    { WM_GETMINMAXINFO, 0, 0, 0, AfxSig_vM, \
      (AFX_PMSG)(void (ThisClass::*)(MINMAXINFO*))&ThisClass::OnGetMinMaxInfo },
#define ON_WM_QUERYNEWPALETTE() \
    { WM_QUERYNEWPALETTE, 0, 0, 0, AfxSig_bv, \
      (AFX_PMSG)(BOOL (ThisClass::*)(void))&ThisClass::OnQueryNewPalette },
#define ON_WM_PALETTECHANGED() \
    { WM_PALETTECHANGED, 0, 0, 0, AfxSig_vW, \
      (AFX_PMSG)(void (ThisClass::*)(CWnd*))&ThisClass::OnPaletteChanged },
#define ON_WM_PALETTEISCHANGING() \
    { WM_PALETTEISCHANGING, 0, 0, 0, AfxSig_vW, \
      (AFX_PMSG)(void (ThisClass::*)(CWnd*))&ThisClass::OnPaletteIsChanging },
#define ON_WM_SYSCOMMAND() \
    { WM_SYSCOMMAND, 0, 0, 0, AfxSig_vwl, \
      (AFX_PMSG)(void (ThisClass::*)(UINT, LPARAM))&ThisClass::OnSysCommand },
#define ON_WM_QUERYENDSESSION() \
    { WM_QUERYENDSESSION, 0, 0, 0, AfxSig_bv, \
      (AFX_PMSG)(BOOL (ThisClass::*)(void))&ThisClass::OnQueryEndSession },

// ── CCmdTarget / CCmdUI ──────────────────────────────────────────────────────────────────────
class CCmdTarget : public CObject
{
public:
    virtual const AFX_MSGMAP* GetMessageMap() const;
    static const AFX_MSGMAP* GetThisMessageMap();
    virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, void* pHandlerInfo);
    void BeginWaitCursor();
    void EndWaitCursor();
};

class CCmdUI
{
public:
    CCmdUI() : m_nID(0), m_nIndex(0), m_pMenu(0), m_pOther(0), m_bEnabled(TRUE), m_nCheck(0) {}
    virtual void Enable(BOOL bOn = TRUE) { m_bEnabled = bOn; }
    virtual void SetCheck(int nCheck = 1) { m_nCheck = nCheck; }
    virtual void SetText(LPCSTR lpszText) { m_strText = lpszText; }
    UINT  m_nID;
    UINT  m_nIndex;
    void* m_pMenu;
    CWnd* m_pOther;
    // microfx bookkeeping (read back by the menu/UI layer at M4)
    BOOL    m_bEnabled;
    int     m_nCheck;
    CString m_strText;
};

// ── CWnd ─────────────────────────────────────────────────────────────────────────────────────
class CWnd : public CCmdTarget
{
public:
    CWnd() : m_mfxRedraw(1), m_hWnd(0) {}
    virtual ~CWnd();

    HWND GetSafeHwnd() const;
    static CWnd* FromHandle(HWND hWnd);

    virtual BOOL Create(LPCSTR lpszClassName, LPCSTR lpszWindowName, DWORD dwStyle,
                        const RECT& rect, CWnd* pParentWnd, UINT nID,
                        CCreateContext* pContext = 0);
    virtual BOOL DestroyWindow();
    BOOL ShowWindow(int nCmdShow) { return ::ShowWindow(m_hWnd, nCmdShow); }
    BOOL EnableWindow(BOOL bEnable = TRUE) { return ::EnableWindow(m_hWnd, bEnable); }
    BOOL IsWindowVisible() const;
    BOOL IsWindowEnabled() const;
    void UpdateWindow() { ::UpdateWindow(m_hWnd); }
    void Invalidate(BOOL bErase = TRUE) { ::InvalidateRect(m_hWnd, 0, bErase); }
    void InvalidateRect(LPCRECT lpRect, BOOL bErase = TRUE) { ::InvalidateRect(m_hWnd, lpRect, bErase); }
    BOOL RedrawWindow(LPCRECT lpRectUpdate = 0, void* prgnUpdate = 0,
                      UINT flags = RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE)
        { return ::RedrawWindow(m_hWnd, lpRectUpdate, 0, flags); }
    CDC* GetDC();
    int  ReleaseDC(CDC* pDC);
    void GetClientRect(LPRECT lpRect) const { ::GetClientRect(m_hWnd, lpRect); }
    void GetWindowRect(LPRECT lpRect) const { ::GetWindowRect(m_hWnd, lpRect); }
    void MoveWindow(int x, int y, int nWidth, int nHeight, BOOL bRepaint = TRUE)
        { ::MoveWindow(m_hWnd, x, y, nWidth, nHeight, bRepaint); }
    void MoveWindow(LPCRECT lpRect, BOOL bRepaint = TRUE);
    BOOL SetWindowPos(const CWnd* pWndInsertAfter, int x, int y, int cx, int cy, UINT nFlags);
    void SetWindowText(LPCSTR lpszString) { ::SetWindowTextA(m_hWnd, lpszString); }
    int  GetWindowText(LPSTR lpszString, int nMaxCount) const;
    void GetWindowText(CString& rString) const;
    void CenterWindow(CWnd* pAlternateOwner = 0);
    CWnd* GetParent() const;
    CFrameWnd* GetParentFrame() const;
    CWnd* GetDlgItem(int nID) const;
    CWnd* SetFocus() { ::SetFocus(m_hWnd); return this; }
    CWnd* SetCapture() { ::SetCapture(m_hWnd); return this; }
    void  ScreenToClient(LPPOINT lpPoint) const { ::ScreenToClient(m_hWnd, lpPoint); }
    void  ScreenToClient(LPRECT lpRect) const;
    void  ClientToScreen(LPPOINT lpPoint) const { ::ClientToScreen(m_hWnd, lpPoint); }
    UINT SetTimer(UINT nIDEvent, UINT nElapse, void* lpfnTimer)
        { return ::SetTimer(m_hWnd, nIDEvent, nElapse, lpfnTimer); }
    BOOL KillTimer(int nIDEvent) { return ::KillTimer(m_hWnd, (UINT)nIDEvent); }
    LRESULT SendMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
        { return ::SendMessageA(m_hWnd, message, wParam, lParam); }
    BOOL PostMessage(UINT message, WPARAM wParam = 0, LPARAM lParam = 0)
        { return ::PostMessageA(m_hWnd, message, wParam, lParam); }
    int  MessageBox(LPCSTR lpszText, LPCSTR lpszCaption = 0, UINT nType = MB_OK);
    int  GetScrollPos(int nBar) const { return ::GetScrollPos(m_hWnd, nBar); }
    int  SetScrollPos(int nBar, int nPos, BOOL bRedraw = TRUE)
        { return ::SetScrollPos(m_hWnd, nBar, nPos, bRedraw); }
    void GetScrollRange(int nBar, LPINT lpMinPos, LPINT lpMaxPos) const
        { ::GetScrollRange(m_hWnd, nBar, lpMinPos, lpMaxPos); }
    void SetScrollRange(int nBar, int nMinPos, int nMaxPos, BOOL bRedraw = TRUE)
        { ::SetScrollRange(m_hWnd, nBar, nMinPos, nMaxPos, bRedraw); }
    void ShowScrollBar(UINT nBar, BOOL bShow = TRUE) { ::ShowScrollBar(m_hWnd, (int)nBar, bShow); }

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
    virtual void PostNcDestroy();
    LRESULT Default();

    // microfx-internal (M4): DefWindowProc-equivalent for messages no map entry handled —
    // control classes (CEdit/CButton/CScrollBar) implement their EM_*/BM_*/paint behavior
    // here. MfxCtlPaint draws the control into the shared screen DC (children paint AFTER
    // the view in MfxPaintIfDirty, Win32 z-order).
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void MfxCtlPaint();
    int m_mfxRedraw;               // WM_SETREDRAW state (controls; init 1)

    afx_msg int  OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg void OnTimer(UINT nIDEvent);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnChar(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
    afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg BOOL OnQueryNewPalette();
    afx_msg void OnPaletteChanged(CWnd* pFocusWnd);
    afx_msg void OnPaletteIsChanging(CWnd* pRealizeWnd);
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg BOOL OnQueryEndSession();

    HWND m_hWnd;
    DECLARE_MESSAGE_MAP()
};

// ── controls ─────────────────────────────────────────────────────────────────────────────────
// The bubble text control (wndDialogText): a word-wrapping multiline read-only EDIT. Only
// what TextDialog exercises — WM_SETTEXT/WM_SETFONT/EM_GETLINECOUNT/EM_LINESCROLL + painting
// visible lines into the screen DC (mfxctl.cpp).
class CEdit : public CWnd
{
public:
    CEdit() : m_mfxFont(0), m_mfxFirstLine(0), m_mfxLineCount(0) {}
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void MfxCtlPaint();

    CString m_mfxText;
    HFONT   m_mfxFont;
    int     m_mfxFirstLine;
    int     m_mfxLineCount;
    int     m_mfxLineStart[96];    // wrapped-line cache (bubble text is short)
    int     m_mfxLineLen[96];
    void    MfxRecalcLines();
};

class CScrollBar : public CWnd
{
public:
    CScrollBar() : m_mfxDragging(0), m_mfxDragOffset(0) {}
    BOOL Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    int  GetScrollPos() const { return ::GetScrollPos(m_hWnd, SB_CTL); }
    int  SetScrollPos(int nPos, BOOL bRedraw = TRUE) { return ::SetScrollPos(m_hWnd, SB_CTL, nPos, bRedraw); }
    void GetScrollRange(LPINT lpMinPos, LPINT lpMaxPos) const { ::GetScrollRange(m_hWnd, SB_CTL, lpMinPos, lpMaxPos); }
    void SetScrollRange(int nMinPos, int nMaxPos, BOOL bRedraw = TRUE) { ::SetScrollRange(m_hWnd, SB_CTL, nMinPos, nMaxPos, bRedraw); }
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void MfxCtlPaint();
    int m_mfxDragging;             // thumb drag in progress
    int m_mfxDragOffset;           // grab offset within the thumb
};

class CButton : public CWnd
{
public:
    CButton() : m_mfxPushed(0) {}
    BOOL Create(LPCSTR lpszCaption, DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID);
    void SetCheck(int nCheck);
    int  GetCheck() const;
    virtual LRESULT MfxCtlProc(UINT message, WPARAM wParam, LPARAM lParam);
    int m_mfxPushed;               // BM_GETSTATE bit 2 (BST_PUSHED) — dialog auto-repeat
};

class CBitmapButton : public CButton
{
public:
    BOOL LoadBitmaps(LPCSTR lpszBitmapResource, LPCSTR lpszBitmapResourceSel = 0,
                     LPCSTR lpszBitmapResourceFocus = 0, LPCSTR lpszBitmapResourceDisabled = 0);
    BOOL AutoLoad(UINT nID, CWnd* pParent);
    void SizeToContent();
    virtual void MfxCtlPaint();
    CBitmap m_bitmap, m_bitmapSel, m_bitmapFocus, m_bitmapDisabled;
};

// ── frame / doc / view / dialogs ─────────────────────────────────────────────────────────────
class CDocTemplate;

struct CCreateContext
{
    CRuntimeClass* m_pNewViewClass;
    CDocument*     m_pCurrentDoc;
    CDocTemplate*  m_pNewDocTemplate;
    CView*         m_pLastView;
    CFrameWnd*     m_pCurrentFrame;
    CCreateContext() { m_pNewViewClass = 0; m_pCurrentDoc = 0; m_pNewDocTemplate = 0; m_pLastView = 0; m_pCurrentFrame = 0; }
};

class CDocument : public CCmdTarget
{
    DECLARE_DYNAMIC(CDocument)
public:
    CDocument();
    virtual ~CDocument();

    virtual BOOL OnNewDocument();
    virtual BOOL OnOpenDocument(LPCSTR lpszPathName);
    virtual BOOL OnSaveDocument(LPCSTR lpszPathName);
    virtual void OnCloseDocument();
    virtual void DeleteContents();
    virtual void Serialize(CArchive& ar);
    virtual void ReportSaveLoadException(LPCSTR lpszPathName, CException* e,
                                         BOOL bSaving, UINT nIDPDefault);
    void SetModifiedFlag(BOOL bModified = TRUE) { m_bModified = bModified; }
    BOOL IsModified() const { return m_bModified; }
    const CString& GetPathName() const { return m_strPathName; }
    void SetPathName(LPCSTR lpszPathName, BOOL bAddToMRU = TRUE) { m_strPathName = lpszPathName; }
    const CString& GetTitle() const { return m_strTitle; }
    void SetTitle(LPCSTR lpszTitle) { m_strTitle = lpszTitle; }
    POSITION GetFirstViewPosition() const;
    CView* GetNextView(POSITION& rPosition) const;
    void UpdateAllViews(CView* pSender, LPARAM lHint = 0, CObject* pHint = 0);
    void AddView(CView* pView);

    CString m_strPathName, m_strTitle;
    BOOL    m_bModified;
    CView*  m_pOnlyView;              // SDI: the single view
    DECLARE_MESSAGE_MAP()
};

class CView : public CWnd
{
    DECLARE_DYNAMIC(CView)
public:
    CView();
    virtual ~CView();
    CDocument* GetDocument() const { return m_pDocument; }
    virtual void OnDraw(CDC* pDC) = 0;
    virtual void OnInitialUpdate();
    virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
    virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView);
    afx_msg void OnPaint();            // real MFC: CPaintDC + OnDraw (map entry on CView's map)

    CDocument* m_pDocument;
    DECLARE_MESSAGE_MAP()
};

class CFrameWnd : public CWnd
{
    DECLARE_DYNCREATE(CFrameWnd)
public:
    CFrameWnd();
    virtual ~CFrameWnd();
    static AFX_DATA const RECT rectDefault;
    virtual BOOL LoadFrame(UINT nIDResource, DWORD dwDefaultStyle = 0,
                           CWnd* pParentWnd = 0, CCreateContext* pContext = 0);
    virtual BOOL OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext);
    afx_msg int OnCreate(LPCREATESTRUCT lpcs);   // real MFC: chains to OnCreateClient (the game's
                                                 // CMainFrame::OnCreate calls this explicitly)
    afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);  // real MFC: notifies the
                                                 // active view (OnActivateView) — CMainFrame::OnActivate
                                                 // chains here, which is how CDeskcppView::bViewActive is set
    CView* GetActiveView() const { return m_pViewActive; }
    void SetActiveView(CView* pViewNew, BOOL bNotify = TRUE) { m_pViewActive = pViewNew; }
    CDocument* GetActiveDocument();
    void RecalcLayout(BOOL bNotify = TRUE) {}

    CView* m_pViewActive;
    BOOL   m_bAutoMenuEnable;
    DECLARE_MESSAGE_MAP()
};

class CDialog : public CWnd
{
public:
    CDialog();
    explicit CDialog(UINT nIDTemplate, CWnd* pParentWnd = 0);
    explicit CDialog(LPCSTR lpszTemplateName, CWnd* pParentWnd = 0);
    virtual ~CDialog();
    virtual int  DoModal();
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
    virtual void DoDataExchange(CDataExchange* pDX);
    BOOL UpdateData(BOOL bSaveAndValidate = TRUE);
    void EndDialog(int nResult);

    UINT  m_nIDTemplate;
    CWnd* m_pParentWnd;
    int   m_nModalResult;
    DECLARE_MESSAGE_MAP()
};

class CDataExchange
{
public:
    CDataExchange(CDialog* pDlg, BOOL bSaveAndValidate)
        : m_pDlgWnd(pDlg), m_bSaveAndValidate(bSaveAndValidate) {}
    BOOL     m_bSaveAndValidate;
    CDialog* m_pDlgWnd;
};
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, CString& value);
void AFXAPI DDX_Text(CDataExchange* pDX, int nIDC, int& value);
void AFXAPI DDX_Control(CDataExchange* pDX, int nIDC, CWnd& rControl);
void AFXAPI DDX_Check(CDataExchange* pDX, int nIDC, int& value);

// common dialogs (real MFC: afxdlgs.h; folded in here — the game gets it via this chain)
class CFileDialog : public CDialog
{
public:
    CFileDialog(BOOL bOpenFileDialog, LPCSTR lpszDefExt = 0, LPCSTR lpszFileName = 0,
                DWORD dwFlags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT,
                LPCSTR lpszFilter = 0, CWnd* pParentWnd = 0);
    virtual int DoModal();
    CString GetPathName() const;
    OPENFILENAME m_ofn;
    BOOL    m_bOpenFileDialog;
    CString m_strPath;
};

// ── threads / app ────────────────────────────────────────────────────────────────────────────
typedef UINT (AFX_CDECL* AFX_THREADPROC)(LPVOID);

class CWinThread : public CCmdTarget
{
public:
    CWinThread();
    virtual ~CWinThread();
    virtual BOOL InitInstance() { return FALSE; }
    virtual int  ExitInstance();
    virtual int  Run();
    virtual BOOL OnIdle(LONG lCount);
    CWnd*  m_pMainWnd;
    BOOL   m_bAutoDelete;
    HANDLE m_hThread;
};

CWinThread* AfxBeginThread(AFX_THREADPROC pfnThreadProc, LPVOID pParam,
                           int nPriority = 0, UINT nStackSize = 0,
                           DWORD dwCreateFlags = 0, void* lpSecurityAttrs = 0);

class CDocTemplate : public CCmdTarget
{
public:
    CDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
                 CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass);
    UINT m_nIDResource;
    CRuntimeClass *m_pDocClass, *m_pFrameClass, *m_pViewClass;
    CDocument* m_pDoc;                 // SDI: the one open document (set by CWinApp::OnFileNew)
};

class CSingleDocTemplate : public CDocTemplate
{
public:
    CSingleDocTemplate(UINT nIDResource, CRuntimeClass* pDocClass,
                       CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass)
        : CDocTemplate(nIDResource, pDocClass, pFrameClass, pViewClass) {}
};

class CWinApp : public CWinThread
{
public:
    CWinApp(LPCSTR lpszAppName = 0);
    virtual ~CWinApp();
    virtual BOOL InitInstance();
    virtual int  ExitInstance();
    virtual BOOL OnIdle(LONG lCount);
    void AddDocTemplate(CDocTemplate* pTemplate);
    void EnableShellOpen() {}
    void RegisterShellFileTypes(BOOL bCompat = FALSE) {}
    void LoadStdProfileSettings(UINT nMaxMRU = 4) {}
    void SetDialogBkColor(COLORREF clrCtlBk = RGB(192,192,192), COLORREF clrCtlText = RGB(0,0,0)) {}
    void DoWaitCursor(int nCode) {}      // 1=wait, -1=restore, 0=refresh (visual only)
    HCURSOR LoadCursor(UINT nIDResource) const;
    HCURSOR LoadCursor(LPCSTR lpszResourceName) const;
    HCURSOR LoadStandardCursor(LPCSTR lpszCursorName) const;
    HICON   LoadIcon(UINT nIDResource) const;
    UINT    GetProfileInt(LPCSTR lpszSection, LPCSTR lpszEntry, int nDefault);
    BOOL    WriteProfileInt(LPCSTR lpszSection, LPCSTR lpszEntry, int nValue);
    CString GetProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszDefault = 0);
    BOOL    WriteProfileString(LPCSTR lpszSection, LPCSTR lpszEntry, LPCSTR lpszValue);

    // AppWizard message-map targets (the app's map routes ON_COMMAND entries here)
    afx_msg void OnFileNew();
    afx_msg void OnFileOpen();
    afx_msg void OnAppExit();
    afx_msg void OnHelp();
    afx_msg void OnHelpIndex();
    afx_msg void OnHelpUsing();
    afx_msg void OnContextHelp();

    LPCSTR    m_pszAppName;
    LPCSTR    m_pszProfileName;
    LPCSTR    m_pszExeName;
    HINSTANCE m_hInstance;
    HINSTANCE m_hPrevInstance;
    LPSTR     m_lpCmdLine;
    int       m_nCmdShow;
    CDocTemplate* m_pDocTemplate;      // SDI: the one template
    DECLARE_MESSAGE_MAP()
};

// ── afx globals ──────────────────────────────────────────────────────────────────────────────
CWinApp* AfxGetApp();
CWnd*    AfxGetMainWnd();
HINSTANCE AfxGetInstanceHandle();
HINSTANCE AfxGetResourceHandle();
int  AfxMessageBox(LPCSTR lpszText, UINT nType = MB_OK, UINT nIDHelp = 0);
int  AfxMessageBox(UINT nIDPrompt, UINT nType = MB_OK, UINT nIDHelp = 0);
void AfxAbort();

struct AFX_MODULE_STATE
{
    HINSTANCE m_hCurrentInstanceHandle;
    HINSTANCE m_hCurrentResourceHandle;
};
AFX_MODULE_STATE* AfxGetModuleState();

// standard MFC command/string IDs (afxres.h values referenced by the game's maps/LoadString)
#define ID_FILE_NEW       0xE100
#define ID_FILE_OPEN      0xE101
#define ID_FILE_CLOSE     0xE102
#define ID_FILE_SAVE      0xE103
#define ID_FILE_SAVE_AS   0xE104
#define ID_EDIT_COPY      0xE122
#define ID_EDIT_CUT       0xE123
#define ID_EDIT_PASTE     0xE125
#define ID_EDIT_UNDO      0xE12B
#define ID_WINDOW_NEW     0xE130
#define ID_APP_ABOUT      0xE140
#define ID_APP_EXIT       0xE141
#define ID_HELP_INDEX     0xE142
#define ID_HELP_USING     0xE144
#define ID_CONTEXT_HELP   0xE145
#define ID_HELP           0xE146
#define ID_DEFAULT_HELP   0xE147
#define AFX_IDS_APP_TITLE 0xE000
#define AFX_IDP_FAILED_TO_OPEN_DOC 0xF005
#define AFX_IDW_PANE_FIRST 0xE900

// ── MSVC-4.2-compatible rand()/srand() ───────────────────────────────────────────────────────
// Worldgen seeds srand(worldSeed) and consumes rand() heavily; the same seed must produce the
// same world as the /MT-static-CRT Win32 build (the M0 log-diff oracle), so the game TUs'
// rand/srand are redirected to the exact MSVC CRT LCG. Declared before the defines so a later
// <cstdlib> `using ::rand;` still resolves. Every game TU includes this header, so all call
// sites are covered.
extern "C" int  mfx_rand(void);
extern "C" void mfx_srand(unsigned int nSeed);
#define rand  mfx_rand
#define srand mfx_srand

// ── Win32-CRT-compatible clock() ─────────────────────────────────────────────────────────────
// MSVC's clock() is WALL time with CLOCKS_PER_SEC==1000; the game busy-waits on it for pacing
// (ScrollZoneTransition `clock()+50` = 50ms/step, Iact WaitTicks, palette flashes). Host
// clock() is CPU time at 1e6/sec — those waits would spin ~1000x too fast. Remap to monotonic
// milliseconds. <time.h> is pulled first so the host declaration predates the macro (a later
// re-include is guarded away and can't be rewritten into a conflicting mfx_clock declaration).
#include <time.h>
extern "C" clock_t mfx_clock(void);
#define clock mfx_clock

#endif // MICROFX_AFXWIN_H
