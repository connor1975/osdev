section .bss
align 16
resb 16384
global boot_stack_top
boot_stack_top:

section .text
global _start:
_start:

    mov rsp, boot_stack_top
    extern main
    call main

    cli
    halt:
    hlt
    jmp halt