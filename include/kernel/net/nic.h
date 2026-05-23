#ifndef NIC_H
#define NIC_H

#include <stdint.h>
#include <kernel/net/networking.h>

typedef uint8_t* (*nic_get_mac_func)();
typedef void (*nic_send_func)(void *packet, uint32_t packetsize);

typedef struct nic{
    nic_get_mac_func get_mac_ptr;
    nic_send_func nic_send_ptr;
}nic_t;

uint8_t* nic_get_mac();
void nic_send_frame(void *packet, uint32_t packetsize);
void register_nic(void* get_mac, void* send);

#endif