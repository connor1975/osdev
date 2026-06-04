#include <common.h>
#include <disk.h>
#include <stdint.h>
#include <string.h>

void* ramdisks[32];
int ramdisk_count = 0;

void ramdisk_read(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    memcpy(buffer,ramdisks[device_no] + (lba * 512),sector_count * 512);
}

void ramdisk_write(int device_no, uint64_t lba, uint16_t sector_count, void* buffer){
    memcpy(ramdisks[device_no] + (lba * 512),buffer,sector_count * 512);
}

int init_ramdisk(void* address){
    int index = ramdisk_count;
    ramdisk_count++;

    ramdisks[index] = address;
    int diskno = register_disk(index,DISK_RAMDISK,ramdisk_read,ramdisk_write, "RAMDISK" );
    return diskno;
}