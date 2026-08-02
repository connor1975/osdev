[bits 16]

%include "stage2/modeswitch.asm"

global _start
_start:

; drive num in dl
mov [drive_num], dl

in al, 0x92
or al, 2
out 0x92, al    ; enable a20 line

mov ah, 00h
mov al, 03h
int 10h         ; clear screen by setting video mode

cli
mov edi, boot_pt
mov ecx, 512
mov ebx, 0x3
.setup_identity_map:    ; identity map first 2 mb (only 1 page table)
mov dword [edi], ebx
add ebx, 0x1000
add edi, 8
loop .setup_identity_map    
mov edx, boot_pml4
mov cr3, edx

enter_lmode
extern kmain
call enable_cpu_features
movzx rdi, byte [drive_num]
call kmain

halt:
jmp halt

drive_num: db 0

gdt:
    dq 0

.code_seg64:
    dw 0xFFFF
    dw 0
    db 0
    db 0b10011010
    db 0b10101111
    db 0

.data_seg64:
    dw 0xFFFF
    dw 0
    db 0
    db 0b10010010
    db 0b11001111
    db 0

.code_seg32:
    dw 0xFFFF
    dw 0
    db 0
    db 0b10011010
    db 0b11001111
    db 0

.code_seg16:
    dw 0xFFFF
    dw 0
    db 0
    db 0b10011010
    db 0b00001111
    db 0

global gdtr
gdtr:
    dw gdtr - gdt - 1                    ; 16-bit Size (Limit) of GDT.
    dd gdt                            ; 32-bit Base Address of GDT. (CPU will zero extend to 64-bit)
 

align 4096
global boot_pml4
boot_pml4:
    dq (boot_pdpt + 3)
    times 511 dq 0

boot_pdpt:
    dq (boot_pd + 3)
    times 511 dq 0

boot_pd:
    dq (boot_pt + 3)
    times 511 dq 0

boot_pt:
    times 512 dq 0

; void bios_read_disk(uint32_t dap_addr,uint8_t drive_num);
global bios_read_disk
bios_read_disk:
    push rbx
    enter_rmode
    push ds
    mov dx, si
    mov si, di
    shr di, 16
    mov ds, di

    mov ah, 42h
    int 13h
    pop ds
    enter_lmode
    pop rbx
    ret

; enable sse, initialises fpu and other cpu features
enable_cpu_features:
mov rax, cr0
and ax, 0xFFFB		;clear coprocessor emulation CR0.EM
or ax, 0x2			;set coprocessor monitoring  CR0.MP
mov cr0, rax
finit
mov rax, cr4
or ax, 3 << 9		;set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
or ax, 1 << 7       ; set CR4.PGE to enable global pages
or ax, 1 << 6       ; set CR4.MCE to enable machine check exception
mov cr4, rax
ret

global load_cr3
load_cr3:
    mov cr3, rdi
    ret

e820_block:
    times 24 db 0


; void* get_e820_block(uint32_t* continuation_id);
global get_e820_block
get_e820_block:
    push rbx
    push rdi

    enter_rmode
    mov ebx, [di]
    mov ecx, 24
    mov eax, 0xe820
    mov di, e820_block
    mov edx, 0x534D4150 ; 'SMAP'
    int 15h
    enter_lmode

    pop rdi
    mov [rdi], ebx  ; Store the new continuation id
    pop rbx
    mov rax, e820_block
    ret