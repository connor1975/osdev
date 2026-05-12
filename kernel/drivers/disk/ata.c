#include <kernel/common.h>
#include <kernel/ata.h>
#include <kernel/disk.h>
#include <stdio.h>

#define ATA_DATA_REG 0
#define ATA_ERROR_REG 1
#define ATA_FEATURES_REG 1
#define ATA_SECCOUNT_REG 2
#define ATA_LBA_LOW_REG 3
#define ATA_LBA_MID_REG 4
#define ATA_LBA_HIGH_REG 5
#define ATA_DRIVE_REG 6
#define ATA_STATUS_REG 7
#define ATA_COMMAND_REG 7

#define PRIMARY_IO_BASE     0x1F0
#define SECONDARY_IO_BASE   0x170

void ata_poll(int secondary){
    int io_base = PRIMARY_IO_BASE;
    if(secondary) io_base = SECONDARY_IO_BASE;
    while((inb(io_base + ATA_STATUS_REG) & 0x80) && (!(inb(io_base + ATA_STATUS_REG) & 8)));
}

void ata_pio28_read(int secondary, int slave, uint32_t lba, uint16_t sector_count,void* buffer){
    int io_base = PRIMARY_IO_BASE;
    if(secondary) io_base = SECONDARY_IO_BASE;
    outb(io_base + ATA_DRIVE_REG, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
    outb(io_base + sector_count, (unsigned char)sector_count);
    outb(io_base + ATA_LBA_LOW_REG, (uint8_t)lba);
    outb(io_base + ATA_LBA_MID_REG, (uint8_t)(lba >> 8));
    outb(io_base + ATA_LBA_HIGH_REG, (uint8_t)(lba >> 16));
    outb(io_base + ATA_COMMAND_REG, READ_SECTORS_PIO);
    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        ata_poll(secondary);
        for(int x = 0; x < 256; x++){
            bufferptr[x] = inw(io_base);
        }
        bufferptr+=256;
    }
}

void ata_pio28_write(int secondary, int slave, uint32_t lba, uint16_t sector_count,void* buffer){
    int io_base = PRIMARY_IO_BASE;
    if(secondary) io_base = SECONDARY_IO_BASE;
    outb(io_base + ATA_DRIVE_REG, 0xE0 | (slave << 4) | ((lba >> 24) & 0x0F));
    outb(io_base + sector_count, (unsigned char)sector_count);
    outb(io_base + ATA_LBA_LOW_REG, (uint8_t)lba);
    outb(io_base + ATA_LBA_MID_REG, (uint8_t)(lba >> 8));
    outb(io_base + ATA_LBA_HIGH_REG, (uint8_t)(lba >> 16));
    outb(io_base + ATA_COMMAND_REG, WRITE_SECTORS_PIO);
    uint16_t* bufferptr = buffer;
    for(int i = 0; i < sector_count; i++){
        ata_poll(secondary);
        for(int x = 0; x < 256; x++){
            outw(io_base,bufferptr[x]);
        }
        bufferptr+=256;
    }
}

int ata_identify(int secondary, int slave, void* buffer){ 
    if(buffer == NULL) return -1;

    int io_base = PRIMARY_IO_BASE;
    if(secondary) io_base = SECONDARY_IO_BASE;
    outb(io_base + ATA_DRIVE_REG, 0xA0 | (slave << 4));
    outb(io_base + ATA_SECCOUNT_REG, 0);
    outb(io_base + ATA_LBA_LOW_REG, 0);
    outb(io_base + ATA_LBA_MID_REG, 0);
    outb(io_base + ATA_LBA_HIGH_REG, 0);
    outb(io_base + ATA_COMMAND_REG, IDENTIFY);
    if(inb(io_base + ATA_STATUS_REG) == 0) return 1;
    uint16_t* bufferptr = buffer;
    ata_poll(secondary);
    for(int x = 0; x < 256; x++){
        bufferptr[x] = inw(io_base);
    }
    bufferptr+=256;
    return 0;
}