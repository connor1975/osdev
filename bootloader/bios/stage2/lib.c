#include <stdint.h>
#include <stddef.h>

int memcmp(const void* aptr, const void* bptr, unsigned int size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (unsigned int i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

void memcpy(void* dest, void* src, uint64_t size){
    uint8_t* destptr = dest;
    uint8_t* srcptr = src;
    for(uint64_t i = 0; i < size; i++){
        destptr[i] = srcptr[i];
    }
}

void* memmove(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	if (dst < src) {
		for (size_t i = 0; i < size; i++)
			dst[i] = src[i];
	} else {
		for (size_t i = size; i != 0; i--)
			dst[i-1] = src[i-1];
	}
	return dstptr;
}

void memset(void *dest, uint8_t value, uint64_t length) {
    uint8_t* destptr = dest;
    for(int i = 0; i < length; i++){
        destptr[i] = value;
    }
}

int strlen(const char* str){
    int i = 0;
    while(*str != 0){
        i++;
        str++;
    }
    return i;
}