#include <stdint.h>
#include <string.h>
#include <kernel/interrupts.h>

typedef struct {
	uint16_t    isr_low;
	uint16_t    selector;
	uint8_t	    ist;
	uint8_t     attributes;
	uint16_t    isr_mid;
	uint32_t    isr_high;
	uint32_t    reserved;
} __attribute__((packed)) idt_entry;

typedef struct {
	uint16_t	limit;
	uint64_t	base;
} __attribute__((packed)) idt_descriptor;

idt_entry idt[256];
idt_descriptor idtr;

void idt_set_entry(uint8_t vector, void* isr, uint8_t flags) {
	idt_entry* descriptor = &idt[vector];

	descriptor->isr_low = (uint64_t)isr & 0xFFFF;
	descriptor->selector = 0x8;
	descriptor->ist = 0;
	descriptor->attributes = flags;
	descriptor->isr_mid = ((uint64_t)isr >> 16) & 0xFFFF;
	descriptor->isr_high = ((uint64_t)isr >> 32) & 0xFFFFFFFF;
	descriptor->reserved = 0;
}

extern void load_idt(void*);

void idt_init() {
	idtr.limit = (sizeof(idt_entry) * 256) - 1;
	idtr.base = (uint64_t)&idt;
	memset((void*)&idt, 0, sizeof(idt_entry) * 256);

	load_idt(&idtr);
}