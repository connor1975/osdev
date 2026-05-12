#ifndef RTL8169_H
#define RTL8169_H

uint8_t* rtl8169_get_mac();
void rtl8169_init();
void rtl8169_send(void *packet, uint32_t packetsize);

#endif