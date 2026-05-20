#include <kernel/common.h>
#include <kernel/mm.h>
#include <kernel/interrupts.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

uint64_t* current_pml4;

static inline void __native_flush_tlb_single(void* addr) {
	asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

void map_page_current(void* virt, void* phys,uint32_t flags) {
	map_page(virt,phys,flags,current_pml4);
}   

uint64_t get_page_entry(void* virt, uint64_t* pml4){
	uint64_t pml4off = ((uint64_t)virt >> 39) & 0x1FF;
	uint64_t pdptoff = ((uint64_t)virt >> 30) & 0x1FF;
	uint64_t pdoff = ((uint64_t)virt >> 21) & 0x1FF;
	uint64_t ptoff = ((uint64_t)virt >> 12) & 0x1FF;	
	uint64_t* pdpt = (uint64_t*)phys_to_virt((void*)(pml4[pml4off] & 0xFFFFFFFFFFFFF000));
	if(!(pml4[pml4off] & PAGE_FLAG_PRESENT)) return 0;
	uint64_t* pd = (uint64_t*)phys_to_virt((void*)(pdpt[pdptoff] & 0xFFFFFFFFFFFFF000));
	if(!(pdpt[pdptoff] & PAGE_FLAG_PRESENT)) return 0;
	uint64_t* pt = (uint64_t*)phys_to_virt((void*)(pd[pdoff] & 0xFFFFFFFFFFFFF000));
	if(!(pt[ptoff] & PAGE_FLAG_PRESENT)) return 0;
	return pt[ptoff];
}

void* get_physical_address(void* virt,uint64_t* pml4){
	return (void*)(get_page_entry(virt,pml4) & 0xFFFFFFFFFFFFF000);
}

void* is_page_mapped(void* virt,uint64_t* pml4){
	return (void*)(get_page_entry(virt,pml4) & PAGE_FLAG_PRESENT);
}

void copy_to_pml4(void* virt, uint8_t* buffer, uint64_t size, uint64_t* pml4) {
    uint64_t offset = (uint64_t)virt % 4096;     // Offset within the first page
    uint64_t page_size = 4096;                   // Page size in bytes

    while (size > 0) {
        void* current_page_phys = get_physical_address(virt, pml4);
        if (!current_page_phys) {
            return;
        }
        
        uint8_t* page_buffer = phys_to_virt(current_page_phys);
        uint64_t bytes_to_copy = page_size - offset;
        if (bytes_to_copy > size) {
            bytes_to_copy = size;
        }
        
        memcpy(page_buffer + offset, buffer, bytes_to_copy);
        
        virt = (void*)((uint64_t)virt + bytes_to_copy);
        buffer += bytes_to_copy;
        size -= bytes_to_copy;
        offset = 0;
        
        if (size > 0) {
            uint64_t next_page = ((uint64_t)virt + page_size - 1) & ~(page_size - 1);
            virt = (void*)next_page;
        }
    }
}


void map_page(void* virt, void* phys,uint32_t flags,uint64_t* pml4) {
	uint64_t pml4off = ((uint64_t)virt >> 39) & 0x1FF;
	uint64_t pdptoff = ((uint64_t)virt >> 30) & 0x1FF;
	uint64_t pdoff = ((uint64_t)virt >> 21) & 0x1FF;
	uint64_t ptoff = ((uint64_t)virt >> 12) & 0x1FF;	
    if (pml4[pml4off] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset(phys_to_virt((void*)address), 0, 4096);
		pml4[pml4off] = address | flags;
	}
	uint64_t* pdpt = (uint64_t*)phys_to_virt((void*)(pml4[pml4off] & 0xFFFFFFFFFFFFF000));

	if (pdpt[pdptoff] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset(phys_to_virt((void*)address), 0, 4096);
		pdpt[pdptoff] = address | flags;
	}
	uint64_t* pd = (uint64_t*)phys_to_virt((void*)(pdpt[pdptoff] & 0xFFFFFFFFFFFFF000));

	if (pd[pdoff] == 0) {
		uint64_t address = (uint64_t)allocate_frame();
		memset(phys_to_virt((void*)address), 0, 4096);
		pd[pdoff] = address | flags;
	}
	uint64_t* pt = (uint64_t*)phys_to_virt((void*)(pd[pdoff] & 0xFFFFFFFFFFFFF000));
	pt[ptoff] = (uint64_t)phys | flags;
	__native_flush_tlb_single((void*)virt);
}

void map_pages(void* virt, void* phys,uint32_t flags,uint64_t* pml4, int count){
	for(int i = 0; i < count; i++){
		map_page(virt + (i * 4096), phys + (i * 4096),flags,pml4);
	}
}

void switch_pml4(void* pml4phys) {
	current_pml4 = (uint64_t*)phys_to_virt(pml4phys);
	asm("mov %0, %%cr3" : : "r" (pml4phys));
}

// clones the user half of the current address space
// clones physical memory and virtual mappings
void clone_pd(void* newpd){
    uint64_t* original_pml4 = phys_to_virt((void*)current_task->cr3);
    for(int q = 0; q < 256; q++){
        if(original_pml4[q] & PAGE_FLAG_PRESENT){
            uint64_t* pdpt = phys_to_virt((void*)(original_pml4[q] & 0xFFFFFFFFFFFFF000));
            for(int i = 0; i < 512; i++){
                if(pdpt[i] & PAGE_FLAG_PRESENT){
                    uint64_t* pd = phys_to_virt((void*)(pdpt[i] & 0xFFFFFFFFFFFFF000));
                    for(int x = 0; x < 512; x++){
                        if(pd[x] & PAGE_FLAG_PRESENT){
                            uint64_t* pt = phys_to_virt((void*)(pd[x] & 0xFFFFFFFFFFFFF000));
                            for(int y = 0; y < 512; y++){
                                if(pt[y] & 0xFFFFFFFFFFFFF000){
                                    void* new_phys = allocate_frame();
                                    memcpy(phys_to_virt(new_phys),phys_to_virt((void*)(pt[y] & 0xFFFFFFFFFFFFF000)),4096);
                                    uint64_t address = 0;
									address |= ((uint64_t)q & 0x1FF) << 39;
                                    address |= ((uint64_t)i & 0x1FF) << 30;
                                    address |= ((uint64_t)x & 0x1FF) << 21;
                                    address |= ((uint64_t)y & 0x1FF) << 12;
                                    map_page((void*)address,new_phys,PAGE_FLAG_USER | PAGE_FLAG_RW | PAGE_FLAG_PRESENT,newpd);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void free_process_memory(uint64_t cr3){
	uint64_t* pml4 = phys_to_virt((void*)cr3);
	    for(int q = 0; q < 256; q++){
        if(pml4[q] & PAGE_FLAG_PRESENT){
            uint64_t* pdpt = phys_to_virt((void*)(pml4[q] & 0xFFFFFFFFFFFFF000));
            for(int i = 0; i < 512; i++){
                if(pdpt[i] & PAGE_FLAG_PRESENT){
                    uint64_t* pd = phys_to_virt((void*)(pdpt[i] & 0xFFFFFFFFFFFFF000));
                    for(int x = 0; x < 512; x++){
                        if(pd[x] & PAGE_FLAG_PRESENT){
                            uint64_t* pt = phys_to_virt((void*)(pd[x] & 0xFFFFFFFFFFFFF000));
                            for(int y = 0; y < 512; y++){
                                if(pt[y] & 0xFFFFFFFFFFFFF000){
									free_frame((void*)(pt[y] & 0xFFFFFFFFFFFFF000));
									pt[y] = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void* allocate_new_pd(){
	void* new_pd_phys = allocate_frame();
	task_t* kernel_task = find_task(0);
	void* kernel_pd = phys_to_virt((void*)kernel_task->cr3);
	memcpy(phys_to_virt(new_pd_phys),kernel_pd,4096);
	return new_pd_phys;
}

void paging_init() {
    uint64_t cr3;
    asm volatile ("movq %%cr3, %0" : "=r" (cr3));	// get the address of the pml4 from the bootloader
	current_pml4 = phys_to_virt((void*)cr3);
	current_pml4[0] = 0;	// remove the identity mapping of the first 4gb
	switch_pml4((void*)cr3);
}