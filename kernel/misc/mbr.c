#include <stdio.h>
#include <stdint.h>
#include <mbr.h>
#include <disk.h>
#include <mm.h>
#include <common.h>

int has_mbr_signature(void* bootsector){
    if(*(uint16_t*)(bootsector + 510) == 0xAA55){
        return 1;
    }else{
        return 0;
    }
}

void mbr_enumerate_partitions(int disk){
    void* bootsector = malloc(512);
    read_disk(disk,0,1,bootsector);
    if(!has_mbr_signature(bootsector)){
        free(bootsector);
        printf("Disk %d has invalid mbr signature\n", disk);
        return;
    }

    struct mbr_table_entry* partition_table = (bootsector + 0x1BE);
    for(int i = 0; i < 4; i++){
        if(partition_table[i].type != 0){
            printf("found partition of size %dMB of type %x starting at %d\n",((partition_table[i].sector_count * 512) / 1024) / 1024,partition_table[i].type, partition_table[i].lba_start);
        }
    }
    free(bootsector);
}

int get_partition_lba(int disk, int partition_no, uint32_t* lba){
    *lba = 0;
    void* bootsector = malloc(512);
    read_disk(disk,0,1,bootsector);
    if(!has_mbr_signature(bootsector)){
        free(bootsector);
        return 1;
    }

    struct mbr_table_entry* partition_table = (bootsector + 0x1BE);
    for(int i = 0; i < 4; i++){
        if(partition_table[i].type != 0){
            if(i == partition_no){
                uint32_t partition_lba = partition_table[i].lba_start;
                *lba = partition_lba;
                free(bootsector);
                return 0;
            }
        }
    }
    return 1;
}