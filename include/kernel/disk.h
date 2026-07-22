#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#include <fs/vfs.h>

#define BYTES_PER_SECTOR 512
#define BYTES_PER_SECTOR_OPTICAL 2048

typedef void (*disk_access)(int internal_no, uint64_t lba, uint16_t sector_count, void* buffer);

enum disk_type{
    DISK_HARDDRIVE,
    DISK_REMOVABLE_FLASH,
    DISK_FLOPPY,
    DISK_RAMDISK,
    DISK_OPTICAL,
    DISK_OTHER
};

extern const char* disk_type_strings[];

struct partition{
    uint64_t lba;
    uint64_t sector_count;
    int type;

    fs_node_t* device_node;
};

struct disk{
    char disk_name[128];
    disk_access read;
    disk_access write;
    int internal_no;
    int type;
    int block_size;
    uint64_t lba_size;

    fs_node_t* device_node;

    int partition_count;
    struct partition** partitions;
};

int register_disk(struct disk disk);
void read_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer);
void write_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer);
void read_disk(int disk, uint64_t offset, uint64_t size, void* buffer);
void write_disk(int disk, uint64_t offset, uint64_t size, void* buffer);
void read_partition(int disk, int partition, uint64_t offset, uint64_t size, void* buffer);
void write_partition(int disk, int partition, uint64_t offset, uint64_t size, void* buffer);
void read_partition_lba(int disk, int partition, uint64_t lba, uint16_t sector_count, void* buffer);
void write_partition_lba(int disk, int partition, uint64_t lba, uint16_t sector_count, void* buffer);
struct disk* get_disk_info(int num);
void create_disk_devices();

#endif