#include <disk.h>
#include <stdint.h>
#include <string.h>
#include <debug.h>

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
    struct disk disk;
    disk.internal_no = index;
    disk.read = ramdisk_read;
    disk.write = ramdisk_write;
    disk.block_size = BYTES_PER_SECTOR;
    disk.type = DISK_RAMDISK;
    disk.lba_size = 0; 
    strcpy(disk.disk_name,"RAMDISK");
    
    kprintf(KPRINTF_INFO,"creating ramdisk from address %p\n",address);
    
    int diskno = register_disk(disk);

    return diskno;
}