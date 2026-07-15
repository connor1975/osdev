#ifndef IDE_H
#define IDE_H

#include <stdint.h>

void ide_init(uint8_t bus, uint8_t dev, uint8_t func);

#endif