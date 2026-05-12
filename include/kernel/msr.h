#ifndef MSR_H
#define MSR_H

#include <stdint.h>

#define IA32_EFER  0xC0000080
#define MSR_FSBASE 0xC0000100

uint64_t rdmsr(uint32_t msr);
void wrmsr(uint32_t msr, uint64_t val);

#endif