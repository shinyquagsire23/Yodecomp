// Canvas — DIBSection offscreen buffer.
// Each function is annotated with its original address for the match harness
// (tools/match.py). Flags: /nologo /c /MT /W3 /GX /O2 /D WIN32 /D NDEBUG /D _WINDOWS
#include "Canvas.h"
#include <string.h>
#pragma intrinsic(memset, memcpy)

// Set once at startup: 1 if the CPU supports MMX. Gates the accelerated blit path.
extern int App_bCpuHasMMX;

// EFFECTIVE MATCH (22B reg-alloc residual): scheduler places the `height` CSE-temp load ~2 stores earlier
// than the original + a push-edi timing diff. Semantically identical; not source-steerable (see CLAUDE.md).
// FUNCTION: YODA 0x00407df0
Canvas* Canvas::Init(int width, int height)
{
    BITMAPINFOHEADER* h = &biHeader;
    biHeader.biCompression = 0;
    biHeader.biPlanes = 1;
    h->biSize = 0x28;
    biHeader.biXPelsPerMeter = 0;
    biHeader.biYPelsPerMeter = 0;
    biHeader.biClrImportant = 0;
    biHeader.biHeight = height;
    biHeader.biBitCount = 8;
    if (0 < height)
        biHeader.biHeight = -height;
    biHeader.biClrUsed = 0x100;
    biHeader.biWidth = width;
    biHeader.biSizeImage = height * width;
    hdc = CreateCompatibleDC(0);
    if (hdc != 0) {
        CreatePalette();
        hDib = CreateDIBSection(hdc, (BITMAPINFO*)h, 0, &pData, 0, 0);
        if (hDib != 0) {
            hOldBitmap = SelectObject(hdc, hDib);
            return this;
        }
        DeleteDC(hdc);
        if (hPalette != 0)
            DeleteObject(hPalette);
        hdc = 0;
    }
    return this;
}

// FUNCTION: YODA 0x00407eb0
void Canvas::Free()
{
    HDC dc = hdc;
    if (dc != 0 && hOldBitmap != 0) {
        SelectObject(dc, hOldBitmap);
        DeleteObject(hDib);
        DeleteDC(hdc);
        hOldBitmap = 0;
        hDib = 0;
        hdc = 0;
    }
    else if (dc != 0 && hDib != 0 && hOldBitmap == 0) {
        DeleteObject(hDib);
        DeleteDC(hdc);
        hdc = 0;
        hDib = 0;
    }
    else if (hDib != 0 && dc == 0) {
        DeleteObject(hDib);
        hDib = 0;
    }
    if (hPalette != 0)
        DeleteObject(hPalette);
}


// FUNCTION: YODA 0x00407f50
void* Canvas::GetData()
{
    return pData;
}


// FUNCTION: YODA 0x00407f60
void Canvas::GetSize(short* outWidth, short* outHeight)
{
    if (hDib != 0) {
        *outWidth  = (unsigned short)biHeader.biWidth;
        *outHeight = -(short)biHeader.biHeight;
    }
}


// FUNCTION: YODA 0x00407f80
int Canvas::CreatePalette()
{
    HWND     hWnd = GetActiveWindow();
    HDC      dc   = GetDC(hWnd);
    HPALETTE pal  = CreateHalftonePalette(dc);
    hPalette = pal;
    if (pal != 0) {
        RGBQUAD* pe = palette;
        GetPaletteEntries(pal, 0, 0x100, (LPPALETTEENTRY)pe);
        for (int i = 0x1a; i != 0; i--) {
            BYTE t = pe->rgbBlue;
            pe->rgbBlue = pe->rgbRed;
            pe->rgbRed = t;
            pe++;
        }
        return 1;
    }
    return 0;
}


// FUNCTION: YODA 0x00407fd0
UINT Canvas::SetPalette(UINT start, UINT count, RGBQUAD* colors)
{
    if (hdc == 0)
        return 0;
    return SetDIBColorTable(hdc, start, count, colors);
}


// FUNCTION: YODA 0x00408000
int Canvas::BitBlt(CDC* dest, int destX, int destY, int width, int height, int srcX, int srcY)
{
    if (hdc == 0)
        return 0;
    return ::BitBlt(dest->m_hDC, destX, destY, width, height, hdc, srcX, srcY, SRCCOPY);
}


// EFFECTIVE MATCH (2B reg-alloc residual): `y` (user var) vs the inlined-memset `width` CSE-temp get the
// swapped EBX/ESI pair. Fill matches (its `value` param shifts reg pressure). Not source-steerable.
// FUNCTION: YODA 0x00408040
void Canvas::Clear()
{
    short width, height;
    GetSize(&width, &height);
    char* p = (char*)pData;
    for (int y = 0; y < height; y++)
        memset(p + y * width, 0, width);
}


// FUNCTION: YODA 0x004080a0
void Canvas::Fill(unsigned char value)
{
    short width, height;
    GetSize(&width, &height);
    char* p = (char*)pData;
    for (int y = 0; y < height; y++)
        memset(p + y * width, value, width);  // TODO: y/width reg-alloc flip (permuter)
}

// FUNCTION: YODA 0x00408110
// 32-byte-wide (one tile) row blit. Both paths hand-asm: an MMX movq copy and a scalar
// dword-copy loop (VC++4.2 predates MMX so the movq's are hand-emitted bytes).
void Canvas::BlitFast(void* src, int flags, short height,
                      unsigned short srcStride, short destX, short destY)
{
    short canvasW, canvasH;
    GetSize(&canvasW, &canvasH);
    char* dst = (char*)pData + destX + canvasW * destY;
    void* s;
    if (canvasH <= height + destY - 1)
        height = canvasH - destY;
    s = src;
    int rows = height;
    int stride = (short)srcStride;
    int cw     = canvasW;
    if (App_bCpuHasMMX != 0) {
        __asm {
        _emit 0x66      // pushaw
        _emit 0x60
        mov  ecx, rows
        mov  esi, s
        mov  edi, dst
    mloop:
        _emit 0x0f      // movq mm0, [esi+0x0]
        _emit 0x6f
        _emit 0x06
        _emit 0x0f      // movq mm1, [esi+0x8]
        _emit 0x6f
        _emit 0x4e
        _emit 0x08
        _emit 0x0f      // movq mm2, [esi+0x10]
        _emit 0x6f
        _emit 0x56
        _emit 0x10
        _emit 0x0f      // movq mm3, [esi+0x18]
        _emit 0x6f
        _emit 0x5e
        _emit 0x18
        _emit 0x0f      // movq [edi+0x0], mm0
        _emit 0x7f
        _emit 0x07
        _emit 0x0f      // movq [edi+0x8], mm1
        _emit 0x7f
        _emit 0x4f
        _emit 0x08
        _emit 0x0f      // movq [edi+0x10], mm2
        _emit 0x7f
        _emit 0x57
        _emit 0x10
        _emit 0x0f      // movq [edi+0x18], mm3
        _emit 0x7f
        _emit 0x5f
        _emit 0x18
        mov  eax, stride
        add  esi, eax
        mov  eax, cw
        add  edi, eax
        dec  ecx
        jnz  mloop
        _emit 0x66      // popaw
        _emit 0x61
        _emit 0x0f      // emms
        _emit 0x77
        }
        return;
    }
    __asm {
        _emit 0x66      // pushaw
        _emit 0x60
        mov  ecx, rows
        mov  esi, s
        mov  edi, dst
    srow:
        xor  ebx, ebx
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        add  ebx, 4
        mov  eax, [ebx+esi]
        mov  [ebx+edi], eax
        xor  eax, eax
        mov  ax, srcStride
        add  esi, eax
        xor  eax, eax
        mov  ax, canvasW
        add  edi, eax
        dec  ecx
        jnz  srow
        _emit 0x66      // popaw
        _emit 0x61
    }
}

// EFFECTIVE MATCH (4B reg-alloc residual): the two independent movsx loads (destX, canvasW) in the dst
// setup are emitted in swapped order (same target regs). Scheduler slot-choice; not source-steerable
// (joint commutative-chain enumeration didn't flip it). The 676 asm bytes are byte-identical.
// FUNCTION: YODA 0x00408240
// Color-key (transparent) 32-byte row blit. Both paths are hand-asm (the dev's own style):
// a scalar masked loop, and an MMX pcmpeqb/pand/por path (movq etc. hand-emitted -- VC++4.2
// predates MMX). The MMX color key is a zeroed qword (keyq); transparent color is 0.
void Canvas::BlitMasked(char* src, unsigned short srcStride, short height,
                        short destX, short destY, char key)
{
    short canvasW, canvasH;
    GetSize(&canvasW, &canvasH);
    char* dst = (char*)pData + destX + canvasW * destY;
    char* s;
    if (canvasH <= height + destY - 1)
        height = canvasH - destY;
    s = src;
    int  rows = height;
    volatile struct { int lo, hi; } keyq;
    keyq.lo = 0;
    keyq.hi = 0;
    if (App_bCpuHasMMX != 0) {
        __asm {
        _emit 0x66      // pushaw
        _emit 0x60
        mov  ecx, rows
        mov  esi, s
        mov  edi, dst
    mloop:
        _emit 0x0f      // movq mm3, keyq
        _emit 0x6f
        _emit 0x5d
        _emit 0xe8
        _emit 0x0f      // movq mm0, [esi+0x0]
        _emit 0x6f
        _emit 0x06
        _emit 0x0f      // movq mm1, [edi+0x0]
        _emit 0x6f
        _emit 0x0f
        _emit 0x0f      // pcmpeqb mm3, mm0
        _emit 0x74
        _emit 0xd8
        _emit 0x0f      // pand mm1, mm3
        _emit 0xdb
        _emit 0xcb
        _emit 0x0f      // por mm1, mm0
        _emit 0xeb
        _emit 0xc8
        _emit 0x0f      // movq [edi+0x0], mm1
        _emit 0x7f
        _emit 0x0f
        _emit 0x0f      // movq mm3, keyq
        _emit 0x6f
        _emit 0x5d
        _emit 0xe8
        _emit 0x0f      // movq mm0, [esi+0x8]
        _emit 0x6f
        _emit 0x46
        _emit 0x08
        _emit 0x0f      // movq mm1, [edi+0x8]
        _emit 0x6f
        _emit 0x4f
        _emit 0x08
        _emit 0x0f      // pcmpeqb mm3, mm0
        _emit 0x74
        _emit 0xd8
        _emit 0x0f      // pand mm1, mm3
        _emit 0xdb
        _emit 0xcb
        _emit 0x0f      // por mm1, mm0
        _emit 0xeb
        _emit 0xc8
        _emit 0x0f      // movq [edi+0x8], mm1
        _emit 0x7f
        _emit 0x4f
        _emit 0x08
        _emit 0x0f      // movq mm3, keyq
        _emit 0x6f
        _emit 0x5d
        _emit 0xe8
        _emit 0x0f      // movq mm0, [esi+0x10]
        _emit 0x6f
        _emit 0x46
        _emit 0x10
        _emit 0x0f      // movq mm1, [edi+0x10]
        _emit 0x6f
        _emit 0x4f
        _emit 0x10
        _emit 0x0f      // pcmpeqb mm3, mm0
        _emit 0x74
        _emit 0xd8
        _emit 0x0f      // pand mm1, mm3
        _emit 0xdb
        _emit 0xcb
        _emit 0x0f      // por mm1, mm0
        _emit 0xeb
        _emit 0xc8
        _emit 0x0f      // movq [edi+0x10], mm1
        _emit 0x7f
        _emit 0x4f
        _emit 0x10
        _emit 0x0f      // movq mm3, keyq
        _emit 0x6f
        _emit 0x5d
        _emit 0xe8
        _emit 0x0f      // movq mm0, [esi+0x18]
        _emit 0x6f
        _emit 0x46
        _emit 0x18
        _emit 0x0f      // movq mm1, [edi+0x18]
        _emit 0x6f
        _emit 0x4f
        _emit 0x18
        _emit 0x0f      // pcmpeqb mm3, mm0
        _emit 0x74
        _emit 0xd8
        _emit 0x0f      // pand mm1, mm3
        _emit 0xdb
        _emit 0xcb
        _emit 0x0f      // por mm1, mm0
        _emit 0xeb
        _emit 0xc8
        _emit 0x0f      // movq [edi+0x18], mm1
        _emit 0x7f
        _emit 0x4f
        _emit 0x18
        xor  eax, eax
        mov  ax, srcStride
        add  esi, eax
        xor  eax, eax
        mov  ax, canvasW
        add  edi, eax
        dec  ecx
        jnz  mloop
        _emit 0x66      // popaw
        _emit 0x61
        _emit 0x0f      // emms
        _emit 0x77
        }
        return;
    }
    __asm {
        _emit 0x66      // pushaw
        _emit 0x60
        mov  ecx, rows
        mov  esi, s
        mov  edi, dst
    srow:
        xor  ebx, ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s0
        mov  [ebx+edi], al
    s0:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s1
        mov  [ebx+edi], al
    s1:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s2
        mov  [ebx+edi], al
    s2:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s3
        mov  [ebx+edi], al
    s3:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s4
        mov  [ebx+edi], al
    s4:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s5
        mov  [ebx+edi], al
    s5:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s6
        mov  [ebx+edi], al
    s6:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s7
        mov  [ebx+edi], al
    s7:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s8
        mov  [ebx+edi], al
    s8:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s9
        mov  [ebx+edi], al
    s9:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s10
        mov  [ebx+edi], al
    s10:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s11
        mov  [ebx+edi], al
    s11:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s12
        mov  [ebx+edi], al
    s12:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s13
        mov  [ebx+edi], al
    s13:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s14
        mov  [ebx+edi], al
    s14:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s15
        mov  [ebx+edi], al
    s15:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s16
        mov  [ebx+edi], al
    s16:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s17
        mov  [ebx+edi], al
    s17:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s18
        mov  [ebx+edi], al
    s18:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s19
        mov  [ebx+edi], al
    s19:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s20
        mov  [ebx+edi], al
    s20:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s21
        mov  [ebx+edi], al
    s21:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s22
        mov  [ebx+edi], al
    s22:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s23
        mov  [ebx+edi], al
    s23:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s24
        mov  [ebx+edi], al
    s24:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s25
        mov  [ebx+edi], al
    s25:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s26
        mov  [ebx+edi], al
    s26:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s27
        mov  [ebx+edi], al
    s27:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s28
        mov  [ebx+edi], al
    s28:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s29
        mov  [ebx+edi], al
    s29:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s30
        mov  [ebx+edi], al
    s30:
        inc  ebx
        mov  al, [ebx+esi]
        cmp  al, key
        je   s31
        mov  [ebx+edi], al
    s31:
        xor  eax, eax
        mov  ax, srcStride
        add  esi, eax
        xor  eax, eax
        mov  ax, canvasW
        add  edi, eax
        dec  ecx
        jnz  srow
        _emit 0x66      // popaw
        _emit 0x61
    }
}
