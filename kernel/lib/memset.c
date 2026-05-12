#include <stdint.h>

void memset(void *dest, uint8_t value, uint64_t length) {
    uint8_t* destptr = dest;
    for(int i = 0; i < length; i++){
        destptr[i] = value;
    }
}