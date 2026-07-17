#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <mm.h>
#include <string.h>
#include <spinlock.h>
#include <heap.h>
#include <debug.h>

// Simple heap implementation
// some room for optimization

atomic_flag heap_lock = ATOMIC_FLAG_INIT;

#define HEAP_START_SIZE 4 * 1024 * 1024     // heap starts at 4 mb size but can grow
#define BLOCK_MAGIC 0xDEADBEEF

struct heap_block{
    struct heap_block* next;
    struct heap_block* prev;
    uint64_t size;
    uint32_t free;
    uint32_t magic;
}__attribute__((packed));

struct heap_block* heap_start;
struct heap_block* last_block;

// if the block infront is free merge it with block
void combine_free_forward(struct heap_block* block){
    if(block->next != NULL && block->next->free){
        if(block->next == last_block){
            last_block = block;
        }
        block->size+=block->next->size + sizeof(struct heap_block);
        if(block->next->next != NULL) block->next->next->prev = block;
        block->next = block->next->next;
        return;
    }
}

// if the block behind is free merge block with it
void combine_free_back(struct heap_block* block){
    if(block->prev != NULL && block->prev->free){
        block->prev->next = block->next;
        block->prev->size+= block->size + sizeof(struct heap_block);
        if(block->next != NULL){
            block->next->prev = block->prev;
        }else{
            last_block = block->prev;
        }
    }
}

void block_split(struct heap_block* block, int size){
    struct heap_block* new_block = (void*)block + size + sizeof(struct heap_block);
    new_block->next = block->next;
    new_block->prev = block;
    new_block->free = 1;
    new_block->size = block->size - size - sizeof(struct heap_block);
    new_block->magic = BLOCK_MAGIC;
    if(new_block->next == NULL) last_block = new_block;
    
    block->size = size;
    block->next = new_block;
}

void heap_expand(uint64_t size){
    void* new_mem = (void*)last_block + last_block->size + sizeof(struct heap_block);
    int page_count = ((size + 4095) / 4096);
    for(int i = 0; i < page_count; i++){
        map_page_current(new_mem + (4096 * i), allocate_frame(), PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
    }
    struct heap_block* new_block = new_mem;
    new_block->prev = last_block;
    new_block->size = (page_count * 4096) - sizeof(struct heap_block);
    new_block->free = 1;
    new_block->next = NULL;
    new_block->magic = BLOCK_MAGIC;
    last_block->next = new_block;
    last_block = new_block;
    combine_free_back(new_block);
}

void* malloc_internal(uint64_t size){
    size = ((size + 15) / 16) * 16;
    struct heap_block* cur = heap_start;
    while(1){
        if(cur->free && cur->size >= size){
            if(cur->size > size + (sizeof(struct heap_block) * 2)){
                block_split(cur,size);
            }
            cur->free = 0;
            return (void*)cur + sizeof(struct heap_block);
        }
        cur = cur->next;
        if(cur == NULL) break;
    }
    heap_expand(size + sizeof(struct heap_block));
    return malloc_internal(size);    
}

void* malloc(uint64_t size){
    spinlock_acquire(&heap_lock);

    void* ret = malloc_internal(size);

    spinlock_release(&heap_lock);
    return ret;
}

void free(void* ptr){
    if(ptr == NULL) return;
    spinlock_acquire(&heap_lock);

    struct heap_block* block = ptr - sizeof(struct heap_block);
    block->free = 1;
    combine_free_forward(block);
    combine_free_back(block);

    spinlock_release(&heap_lock);
}

void* realloc(void* ptr, uint64_t new_size){
    void* new_mem = malloc(new_size);
    if(new_mem == NULL) return NULL;
    if(ptr == NULL) return new_mem;

    struct heap_block* block = ptr - sizeof(struct heap_block);
    uint64_t size;
    if(new_size < block->size){
        size = new_size;
    }else{
        size = block->size;
    }

    memcpy(new_mem,ptr,size);
    free(ptr);
    return new_mem;
}

void* calloc(uint64_t num, uint64_t size){
    void* ret = malloc(num * size);
    memset(ret,0,num * size);
    return ret;
}

void verify_heap_integrity(){
    struct heap_block* cur = heap_start;
    while(cur != NULL){
        if(cur->magic != BLOCK_MAGIC){
            panic("Heap corruption detected!");
            return;
        }
        cur = cur->next;
    }
}

void heap_init(){
    for(int i = 0; i < HEAP_START_SIZE / 4096; i++){
        map_page_current((void*)KERNEL_HEAP_ADDR + (4096 * i),allocate_frame(), PAGE_FLAG_PRESENT | PAGE_FLAG_RW);
    }

    heap_start = (struct heap_block*)KERNEL_HEAP_ADDR;
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->magic = BLOCK_MAGIC;
    heap_start->size = HEAP_START_SIZE - sizeof(struct heap_block);
    heap_start->free = 1;
    last_block = heap_start;
}