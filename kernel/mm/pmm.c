#include <bootloader.h>
#include <mm.h>
#include <stdio.h>
#include <string.h>
#include <spinlock.h>
#include <debug.h>

unsigned char* bitmap = NULL;
uint64_t frame_count;
uint64_t next_free_hint = 0;

#define INDEX_FROM_BIT(a) (a / 8)
#define OFFSET_FROM_BIT(a) (a % 8)

void set_frame_bit(uint64_t frame_index){
	uint64_t index =  INDEX_FROM_BIT(frame_index);
	uint64_t offset = OFFSET_FROM_BIT(frame_index);
	bitmap[index] |= 0x1 << offset;
}

void clear_frame_bit(uint64_t frame_index){
	uint64_t index = INDEX_FROM_BIT(frame_index);
	uint64_t offset = OFFSET_FROM_BIT(frame_index);
	bitmap[index] &= ~ ( 0x1 << offset );
}

int test_frame_bit(uint64_t frame_index){
    uint64_t index = INDEX_FROM_BIT(frame_index);
	uint64_t offset = OFFSET_FROM_BIT(frame_index);
	return (bitmap[index] & (0x1 << offset ));
}

void pmm_bios_init(bootinfo_t* bootinfo){
    uint64_t memory_size = 0;
    kprintf(KPRINTF_INFO,"bios provided physical memory map:\n");
    for(int i = 0; i < bootinfo->mmap_entry_count; i++){
        struct e820_block* block = (bootinfo->mmap + (24 * i));
        kprintf(KPRINTF_INFO,"e820 block: %p-%p type %d\n",block->base, block->base + block->length, block->type);
        if(block->type != E820_TYPE_RESERVED) memory_size+=block->length;
    }
    kprintf(KPRINTF_INFO,"total physical memory detected: %dMB\n",((memory_size) / 1024) / 1024);
    bitmap = bootinfo->reserved_mem_end + DIRECT_MAP_OFFSET;
    frame_count = memory_size / 4096;
    uint64_t bitmap_size = frame_count / 8;
    memset((void*)bitmap,0,bitmap_size);   // mark all memory as free

    for(int i = 0; i < bootinfo->mmap_entry_count; i++){
        struct e820_block* block = (bootinfo->mmap + (24 * i));
        if(block->type != E820_TYPE_FREE){
            int frames = (block->length + (PAGE_SIZE - 1)) / 4096;
            uint64_t index = block->base / PAGE_SIZE;
            for(int j = 0; j < frames; j++){
                set_frame_bit(index + j);
            }
        }    
    }

    int usedframes = (((uint64_t)bootinfo->reserved_mem_end + bitmap_size) + 4095) / 4096;
    int i;
    for(i = 0; i < usedframes; i++){
        set_frame_bit(i);
    }
    next_free_hint = i;
}

void pmm_uefi_init(bootinfo_t* bootinfo){
    kprintf(KPRINTF_INFO,"using uefi firmware provided memory map\n");
    EFI_MEMORY_DESCRIPTOR* MemoryMap = bootinfo->efi_memory_map.MemoryMap;
    uint64_t descriptor_size = bootinfo->efi_memory_map.DescriptorSize;
    uint64_t memory_map_size = bootinfo->efi_memory_map.MemoryMapSize;
    uint64_t total_memory_size = 0;

    uint64_t offset = 0;
    while(offset < memory_map_size){
        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)MemoryMap + offset);
        if(descriptor->Type != EfiMemoryMappedIO && descriptor->Type != EfiMemoryMappedIOPortSpace && descriptor->Type != EfiPalCode && descriptor->Type != EfiPersistentMemory && descriptor->Type != EfiUnusableMemory && descriptor->Type != EfiReservedMemoryType){ 
            total_memory_size += descriptor->NumberOfPages * PAGE_SIZE;
        }
        kprintf(KPRINTF_INFO,"efi-memmap entry - base: %p length: %lluKB type: %d\n",descriptor->PhysicalStart,(descriptor->NumberOfPages * PAGE_SIZE) / 1024,descriptor->Type);
        offset+=descriptor_size;
    }

    kprintf(KPRINTF_INFO,"total physical memory detected: %dMB\n",((total_memory_size) / 1024) / 1024);

    frame_count = total_memory_size / PAGE_SIZE;

    uint64_t bitmap_size = (total_memory_size / PAGE_SIZE) / 8;
    uint64_t bitmap_phys = 0;

    MemoryMap = bootinfo->efi_memory_map.MemoryMap;
    offset = 0;
    while(offset < memory_map_size){
        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)MemoryMap + offset);
        if(descriptor->Type == EfiConventionalMemory && (descriptor->NumberOfPages * PAGE_SIZE) > bitmap_size){
            if(descriptor->PhysicalStart == NULL)
                continue;
            bitmap = descriptor->PhysicalStart + DIRECT_MAP_OFFSET;
            bitmap_phys = (uint64_t)descriptor->PhysicalStart; 
            break;
        }
        offset+=descriptor_size;
    }
    if(bitmap_phys == 0)
        panic("Failed to initialise page frame allocator bitmap\n");

    memset(bitmap,0xff,bitmap_size);
    MemoryMap = bootinfo->efi_memory_map.MemoryMap;
    offset = 0;
    while(offset < memory_map_size){
        EFI_MEMORY_DESCRIPTOR* descriptor = (EFI_MEMORY_DESCRIPTOR*)((uint64_t)MemoryMap + offset);
        if(descriptor->Type == EfiConventionalMemory){
            uint64_t start_index = (uint64_t)descriptor->PhysicalStart / PAGE_SIZE;
            for(int i = 0; i < descriptor->NumberOfPages; i++){
                clear_frame_bit(start_index + i);
            }
        }
        offset+=descriptor_size;
    }

    uint64_t bitmap_index = (uint64_t)bitmap_phys / PAGE_SIZE;
    uint64_t bitmap_page_size = (bitmap_size + (PAGE_SIZE - 1)) / PAGE_SIZE;
    for(int i = 0; i < bitmap_page_size; i++){
        set_frame_bit(bitmap_index + i);
    }
}

void pmm_init(bootinfo_t* bootinfo){
    if(bootinfo->boot_type == BOOT_UEFI){
        return pmm_uefi_init(bootinfo);
    }
    if(bootinfo->boot_type == BOOT_BIOS){
        return pmm_bios_init(bootinfo);
    }
    panic("This bootloader is unsupported");
}

atomic_flag pmm_lock = ATOMIC_FLAG_INIT;

void* allocate_frame() {
    spinlock_acquire(&pmm_lock);
    for (uint64_t i = next_free_hint; i < frame_count; i++) {
        if (!test_frame_bit(i)) {
            next_free_hint = i;
            set_frame_bit(i);
            spinlock_release(&pmm_lock);
            return (void*)(i * 4096);
        }
    }
    spinlock_release(&pmm_lock);
    panic("out of memory!\n");
    return (void*)0;
}

void* allocate_frames(uint64_t count) {
    spinlock_acquire(&pmm_lock);
    for (uint64_t i = next_free_hint; i < frame_count - count; i++) {
        if (!test_frame_bit(i)) {
            next_free_hint = i;
            unsigned char success = 1;
            for (uint64_t c = 0; c < count; c++) {
                if (test_frame_bit(i + c)) {
                    success = 0;
                    break;
                }
            }
            if (success == 0) continue;
            for (uint64_t c = 0; c < count; c++) {
                set_frame_bit(i + c);
            }
            spinlock_release(&pmm_lock);
            return (void*)(i * 4096);
        }
    }
    spinlock_release(&pmm_lock);
    panic("out of memory!\n");
    return (void*)0;
}

void free_frame(void* frame) {
    spinlock_acquire(&pmm_lock);
    
    uint64_t index = (uint64_t)frame / 4096;
    if(index < next_free_hint) next_free_hint = index; 

    clear_frame_bit((uint64_t)index);
    
    spinlock_release(&pmm_lock);
}

void free_frames(void* frames, int nframes){
    for(int i = 0; i < nframes; i++){
        free_frame(frames + (4096 * i));
    }
}

uint64_t get_used_frame_count(){
    uint64_t used = 0;
    for (uint64_t i = 0; i < frame_count; i++) {
        if(test_frame_bit(i))used++;
    }
    return used;
}

uint64_t get_free_frame_count(){
    uint64_t free = 0;
    for (uint64_t i = 0; i < frame_count; i++) {
        if(!test_frame_bit(i))free++;
    }
    return free;
}