#include <bootloader.h>
#include <mm.h>
#include <stdio.h>
#include <string.h>
#include <spinlock.h>
#include <debug.h>

unsigned char* bitmap;
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

void pmm_init(bootinfo_t* bootinfo){
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