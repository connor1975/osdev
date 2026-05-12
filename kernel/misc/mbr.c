#include <stdio.h>
#include <stdint.h>
#include <kernel/mbr.h>
#include <kernel/disk.h>
#include <kernel/mm.h>
#include <kernel/common.h>

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
            printf("found partition of size %dMB of type %x\n",((partition_table[i].sector_count * 512) / 1024) / 1024,partition_table[i].type);
        }
    }
    free(bootsector);
}

int get_partition_lba(int disk, int partition_no, uint32_t* lba){
    void* bootsector = malloc(512);
    read_disk(disk,0,1,bootsector);
    if(!has_mbr_signature(bootsector)){
        free(bootsector);
        return -1; // Disk is not mbr formatted
    }

    struct mbr_table_entry* partition_table = (bootsector + 0x1BE);
    for(int i = 0; i < 4; i++){
        if(partition_table[i].type != 0 && i == partition_no){
            *lba = partition_table[i].lba_start;
            free(bootsector);
            return 0;
        }
    }
    return 1; // Cannot find  partition
}