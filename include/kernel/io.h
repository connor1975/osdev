#ifndef IO_H
#define IO_H

#include <stdint.h>

void outb(unsigned short port, unsigned char val);
uint8_t inb(uint16_t port);
void outw(unsigned short port, unsigned short val);
unsigned short inw(unsigned short port);
unsigned int inl(unsigned short port);
void outl(unsigned short port, unsigned int val);

#endif