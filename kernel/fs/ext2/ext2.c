#include <stdint.h>
#include <string.h>
#include <disk.h>
#include <fs/vfs.h>
#include <fs/ext2.h>
#include <common.h>
#include <stdio.h>

// currently experimental

struct ext2_node_cache_entry {
    uint32_t inode;
    fs_node_t* node;
    ext2_inode_t* inode_data;
    struct ext2_node_cache_entry* next;
};

struct ext2_volume{
    fs_node_t* root_node;
    int disk_no;
    uint32_t partition_offset;
    ext2_superblock_t superblock;
    uint32_t block_group_count;
    uint32_t sectors_per_block;
    uint32_t block_size;

    block_group_descriptor_t* bgdt;
    struct ext2_node_cache_entry* node_cache;
};

struct ext2_node_cache_entry* ext2_find_cached_node(struct ext2_volume* volume, int inode){
    struct ext2_node_cache_entry* ptr = volume->node_cache;
    while(ptr != NULL){
        if(ptr->inode == inode){
            return ptr;
        }
        ptr = ptr->next;
    }
    return NULL;
}

void ext2_node_cache_append(struct ext2_volume* volume, fs_node_t* node, ext2_inode_t* inode){
    struct ext2_node_cache_entry* ptr = volume->node_cache;
    while(ptr->next != NULL){
        ptr = ptr->next;
    }
    
    struct ext2_node_cache_entry* new_entry = malloc(sizeof(struct ext2_node_cache_entry));
    new_entry->inode = node->inode;
    new_entry->next = NULL;
    new_entry->node = node;
    new_entry->inode_data = inode;
    ptr->next = new_entry;
}

void ext2_volume_read_sectors(struct ext2_volume* volume, int lba, int sector_count, void* buffer){
    read_disk_lba(volume->disk_no,lba + volume->partition_offset,sector_count,buffer);
}

void ext2_volume_read(struct ext2_volume* volume, int offset, int size, void* buffer){
    read_disk(volume->disk_no, offset,size,buffer);
}

void ext2_volume_read_block(struct ext2_volume* volume, int block, void* buffer){
    read_disk_lba(volume->disk_no,(block * (volume->block_size / BYTES_PER_SECTOR)) + volume->partition_offset,volume->sectors_per_block,buffer);
}

void ext2_volume_read_blocks(struct ext2_volume* volume, int block, int count, void* buffer){
    read_disk_lba(volume->disk_no,(block * (volume->block_size / BYTES_PER_SECTOR)) + volume->partition_offset,count * volume->sectors_per_block,buffer);
}

void ext2_inode_read_block(struct ext2_volume* volume, ext2_inode_t* inode, int block_index, void* buffer){
    int pointers_per_block = volume->block_size / sizeof(uint32_t);
    if(block_index < 12){
        // direct block
        ext2_volume_read_block(volume,inode->block[block_index],buffer);
    }else if(block_index < 12 + pointers_per_block){
        // singly indirect block
        uint32_t* indirect_block = malloc(volume->block_size);
        ext2_volume_read_block(volume,inode->block[12],indirect_block);
        ext2_volume_read_block(volume,indirect_block[block_index - 12],buffer);
        free(indirect_block);
    }else if(block_index < 12 + (pointers_per_block * pointers_per_block)){
        // doubly indirect block
        uint32_t* double_indirect_block = malloc(volume->block_size);
        uint32_t* single_indirect_block = malloc(volume->block_size);

        uint32_t double_indirect_index = (block_index - 12 - pointers_per_block) / pointers_per_block;
        uint32_t single_indirect_index = (block_index - 12 - pointers_per_block) % pointers_per_block;

        ext2_volume_read_block(volume,inode->block[13],double_indirect_block);
        ext2_volume_read_block(volume,double_indirect_block[double_indirect_index],single_indirect_block);
        ext2_volume_read_block(volume,single_indirect_block[single_indirect_index],buffer);
        free(double_indirect_block);
        free(single_indirect_block);
    }else if(block_index < 12 + (pointers_per_block * pointers_per_block * pointers_per_block)){
        // triply indirect block
        uint32_t* triply_indirect_block = malloc(volume->block_size);
        uint32_t* double_indirect_block = malloc(volume->block_size);
        uint32_t* single_indirect_block = malloc(volume->block_size);

        uint32_t triply_indirect_index = (block_index - 12 - pointers_per_block - (pointers_per_block * pointers_per_block)) / (pointers_per_block * pointers_per_block);
        uint32_t double_indirect_index = ((block_index - 12 - pointers_per_block - (pointers_per_block * pointers_per_block)) % (pointers_per_block * pointers_per_block)) / pointers_per_block;
        uint32_t single_indirect_index = ((block_index - 12 - pointers_per_block - (pointers_per_block * pointers_per_block)) % (pointers_per_block * pointers_per_block)) % pointers_per_block;

        ext2_volume_read_block(volume,inode->block[14],triply_indirect_block);
        ext2_volume_read_block(volume,triply_indirect_block[triply_indirect_index],double_indirect_block);
        ext2_volume_read_block(volume,double_indirect_block[double_indirect_index],single_indirect_block);
        ext2_volume_read_block(volume,single_indirect_block[single_indirect_index],buffer);
        free(triply_indirect_block);
        free(double_indirect_block);
        free(single_indirect_block);
    }

}

ext2_inode_t* ext2_read_inode(struct ext2_volume* volume, int inode){    

    struct ext2_node_cache_entry* cache_entry = ext2_find_cached_node(volume,inode);
    if(cache_entry != NULL){
        return cache_entry->inode_data;
    }

    int block_group = (inode - 1) / volume->superblock.inodes_per_group;
    int inode_table_index = (inode - 1) % volume->superblock.inodes_per_group;
    
    block_group_descriptor_t* bgdt_entry = &volume->bgdt[block_group];

    ext2_inode_t* inode_table = malloc(volume->block_size);
    uint32_t inode_byte_offset = inode_table_index * INODE_SIZE;
    int inode_table_block_index = inode_byte_offset / volume->block_size;
    ext2_volume_read_block(volume,bgdt_entry->inode_table + inode_table_block_index,inode_table);
    
    uint32_t index_in_block = (inode_byte_offset % volume->block_size) / INODE_SIZE;
    ext2_inode_t* file_inode = &inode_table[index_in_block];

    ext2_inode_t* ret_inode = malloc(INODE_SIZE);
    memcpy(ret_inode,file_inode,INODE_SIZE);
    
    free(inode_table);
    return ret_inode;
}

uint32_t ext2_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    struct ext2_volume* volume = (void*)node->impl;

    if(node->length == 0) return 0;
    if(size == 0) return 0;
    if(offset >= node->length) return 0;
    if(offset + size >= node->length){
        size = node->length - offset;
    }
    
    ext2_inode_t* file_inode = ext2_read_inode(volume,node->inode);

    int block_offset = offset / volume->block_size;
    int byte_offset = offset % volume->block_size;
    int blocks_to_read = ((byte_offset + size) + (volume->block_size - 1)) / volume->block_size;
    
    void* block_buffer = malloc(blocks_to_read * volume->block_size);
    
    for(int i = 0; i < blocks_to_read; i++){
        ext2_inode_read_block(volume,file_inode,block_offset + i, block_buffer + (i * volume->block_size));
    }

    memcpy(buffer,block_buffer + byte_offset,size);
    free(block_buffer);
    return size;
}

static struct dirent dirent;

static struct dirent* ext2_readdir(fs_node_t *node, uint32_t index){
    struct ext2_volume* volume = (void*)node->impl;

    ext2_inode_t* file_inode = ext2_read_inode(volume,node->inode);

    int blocks_to_read = (file_inode->size + (volume->block_size - 1)) / volume->block_size;
    void* buffer = malloc(blocks_to_read * volume->block_size);

    for(int i = 0; i < blocks_to_read; i++){
        ext2_inode_read_block(volume,file_inode,i,buffer + (i * volume->block_size));
    }

    ext2_dirent_t* file_dirent = buffer;

    int offset = 0;
    int i = 0;
    while(offset < file_inode->size){
        if(i == index){
            if(file_dirent->inode == 0) {
                free(buffer);
                return NULL;
            }
            memcpy(dirent.name,file_dirent->name,file_dirent->name_length);
            dirent.name[file_dirent->name_length] = 0;
            dirent.ino = file_dirent->inode;

            free(buffer);            
            return &dirent;
        }
        
        offset+=file_dirent->entry_size;
        file_dirent = buffer + offset;
        i++;
    }
    free(buffer);
    return NULL;
}

fs_node_t* ext2_gen_fs_node(struct ext2_volume* volume, ext2_inode_t* file_inode, ext2_dirent_t* dirent){
    fs_node_t* new_node = malloc(sizeof(fs_node_t)); 
    memset(new_node,0,sizeof(fs_node_t));
    memcpy(new_node->name,dirent->name,dirent->name_length);
    new_node->name[dirent->name_length] = 0;
    if(file_inode->mode & 0x4000){
        new_node->flags = FS_DIRECTORY;
        new_node->readdir = ext2_readdir;
        new_node->finddir = ext2_finddir;
        new_node->read = 0;
        new_node->write = 0;
    }else{
        new_node->flags = FS_FILE;
        new_node->readdir = 0;
        new_node->finddir = 0;
        new_node->read = ext2_read;
        new_node->write = 0;
    }
    new_node->ptr = NULL;
    new_node->gid = file_inode->gid;
    new_node->uid = file_inode->uid;
    new_node->mask = file_inode->mode & 0xfff;
    new_node->length = file_inode->size;
    new_node->impl = (uint64_t)volume;
    new_node->inode = dirent->inode;
    return new_node;
}

fs_node_t* ext2_finddir(fs_node_t *node, char *name){
    struct ext2_volume* volume = (void*)node->impl;

    ext2_inode_t* dir_inode = ext2_read_inode(volume,node->inode);

    int blocks_to_read = (dir_inode->size + (volume->block_size - 1)) / volume->block_size;
    void* buffer = malloc(blocks_to_read * volume->block_size);

    for(int i = 0; i < blocks_to_read; i++){
        ext2_inode_read_block(volume,dir_inode,i,buffer + (i * volume->block_size));
    }

    ext2_dirent_t* file_dirent = buffer;

    int offset = 0;
    int i = 0;
    while(offset < dir_inode->size){
        if(file_dirent->inode != 0) {
            if(file_dirent->name_length == strlen(name) && memcmp(name,file_dirent->name,file_dirent->name_length) == 0){
                
                struct ext2_node_cache_entry* cache_entry = ext2_find_cached_node(volume,file_dirent->inode);

                fs_node_t* file = NULL;
                if(cache_entry == NULL){
                    ext2_inode_t* file_inode = ext2_read_inode(volume,file_dirent->inode);
                    file = ext2_gen_fs_node(volume,file_inode,file_dirent);

                    ext2_node_cache_append(volume,file,file_inode);
                }else{
                    file = cache_entry->node;
                }

                free(buffer);
                return file;
            }           
        }

        offset+=file_dirent->entry_size;
        file_dirent = buffer + offset;
        i++;
    }
    free(buffer);
    return NULL;
}

fs_node_t* ext2_mount_partition(int disk_no, int partition_lba){
    struct ext2_volume* volume = malloc(sizeof(struct ext2_volume));
    volume->disk_no = disk_no;
    volume->partition_offset = partition_lba;
    
    struct ext2_superblock* superblock = malloc(BYTES_PER_SECTOR * 2);
    ext2_volume_read_sectors(volume,2,2,superblock);
    memcpy(&volume->superblock,superblock,sizeof( ext2_superblock_t));
    free(superblock);

    if(volume->superblock.magic != 0xef53){
        printf("failed to mount: not a valid ext2 partition");
        free(volume);
        return NULL;
    }

    volume->block_size = 1024 << volume->superblock.log_block_size;
    volume->sectors_per_block = volume->block_size / BYTES_PER_SECTOR;

    volume->block_group_count = (volume->superblock.blocks_count + (volume->superblock.blocks_per_group - 1)) / volume->superblock.blocks_per_group;
    uint32_t bgdt_blocks = ((volume->block_group_count * sizeof(block_group_descriptor_t)) + (volume->block_size - 1)) / volume->block_size;
    volume->bgdt = malloc(bgdt_blocks * volume->block_size);
    if(volume->block_size == 1024){
        ext2_volume_read_blocks(volume,2,bgdt_blocks,volume->bgdt);
    }else{
        ext2_volume_read_blocks(volume,1,bgdt_blocks,volume->bgdt);
    }

    ext2_inode_t* root_inode = ext2_read_inode(volume,2);
    fs_node_t* root_node = malloc(sizeof(fs_node_t));
    memset(root_node,0,sizeof(fs_node_t));
    strcpy(root_node->name,"ext2");
    root_node->gid = root_inode->gid;
    root_node->uid = root_inode->uid;
    root_node->flags = FS_DIRECTORY;
    root_node->impl = (uint64_t)volume;
    root_node->ptr = NULL;
    root_node->mask = 777;
    root_node->length = 0;
    root_node->readdir = ext2_readdir;
    root_node->finddir = ext2_finddir;
    root_node->inode = 2;

    volume->node_cache = malloc(sizeof(struct ext2_node_cache_entry));
    volume->node_cache->next = NULL;
    volume->node_cache->node = root_node;
    volume->node_cache->inode = 2;
    volume->node_cache->inode_data = root_inode;

    return root_node;
}