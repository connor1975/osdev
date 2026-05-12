#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>

typedef struct rsdp{
    char signature[8];
    unsigned char checksum;
    char oem_id[6];
    unsigned char revision;
    uint32_t rsdt_addr;
    
    uint32_t length;
    uint64_t xsdt_addr;
    uint8_t ext_checksum;
    uint8_t reserved[8];
} __attribute__((packed)) rsdp_t;

typedef struct acpi_table_header{
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_table_header_t;

void acpi_init(void* rsdp_phys);
void* find_acpi_table(char* signature);

#endif