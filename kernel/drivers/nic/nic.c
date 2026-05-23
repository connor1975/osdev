#include <stddef.h>
#include <stdio.h>
#include <kernel/net/nic.h>
#include <kernel/common.h>

nic_t* default_nic = NULL;

void nic_send_frame(void *packet, uint32_t packetsize){
    if(default_nic == NULL)
        return;
    default_nic->nic_send_ptr(packet,packetsize);
}

uint8_t* nic_get_mac(){
    return default_nic->get_mac_ptr();
}

void register_nic(void* get_mac, void* send){
    if(default_nic != NULL){
        printf("nic already registered\n");
        return;
    }
    default_nic = malloc(sizeof(nic_t));
    default_nic->get_mac_ptr = get_mac;
    default_nic->nic_send_ptr = send;
}
