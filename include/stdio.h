#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

void vprintf(char* fmt, va_list valist);
int sprintf(char *str, char *fmt, ...);
void printf(char* fmt, ...);
void putchar(int c);
void puts(char*);

#endif