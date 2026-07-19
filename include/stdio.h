#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

int vsprintf (char* str, char* fmt, va_list valist );
void vprintf(char* fmt, va_list valist);
int sprintf(char *str, char *fmt, ...);
void printf(char* fmt, ...);
void putchar(int c);
void puts(char*);

#endif