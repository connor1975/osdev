#include <kernel/common.h>
#include <kernel/pci.h>
#include <kernel/ahci.h>
#include <kernel/ata.h>
#include <kernel/rtl8169.h>
#include <kernel/rtl8139.h>
#include <stdint.h>
#include <stdio.h>

uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg_off){
    uint32_t x = reg_off | (func << 8) | (device << 11) | (bus << 16) | (1 << 31);
    outl(0xCF8, x);
    return inl(0xCFC);      
}

void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg_off,uint32_t reg){
    uint32_t x = reg_off | (func << 8) | (device << 11) | (bus << 16) | (1 << 31);
    outl(0xCF8, x);
    outl(0xCFC,reg);      
}

int pci_find_device(uint16_t target_class, uint16_t target_subclass, uint8_t* bus, uint8_t* device, uint8_t* func){
    for(int b = 0; b < 256; b++){
        for(int d = 0; d < 32; d++){
            uint32_t val = pci_config_read(b,d,0,0);
            if(val != 0xffffffff){
                for(int f = 0; f < 8; f++){
                    val = pci_config_read(b,d,f,0);
                    uint32_t val2 = pci_config_read(b,d,f,0x8);
                    uint16_t subclass = (val2 >> 16) & 0xff;
                    uint16_t class = (val2 >> 24);
                    if(class == target_class && subclass == target_subclass){
                        *bus = b;
                        *device = d;
                        *func = f;
                        return 0;
                    }
                }
            }
        }
    }
    return 1;
}

uint32_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function){
    uint32_t reg = pci_config_read(bus,device,function,0) & 0xffff0000;
    reg = reg >> 16;
    return reg;
}

uint32_t pci_get_vendor_id(uint8_t bus, uint8_t device, uint8_t function){
    uint32_t reg = pci_config_read(bus,device,function,0) & 0xffff;
    return reg;
}

void enumerate_pci(){
    for(int b = 0; b < 256; b++){
        for(int d = 0; d < 32; d++){
            uint32_t val = pci_config_read(b,d,0,0);
            if(val != 0xffffffff){
                printf("Found PCI Device with vendor ID 0x%x\n",val & 0xffff);
                for(int f = 0; f < 8; f++){
                    val = pci_config_read(b,d,f,0);
                    uint32_t val2 = pci_config_read(b,d,f,0x8);
                    if(val != 0xffffffff){
                        printf("    function %d - device id 0x%x class: 0x%x subclass: 0x%x\n",f,(val >> 16) & 0xffff, (val2 >> 24), (val2 >> 16) & 0xff);
                    }
                }
            }
        }
    }
}

void init_pci_devices(){
    for(int b = 0; b < 256; b++){
        for(int d = 0; d < 32; d++){
            uint32_t val = pci_config_read(b,d,0,0);
            if(val != 0xffffffff){
                for(int f = 0; f < 8; f++){
                    val = pci_config_read(b,d,f,0);
                    uint32_t val2 = pci_config_read(b,d,f,0x8);
                    uint16_t subclass = (val2 >> 16) & 0xff;
                    uint16_t class = (val2 >> 24);

                    if(class == PCI_CLASS_MASS_STORAGE && subclass == PCI_SUBCLASS_STORAGE_AHCI) ahci_init(b,d,f);
                    // soon implement a real ide driver to replace old buggy ata pio
                    if(class == PCI_CLASS_MASS_STORAGE && subclass == PCI_SUBCLASS_STORAGE_IDE) panic("IDE is currently unsupported, please configure your virtual machine or bios to use AHCI");
                    if(class == PCI_CLASS_NETWORK_CONTROLLER && subclass == PCI_SUBCLASS_NETWORK_ETHERNET) {
                        // let the drivers themselves figure out which one they are
                        rtl8169_init(b,d,f);
                        rtl8139_init(b,d,f);
                    }
                }
            }
        }
    }
}