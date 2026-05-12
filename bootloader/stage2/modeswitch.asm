%macro enter_rmode 0
    [bits 64]
    push rax
    push rcx
    push 0x18                
    lea rax, [rel .pmode32] 
    push rax                 
    retfq    ; Jump to compatibility mode
[bits 32]
.pmode32:
    ; Disable paging
    mov eax, cr0
    and eax, ~0x80000000
    mov cr0, eax
    ; Exit long mode
    mov ecx, 0xC0000080
    rdmsr
    and eax, ~(1 << 8)
    wrmsr
    jmp word 20h:.pmode16         ; 1 - jump to 16-bit protected mode segment
.pmode16:
    [bits 16]
    mov eax, cr0
    and al, ~1
    mov cr0, eax
    jmp word 00h:.rmode
.rmode:
    mov ax, 0
    mov ds, ax
    mov ss, ax
    mov es, ax
    pop ecx
    add esp, 4
    pop eax
    add esp, 4
    sti
%endmacro

%macro enter_lmode 0
    [bits 16]
    cli
    push eax
    push ebx
    push ecx
    mov eax, cr4
    or eax, 10100000b                 ; Set the PAE and PGE bit.  
    mov cr4, eax
    mov ecx, 0xC0000080               ; Read from the EFER MSR. 
    rdmsr    
    or eax, 0x00000100                ; Set the LME bit.
    wrmsr
    mov ebx, cr0                      ; Activate long mode -
    or ebx,0x80000001                 ; - by enabling paging and protection simultaneously.
    mov cr0, ebx
    pop ecx    
    pop ebx                
    pop eax
    lgdt [gdtr]
    jmp 0x8:.LongMode
    .LongMode:
    [bits 64]
    push rax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    pop rax
%endmacro