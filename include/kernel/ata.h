#ifndef ATA_H
#define ATA_H

#define READ_DMA_EXT 0x25
#define WRITE_DMA_EXT 0x35
#define READ_SECTORS_PIO 0x20
#define WRITE_SECTORS_PIO 0x30
#define READ_SECTORS_PIO_EXT 0x24
#define WRITE_SECTORS_PIO_EXT 0x34
#define IDENTIFY 0xEC
#define IDENTIFY_PACKET_DEVICE 0xA1
#define ATA_PACKET 0xA0
#define ATAPI_READ 0xA8

#include <stdint.h>

struct ata_identity {
        uint16_t ignore_a[27]; /* words 0-26 */
        uint16_t model[20]; /* words 27-46 */
        uint16_t ignore_b[13]; /* words 47-59 */
        uint32_t lba_sectors; /* words 60-61 */
        uint16_t ignore_c[21]; /* words 62-82 */
        uint16_t supports_lba48; /* word 83 */
        uint16_t ignore_d[16]; /* words 84-99 */
        uint64_t lba48_sectors; /* words 100-103 */
        uint16_t ignore_e[152]; /* words 104-255 */
};

void ata_pio28_read(int secondary, int slave, uint32_t lba, uint16_t sector_count,void* buffer);
void ata_pio28_write(int secondary, int slave, uint32_t lba, uint16_t sector_count,void* buffer);
int ata_identify(int secondary, int slave, void* buffer);
void ata_init(uint8_t bus, uint8_t dev, uint8_t func);
void ata_string_convert(char* str);

#endif