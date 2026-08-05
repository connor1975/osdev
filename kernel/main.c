#include <mm.h>
#include <multitasking.h>
#include <fs/vfs.h>
#include <gdt.h>
#include <keyboard.h>
#include <pci.h>
#include <acpi.h>
#include <mbr.h>
#include <ata.h>
#include <disk.h>
#include <ramdisk.h>
#include <ahci.h>
#include <fs/fat.h>
#include <fs/ext2.h>
#include <rtl8139.h>
#include <pipe.h>
#include <tty.h>
#include <net/networking.h>
#include <bootloader.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <elf.h>
#include <screen.h>
#include <heap.h>
#include <debug.h>

void syscall_install();
void floppy_init();

void fs_init(void* initrd){
    int ramdisk = init_ramdisk(initrd);
    devfs_init();
    mount_internal(ramdisk,0,"/dev/ramdisk0","/","ext2");
    task_open_stdio();
}

int run_init(char* path){
    fs_node_t* file = kopen(path);
    if(file == NULL)
        return 1;

    void* buffer = malloc(sizeof(Elf64_Ehdr));
    read_fs(file,0,sizeof(Elf64_Ehdr),buffer);
    if(verify_elf(buffer)){
        free(buffer);
        return 1;
    }
    free(buffer);
    char** argv = gen_argv(path);
    spawn_elf(file,argv,NULL);
    return 0;
}

void spawn_init(){
    if(run_init("/init") == 0)
        return;
    if(run_init("/sbin/init") == 0)
        return;
    if(run_init("/bin/init") == 0)
        return;
    panic("failed to spawn init!\n");
}

int main(bootinfo_t* bootinfo){
    irq_disable();
    pmm_init(bootinfo);
    screen_init(bootinfo);  
    acpi_init(bootinfo->rsdp);
    gdt_init();
    idt_init();
    register_isrs();
    pic_init();
    paging_init();
    heap_init();
    tty_init();
    kbd_init();
    kprintf(KPRINTF_INFO, "early initialisation finished\n");
    
    multitasking_init();
    irq_enable();

    floppy_init();
    init_pci_devices();

    fs_init(phys_to_virt(bootinfo->initrd));
    
    syscall_install();

    verify_heap_integrity();

    spawn_init();
    
    while(1) asm volatile("hlt");
}