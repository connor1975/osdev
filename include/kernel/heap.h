#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

void* malloc(uint64_t size);
void free(void*);
void* realloc(void* ptr, uint64_t new_size);
void* calloc(uint64_t num, uint64_t size);
void heap_init();

#endif