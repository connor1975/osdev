[bits 16]
[org 0x7c00]
 
start:
cli
xor ax, ax                  ; 0 AX
mov ds, ax                  ; Set Data Segment to 0
mov es, ax                  ; Set Extra Segment to 0
mov ss, ax                  ; Set Stack Segment to 0
jmp 0:continue

continue:
mov [disk_no],dl

mov sp, 0x7c00
mov [disk_address_packet.lba], word 1
mov [disk_address_packet.offset], word 0x8000
mov [disk_address_packet.segment], word 0
mov [disk_address_packet.sector_count], word 127
mov ah, 0x42    ; Extended Read Sectors
mov si, disk_address_packet
mov dl,[disk_no]
int 0x13

mov dl, [disk_no]
jmp 0x8000

disk_no: db 0

disk_address_packet:
    db 10h
    db 0
    .sector_count: dw 0
    .offset: dw 0
    .segment: dw 0
    .lba: dq 0

times (0x1b4 - ($-$$)) nop    ; Pad For MBR Partition Table
 
UID times 10 db 0             ; Unique Disk ID
PT1 times 16 db 0             ; First Partition Entry
PT2 times 16 db 0             ; Second Partition Entry
PT3 times 16 db 0             ; Third Partition Entry
PT4 times 16 db 0             ; Fourth Partition Entry
 
dw 0xAA55                     ; Boot Signature