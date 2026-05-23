#include <stdint.h>
#include <kernel/net/networking.h>
#include <kernel/net/nic.h>
#include <kernel/pci.h>
#include <kernel/interrupts.h>
#include <kernel/mm.h>
#include <kernel/common.h>
#include <stdio.h>
#include <string.h>

#define COMMAND_REG_OFF 0x37
#define CONFIG1_REG_OFF 0x52
#define RBSTART_REG_OFF 0x30 // Recieve buffer start
#define INTERRUPT_MASK_REG_OFF 0x3c
#define ISR_REG_OFF 0x3e
#define TCR_REG_OFF 0x40 // Transmit configuration register
#define RCR_REG_OFF 0x44 // Recieve configuration register
#define CAPR_REG_OFF 0x38

#define TRANSMIT_ERROR (1 << 3)
#define TRANSMIT_OK (1 << 2)
#define RECEIVE_OK 1
#define RECEIVE_ERROR (1 << 1)

static const uint8_t TSAD[4] = {0x20, 0x24, 0x28, 0x2c}; // Transmit start address registers
static const uint8_t TSD[4] = {0x10,0x14,0x18,0x1c};  // Transmit status registers
int tx_cur = 0;
volatile int tx_busy = 0;
static uint32_t tx_buffers[4]; // physical addresses

uint8_t rtl8139_mac_address[6];
static uint32_t iobase;

static uint32_t rx_buffer_phys;
static void* rx_buffer;
int rx_offset = 0;

void setup_tx_descriptors(){
    for(int i = 0; i < 4; i++){
        tx_buffers[i] = (uint32_t)(uint64_t)allocate_frame();
        memset(phys_to_virt((void*)(uint64_t)tx_buffers[i]),0,PAGE_SIZE);
        outl(iobase + TSAD[i],tx_buffers[i]);
    }
}

void rtl8139_send(void *frame, uint32_t size) {
    while(tx_busy);
    tx_busy = 1;

    memcpy(phys_to_virt((void*)(uint64_t)tx_buffers[tx_cur]),frame,size);
    outl(iobase + TSD[tx_cur], (0 << 16) | (size & 0x1FFF));
    tx_cur++;
    if(tx_cur == 4) tx_cur = 0;
}

void rtl8139_receive(){
    while(!(inb(iobase + COMMAND_REG_OFF) & 0x01)){ // while buffer not empty    
        uint8_t* packet = (uint8_t*)(rx_buffer + rx_offset);
        
        uint16_t packet_size = *(uint16_t*)(packet + 2) - 4; // minus CRC
        uint8_t* packet_data = packet + 4; // skip header
        
        handle_packet(packet_data, packet_size);
        
        // advance offset, must be 4 byte aligned
        rx_offset = (rx_offset + packet_size + 4 + 4 + 3) & ~3; // +4 header +4 CRC, align
        rx_offset %= 8192;
        
        outw(iobase + CAPR_REG_OFF, rx_offset - 16); // update CAPR
    }
}

void rtl8139_irq_handler(){
    uint16_t status = inw(iobase + ISR_REG_OFF);
    outw(iobase + ISR_REG_OFF, status);
    if(status & TRANSMIT_ERROR){
    }
    if(status & TRANSMIT_OK){
        tx_busy = 0;
    }
    if(status & RECEIVE_OK){
        rtl8139_receive();
    }
}

int is_rtl8139(uint32_t vendor_id, uint32_t device_id){
    if(vendor_id == 0x10ec && device_id == 0x8139) return 0;
    return 1;
}

void rtl8139_enable_bus_mastering(uint8_t bus, uint8_t dev,uint8_t func){
    uint32_t reg = pci_config_read(bus,dev,func,0x4);
    reg |= 0x4;
    pci_config_write(bus,dev,func,0x4,reg);
}

uint8_t* rtl8139_get_mac(){
    return rtl8139_mac_address;
}

void rtl8139_init(uint8_t bus, uint8_t dev, uint8_t func){
    if(is_rtl8139(pci_get_vendor_id(bus,dev,func),pci_get_device_id(bus,dev,func))) return;

    uint32_t bar0 = pci_config_read(bus,dev,func,0x10);
    iobase = bar0 & 0xFFFFFFFC;
    for(int i = 0; i < 6;i++){
        rtl8139_mac_address[i] = inb(iobase + i);
    }

    uint32_t reg = pci_config_read(bus,dev,func,0x3c);
    int irq = reg & 0xff;
    register_irq_handler(irq,rtl8139_irq_handler);

    rtl8139_enable_bus_mastering(bus,dev,func);

    outb(iobase + CONFIG1_REG_OFF, 0x0); // power on

    outb(iobase + COMMAND_REG_OFF,0x10); // RESET bit
    while(inb(iobase + COMMAND_REG_OFF) & 0x10);
    
    rx_buffer_phys = (uint32_t)(uint64_t)allocate_frames(4); // practically guaranteed to return physical memory under 4gb with the size of our os
    rx_buffer = phys_to_virt((void*)(uint64_t)rx_buffer_phys);

    outl(iobase + RBSTART_REG_OFF,rx_buffer_phys);
    outw(iobase + INTERRUPT_MASK_REG_OFF, 0x0005); // Sets the TOK and ROK bits high
    
    outl(iobase + RCR_REG_OFF, 0b1110);
    setup_tx_descriptors();

    outb(iobase + COMMAND_REG_OFF, (1 << 3) | (1 << 2)); // Reciever and transmitter enable
    register_nic(rtl8139_get_mac,rtl8139_send);
}