#include <kernel/common.h>
#include <kernel/net/networking.h>
#include <kernel/rtl8169.h>
#include <string.h>
#include <stdint.h>

uint8_t broadcast_mac[] = { 255,255,255,255,255,255};
uint8_t null_mac[] = {0,0,0,0,0,0};

uint16_t switch_endian16(uint16_t nb) {
    return (nb>>8) | (nb<<8);
}
   
uint32_t switch_endian32(uint32_t nb) {
    return  ((nb>>24)&0xff)      |
            ((nb<<8)&0xff0000)   |
            ((nb>>8)&0xff00)     |
            ((nb<<24)&0xff000000);
}

void send_packet(uint8_t* target_mac, uint16_t ether_type, void* data, uint32_t size){
    uint32_t packet_length = size + sizeof(ethernet_frame_hdr);
    void* packet = malloc(packet_length);
    memcpy(packet + sizeof(ethernet_frame_hdr),data,size);

    ethernet_frame_hdr* hdr = packet;
    memcpy(hdr->source_mac,rtl8169_get_mac(),6);
    memcpy(hdr->destination_mac,target_mac,6);
    hdr->ethertype = switch_endian16(ether_type);

    rtl8169_send(packet,packet_length);
    free(packet);
}

void handle_packet(void* data, uint32_t size){
    ethernet_frame_hdr* hdr = data;

    // check if packet is for us
    if(memcmp(hdr->destination_mac,rtl8169_get_mac(),6) != 0 
    && memcmp(hdr->destination_mac,broadcast_mac,6) != 0 
    && memcmp(hdr->destination_mac,null_mac,6) != 0) return;

    // do packet stuff
}