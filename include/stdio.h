#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

void vprintf(char* fmt, va_list valist);
void printf(char* fmt, ...);
void putchar(int c);
void puts(char*);

#endif