#include <heap.h>
#include <fs/vfs.h>
#include <disk.h>
#include <mbr.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <debug.h>

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

void read_partition_lba(int disk, int partition, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].read == NULL) return;
    uint64_t partition_offset;
    if(disks[disk].partition_count == 0)
        partition_offset = 0;
    else
        partition_offset = disks[disk].partitions[partition]->lba;

    disks[disk].read(disks[disk].internal_no,lba + partition_offset,sector_count,buffer);
}

void write_partition_lba(int disk, int partition, uint64_t lba, uint16_t sector_count, void* buffer){
    if(disk >= disk_count) return;
    if(disks[disk].write == NULL) return;
    uint64_t partition_offset;
    if(disks[disk].partition_count == 0)
        partition_offset = 0;
    else
        partition_offset = disks[disk].partitions[partition]->lba;

    disks[disk].write(disks[disk].internal_no,lba + partition_offset,sector_count,buffer);
}

void read_disk(int disk, uint64_t offset, uint64_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    uint64_t lba_off = offset / bytes_per_sector;
    uint64_t byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_disk_lba(disk,lba_off,sector_count,sector_buffer);
    memcpy(buffer,sector_buffer + byte_off,size);
    free(sector_buffer);
}

void write_disk(int disk, uint64_t offset, uint64_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    uint64_t lba_off = offset / bytes_per_sector;
    uint64_t byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_disk_lba(disk,lba_off,sector_count,sector_buffer);
    memcpy(sector_buffer + byte_off,buffer,size);
    write_disk_lba(disk,lba_off,sector_count,sector_buffer);
    free(sector_buffer);
}

void read_partition(int disk, int partition, uint64_t offset, uint64_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    uint64_t lba_off = offset / bytes_per_sector;
    uint64_t byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_partition_lba(disk,partition,lba_off,sector_count,sector_buffer);
    memcpy(buffer,sector_buffer + byte_off,size);
    free(sector_buffer);
}

void write_partition(int disk, int partition, uint64_t offset, uint64_t size, void* buffer){
    uint32_t bytes_per_sector = disks[disk].block_size;
    uint64_t lba_off = offset / bytes_per_sector;
    uint64_t byte_off = offset % bytes_per_sector;
    int sector_count = ((byte_off + size) + (bytes_per_sector - 1)) / bytes_per_sector;
    void* sector_buffer = malloc(bytes_per_sector * sector_count);
    read_partition_lba(disk,partition,lba_off,sector_count,sector_buffer);
    memcpy(sector_buffer + byte_off,buffer,size);
    write_partition_lba(disk,partition,lba_off,sector_count,sector_buffer);
    free(sector_buffer);
}

uint64_t read_disk_fs(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    read_disk(node->impl,offset,size,buffer);
    return size;
}

uint64_t write_disk_fs(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    write_disk(node->impl,offset,size,buffer);
    return size;
}

uint64_t read_partition_fs(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    uint32_t disk = node->impl & 0xffffffff;
    uint32_t partition = (node->impl >> 32) & 0xffffffff;
    read_partition(disk,partition,offset,size,buffer);
    return size;
}

uint64_t write_partition_fs(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    uint32_t disk = node->impl & 0xffffffff;
    uint32_t partition = (node->impl >> 32) & 0xffffffff;
    write_partition(disk,partition,offset,size,buffer);
    return size;
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

fs_node_t* generate_partition_device(int disk_no, int partition){    
    fs_node_t* node = calloc(1,sizeof(fs_node_t));

    struct disk* disk = &disks[disk_no];

    sprintf(node->name,"%sp%d",disk->device_node->name,partition);

    node->flags = FS_BLOCKDEVICE;
    node->read = &read_partition_fs;
    node->write = &write_partition_fs;
    node->impl = (uint64_t)disk_no | ((uint64_t)partition << 32);
    node->mask = 0666;

    return node;
}

void disk_enumerate_partitions(int index){
    struct disk* disk = &disks[index];
    for(int i = 0; i < 4; i++){
        struct partition partition_info;
        if(!mbr_get_partition_info(index,i,&partition_info)){
            disk->partition_count++;
            disk->partitions = realloc(disk->partitions,sizeof(struct partition*) * disk->partition_count);
            disk->partitions[disk->partition_count - 1] = malloc(sizeof(struct partition));
            memcpy(disk->partitions[disk->partition_count - 1],&partition_info,sizeof(struct partition));

            disk->partitions[disk->partition_count - 1]->device_node = generate_partition_device(index,disk->partition_count - 1);
        }
    }
    kprintf(KPRINTF_INFO,"disk manager detected %d partitions on disk %d\n",disk->partition_count,index);
}

int register_disk(struct disk disk){
    int index = disk_count;
    disk_count++;

    disks = realloc(disks,sizeof(struct disk) * disk_count);

    memcpy(&disks[index],&disk,sizeof(struct disk));

    kprintf(KPRINTF_INFO,"disk manager registering disk %d of type %s\n",disk_count - 1, disk_type_strings[disk.type]);

    disks[index].device_node = generate_disk_device(index);
    disks[index].partition_count = 0;
    disks[index].partitions = NULL;

    if(disk.type == DISK_HARDDRIVE)
        disk_enumerate_partitions(index);
    
    return index;
}

void create_disk_devices(){
    for(int i = 0; i < disk_count; i++){
        dev_add_node(disks[i].device_node);
        if(disks[i].partition_count != 0){
            for(int j = 0; j < disks[i].partition_count; j++){
                dev_add_node(disks[i].partitions[j]->device_node);
            }
        }
    }
}

struct disk* get_disk_info(int num){
    if(num >= disk_count) return NULL;
    return &disks[num];
}