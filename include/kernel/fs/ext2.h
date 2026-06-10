#ifndef EXT2_H
#define EXT2_H

#include <stdint.h>

#define INDIRECT_BLOCK_OFFSET 12
#define DOUBLY_INDIRECT_BLOCK_OFFSET 13
#define TRIPLY_INDIRECT_BLOCK_OFFSET 14

#define INODE_SIZE 128

typedef struct{
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;

    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    uint8_t volume_name[16];
    uint8_t last_mounted[64];
    uint32_t algo_bitmap;

    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
} __attribute__((packed)) ext2_superblock_t;

typedef struct{
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t reserved[12];
} __attribute__((packed)) block_group_descriptor_t;

typedef struct{
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
} __attribute__((packed)) ext2_inode_t;

typedef struct{
    uint32_t inode;
    uint16_t entry_size;
    uint8_t name_length;
    uint8_t type;
    char name[];
} __attribute((packed)) ext2_dirent_t;

fs_node_t* ext2_mount_partition(int disk_no, int partition_lba);
fs_node_t* ext2_finddir(fs_node_t *node, char *name);

#endif