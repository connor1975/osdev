#include <io.h>
#include <pci.h>
#include <ahci.h>
#include <ide.h>
#include <rtl8169.h>
#include <rtl8139.h>
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
            uint32_t val = pci_config_read(b,d,0,0xC);
            uint8_t header_type = (val >> 16) & 0xff;
            int function_count;
            if(header_type & (1 << 7))  // is device multifunction
                function_count = 8;
            else
                function_count = 1;

            val = pci_config_read(b,d,0,0);
            if(val != 0xffffffff){
                for(int f = 0; f < function_count; f++){
                    val = pci_config_read(b,d,f,0);
                    if(val == 0xffffffff)
                        continue;
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

uint8_t pci_get_progif(uint8_t bus, uint8_t device, uint8_t function){
    uint32_t reg = pci_config_read(bus,device,function,0x8);
    return (reg >> 8) & 0xff;
}

void pci_enable_bus_mastering(uint8_t bus, uint8_t dev,uint8_t func){
    uint32_t reg = pci_config_read(bus,dev,func,0x4);
    reg |= 0x4;
    pci_config_write(bus,dev,func,0x4,reg);
}

int pci_get_irq(uint8_t bus,uint8_t dev,uint8_t func){
    uint32_t reg = pci_config_read(bus,dev,func,0x3c);
    int irq = reg & 0xff;
    return irq;
}

pci_bar_t pci_read_bar(uint8_t bus, uint8_t dev, uint8_t func,int bar_num){
    pci_bar_t bar = {0};
    int bar_offset = 0x10 + (0x4 * bar_num);
    uint32_t value = pci_config_read(bus,dev,func,bar_offset);
    if(value & 1){ // io address        
        bar.address = value & 0xfffffffc;
        bar.type = PCI_BAR_IO;
    }else{ // memory space
        bar.type = PCI_BAR_MEMORY;
        
        int mem_type = (value >> 1) & 0x3;
        if(mem_type == 0){ // 32 bit
            bar.address = value & 0xfffffff0;
        }else if(mem_type == 2){ // 64 bit
            uint32_t upper = pci_config_read(bus,dev,func,bar_offset + 0x4);
            uint64_t address = ((uint64_t)value & 0xFFFFFFF0) | (((uint64_t)upper & 0xFFFFFFFF) << 32);
            bar.address = address;
        }else{
            bar.type = PCI_BAR_UNKNOWN;
        }
    }
    return bar;
}

void enumerate_pci(){
    for(int b = 0; b < 256; b++){
        for(int d = 0; d < 32; d++){

            uint32_t val = pci_config_read(b,d,0,0xC);
            uint8_t header_type = (val >> 16) & 0xff;
            int function_count;
            if(header_type & (1 << 7))  // is device multifunction
                function_count = 8;
            else
                function_count = 1;

            val = pci_config_read(b,d,0,0);
            if(val != 0xffffffff){

                for(int f = 0; f < function_count; f++){
                    val = pci_config_read(b,d,f,0);
                    if(val == 0xffffffff)
                        continue;
                    uint32_t val2 = pci_config_read(b,d,f,0x8);
                    uint16_t subclass = (val2 >> 16) & 0xff;
                    uint16_t class = (val2 >> 24);

                    if(class == PCI_CLASS_MASS_STORAGE && subclass == PCI_SUBCLASS_STORAGE_AHCI) ahci_init(b,d,f);
                    if(class == PCI_CLASS_MASS_STORAGE && subclass == PCI_SUBCLASS_STORAGE_IDE) ide_init(b,d,f);
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

void init_pci_devices(){
    enumerate_pci();
}