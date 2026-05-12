#include <stdint.h>

void memcpy(void* dest, void* src, uint64_t size){
    uint8_t* destptr = dest;
    uint8_t* srcptr = src;
    for(uint64_t i = 0; i < size; i++){
        destptr[i] = srcptr[i];
    }
}