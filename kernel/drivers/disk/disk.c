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

void read_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].read == NULL) return;
    disks[disk].read(disks[disk].internal_no,lba,sector_count,buffer);
}

void write_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].write == NULL) return;
    disks[disk].write(disks[disk].internal_no,lba,sector_count,buffer);
}

void read_disk(int disk, uint32_t offset, uint32_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    int lba_off = offset / bytes_per_sector;
    int byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_disk_lba(disk,lba_off,sector_count,sector_buffer);
    memcpy(buffer,sector_buffer + byte_off,size);
    free(sector_buffer);
}

void write_disk(int disk, uint32_t offset, uint32_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    int lba_off = offset / bytes_per_sector;
    int byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_disk_lba(disk,lba_off,sector_count,sector_buffer);
    memcpy(sector_buffer + byte_off,buffer,size);
    write_disk_lba(disk,lba_off,sector_count,sector_buffer);
    free(sector_buffer);
}

uint32_t read_disk_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    read_disk(node->impl,offset,size,buffer);
    return size;
}

uint32_t write_disk_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    write_disk(node->impl,offset,size,buffer);
    return size;
}

int register_disk(int internal_no, int type, void* read, void* write, char* name){
    int index = disk_count;
    disk_count++;

    disks = realloc(disks,sizeof(struct disk) * disk_count);
    disks[index].read = read;
    disks[index].write = write;
    disks[index].internal_no = internal_no;
    disks[index].type = type;

    if(disks[index].type == DISK_OPTICAL)
        disks[index].block_size = BYTES_PER_SECTOR_OPTICAL;
    else
        disks[index].block_size = BYTES_PER_SECTOR;
        
    strcpy(disks[index].disk_name,name);

    return index;
}

fs_node_t* generate_disk_device(int disk){
    static int optical_count = 0;
    static int hdd_count = 0;
    static int ramdisk_count = 0;

    struct disk* disk_info = &disks[disk];
    
    fs_node_t* node = calloc(1,sizeof(fs_node_t));

    if(disk_info->type == DISK_OPTICAL){
        sprintf(node->name,"cdrom%d",optical_count++);        
    }else if(disk_info->type == DISK_RAMDISK){
        sprintf(node->name,"ramdisk%d",ramdisk_count++);
    }else{
        sprintf(node->name,"disk%d",hdd_count++);
    }

    node->flags = FS_BLOCKDEVICE;
    node->read = &read_disk_fs;
    node->write = &write_disk_fs;
    node->impl = disk;
    node->mask = 0666;

    return node;
}

void create_disk_devices(){
    for(int i = 0; i < disk_count; i++){
        dev_add_node(generate_disk_device(i));
    }
}

struct disk* get_disk_info(int num){
    if(num >= disk_count) return NULL;
    return &disks[num];
}