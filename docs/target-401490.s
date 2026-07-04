.intel_syntax noprefix
.globl World_CalcCompletionScore
World_CalcCompletionScore:
    sub esp, 8
    mov eax, 0xa
    push ebx
    push esi
    push edi
    lea esi, [ecx + 0x4b4]
    xor edi, edi
    mov dword ptr [esp + 0xc], edi
    fld dword ptr [esp + 0xc]
    fld dword ptr [0x44b09c]
.L4014b1:
    mov edx, 0xa
    mov ebx, 1
.L4014bb:
    cmp word ptr [esi], 0
    jle .L4014d1
    cmp dword ptr [esi + 0x24], ebx
    jne .L4014d1
    cmp dword ptr [esi + 0x20], ebx
    jne .L4014d1
    fld st(1)
    fadd st(1)
    fstp st(2)
.L4014d1:
    add esi, 0x34
    dec edx
    jne .L4014bb
    dec eax
    jne .L4014b1
    fstp st(0)
    fst dword ptr [esp + 0xc]
    fidiv dword ptr [ecx + 0x58]
    fmul dword ptr [0x44b0a0]
    fcom qword ptr [0x44b0a8]
    fstp qword ptr [esp + 0xc]
    fnstsw ax
    test ah, 1
    jne .L401519
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0b0]
    fnstsw ax
    test ah, 0x41
    je .L401519
    mov edi, 0x12c
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401519:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0b8]
    fnstsw ax
    test ah, 1
    jne .L401549
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0c0]
    fnstsw ax
    test ah, 0x41
    je .L401549
    mov edi, 0x10e
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401549:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0c8]
    fnstsw ax
    test ah, 1
    jne .L401579
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0d0]
    fnstsw ax
    test ah, 0x41
    je .L401579
    mov edi, 0xf0
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401579:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0d8]
    fnstsw ax
    test ah, 1
    jne .L4015a9
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0e0]
    fnstsw ax
    test ah, 0x41
    je .L4015a9
    mov edi, 0xd2
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L4015a9:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0e8]
    fnstsw ax
    test ah, 1
    jne .L4015d9
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0f0]
    fnstsw ax
    test ah, 0x41
    je .L4015d9
    mov edi, 0xb4
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L4015d9:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b0f8]
    fnstsw ax
    test ah, 1
    jne .L401609
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b100]
    fnstsw ax
    test ah, 0x41
    je .L401609
    mov edi, 0x96
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401609:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b108]
    fnstsw ax
    test ah, 1
    jne .L401639
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b110]
    fnstsw ax
    test ah, 0x41
    je .L401639
    mov edi, 0x78
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401639:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b118]
    fnstsw ax
    test ah, 1
    jne .L401669
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b120]
    fnstsw ax
    test ah, 0x41
    je .L401669
    mov edi, 0x5a
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401669:
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b128]
    fnstsw ax
    test ah, 1
    jne .L401699
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b130]
    fnstsw ax
    test ah, 0x41
    je .L401699
    mov edi, 0x3c
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret
.L401699:
    fldz
    fcomp qword ptr [esp + 0xc]
    fnstsw ax
    test ah, 0x41
    je .L4016bc
    fld qword ptr [esp + 0xc]
    fcomp qword ptr [0x44b140]
    fnstsw ax
    test ah, 0x41
    je .L4016bc
    mov edi, 0x1e
.L4016bc:
    mov eax, edi
    pop edi
    pop esi
    pop ebx
    add esp, 8
    ret