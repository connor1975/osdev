#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <common.h>
#include <acpi.h>

void* rsdt;
void* xsdt;

void* find_acpi_table(char* signature){
    if(rsdt != NULL){
        acpi_table_header_t* hdr = rsdt;
        uint64_t entry_count = (hdr->length - sizeof(acpi_table_header_t)) / 4;
        uint32_t* entries = (rsdt + sizeof(acpi_table_header_t));
        for(uint64_t i = 0; i < entry_count; i++){
            void* table = phys_to_virt((void*)(uint64_t)entries[i]);
            if(memcmp(table,signature,4) == 0) return table;
        }
    }else{
        acpi_table_header_t* hdr = xsdt;
        uint64_t entry_count = (hdr->length - sizeof(acpi_table_header_t)) / 8;
        uint64_t* entries = (xsdt + sizeof(acpi_table_header_t));
        for(uint64_t i = 0; i < entry_count; i++){
            void* table = phys_to_virt((void*)(uint64_t)entries[i]);
            if(memcmp(table,signature,4) == 0) return table;
        }
    }
    return NULL;
}

void acpi_init(void* rsdp_phys){
    rsdp_t* rsdp = phys_to_virt(rsdp_phys);
    if(rsdp->revision == 0){
        rsdt = phys_to_virt((void*)(uint64_t)rsdp->rsdt_addr);
        xsdt = NULL;
    } else{
        xsdt = phys_to_virt((void*)rsdp->xsdt_addr);
        rsdt = NULL;
    }
}