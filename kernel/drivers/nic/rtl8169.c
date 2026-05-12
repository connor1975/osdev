#include <kernel/common.h>
#include <kernel/mm.h>
#include <kernel/pci.h>
#include <kernel/net/networking.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define COMMAND_REG_OFF 0x37
#define RX_CONFIG_REG_OFF 0x44
#define TX_CONFIG_REG_OFF 0x40
#define RX_PACKET_MAX_SIZE_REG_OFF 0xDA
#define TX_DESC_START_REG_OFF 0x20
#define RX_DESC_START_REG_OFF 0xE4
#define CR93C46_OFF 0x50
#define INT_STATUS_REG_OFF 0x3E
#define ETT_REG_OFF 0xEC
#define INT_MASK_REG_OFF 0x3C

#define RTL8169_RECV 0x1
#define RTL8169_LINK_CHANGE 0x20
#define RTL8169_SENT 0x4
#define RTL8169_OWN 0x80000000
#define RTL8169_EOR 0x40000000

struct descriptor{
    uint32_t flags;
    uint32_t vlan;
    uint32_t buffer_low;
    uint32_t buffer_high;
}__attribute__((packed));

uint8_t rtl8169_mac_address[6];
uint32_t iobase;

struct descriptor* rx_descriptors;
struct descriptor* tx_descriptors;

int is_rtl8169(uint32_t vendor_id, uint32_t device_id){
    if(vendor_id == 0x10ec){
        if(device_id == 0x8161 || device_id == 0x8168 || device_id == 0x8169) return 0;
    }
    if(vendor_id == 0x1259 && device_id == 0xc107) return 0;
    if(vendor_id == 0x1737 && device_id == 0x1032) return 0;
    if(vendor_id == 0x16ec && device_id == 0x0116) return 0;

    return 1;
}

volatile int sent = 0;

void rtl8169_irq_handler(){
    uint16_t status = inw(iobase + INT_STATUS_REG_OFF);
    if(status & RTL8169_LINK_CHANGE){
        status |= RTL8169_LINK_CHANGE;
    }
    if(status & RTL8169_RECV){
        status |= RTL8169_RECV;
        for(int i = 0; i < 1024; i++){
            if(rx_descriptors[i].flags & RTL8169_OWN) continue;
            uint32_t buffer_size = rx_descriptors[i].flags & 0x3FFF;
            void* buffer = phys_to_virt((void*)((uint64_t)rx_descriptors[i].buffer_low | ((uint64_t)rx_descriptors[i].buffer_high << 32)));
            handle_packet(buffer,buffer_size);
            rx_descriptors[i].flags |= RTL8169_OWN;
        }
    }
    if(status & RTL8169_SENT){
        sent = 1;
        status |= RTL8169_SENT;
    }
    outw(iobase+INT_STATUS_REG_OFF,status);
}

void setup_descriptors(){
    rx_descriptors = phys_to_virt(allocate_frames(4));
    tx_descriptors = phys_to_virt(allocate_frames(4));
    memset(rx_descriptors,0,16384);
    memset(tx_descriptors,0,16384);

    unsigned int buffer_size = 1536;
    for(int i = 0; i < 1024; i++)   {
        if(i == (1024 - 1))
            rx_descriptors[i].flags = (RTL8169_OWN | RTL8169_EOR | (buffer_size & 0x3FFF));
        else
            rx_descriptors[i].flags = (RTL8169_OWN | (buffer_size & 0x3FFF));
        uint64_t phys = (uint64_t)allocate_frame();
        rx_descriptors[i].buffer_low = phys & 0xffffffff;
        rx_descriptors[i].buffer_high = phys >> 32;
    }
    for(int i = 0; i < 1024; i++){
        if(i == (1024 - 1))
            tx_descriptors[i].flags = (RTL8169_OWN | RTL8169_EOR | (buffer_size & 0x3FFF));
        else
            tx_descriptors[i].flags = (RTL8169_OWN | (buffer_size & 0x3FFF));
        uint64_t phys = (uint64_t)allocate_frame();
        tx_descriptors[i].buffer_low = phys & 0xffffffff;
        tx_descriptors[i].buffer_high = phys >> 32;    
    }
}

void rtl8169_send(void *packet, uint32_t packetsize) {
    uint32_t cmd = RTL8169_OWN | RTL8169_EOR | 0x40000 | 0x20000000 | 0x10000000 | (packetsize & 0x3FFF);

    struct descriptor* desc = &tx_descriptors[0];

    uint64_t phys = (uint64_t)desc->buffer_low | ((uint64_t)desc->buffer_high >> 32);
    void* virt = phys_to_virt((void*)phys);

    memcpy(virt, packet, packetsize);
    desc->vlan = 0;
    desc->flags = cmd;

    sent = 0;
    outb(iobase + 0x38, 0x40);
    while (!sent && (inb(iobase + 0x38) & 0x40));
    sent = 0;
}

uint8_t* rtl8169_get_mac(){
    return rtl8169_mac_address;
}

void rtl8169_init(){
    uint8_t bus,dev,func;
    pci_find_device(PCI_CLASS_NETWORK_CONTROLLER,PCI_SUBCLASS_NETWORK_ETHERNET,&bus,&dev,&func);
    uint32_t device_id = pci_get_device_id(bus,dev,func);
    uint32_t vendor_id = pci_get_vendor_id(bus, dev, func);

    if(is_rtl8169(vendor_id,device_id)) return;

    uint32_t bar0 = pci_config_read(bus,dev,func,0x10);
    iobase = bar0 & 0xFFFFFFFC;
    for(int i = 0; i < 6;i++){
        rtl8169_mac_address[i] = inb(iobase + i);
    }

    uint32_t reg = pci_config_read(bus,dev,func,0x3c);
    int irq = reg & 0xff;
    register_irq_handler(irq,rtl8169_irq_handler);

    outb(iobase + COMMAND_REG_OFF,0x10); // RESET bit
    while(inb(iobase + COMMAND_REG_OFF) & 0x10);
    setup_descriptors();

    outb(iobase + CR93C46_OFF,0xC0);
    outl(iobase + RX_CONFIG_REG_OFF, 0xE70F);
    outb(iobase + COMMAND_REG_OFF, 0x04);
    outl(iobase + TX_CONFIG_REG_OFF, 0x03000700);
    outw(iobase + RX_PACKET_MAX_SIZE_REG_OFF, 0x1FFF);
    outb(iobase + ETT_REG_OFF,0x3B);
    uint64_t phys_base = (uint64_t)get_physical_address(tx_descriptors,current_pml4);
    outl(iobase + TX_DESC_START_REG_OFF, phys_base & 0xffffffff);
    outl(iobase + TX_DESC_START_REG_OFF + 4, phys_base >> 32); 
    phys_base = (uint64_t)get_physical_address(rx_descriptors,current_pml4);
    outl(iobase + RX_DESC_START_REG_OFF, phys_base & 0xffffffff);
    outl(iobase + RX_DESC_START_REG_OFF + 4, phys_base >> 32);
    outw(iobase + INT_MASK_REG_OFF, 0xC1FF);
    outb(iobase + COMMAND_REG_OFF, 0x0C);
    outb(iobase + CR93C46_OFF, 0x00);
}