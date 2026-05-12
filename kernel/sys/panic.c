#include <kernel/common.h>
#include <kernel/interrupts.h>
#include <kernel/multitasking.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

void panic(char* fmt, ...){
    irq_disable();
    set_text_colour(WHITE);
    printf("\nKERNEL PANIC\n");

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    while(1) asm volatile("hlt");
}