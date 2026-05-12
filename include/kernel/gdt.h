#ifndef GDT_H
#define GDT_H

struct tss{
    uint32_t reserved1;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved2;
    uint64_t ist[7];
    uint64_t reserved3;
    uint16_t reserved4;
    uint16_t iopb;
}__attribute__((packed));

typedef struct{
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t limit_high : 4;
    uint8_t flags : 4;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry;

typedef struct{
    uint16_t limit_low;
    uint16_t base_1;
    uint8_t base_2;
    uint8_t access;
    uint8_t limit_high : 4;
    uint8_t flags : 4;
    uint8_t base_3;
    uint32_t base_4;
    uint32_t reserved;
} __attribute__((packed)) system_segment_descriptor;

struct gdt{
    gdt_entry entries[5];
    system_segment_descriptor tss_entry;
} __attribute__((packed));

typedef struct{
    uint16_t limit;
    void* gdt;
} __attribute__((packed)) gdt_descriptor;

void gdt_init();

void tss_set_kernel_stack(void* rsp0);

#endif