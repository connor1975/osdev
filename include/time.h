#ifndef TIME_H
#define TIME_H

#include <stdint.h>
#include <kernel/time.h>

struct timeval {
    uint64_t tv_sec;	
    uint32_t tv_usec;
};

struct timespec {
	uint64_t tv_sec;
	long	tv_nsec;
};

#endif