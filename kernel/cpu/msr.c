#include <stdint.h>

uint64_t rdmsr(uint32_t msr){
    uint32_t lo;
    uint32_t hi;
    asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    uint64_t ret = (uint64_t)lo | ((uint64_t)hi << 32);
    return ret;
}

void wrmsr(uint32_t msr, uint64_t val){
    uint32_t lo = val & 0xffffffff;
    uint32_t hi = (val >> 32) & 0xffffffff;
    asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}