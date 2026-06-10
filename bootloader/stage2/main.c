#include <bootloader/common.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <elf.h>
#include <bootloader.h>

void* load_kernel(void* elfdata, void** kernelend){
	Elf64_Ehdr* hdr = elfdata;
    void* kernel_end;
    Elf64_Phdr* phdr = elfdata + hdr->e_phoff;
    for(int i = 0; i < hdr->e_phnum; i++){
        if(phdr[i].p_type == 1){
			int pagecount = (phdr[i].p_memsz + 4095) / 4096;
            memset((void*)phdr[i].p_paddr,0,phdr[i].p_memsz);
			memcpy((void*)phdr[i].p_paddr, elfdata + phdr[i].p_offset, phdr[i].p_filesz);
            for (int c = 0; c <= ((phdr[i].p_memsz + 4095) / 4096); c++) {
				map_memory(phdr[i].p_vaddr + (4096 * c), phdr[i].p_paddr + (4096 * c), 0x3);
			}
            kernel_end = (void*)(phdr[i].p_paddr + phdr[i].p_memsz + 4096);
        }
    }
    *kernelend = kernel_end;
	return (void*)hdr->e_entry;
}

void* find_rsdp(){
    void* ptr = (void*)0x0E0000;
    while((uint64_t)ptr < 0x100000){
        if(memcmp(ptr,"RSD PTR ",8) == 0) return ptr;
        ptr+=16;
    }
    return NULL;
}

void* relocate(void* old, void* new, uint64_t size){
    memcpy(new,old,size);
    return new;
}

void* initrd_read_buffer;

extern void bios_read_disk(uint32_t dap_addr, uint8_t drive_num);

void* load_initrd(uint8_t drive_num){
    struct fat_dirent* file = find_file("boot/initrd");
    if(file == NULL) return NULL;
    uint32_t starting_cluster = file->cluster_low | ((uint32_t)file->cluster_high << 16);
    uint32_t start_lba = cluster_to_lba(starting_cluster);
    uint32_t sectors_to_read = (file->file_size + 511) / 512;
    void* buffer_ptr = NULL;
    struct dap dap;
    for(int i = 0; i < (sectors_to_read + 63) / 64; i++){
        dap.reserved = 0;
        dap.size = 0x10;
        dap.sector_count = 64;
        dap.buffer = linear_to_segoff((uint32_t)(uint64_t)initrd_read_buffer);
        dap.lba = start_lba + (i * 64);
        bios_read_disk((uint32_t)(uint64_t)&dap, drive_num);
        void* buffer = malloc(32768);
        if(buffer_ptr == NULL) buffer_ptr = buffer;
        memcpy(buffer,initrd_read_buffer,32768);
    }
    return buffer_ptr;
}

void kmain(uint8_t drive_num){
    fat_init(2048,drive_num);
    
    struct fat_dirent* file = find_file("boot/kernel.elf");
    void* buffer = read_file(file);

    void* kernelend;
    int(*entry)(bootinfo_t*) = load_kernel(buffer,&kernelend);

    initrd_read_buffer = malloc(65536);

    malloc_ptr = kernelend;
    setup_paging();

    printf("loading initrd...");
    void* initrd = load_initrd(drive_num);

    uint32_t mmap_entry_count;
    void* memory_map = get_memory_map(&mmap_entry_count);

    //printf("setting graphics mode");
    struct vbe_mode_info* modeinfo = vbe_init(1024,768,32);

    bootinfo_t* bootinfo = (malloc(sizeof(bootinfo_t)) + DIRECT_MAP_OFFSET);
    bootinfo->initrd = initrd;
    bootinfo->mmap = memory_map;
    bootinfo->mmap_entry_count = mmap_entry_count;
    bootinfo->rsdp = find_rsdp();
    bootinfo->framebuffer = modeinfo;
    bootinfo->reserved_mem_start = kernelend;
    bootinfo->reserved_mem_end = malloc_ptr;

    printf("%d",entry(bootinfo));

    while(1==1);
}