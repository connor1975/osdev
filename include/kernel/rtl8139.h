#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

void rtl8139_init(uint8_t bus, uint8_t dev, uint8_t func);
void rtl8139_send(void *frame, uint32_t size);
uint8_t* rtl8139_get_mac();

#endif