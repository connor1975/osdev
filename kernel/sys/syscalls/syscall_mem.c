#include <kernel/interrupts.h>
#include <kernel/common.h>
#include <kernel/multitasking.h>
#include <kernel/mm.h>
#include <stdint.h>

uint64_t sys_brk(uint64_t addr){
    if(addr == 0) return (uint64_t)current_task->brk;
    if(addr >= (uint64_t)current_task->brk_next_page && addr > (uint64_t)current_task->brk){
        uint64_t pagecount = ((addr - (uint64_t)current_task->brk_next_page) + 4095) / 4096;
        for(int i = 0; i <= pagecount; i++){
            map_page(current_task->brk_next_page,allocate_frame(), PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_USER,phys_to_virt((void*)current_task->cr3));
            current_task->brk_next_page += 4096;
        }
    }
    current_task->brk = (void*)addr;
    return addr;
}

uint64_t sys_mmap(void* addr, uint64_t length, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset){
    if(fd != 0) panic("mmap not fully implemnted");
    length = ((length + 4095) / 4096) * 4096;
    if(addr == NULL){
        addr = current_task->mmap_ptr;
        uint64_t pagecount = length / 4096;
        map_pages(addr,allocate_frames(pagecount),PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_USER,phys_to_virt((void*)current_task->cr3),pagecount);
        current_task->mmap_ptr+=(length);
    }else{
        uint64_t pagecount = length / 4096;
        map_pages(addr,allocate_frames(pagecount),PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_USER,phys_to_virt((void*)current_task->cr3),pagecount);
    }
    return (uint64_t)addr;
}

uint64_t sys_munmap(){
    return 0;
}