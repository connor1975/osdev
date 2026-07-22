#include <stdio.h>
#include <stdint.h>
#include <mbr.h>
#include <disk.h>
#include <mm.h>
#include <heap.h>

int has_mbr_signature(void* bootsector){
    if(*(uint16_t*)(bootsector + 510) == 0xAA55){
        return 1;
    }else{
        return 0;
    }
}

int mbr_get_partition_info(int disk, int partition_no, struct partition* partition_info){
    void* bootsector = malloc(BYTES_PER_SECTOR);
    read_disk_lba(disk,0,1,bootsector);
    if(!has_mbr_signature(bootsector)){
        free(bootsector);
        return 1;
    }

    struct mbr_table_entry* partition_table = (bootsector + 0x1BE);
    for(int i = 0; i < 4; i++){
        if(partition_table[i].type != 0){
            if(i == partition_no){
                partition_info->type = partition_table[i].type;
                partition_info->lba = (uint64_t)partition_table[i].lba_start;
                partition_info->sector_count = (uint64_t)partition_table[i].sector_count;
                free(bootsector);
                return 0;
            }
        }
    }
    return 1;
}