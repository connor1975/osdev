#include <kernel/common.h>
#include <kernel/mm.h>
#include <kernel/multitasking.h>
#include <kernel/fs/vfs.h>
#include <kernel/gdt.h>
#include <kernel/keyboard.h>
#include <kernel/mouse.h>
#include <kernel/pci.h>
#include <kernel/acpi.h>
#include <kernel/ata.h>
#include <kernel/disk.h>
#include <kernel/ramdisk.h>
#include <kernel/ahci.h>
#include <kernel/fs/fat.h>
#include <kernel/rtl8169.h>
#include <kernel/pipe.h>
#include <kernel/tty.h>
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
    rtl8169_init();

    //devfs_init();

    //multitasking_init();
    irq_enable();

    //syscall_install();

    while(1)asm("hlt");
}