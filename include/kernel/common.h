#ifndef COMMON_H
#define COMMON_H

#include <bootloader.h>
#include <stdint.h>
#include <mm.h>
#include <screen.h>

void set_text_colour(uint32_t colour);
void clear_screen(uint32_t colour);
void terminal_init();

void panic(char* fmt, ...);

void outb(unsigned short port, unsigned char val);
uint8_t inb(uint16_t port);
void outw(unsigned short port, unsigned short val);
unsigned short inw(unsigned short port);
unsigned int inl(unsigned short port);
void outl(unsigned short port, unsigned int val);
void* malloc(uint64_t size);
void free(void*);
void* realloc(void* ptr, uint64_t new_size);
void* calloc(uint64_t num, uint64_t size);
void heap_init();

void syscall_install();
void start_shell();

#include <multitasking.h>
#include <interrupts.h>
#include <msr.h>

#endif