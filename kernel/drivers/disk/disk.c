#include <common.h>
#include <fs/vfs.h>
#include <disk.h>
#include <mbr.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

struct disk* disks = NULL;
int disk_count = 0;

const char* disk_type_strings[] = {
    "DISK_HARDDRIVE",
    "DISK_REMOVABLE_FLASH",
    "DISK_FLOPPY",
    "DISK_RAMDISK",
    "DISK_OPTICAL",
    "DISK_OTHER"
};

void read_disk(int disk, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].read == NULL) return;
    disks[disk].read(disks[disk].internal_no,lba,sector_count,buffer);
}

void write_disk(int disk, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].write == NULL) return;
    disks[disk].write(disks[disk].internal_no,lba,sector_count,buffer);
}

int register_disk(int internal_no, int type, void* read, void* write, char* name){
    int index = disk_count;
    disk_count++;

    disks = realloc(disks,sizeof(struct disk) * disk_count);
    disks[index].read = read;
    disks[index].write = write;
    disks[index].internal_no = internal_no;
    disks[index].type = type;
    
    strcpy(disks[index].disk_name,name);

    return index;
}

struct disk* get_disk_info(int num){
    if(num >= disk_count) return NULL;
    return &disks[num];
}