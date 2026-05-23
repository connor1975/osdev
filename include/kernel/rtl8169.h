#ifndef RTL8169_H
#define RTL8169_H

void rtl8169_init(uint8_t bus, uint8_t dev, uint8_t func);

uint8_t* rtl8169_get_mac();
void rtl8169_send(void *packet, uint32_t packetsize);

#endif