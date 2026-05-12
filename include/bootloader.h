#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <vbe.h>
#include <stdint.h>

#define E820_TYPE_FREE              1
#define E820_TYPE_RESERVED          2
#define E820_TYPE_ACPI_RECLAIMABLE  3

struct e820_block{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t ext_attributes;
}__attribute__((packed));

typedef struct bootinfo{
    void* mmap;
    uint32_t mmap_entry_count;
    void* reserved_mem_start;   // Bootloader stores things that are still needed after the kernel is loaded here, like page tables
    void* reserved_mem_end;
    struct vbe_mode_info* framebuffer;
    void* rsdp;
    void* initrd;
} bootinfo_t;

#endif