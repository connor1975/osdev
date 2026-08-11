#include <interrupts.h>
#include <mm.h>
#include <multitasking.h>
#include <mm.h>
#include <string.h>
#include <stdint.h>
#include <debug.h>
#include <errno.h>

uint64_t sys_brk(uint64_t addr){
    if(addr == 0) 
        return (uint64_t)current_task->brk;
    
    if(addr >= (uint64_t)current_task->brk_next_page && addr > (uint64_t)current_task->brk){
        uint64_t pagecount = ((addr - (uint64_t)current_task->brk_next_page) + (PAGE_SIZE - 1)) / PAGE_SIZE;
        for(int i = 0; i <= pagecount; i++){
            void* phys = allocate_frame();
            memset(phys_to_virt(phys),0,PAGE_SIZE);
            map_page(current_task->brk_next_page,phys, PAGE_FLAG_PRESENT | PAGE_FLAG_RW | PAGE_FLAG_USER, phys_to_virt((void*)current_task->cr3));
            current_task->brk_next_page += PAGE_SIZE;
        }
    }
    
    current_task->brk = (void*)addr;
    return addr;
}