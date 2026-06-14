#ifndef DISK_H
#define DISK_H
#include <stdint.h>

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

struct disk{
    char disk_name[128];
    disk_access read;
    disk_access write;
    int internal_no;
    int type;
    int block_size;
};

int register_disk(int internal_no, int type, void* read, void* write, char* name);
void read_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer);
void write_disk_lba(int disk, uint64_t lba, uint16_t sector_count, void* buffer);
void read_disk(int disk, uint32_t offset, uint32_t size, void* buffer);
void write_disk(int disk, uint32_t offset, uint32_t size, void* buffer);
struct disk* get_disk_info(int num);
void create_disk_devices();

#endif