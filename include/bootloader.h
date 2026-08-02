#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include <vbe.h>
#include <stdint.h>

#define E820_TYPE_FREE              1
#define E820_TYPE_RESERVED          2
#define E820_TYPE_ACPI_RECLAIMABLE  3

#define BOOT_BIOS 1
#define BOOT_UEFI 2

struct e820_block{
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t ext_attributes;
}__attribute__((packed));

struct framebuffer{
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    void* framebuffer;
};

typedef struct {
    uint32_t Type;
    uint32_t Pad;
    void* PhysicalStart;
    void* VirtualStart;
    uint64_t NumberOfPages;
    uint64_t Attribute;
} __attribute__((packed)) EFI_MEMORY_DESCRIPTOR;

typedef enum {
  EfiReservedMemoryType,
  EfiLoaderCode,
  EfiLoaderData,
  EfiBootServicesCode,
  EfiBootServicesData,
  EfiRuntimeServicesCode,
  EfiRuntimeServicesData,
  EfiConventionalMemory,
  EfiUnusableMemory,
  EfiACPIReclaimMemory,
  EfiACPIMemoryNVS,
  EfiMemoryMappedIO,
  EfiMemoryMappedIOPortSpace,
  EfiPalCode,
  EfiPersistentMemory,
  EfiMaxMemoryType
} EFI_MEMORY_TYPE;

struct efi_memory_map{
    EFI_MEMORY_DESCRIPTOR* MemoryMap;
    uint64_t MemoryMapSize;
    uint64_t MapKey;
    uint64_t DescriptorSize;
    uint64_t DescriptorVersion;
};

typedef struct bootinfo{
    int boot_type;

    // BIOS
    void* mmap;
    uint32_t mmap_entry_count;
    void* reserved_mem_start;   // Bootloader stores things that are still needed after the kernel is loaded here, like page tables
    void* reserved_mem_end;

    // UEFI
    struct efi_memory_map efi_memory_map;

    struct framebuffer* framebuffer;
    void* rsdp;
    void* initrd;
} bootinfo_t;

#endif