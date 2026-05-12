global load_gdt
load_gdt:
    lgdt [rdi]
    push 0x8
    lea rax, [rel .continue]
    push rax
    retfq
    .continue:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    ret

global flush_tss
flush_tss:
    mov ax, 0x28
    ltr ax
    ret