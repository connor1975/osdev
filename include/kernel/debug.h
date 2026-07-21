#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

enum debug_level {
    LOG_QUIET = 3,
    LOG_NORMAL = 4,
    LOG_DEBUG = 5,
    LOG_TRACE = 6
};

#define DEBUG_LEVEL LOG_NORMAL
#define PRINT_LOGS 0

enum log_level{
    KPRINTF_FATAL,
    KPRINTF_CRITICAL,
    KPRINTF_ERROR,
    KPRINTF_WARNING,
    KPRINTF_INFO,
    KPRINTF_DEBUG,
    KPRINTF_TRACE
};

extern const char * log_level_names[];

void panic(char* fmt, ...);
void kprintf(int level, char* fmt, ...);
uint64_t get_ticks_since_boot();
void log_vfs_init();

#endif