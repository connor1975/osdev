%include "stage2/modeswitch.asm"
extern gdtr

vbe_info_structure:
    .signature: db "VBE2"
    times 508 db 0

vbe_mode_info_structure:
    times 256 db 0

global get_vbe_info
get_vbe_info:
    push rbx
    enter_rmode
    mov ax, 0x4F00
    mov di, vbe_info_structure
    int 10h
    enter_lmode
    pop rbx
    mov rax, vbe_info_structure
    ret

global get_vbe_mode_info
get_vbe_mode_info:
    push rbx
    enter_rmode
    mov ax, 4F01h
    mov cx, di
    mov di, si
    int 10h
    enter_lmode
    pop rbx
    ret

global set_vbe_mode
set_vbe_mode:
    push rbx
    enter_rmode
    mov ax, 4F02h
    mov bx, di
    int 10h
    enter_lmode
    pop rbx
    ret