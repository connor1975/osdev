#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

int memcmp(const void* aptr, const void* bptr, unsigned int size);
void memcpy(void* dest, void* src, uint64_t size);
void memset(void* dest, uint8_t value, uint64_t length);
int strlen(const char* str);
int strcmp(const char *s1, const char *s2);
char* strcpy(char* destination, const char* source);
char* strchr(const char *str, int c);
char* strtok_r(char *s, char *delim, char **saveptr);
char *strcat(char *destination, const char *source);
void* memmove(void* dstptr, const void* srcptr, size_t size);

#include <stddef.h>

#endif