// microfx — <windows.h> drop-in for the YODA_PORTABLE (SDL) build.        docs/phase-h4-sdl.md
//
// Win32 types + the GDI/USER/kernel subset the game TUs call (measured inventory, v73),
// implemented over SDL2 in microfx/src/. Only what the game uses: a missing declaration here is
// the COMPLETENESS ORACLE (compile error), not an oversight to paper over with a full SDK.
#ifndef MICROFX_WINDOWS_H
#define MICROFX_WINDOWS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ── calling-convention / annotation noise → nothing ─────────────────────────────────────────
#define WINAPI
#define CALLBACK
#define APIENTRY
#define AFXAPI
#define PASCAL
#define FAR
#define NEAR
#ifndef _MSC_VER
#define __stdcall
#define __cdecl
#define __fastcall
#endif

// ── scalar types ─────────────────────────────────────────────────────────────────────────────
typedef int            BOOL;
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   DWORD;     // 32-bit on all our hosts
typedef unsigned int   UINT;
typedef int            INT;
typedef int            LONG;      // Win32 LONG is 32-bit; host long may be 64 — use int
typedef unsigned int   ULONG;
typedef short          SHORT;
typedef unsigned short USHORT;
typedef char           CHAR;
typedef unsigned char  UCHAR;
typedef float          FLOAT;
typedef void*          LPVOID;
typedef const void*    LPCVOID;
typedef char*          LPSTR;
typedef const char*    LPCSTR;
typedef LPCSTR         LPCTSTR;   // MBCS build: TCHAR == char
typedef LPSTR          LPTSTR;
typedef char           TCHAR;
typedef BYTE*          LPBYTE;
typedef DWORD*         LPDWORD;
typedef WORD*          LPWORD;
typedef int*           LPINT;
typedef LONG*          LPLONG;
typedef intptr_t       LONG_PTR;
typedef uintptr_t      ULONG_PTR;
typedef ULONG_PTR      WPARAM;
typedef LONG_PTR       LPARAM;
typedef LONG_PTR       LRESULT;
typedef LONG           HRESULT;
typedef DWORD          COLORREF;

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif
#ifndef NULL
#define NULL  0
#endif
#define VOID void

// ── handles: distinct opaque struct pointers (backed by microfx/src structs) ────────────────
#define MFX_DECLARE_HANDLE(name) struct name##__; typedef struct name##__* name
MFX_DECLARE_HANDLE(HWND);
MFX_DECLARE_HANDLE(HDC);
MFX_DECLARE_HANDLE(HMENU);
MFX_DECLARE_HANDLE(HINSTANCE);
typedef HINSTANCE HMODULE;
typedef void* HGDIOBJ;            // SelectObject-style generic GDI handle
MFX_DECLARE_HANDLE(HBITMAP);
MFX_DECLARE_HANDLE(HPALETTE);
MFX_DECLARE_HANDLE(HBRUSH);
MFX_DECLARE_HANDLE(HPEN);
MFX_DECLARE_HANDLE(HFONT);
MFX_DECLARE_HANDLE(HRGN);
MFX_DECLARE_HANDLE(HCURSOR);
MFX_DECLARE_HANDLE(HKEY);
typedef HCURSOR HICON;            // interchangeable for our uses
typedef void*  HANDLE;
typedef HANDLE HLOCAL;
typedef HANDLE HGLOBAL;

// ── geometry / message structs ───────────────────────────────────────────────────────────────
typedef struct tagPOINT { LONG x, y; } POINT, *PPOINT, *LPPOINT;
typedef struct tagSIZE  { LONG cx, cy; } SIZE, *LPSIZE;
typedef struct tagRECT  { LONG left, top, right, bottom; } RECT, *PRECT, *LPRECT;
typedef const RECT* LPCRECT;
typedef struct tagMSG {
    HWND   hwnd;
    UINT   message;
    WPARAM wParam;
    LPARAM lParam;
    DWORD  time;
    POINT  pt;
} MSG, *LPMSG;
typedef struct tagCREATESTRUCT {
    LPVOID    lpCreateParams;
    HINSTANCE hInstance;
    HMENU     hMenu;
    HWND      hwndParent;
    int       cy, cx, y, x;
    LONG      style;
    LPCSTR    lpszName;
    LPCSTR    lpszClass;
    DWORD     dwExStyle;
} CREATESTRUCT, *LPCREATESTRUCT;
typedef struct tagMINMAXINFO {
    POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
} MINMAXINFO, *LPMINMAXINFO;

// ── GDI structs (8-bit DIB path is the one the game uses) ────────────────────────────────────
typedef struct tagRGBQUAD      { BYTE rgbBlue, rgbGreen, rgbRed, rgbReserved; } RGBQUAD;
typedef struct tagPALETTEENTRY { BYTE peRed, peGreen, peBlue, peFlags; } PALETTEENTRY, *LPPALETTEENTRY;
typedef struct tagLOGPALETTE {
    WORD         palVersion;
    WORD         palNumEntries;
    PALETTEENTRY palPalEntry[1];
} LOGPALETTE, *LPLOGPALETTE;
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG  biWidth, biHeight;
    WORD  biPlanes, biBitCount;
    DWORD biCompression, biSizeImage;
    LONG  biXPelsPerMeter, biYPelsPerMeter;
    DWORD biClrUsed, biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;
typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[1];
} BITMAPINFO, *LPBITMAPINFO;
typedef struct tagBITMAP {
    LONG   bmType, bmWidth, bmHeight, bmWidthBytes;
    WORD   bmPlanes, bmBitsPixel;
    LPVOID bmBits;
} BITMAP;
typedef struct tagLOGFONT {
    LONG lfHeight, lfWidth, lfEscapement, lfOrientation, lfWeight;
    BYTE lfItalic, lfUnderline, lfStrikeOut, lfCharSet;
    BYTE lfOutPrecision, lfClipPrecision, lfQuality, lfPitchAndFamily;
    CHAR lfFaceName[32];
} LOGFONT;
typedef struct tagTEXTMETRIC {
    LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading;
    LONG tmAveCharWidth, tmMaxCharWidth, tmWeight, tmOverhang;
    LONG tmDigitizedAspectX, tmDigitizedAspectY;
    BYTE tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
    BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
} TEXTMETRIC, *LPTEXTMETRIC;
typedef struct tagOPENFILENAME {
    DWORD     lStructSize;
    HWND      hwndOwner;
    HINSTANCE hInstance;
    LPCSTR    lpstrFilter;
    LPSTR     lpstrCustomFilter;
    DWORD     nMaxCustFilter;
    DWORD     nFilterIndex;
    LPSTR     lpstrFile;
    DWORD     nMaxFile;
    LPSTR     lpstrFileTitle;
    DWORD     nMaxFileTitle;
    LPCSTR    lpstrInitialDir;
    LPCSTR    lpstrTitle;
    DWORD     Flags;
    WORD      nFileOffset;
    WORD      nFileExtension;
    LPCSTR    lpstrDefExt;
    LPARAM    lCustData;
    void*     lpfnHook;
    LPCSTR    lpTemplateName;
} OPENFILENAME, *LPOPENFILENAME;

// ── macros ───────────────────────────────────────────────────────────────────────────────────
#define LOWORD(l)        ((WORD)((ULONG_PTR)(l) & 0xffff))
#define HIWORD(l)        ((WORD)(((ULONG_PTR)(l) >> 16) & 0xffff))
#define LOBYTE(w)        ((BYTE)((w) & 0xff))
#define HIBYTE(w)        ((BYTE)(((w) >> 8) & 0xff))
#define MAKEWORD(a,b)    ((WORD)(((BYTE)(a)) | ((WORD)((BYTE)(b))) << 8))
#define MAKELONG(a,b)    ((LONG)(((WORD)(a)) | ((DWORD)((WORD)(b))) << 16))
#define MAKELPARAM(l,h)  ((LPARAM)MAKELONG(l,h))
#define RGB(r,g,b)       ((COLORREF)(((BYTE)(r)) | (((WORD)(BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb)   (LOBYTE(rgb))
#define GetGValue(rgb)   (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb)   (LOBYTE((rgb) >> 16))
#define PALETTERGB(r,g,b) (0x02000000 | RGB(r,g,b))
#define MAKEINTRESOURCE(i) ((LPCSTR)(ULONG_PTR)((WORD)(i)))
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))

// ── constants (measured usage; grouped by subsystem) ─────────────────────────────────────────
// window messages
#define WM_CREATE          0x0001
#define WM_DESTROY         0x0002
#define WM_MOVE            0x0003
#define WM_SIZE            0x0005
#define WM_ACTIVATE        0x0006
#define WM_SETREDRAW       0x000b
#define WM_PAINT           0x000f
#define WM_CLOSE           0x0010
#define WM_QUERYENDSESSION 0x0011
#define WM_QUIT            0x0012
#define WM_ERASEBKGND      0x0014
#define WM_SYSCOLORCHANGE  0x0015
#define WM_SHOWWINDOW      0x0018
#define WM_CTLCOLOR        0x0019
#define WM_SETCURSOR       0x0020
#define WM_GETMINMAXINFO   0x0024
#define WM_SETFONT         0x0030
#define WM_KEYDOWN         0x0100
#define WM_KEYUP           0x0101
#define WM_CHAR            0x0102
#define WM_COMMAND         0x0111
#define WM_SYSCOMMAND      0x0112
#define WM_TIMER           0x0113
#define WM_HSCROLL         0x0114
#define WM_VSCROLL         0x0115
#define WM_MOUSEMOVE       0x0200
#define WM_LBUTTONDOWN     0x0201
#define WM_LBUTTONUP       0x0202
#define WM_LBUTTONDBLCLK   0x0203
#define WM_RBUTTONDOWN     0x0204
#define WM_RBUTTONUP       0x0205
#define WM_QUERYNEWPALETTE 0x030f
#define WM_PALETTEISCHANGING 0x0310
#define WM_PALETTECHANGED  0x0311
#define WM_USER            0x0400
// edit-control messages (balloon text scrolling)
#define EM_LINESCROLL      0x00b6
#define EM_GETLINECOUNT    0x00ba
// WM_ACTIVATE state / WM_SIZE type
#define WA_INACTIVE     0
#define WA_ACTIVE       1
#define WA_CLICKACTIVE  2
#define SIZE_RESTORED   0
#define SIZE_MINIMIZED  1
#define SIZE_MAXIMIZED  2
// non-client / hit-test / button messages
#define WM_NCLBUTTONDOWN   0x00a1
#define HTERROR    (-2)
#define HTNOWHERE  0
#define HTCLIENT   1
#define HTCAPTION  2
#define BM_GETSTATE 0x00f2
#define BM_SETSTATE 0x00f3
#define WM_SETTEXT  0x000c
#define BN_CLICKED  0
#define BST_PUSHED  0x0004
#define BST_PUSHED  0x0004
// virtual keys
#define VK_BACK    0x08
#define VK_TAB     0x09
#define VK_RETURN  0x0d
#define VK_SHIFT   0x10
#define VK_CONTROL 0x11
#define VK_ESCAPE  0x1b
#define VK_SPACE   0x20
#define VK_PRIOR   0x21
#define VK_NEXT    0x22
#define VK_END     0x23
#define VK_HOME    0x24
#define VK_LEFT    0x25
#define VK_UP      0x26
#define VK_RIGHT   0x27
#define VK_DOWN    0x28
#define VK_INSERT  0x2d
#define VK_DELETE  0x2e
#define VK_MENU    0x12
#define VK_F1      0x70
#define VK_F2      0x71
#define VK_F3      0x72
#define VK_F4      0x73
#define VK_F5      0x74
#define VK_F6      0x75
#define VK_F7      0x76
#define VK_F8      0x77
#define VK_F9      0x78
#define VK_F10     0x79
// mouse-key flags
#define MK_LBUTTON 0x0001
#define MK_RBUTTON 0x0002
#define MK_SHIFT   0x0004
#define MK_CONTROL 0x0008
// MessageBox / dialog ids
#define MB_OK               0x0000
#define MB_OKCANCEL         0x0001
#define MB_ABORTRETRYIGNORE 0x0002
#define MB_YESNOCANCEL      0x0003
#define MB_YESNO            0x0004
#define MB_ICONHAND         0x0010
#define MB_ICONQUESTION     0x0020
#define MB_ICONEXCLAMATION  0x0030
#define MB_ICONINFORMATION  0x0040
#define MB_ICONSTOP         MB_ICONHAND
#define IDOK     1
#define IDCANCEL 2
#define IDABORT  3
#define IDRETRY  4
#define IDIGNORE 5
#define IDYES    6
#define IDNO     7
// ShowWindow
#define SW_HIDE       0
#define SW_SHOWNORMAL 1
#define SW_SHOW       5
#define SW_MINIMIZE   6
#define SW_RESTORE    9
// scroll bars
#define SB_HORZ 0
#define SB_VERT 1
#define SB_CTL  2
#define SB_LINEUP    0
#define SB_LINELEFT  0
#define SB_LINEDOWN  1
#define SB_LINERIGHT 1
#define SB_PAGEUP    2
#define SB_PAGELEFT  2
#define SB_PAGEDOWN  3
#define SB_PAGERIGHT 3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK    5
#define SB_TOP       6
#define SB_LEFT      6
#define SB_BOTTOM    7
#define SB_RIGHT     7
#define SB_ENDSCROLL 8
// SetWindowPos / styles
#define SWP_NOSIZE     0x0001
#define SWP_NOMOVE     0x0002
#define SWP_NOZORDER   0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040
#define WS_OVERLAPPED  0x00000000
#define WS_CHILD    0x40000000
#define WS_VISIBLE  0x10000000
#define WS_DISABLED 0x08000000
#define WS_BORDER   0x00800000
#define WS_VSCROLL  0x00200000
#define WS_HSCROLL  0x00100000
#define WS_OVERLAPPEDWINDOW 0x00cf0000
#define ES_MULTILINE 0x0004
#define ES_READONLY  0x0800
#define ES_LEFT      0x0000
#define WS_CAPTION   0x00C00000
#define WS_POPUP     0x80000000
#define WS_DLGFRAME  0x00400000
#define WS_SYSMENU   0x00080000
#define WM_INITDIALOG 0x0110
// button styles (low nibble of a BUTTON control's style)
#define BS_PUSHBUTTON    0x0000
#define BS_DEFPUSHBUTTON 0x0001
#define BS_CHECKBOX      0x0002
#define BS_AUTOCHECKBOX  0x0003
#define BS_RADIOBUTTON   0x0004
#define BS_GROUPBOX      0x0007
#define BS_OWNERDRAW 0x000b
#define BS_TYPEMASK  0x000f
// static styles (low nibble of a STATIC control's style)
#define SS_LEFT      0x0000
#define SS_CENTER    0x0001
#define SS_RIGHT     0x0002
#define SS_ICON      0x0003
#define SS_BLACKRECT 0x0004
#define SS_GRAYRECT  0x0005
#define SS_WHITERECT 0x0006
#define SS_BLACKFRAME 0x0007
#define SS_GRAYFRAME 0x0008
#define SS_WHITEFRAME 0x0009
#define SS_TYPEMASK  0x001f
// scrollbar-control styles
#define SBS_HORZ 0x0000
#define SBS_VERT 0x0001
// system colors
#define COLOR_WINDOW       5
#define COLOR_WINDOWTEXT   8
#define COLOR_HIGHLIGHT    13
#define COLOR_HIGHLIGHTTEXT 14
#define COLOR_BTNFACE      15
#define COLOR_3DFACE       15
#define COLOR_BTNSHADOW    16
#define COLOR_GRAYTEXT     17
#define COLOR_BTNTEXT      18
#define COLOR_BTNHIGHLIGHT 20
// GDI: raster ops / DIB / pens / stock objects / device caps
#define SRCCOPY  0x00CC0020
#define SRCAND   0x008800C6
#define SRCPAINT 0x00EE0086
#define SRCINVERT 0x00660046
#define NOTSRCCOPY 0x00330008
#define BLACKNESS 0x00000042
#define WHITENESS 0x00FF0062
#define PATCOPY   0x00F00021
#define DIB_RGB_COLORS 0
#define DIB_PAL_COLORS 1
#define BI_RGB 0
#define PS_SOLID 0
#define PS_DASH  1
#define PS_NULL  5
#define TRANSPARENT 1
#define OPAQUE      2
#define WHITE_BRUSH 0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH   2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH 4
#define NULL_BRUSH  5
#define WHITE_PEN   6
#define BLACK_PEN   7
#define NULL_PEN    8
#define SYSTEM_FONT 13
#define ANSI_VAR_FONT 12
#define BITSPIXEL    12
#define PLANES       14
#define NUMCOLORS    24
#define RASTERCAPS   38
#define SIZEPALETTE 104
#define RC_PALETTE   0x0100
// CreateFont args
#define FW_NORMAL 400
#define FW_BOLD   700
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY 0
#define DEFAULT_PITCH 0
#define VARIABLE_PITCH 2
#define FF_DONTCARE 0
#define FF_SWISS 0x20
// system metrics
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define SM_CYCAPTION 4
#define SM_CYMENU   15
#define SM_CXFULLSCREEN 16
#define SM_CYFULLSCREEN 17
#define SM_CXFRAME  32
#define SM_CYFRAME  33
// stock cursors
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_WAIT  MAKEINTRESOURCE(32514)
// WM_SYSCOMMAND ids
#define SC_SIZE       0xf000
#define SC_MINIMIZE   0xf020
#define SC_MAXIMIZE   0xf030
#define SC_CLOSE      0xf060
#define SC_RESTORE    0xf120
// CFileDialog / OPENFILENAME flags
#define OFN_READONLY         0x00000001
#define OFN_OVERWRITEPROMPT  0x00000002
#define OFN_HIDEREADONLY     0x00000004
#define OFN_FILEMUSTEXIST    0x00001000
#define OFN_PATHMUSTEXIST    0x00000800
// RedrawWindow flags
#define RDW_INVALIDATE 0x0001
#define RDW_ERASE      0x0004
#define RDW_UPDATENOW  0x0100
#define RDW_ALLCHILDREN 0x0080
// wait / event
#define INFINITE        0xFFFFFFFF
#define WAIT_OBJECT_0   0
// GlobalAlloc flags (kept for completeness)
#define GMEM_FIXED    0x0000
#define GMEM_MOVEABLE 0x0002
#define GMEM_ZEROINIT 0x0040

#ifdef __cplusplus
extern "C" {
#endif

// ── USER subset ──────────────────────────────────────────────────────────────────────────────
BOOL     PtInRect(const RECT* lprc, POINT pt);
BOOL     SetRect(LPRECT lprc, int l, int t, int r, int b);
BOOL     IntersectRect(LPRECT dst, const RECT* a, const RECT* b);
BOOL     IsRectEmpty(const RECT* lprc);
BOOL     OffsetRect(LPRECT lprc, int dx, int dy);
int      GetSystemMetrics(int nIndex);
DWORD    GetSysColor(int nIndex);
HCURSOR  SetCursor(HCURSOR hCursor);
HCURSOR  LoadCursorA(HINSTANCE hInst, LPCSTR name);
#define LoadCursor LoadCursorA
HICON    LoadIconA(HINSTANCE hInst, LPCSTR name);
#define LoadIcon LoadIconA
HBITMAP  LoadBitmapA(HINSTANCE hInst, LPCSTR name);
// (no LoadBitmap alias macro — it would rename CBitmap::LoadBitmap in afxwin.h)
int      ShowCursor(BOOL bShow);
BOOL     GetCursorPos(LPPOINT lpPoint);
SHORT    GetAsyncKeyState(int vKey);
UINT     SetTimer(HWND hWnd, UINT id, UINT elapseMs, void* timerProc);
BOOL     KillTimer(HWND hWnd, UINT id);
LRESULT  SendMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define SendMessage SendMessageA
BOOL     PostMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#define PostMessage PostMessageA
BOOL     GetMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax);
#define GetMessage GetMessageA
BOOL     TranslateMessage(const MSG* lpMsg);
LRESULT  DispatchMessageA(const MSG* lpMsg);
#define DispatchMessage DispatchMessageA
int      MessageBoxA(HWND hWnd, LPCSTR text, LPCSTR caption, UINT type);
#define MessageBox MessageBoxA
BOOL     MessageBeep(UINT uType);
void     PostQuitMessage(int nExitCode);
HWND     FindWindowA(LPCSTR lpClassName, LPCSTR lpWindowName);
#define FindWindow FindWindowA
BOOL     BringWindowToTop(HWND hWnd);
BOOL     IsIconic(HWND hWnd);
HWND     GetLastActivePopup(HWND hWnd);
HWND     GetActiveWindow(void);
HDC      GetDC(HWND hWnd);
int      ReleaseDC(HWND hWnd, HDC hDC);
BOOL     ShowWindow(HWND hWnd, int nCmdShow);
BOOL     EnableWindow(HWND hWnd, BOOL bEnable);
BOOL     DestroyWindow(HWND hWnd);
BOOL     MoveWindow(HWND hWnd, int x, int y, int w, int h, BOOL bRepaint);
BOOL     SetWindowPos(HWND hWnd, HWND hWndAfter, int x, int y, int cx, int cy, UINT uFlags);
BOOL     SetWindowTextA(HWND hWnd, LPCSTR lpsz);
#define SetWindowText SetWindowTextA
HWND     SetFocus(HWND hWnd);
HWND     SetCapture(HWND hWnd);
BOOL     ReleaseCapture(void);
BOOL     ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL     ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL     GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL     GetWindowRect(HWND hWnd, LPRECT lpRect);
int      GetScrollPos(HWND hWnd, int nBar);
int      SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw);
BOOL     GetScrollRange(HWND hWnd, int nBar, LPINT lpMin, LPINT lpMax);
BOOL     SetScrollRange(HWND hWnd, int nBar, int nMin, int nMax, BOOL bRedraw);
BOOL     ShowScrollBar(HWND hWnd, int wBar, BOOL bShow);
BOOL     DrawIcon(HDC hDC, int x, int y, HICON hIcon);
BOOL     InvalidateRect(HWND hWnd, const RECT* lpRect, BOOL bErase);
BOOL     UpdateWindow(HWND hWnd);
BOOL     RedrawWindow(HWND hWnd, const RECT* lprcUpdate, HRGN hrgnUpdate, UINT flags);
HWND     GetParent(HWND hWnd);
void     FatalAppExitA(UINT uAction, LPCSTR lpMessageText);
#define FatalAppExit FatalAppExitA
BOOL     CopyRect(LPRECT lprcDst, const RECT* lprcSrc);
int      FillRect(HDC hDC, const RECT* lprc, HBRUSH hbr);
int      wsprintfA(LPSTR buf, LPCSTR fmt, ...);
#define wsprintf wsprintfA

// ── GDI subset (implemented over SDL surfaces in microfx/src/gdi/) ───────────────────────────
HDC      CreateCompatibleDC(HDC hdc);
BOOL     DeleteDC(HDC hdc);
HGDIOBJ  SelectObject(HDC hdc, HGDIOBJ h);
BOOL     DeleteObject(HGDIOBJ h);
int      GetObjectA(HGDIOBJ h, int cb, LPVOID pv);
#define GetObject GetObjectA
HBITMAP  CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage,
                          void** ppvBits, HANDLE hSection, DWORD offset);
BOOL     BitBlt(HDC hdcDst, int x, int y, int cx, int cy, HDC hdcSrc, int sx, int sy, DWORD rop);
BOOL     PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop);
int      GetDeviceCaps(HDC hdc, int index);
HPALETTE CreatePalette(const LOGPALETTE* plpal);
HPALETTE CreateHalftonePalette(HDC hdc);
UINT     RealizePalette(HDC hdc);
HPALETTE SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd);
BOOL     AnimatePalette(HPALETTE hPal, UINT iStart, UINT cEntries, const PALETTEENTRY* ppe);
UINT     GetPaletteEntries(HPALETTE hPal, UINT iStart, UINT cEntries, LPPALETTEENTRY ppe);
UINT     SetPaletteEntries(HPALETTE hPal, UINT iStart, UINT cEntries, const PALETTEENTRY* ppe);
UINT     GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY ppe);
UINT     GetNearestPaletteIndex(HPALETTE hPal, COLORREF color);
HBRUSH   CreateSolidBrush(COLORREF color);
HPEN     CreatePen(int style, int width, COLORREF color);
HFONT    CreateFontA(int h, int w, int esc, int orient, int weight, DWORD ital, DWORD undl,
                     DWORD strike, DWORD charset, DWORD outPrec, DWORD clipPrec, DWORD qual,
                     DWORD pitch, LPCSTR face);
#define CreateFont CreateFontA
HGDIOBJ  GetStockObject(int i);
LONG     SetBitmapBits(HBITMAP hbm, DWORD cb, const void* pvBits);
LONG     GetBitmapBits(HBITMAP hbm, LONG cb, LPVOID pvBits);
HBITMAP  CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits);
UINT     SetDIBColorTable(HDC hdc, UINT iStart, UINT cEntries, const RGBQUAD* prgbq);
UINT     GetDIBColorTable(HDC hdc, UINT iStart, UINT cEntries, RGBQUAD* prgbq);
BOOL     RoundRect(HDC hdc, int l, int t, int r, int b, int w, int h);
COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color);
COLORREF GetPixel(HDC hdc, int x, int y);
BOOL     Polygon(HDC hdc, const POINT* apt, int cpt);
BOOL     Pie(HDC hdc, int l, int t, int r, int b, int xr1, int yr1, int xr2, int yr2);
BOOL     Chord(HDC hdc, int l, int t, int r, int b, int xr1, int yr1, int xr2, int yr2);
BOOL     Rectangle(HDC hdc, int l, int t, int r, int b);
BOOL     MoveToEx(HDC hdc, int x, int y, LPPOINT lppt);
BOOL     LineTo(HDC hdc, int x, int y);
BOOL     TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
#define TextOut TextOutA
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF SetBkColor(HDC hdc, COLORREF color);
int      SetBkMode(HDC hdc, int mode);
BOOL     GetTextMetricsA(HDC hdc, LPTEXTMETRIC lptm);
#define GetTextMetrics GetTextMetricsA
BOOL     GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl);
#define GetTextExtentPoint32 GetTextExtentPoint32A
int      GetClipBox(HDC hdc, LPRECT lprect);

// ── kernel / thread subset ───────────────────────────────────────────────────────────────────
DWORD    GetTickCount(void);
void     Sleep(DWORD ms);
DWORD    GetLastError(void);
DWORD    GetVersion(void);
HINSTANCE GetModuleHandleA(LPCSTR name);
#define GetModuleHandle GetModuleHandleA
DWORD    GetModuleFileNameA(HINSTANCE hModule, LPSTR lpFilename, DWORD nSize);
#define GetModuleFileName GetModuleFileNameA
HANDLE   CreateEventA(void* lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName);
#define CreateEvent CreateEventA
BOOL     SetEvent(HANDLE hEvent);
BOOL     ResetEvent(HANDLE hEvent);
DWORD    WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD    ResumeThread(HANDLE hThread);
DWORD    SuspendThread(HANDLE hThread);
BOOL     CloseHandle(HANDLE hObject);

// ── registry / drive subset (install-path scan in DeskcppDoc) ───────────────────────────────
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)0x80000002u)
#define HKEY_CURRENT_USER  ((HKEY)(ULONG_PTR)0x80000001u)
#define KEY_READ  0x20019
#define KEY_WRITE 0x20006
#define ERROR_SUCCESS 0
#define DRIVE_UNKNOWN     0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE   2
#define DRIVE_FIXED       3
#define DRIVE_CDROM       5
LONG     RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, DWORD samDesired, HKEY* phkResult);
LONG     RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                          LPBYTE lpData, LPDWORD lpcbData);
LONG     RegCloseKey(HKEY hKey);
UINT     GetDriveTypeA(LPCSTR lpRootPathName);
#define GetDriveType GetDriveTypeA
LPSTR    lstrcpyA(LPSTR dst, LPCSTR src);
LPSTR    lstrcatA(LPSTR dst, LPCSTR src);
int      lstrlenA(LPCSTR s);
#define lstrcpy lstrcpyA
#define lstrcat lstrcatA
#define lstrlen lstrlenA

// MSVC CRT path helpers the game calls (host libc has no equivalents). On real Windows the
// ucrt <stdlib.h> already declares these (with dllimport linkage), so redeclaring them trips
// C2375 "redefinition; different linkage" — only provide our own off-Windows.
#ifndef _WIN32
void     _splitpath(const char* path, char* drive, char* dir, char* fname, char* ext);
void     _makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#endif // MICROFX_WINDOWS_H
