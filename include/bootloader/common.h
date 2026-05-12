#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define BOOTLOADER

#define DIRECT_MAP_OFFSET 0xffff888000000000

uint32_t linear_to_segoff(uint32_t addr);
void* malloc(uint64_t size);
void free(uint64_t size);

#include <vbe.h>
struct vbe_mode_info* vbe_init(int target_width, int target_height, int target_bpp);

void fat_init(uint32_t partition_lba, uint8_t drive_num);
void read_sectors(uint32_t lba, uint16_t sector_count, void* buffer,uint8_t drive_num);
struct fat_dirent* find_file(char* path);
void* read_file(struct fat_dirent* file);
void setup_paging();
void map_memory(uint64_t virt, uint64_t phys, uint32_t flags);
int get_memory_map_entry_count();
void* get_memory_map(uint32_t* entry_count);
void* segoffset_to_linear(uint32_t addr);
unsigned int cluster_to_lba(unsigned int cluster);

extern void* malloc_ptr;

struct fat_bootsector{
	unsigned char 		bootjmp[3];
	unsigned char 		oem_name[8];
	unsigned short 	    bytes_per_sector;
	unsigned char		sectors_per_cluster;
	unsigned short		reserved_sector_count;
	unsigned char		table_count;
	unsigned short		root_entry_count;
	unsigned short		total_sectors_16;
	unsigned char		media_type;
	unsigned short		table_size_16;
	unsigned short		sectors_per_track;
	unsigned short		head_side_count;
	unsigned int 		hidden_sector_count;
	unsigned int 		total_sectors_32;
 	unsigned int		table_size_32;
	unsigned short		extended_flags;
	unsigned short		fat_version;
	unsigned int		root_cluster;
	unsigned short		fat_info;
	unsigned short		backup_BS_sector;
	unsigned char 		reserved_0[12];
	unsigned char		drive_number;
	unsigned char 		reserved_1;
	unsigned char		boot_signature;
	unsigned int 		volume_id;
	unsigned char		volume_label[11];
	unsigned char		fat_type_label[8];
}__attribute__((packed));

typedef struct fat_bootsector fat_bs_t;

struct fat_dirent{
    char name[11];
    unsigned char attr;
    unsigned char unused[8];    // this stuff is useless to us in the bootloader
    unsigned short cluster_high;
    unsigned int unused_1;
    unsigned short cluster_low;
    unsigned int file_size;
}__attribute__((packed));

struct dap{
    uint8_t size;
    uint8_t reserved;
    uint16_t sector_count;
    uint32_t buffer;
    uint64_t lba;
}__attribute__((packed));

#endif