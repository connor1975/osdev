#include <common.h>
#include <net/networking.h>
#include <net/nic.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

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
    if(packet_length < 64) packet_length = 64;
    void* packet = malloc(packet_length);
    memset(packet,0,64);
    memcpy(packet + sizeof(ethernet_frame_hdr),data,size);

    ethernet_frame_hdr* hdr = packet;
    memcpy(hdr->source_mac,nic_get_mac(),6);
    memcpy(hdr->destination_mac,target_mac,6);
    hdr->ethertype = switch_endian16(ether_type);

    nic_send_frame(packet,packet_length); // need to add abstraction layer
    free(packet);
}

void handle_packet(void* data, uint32_t size){
    ethernet_frame_hdr* hdr = data;

    // check if packet is for us
    if(memcmp(hdr->destination_mac,nic_get_mac(),6) != 0 
    && memcmp(hdr->destination_mac,broadcast_mac,6) != 0 
    && memcmp(hdr->destination_mac,null_mac,6) != 0) return;

    // do packet stuff
}