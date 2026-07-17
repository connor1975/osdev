#include <heap.h>
#include <fs/vfs.h>
#include <spinlock.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <multitasking.h>

struct pipe {
    uint64_t size;
    uint64_t current_len;
    uint64_t write_off;
    uint64_t read_off;
    uint8_t* buffer;
    int refcount;
    atomic_flag pipe_lock;
};

int pipe_read(fs_node_t* node, uint32_t offset, uint32_t size, void* buffer) {
    struct pipe* pipe = (void*)node->ptr;
    if(pipe == NULL || buffer == NULL) return -1;
    uint8_t* data = (uint8_t*)buffer;
    if(pipe->current_len == 0){
        while(pipe->current_len == 0) yield();
    }
    for(int i = 0; i < size; i++) {
        if(pipe->current_len == 0) return i;
        spinlock_acquire(&pipe->pipe_lock);
        if(buffer != NULL) data[i] = pipe->buffer[pipe->read_off];
        pipe->read_off = (pipe->read_off + 1) % pipe->size;
        pipe->current_len--;
        spinlock_release(&pipe->pipe_lock);
    }
    return size;
}

int pipe_write(fs_node_t* node, uint32_t offset, uint32_t size, void* buffer) {
    struct pipe* pipe = (void*)node->ptr;
    if(pipe == NULL || buffer == NULL) return -1;
    const uint8_t* data = (uint8_t*)buffer;
    for(int i = 0; i < size; i++) {
        if(pipe->current_len == pipe->size){
            while(pipe->current_len == pipe->size) yield();
        }
        spinlock_acquire(&pipe->pipe_lock);
        pipe->buffer[pipe->write_off] = data[i];
        pipe->write_off = (pipe->write_off + 1) % pipe->size;
        pipe->current_len++;
        spinlock_release(&pipe->pipe_lock);
    }
    return size;
}

void destroy_pipe(struct pipe* pipe) {
    if(pipe == NULL) return;
    free(pipe->buffer);
    free(pipe);
}

void pipe_close(fs_node_t* node){
    struct pipe* pipe = (void*)node->ptr;
    pipe->refcount--;
    if(pipe->refcount < 1){
        destroy_pipe(pipe);
    }
}

void pipe_open(fs_node_t* node, uint8_t read, uint8_t write){
    struct pipe* pipe = (void*)node->ptr;
    pipe->refcount++;
}

uint64_t get_pipe_free(fs_node_t* node){
    struct pipe* pipe = (void*)node->ptr;
    return pipe->size - pipe->current_len;
}

fs_node_t* create_pipe(uint64_t size) {
    struct pipe* pipe = (struct pipe*)malloc(sizeof(struct pipe));
    pipe->size = size;
    pipe->write_off = 0;
    pipe->read_off = 0;
    pipe->current_len = 0;
    pipe->refcount = 0;
    pipe->buffer = malloc(size);
    atomic_flag_clear(&pipe->pipe_lock); 

    fs_node_t* pipe_node = malloc(sizeof(fs_node_t));
    memset(pipe_node,0,sizeof(fs_node_t));
    pipe_node->close = (void*)pipe_close;
    pipe_node->open = (void*)pipe_open;
    pipe_node->write = (void*)pipe_write;
    pipe_node->read = (void*)pipe_read;
    pipe_node->mask = 0666;
    pipe_node->flags = FS_PIPE;
    pipe_node->ptr = (void*)pipe;

    return pipe_node;
}