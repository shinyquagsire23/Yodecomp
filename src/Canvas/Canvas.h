// Canvas — DIBSection offscreen buffer (its own compile unit, 0x407df0–0x4084e8; the TU is
// PARKED as a suspected separately-built library CU — see CLAUDE.md).
// CANONICAL declaration (struct de-dup steps 3+4, 2026-07-07): included by Canvas.cpp,
// GameView.h, WorldDoc.h and GameData/WorldStub.h. Method decl order = .text order.
// ENVIRONMENT SPLIT (the CDC obstacle): Canvas.cpp compiles against minimal <windows.h> plus
// a private 2-field CDC stub (in Canvas.cpp); every MFC TU sees the real MFC CDC. Hence only
// `class CDC;` here, and NO #includes on purpose — <windows.h> or <afxwin.h> must already be
// included by the TU (MapZone.h precedent).
// 0x407df0 is the CTOR (not "Init"): the guarded `new Canvas(w,h)` shape in OnInitialUpdate
// proves ctor-hood (v14); 0x407eb0 is the non-virtual dtor (Ghidra: Canvas::Dtor). Older
// copies' dtor addresses 0x408010/0x408400 were stale comment errors.
#ifndef CANVAS_H
#define CANVAS_H

class CDC;

class Canvas
{
public:
    HDC              hdc;          // +0x00  memory DC (CreateCompatibleDC)
    HBITMAP          hDib;         // +0x04  DIB section (CreateDIBSection)
    HGDIOBJ          hOldBitmap;   // +0x08  bitmap replaced by SelectObject; restored in dtor
    HPALETTE         hPalette;     // +0x0c  halftone palette
    BITMAPINFOHEADER biHeader;     // +0x10  DIB header
    RGBQUAD          palette[256]; // +0x38  color table
    void            *pData;        // +0x438 pixel bits (8bpp)
                                   // sizeof == 0x43c (operator_new size); no vptr

    Canvas(int width, int height);                              // 0x00407df0
    ~Canvas();                                                  // 0x00407eb0 (non-virtual)
    void *GetData();                                            // 0x00407f50
    void  GetSize(short *outWidth, short *outHeight);           // 0x00407f60
    int   CreatePalette();                                      // 0x00407f80
    UINT  SetPalette(UINT start, UINT count, RGBQUAD *colors);  // 0x00407fd0
    int   BitBlt(CDC *dest, int destX, int destY,               // 0x00408000
                 int width, int height, int srcX, int srcY);
    void  Clear();                                              // 0x00408040
    void  Fill(unsigned char value);                            // 0x004080a0
    void  BlitFast(void *src, int flags, short height,          // 0x00408110
                   unsigned short srcStride, short destX, short destY);
    void  BlitMasked(char *src, unsigned short srcStride, short height, // 0x00408240
                     short destX, short destY, char key);
};

#endif
