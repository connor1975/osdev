#ifndef FAT_H
#define FAT_H

#include <stdint.h>

struct fatbs_ext32{
	uint32_t	table_size_32;
	uint16_t	extended_flags;
	uint16_t	fat_version;
	uint32_t	root_cluster;
	uint16_t	fat_info;
	uint16_t	backup_BS_sector;
	uint8_t 	reserved_0[12];
	uint8_t		drive_number;
	uint8_t 	reserved_1;
	uint8_t		boot_signature;
	uint32_t 	volume_id;
	uint8_t		volume_label[11];
	uint8_t		fat_type_label[8];

}__attribute__((packed));

// fat 12 and 16 extended
struct old_fatbs_ext{
	uint8_t		bios_drive_num;
	uint8_t		reserved1;
	uint8_t		boot_signature;
	uint32_t	volume_id;
	uint8_t		volume_label[11];
	uint8_t		fat_type_label[8];
	
}__attribute__((packed));

struct fat_bootsector{
	uint8_t 	bootjmp[3];
	uint8_t 	oem_name[8];
	uint16_t 	bytes_per_sector;
	uint8_t		sectors_per_cluster;
	uint16_t	reserved_sector_count;
	uint8_t		table_count;
	uint16_t	root_entry_count;
	uint16_t	total_sectors_16;
	uint8_t		media_type;
	uint16_t	table_size_16;
	uint16_t	sectors_per_track;
	uint16_t	head_side_count;
	uint32_t 	hidden_sector_count;
	uint32_t 	total_sectors_32;
	
	uint8_t		extended_section[54];
	
}__attribute__((packed));

typedef struct fat_bootsector fat_bs_t;

struct fat_dirent{
    char        name[11];
    uint8_t     attr;
    uint8_t     nt_reserved;
    uint8_t     crt_time_tenth;
    uint16_t    crt_time;
    uint16_t    crt_date;
    uint16_t    last_access_date;
    uint16_t    cluster_high;
    uint32_t    unused_1;
    uint16_t    cluster_low;
    uint32_t    file_size;
}__attribute__((packed));

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LFN FAT_ATTR_READ_ONLY|FAT_ATTR_HIDDEN|FAT_ATTR_SYSTEM|FAT_ATTR_VOLUME_ID

char to_upper(char string);
char to_lower(char string);
char* to_fat_filename(char* filename);
void to_normal_filename(char* fat_name, char* dest);

fs_node_t* fat_mount_partition(int disk_no, int partition_lba);

struct fat_node_info{
	uint32_t parent_dir_cluster;
    uint32_t starting_cluster;
    fs_node_t** children;
    int no_child;
};

#define FAT_VERSION_32 1
#define FAT_VERSION_16 2
#define FAT_VERSION_12 3

typedef struct fat_mounted_volume{
    fat_bs_t* bootsector;
    int partition_offset;
    int disk_no;
    fs_node_t* root;
    struct fat_node_info** fileinfo;
    uint32_t next_inode;
	int fat_version;
    int fat_index;
    void* fat_buffer;
	uint32_t fat_size;
	uint32_t cluster_size;
	char volume_name[12];
} fat_mounted_volume_t;

#define FAT32_CHAIN_END 0xffffff8
#define FAT16_CHAIN_END 0xfff8
#define FAT12_CHAIN_END 0xff8

#endif