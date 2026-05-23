#include <kernel/common.h>
#include <kernel/mm.h>
#include <kernel/multitasking.h>
#include <kernel/fs/vfs.h>
#include <kernel/gdt.h>
#include <kernel/keyboard.h>
#include <kernel/pci.h>
#include <kernel/acpi.h>
#include <kernel/mbr.h>
#include <kernel/ata.h>
#include <kernel/disk.h>
#include <kernel/ramdisk.h>
#include <kernel/ahci.h>
#include <kernel/fs/fat.h>
#include <kernel/rtl8139.h>
#include <kernel/pipe.h>
#include <kernel/tty.h>
#include <kernel/net/networking.h>
#include <bootloader.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include <elf.h>

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
    init_pci_devices();

    uint32_t lba = 0;
    get_partition_lba(0,0,&lba);
    vfs_mount("/",fat_mount_partition(0,lba));
    devfs_init();

    multitasking_init();
    irq_enable();

    syscall_install();

    char** argv = gen_argv("/bin/sh");
    fs_node_t* file=kopen("/bin/sh");
    int pid = spawn_elf(file,argv,NULL);

    free(argv);
    while(get_task_state(pid) != TASK_DEAD);
    
    while(1) asm volatile("hlt");
}