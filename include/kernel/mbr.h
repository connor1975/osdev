#ifndef MBR_H
#define MBR_H

#include <stdint.h>
#include <disk.h>

struct mbr_table_entry{
    uint8_t attr;
    uint8_t chs_start[3];
    uint8_t type;
    uint8_t chs_end[3];
    uint32_t lba_start;
    uint32_t sector_count;
}__attribute__((packed));

void mbr_enumerate_partitions(int disk);
int mbr_get_partition_info(int disk, int partition_no, struct partition* partition_info);

#endif