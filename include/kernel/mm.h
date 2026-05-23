#ifndef MM_H
#define MM_H

#define DIRECT_MAP_OFFSET 0xffff888000000000
#define KERNEL_HEAP_ADDR  0xffffc88000000000 

#define PAGE_FLAG_PRESENT       0b1
#define PAGE_FLAG_RW            0b10
#define PAGE_FLAG_USER          0b100
#define PAGE_FLAG_WRITETHROUGH  0b1000
#define PAGE_FLAG_CACHEDISABLE  0b10000
#define PAGE_SIZE 4096

#include <stdint.h>

void* allocate_frame();
void* allocate_frames(uint64_t count);
void free_frame(void* frame);
void free_frames(void* frames, int nframes);
uint64_t get_used_frame_count();
uint64_t get_free_frame_count();

void verify_heap_integrity();

#include <bootloader.h>

extern uint64_t* current_pml4;

void switch_pml4(void* pml4phys);
void* allocate_new_pd();

void pmm_init(bootinfo_t*);
void map_page_current(void* virt, void* phys,uint32_t flags);
void map_page(void* virt, void* phys,uint32_t flags,uint64_t* pml4);
void paging_init();
void clone_pd(void* newpd);
void map_pages(void* virt, void* phys,uint32_t flags,uint64_t* pml4, int count);
void* get_physical_address(void* virt,uint64_t* pml4);
void free_process_memory(uint64_t cr3);
void* is_page_mapped(void* virt,uint64_t* pml4);
void copy_to_pml4(void* virt, uint8_t* buffer, uint64_t size,uint64_t* pml4);

static inline void* phys_to_virt(void* addr){
    return addr + DIRECT_MAP_OFFSET;
}

#endif