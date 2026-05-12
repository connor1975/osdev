#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

#define ETHERTYPE_IPV4  0x800
#define ETHERTYPE_IPV6  0x86dd
#define ETHERTYPE_ARP   0x806

typedef struct ethernet_frame_hdr {
  uint8_t  destination_mac[6];
  uint8_t  source_mac[6];
  uint16_t ethertype;
} ethernet_frame_hdr;

uint16_t switch_endian16(uint16_t nb);
uint32_t switch_endian32(uint32_t nb);
void handle_packet(void* data, uint32_t size);
void send_packet(uint8_t* target_mac, uint16_t ether_type, void* data, uint32_t size);

#endif