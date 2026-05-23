#ifndef PCI_H
#define PCI_H

#define PCI_CLASS_MASS_STORAGE          0x1
#define PCI_CLASS_NETWORK_CONTROLLER    0x2
#define PCI_CLASS_DISPLAY_CONTROLLER    0x3
#define PCI_CLASS_MULTIMEDIA_CONTROLLER 0x4
#define PCI_CLASS_MEMORY_CONTROLLER     0x5
#define PCI_CLASS_BRIDGE                0x6
#define PCI_CLASS_SIMPLE_COMMUNICATIONS 0x7
#define PCI_CLASS_SYSTEM_PERIPHERAL     0x8
#define PCI_CLASS_INPUT_CONTROLLER      0x9
#define PCI_CLASS_DOCKING_STATION       0xA
#define PCI_CLASS_PROCESSOR             0xB
#define PCI_CLASS_SERIAL_BUS_CONTROLLER 0xC
#define PCI_CLASS_WIRELESS_CONTROLLER   0xD

#define PCI_SUBCLASS_STORAGE_AHCI       0x6
#define PCI_SUBCLASS_STORAGE_IDE        0x1

#define PCI_SUBCLASS_NETWORK_ETHERNET   0x0

#include <stdint.h>

int pci_find_device(uint16_t target_class, uint16_t target_subclass, uint8_t* bus, uint8_t* device, uint8_t* func);
uint32_t pci_config_read(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg_off);
void pci_config_write(uint8_t bus, uint8_t device, uint8_t func, uint8_t reg_off,uint32_t reg);
void enumerate_pci();
void init_pci_devices();
uint32_t pci_get_device_id(uint8_t bus, uint8_t device, uint8_t function);
uint32_t pci_get_vendor_id(uint8_t bus, uint8_t device, uint8_t function);

#endif