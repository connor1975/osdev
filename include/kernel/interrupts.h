#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void idt_init();
void register_isrs();
void pic_init();
void idt_set_entry(uint8_t vector, void* isr, uint8_t flags);
void pic_send_eoi(int irq);
void register_isr_handler(unsigned char num,void* handler);
void register_irq_handler(unsigned char num,void* handler);

static inline void irq_disable(){
	asm volatile("cli");
}

static inline void irq_enable(){
	asm volatile("sti");
}

struct interrupt_frame{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t isr_number;
    uint64_t err_code;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void print_regs(struct interrupt_frame* regs) ;

typedef void (*isr_t)(struct interrupt_frame*);

#endif