# decomp.me target — Canvas::Init @ 0x00407df0 (VC++ 4.2, x86, __thiscall: this=ecx)
# args on stack: width @ 0xc(%esp-after-2-push)=arg0, height @ 0x10(%esp)=arg1
# reloc sites: calll *0x45e6xx = GDI imports; Canvas_CreatePalette = internal call.
# this->biHeader is at ecx+0x10; BITMAPINFOHEADER fields: biSize+0/biWidth+4/biHeight+8/
#   biPlanes+c/biBitCount+e/biCompression+10/biSizeImage+14/biXPels+18/biYPels+1c/biClrUsed+20/biClrImportant+24

# Canvas::Init  (YODA 0x00407df0)  -- AT&T / GAS syntax
.globl Init
Init:
    pushl   %esi
    pushl   %edi
    leal    0x10(%ecx), %edi
    xorl    %eax, %eax
    movl    %eax, 0x20(%ecx)
    movl    %ecx, %esi
    movw    $1, 0x1c(%esi)
    movl    $0x28, (%edi)
    movl    %eax, 0x28(%esi)
    movl    0x10(%esp), %ecx
    movl    %eax, 0x2c(%esi)
    movl    %eax, 0x34(%esi)
    movl    %ecx, 0x18(%esi)
    movw    $8, 0x1e(%esi)
    testl   %ecx, %ecx
    jle     .L39
    movl    %ecx, %eax
    negl    %eax
    movl    %eax, 0x18(%esi)
.L39:
    movl    0xc(%esp), %eax
    pushl   $0
    movl    $0x100, 0x30(%esi)
    imull   %eax, %ecx
    movl    %eax, 0x14(%esi)
    movl    %ecx, 0x24(%esi)
    calll   *0x45e6d0   # -> CreateCompatibleDC (import, reloc)
    movl    %eax, (%esi)
    testl   %eax, %eax
    je      .Lb1
    movl    %esi, %ecx
    calll   Canvas_CreatePalette   # -> Canvas::CreatePalette (internal, reloc)
    pushl   $0
    leal    0x438(%esi), %eax
    pushl   $0
    movl    (%esi), %ecx
    pushl   %eax
    pushl   $0
    pushl   %edi
    pushl   %ecx
    calll   *0x45e6cc   # -> CreateDIBSection (import, reloc)
    movl    %eax, 4(%esi)
    testl   %eax, %eax
    je      .L94
    pushl   %eax
    movl    (%esi), %eax
    pushl   %eax
    calll   *0x45e6c8   # -> SelectObject (import, reloc)
    popl    %edi
    movl    %eax, 8(%esi)
    movl    %esi, %eax
    popl    %esi
    retl    $8
.L94:
    movl    (%esi), %eax
    pushl   %eax
    calll   *0x45e6c4   # -> DeleteDC (import, reloc)
    movl    0xc(%esi), %eax
    testl   %eax, %eax
    je      .Lab
    pushl   %eax
    calll   *0x45e6c0   # -> DeleteObject (import, reloc)
.Lab:
    movl    $0, (%esi)
.Lb1:
    movl    %esi, %eax
    popl    %edi
    popl    %esi
    retl    $8
