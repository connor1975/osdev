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

    multitasking_init();
    irq_enable();

    init_pci_devices();

    int ramdisk = init_ramdisk(phys_to_virt(bootinfo->initrd));
    vfs_mount("/",ext2_mount_partition(ramdisk,0));
    devfs_init();

    syscall_install();

    char** argv = gen_argv("/bin/sh");
    fs_node_t* file=kopen("/bin/sh");
    spawn_elf(file,argv,NULL);
    
    while(1) asm volatile("hlt");
}