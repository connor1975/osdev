#include <string.h>
#include <stdint.h>
#include <kernel/gdt.h>
#include <kernel/common.h>

struct gdt gdt;
gdt_descriptor gdtr;
struct tss* tss;

void gdt_set_entry(uint8_t entry, uint64_t base, uint64_t limit, uint8_t access, unsigned char flags){
    gdt.entries[entry].flags = flags & 0xF;
    gdt.entries[entry].access = access;
    gdt.entries[entry].limit_low = limit & 0xFFFF;
    gdt.entries[entry].limit_high = (limit >> 16) & 0xf;
    gdt.entries[entry].base_low = base & 0xffff;
    gdt.entries[entry].base_mid = (base >> 16) & 0xff;
    gdt.entries[entry].base_high = (base >> 24) & 0xff;
}

extern void load_gdt(void*);
extern void flush_tss();
extern char boot_stack_top;

void tss_set_kernel_stack(void* rsp0){
    tss->rsp0 = (uint64_t)rsp0;
}

void gdt_init(){
    uint16_t gdt_size = sizeof(gdt_entry) * 7;
    gdtr.limit = gdt_size - 1;
    gdtr.gdt = &gdt;

    gdt_set_entry(0,0,0,0,0);                       // NULL segment
    gdt_set_entry(1,0,0xFFFFF,0b10011010,0b1010);   // Kernel code segment
    gdt_set_entry(2,0,0xFFFFF,0b10010010,0b1100);   // Kernel data segment
    gdt_set_entry(3,0,0xFFFFF,0b11111010,0b1010);   // User code segment
    gdt_set_entry(4,0,0xFFFFF,0b11110010,0b1100);   // User data segment

    tss = phys_to_virt(allocate_frame());
    memset((void*)tss,0,4096);
    tss->rsp0 = (uint64_t)&boot_stack_top;
    tss->iopb = sizeof(struct tss);
    gdt.tss_entry.base_1 = (uint64_t)tss & 0xffff;
    gdt.tss_entry.base_2 = ((uint64_t)tss >> 16) & 0xff;
    gdt.tss_entry.base_3 = ((uint64_t)tss >> 24) & 0xff;
    gdt.tss_entry.base_4 = ((uint64_t)tss >> 32) & 0xffffffff;
    gdt.tss_entry.flags = 0;
    gdt.tss_entry.limit_high = 0;
    gdt.tss_entry.limit_low = sizeof(struct tss) - 1;
    gdt.tss_entry.access = 0x89;    // present | system segment descriptor | tss

    load_gdt(&gdtr);
    flush_tss();
}