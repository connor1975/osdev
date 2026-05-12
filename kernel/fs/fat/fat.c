#include <kernel/fs/vfs.h>
#include <kernel/common.h>
#include <kernel/fs/fat.h>
#include <kernel/disk.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// Readonly FAT32 FAT16 and FAT12 driver

#define FAT_BUFFER_SIZE 16

void fat_volume_read_sectors(fat_mounted_volume_t* volume, int lba, int sector_count, void* buffer){
    read_disk(volume->disk_no,lba + volume->partition_offset,sector_count,buffer);
}

void fat_volume_read(fat_mounted_volume_t* volume, int offset, int size, void* buffer){
    int lba_off = offset / volume->bootsector->bytes_per_sector;
    int byte_off = offset % volume->bootsector->bytes_per_sector;
    int sector_count = ((byte_off + size) + (volume->bootsector->bytes_per_sector - 1)) / volume->bootsector->bytes_per_sector;
    void* sector_buffer = malloc(volume->bootsector->bytes_per_sector * sector_count);
    fat_volume_read_sectors(volume,lba_off,sector_count,sector_buffer);
    memcpy(buffer,sector_buffer + byte_off,size);
    free(sector_buffer);
}

void fat_volume_write_sectors(fat_mounted_volume_t* volume, int lba, int sector_count, void* buffer){
    write_disk(volume->disk_no,lba + volume->partition_offset,sector_count,buffer);
}

void fat_volume_write(fat_mounted_volume_t* volume, int offset, int size, void* buffer){
    int lba_off = offset / volume->bootsector->bytes_per_sector;
    int byte_off = offset % volume->bootsector->bytes_per_sector;
    int sector_count = ((byte_off + size) + (volume->bootsector->bytes_per_sector - 1)) / volume->bootsector->bytes_per_sector;
    void* sector_buffer = malloc(volume->bootsector->bytes_per_sector * sector_count);
    fat_volume_read_sectors(volume,lba_off,sector_count,sector_buffer);
    memcpy(sector_buffer + byte_off,buffer, size);
    fat_volume_write_sectors(volume,lba_off,sector_count,sector_buffer);
    free(sector_buffer);
}

uint32_t cluster_to_lba(fat_mounted_volume_t* volume, uint32_t cluster){
    switch(volume->fat_version){
        case FAT_VERSION_32:
        {
            struct fatbs_ext32* extended_bs = (struct fatbs_ext32*)volume->bootsector->extended_section;
            return (volume->bootsector->reserved_sector_count + (extended_bs->table_size_32 * volume->bootsector->table_count)) + ((cluster - 2) * volume->bootsector->sectors_per_cluster);
        }
        default:
        {
            int root_dir_sectors = ((volume->bootsector->root_entry_count * 32) + (volume->bootsector->bytes_per_sector - 1)) / volume->bootsector->bytes_per_sector;
            return (volume->bootsector->reserved_sector_count + (volume->bootsector->table_size_16 * volume->bootsector->table_count)) + ((cluster - 2) * volume->bootsector->sectors_per_cluster) + root_dir_sectors;
        }
    }
}

uint32_t get_next_cluster32(fat_mounted_volume_t* volume, uint32_t cluster){
    uint32_t fat_chunk_offset = (cluster * 4) / (FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    uint32_t fat_offset = (cluster * 4) % (FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    if(volume->fat_buffer == NULL) volume->fat_buffer = malloc(FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    if(volume->fat_index != fat_chunk_offset) {
        fat_volume_read_sectors(volume,volume->bootsector->reserved_sector_count + (fat_chunk_offset * FAT_BUFFER_SIZE),FAT_BUFFER_SIZE,volume->fat_buffer);
        volume->fat_index = fat_chunk_offset;
    }
    uint32_t next_cluster = *(uint32_t*)(volume->fat_buffer + fat_offset);
    return next_cluster;
}

uint32_t get_next_cluster16(fat_mounted_volume_t* volume, uint32_t cluster){
    uint32_t fat_chunk_offset = (cluster * 2) / (FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    uint32_t fat_offset = (cluster * 2) % (FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    if(volume->fat_buffer == NULL) volume->fat_buffer = malloc(FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    if(volume->fat_index != fat_chunk_offset) {
        fat_volume_read_sectors(volume,volume->bootsector->reserved_sector_count + (fat_chunk_offset * FAT_BUFFER_SIZE),FAT_BUFFER_SIZE,volume->fat_buffer);
        volume->fat_index = fat_chunk_offset;
    }
    uint32_t next_cluster = *(uint32_t*)(volume->fat_buffer + fat_offset);
    return next_cluster;
}

uint32_t get_next_cluster12(fat_mounted_volume_t* volume, uint32_t cluster){
    if(volume->fat_buffer == NULL) volume->fat_buffer = malloc(FAT_BUFFER_SIZE * volume->bootsector->bytes_per_sector);
    uint32_t fat_offset = cluster + (cluster / 2);
    uint32_t fat_sector = (volume->bootsector->reserved_sector_count) + (fat_offset / volume->bootsector->bytes_per_sector);
    uint32_t ent_offset = fat_offset % volume->bootsector->bytes_per_sector;

    if(volume->fat_index != fat_sector){
        fat_volume_read_sectors(volume,fat_sector,2,volume->fat_buffer);
        volume->fat_index = fat_sector;
    }
    uint8_t* fat_table = volume->fat_buffer;
    uint16_t table_value = *(uint16_t*)&fat_table[ent_offset];

    table_value = (cluster & 1) ? table_value >> 4 : table_value & 0xfff;
    return table_value;
}

uint32_t get_next_cluster(fat_mounted_volume_t* volume, uint32_t cluster){
    switch(volume->fat_version){
        case FAT_VERSION_32:
            return get_next_cluster32(volume,cluster);
        case FAT_VERSION_16:
            return get_next_cluster16(volume,cluster);
        case FAT_VERSION_12:
            return get_next_cluster12(volume,cluster);
    }
    return 0;
}

int is_end_of_chain(fat_mounted_volume_t* volume, uint32_t cluster){
    switch(volume->fat_version){
        case FAT_VERSION_32:
            return (cluster >= FAT32_CHAIN_END);
        case FAT_VERSION_16:
            return (cluster >= FAT16_CHAIN_END);
        case FAT_VERSION_12:
            return (cluster >= FAT12_CHAIN_END);
    }
    return 0;
}

void* legacy_fat_read_rootdir(fat_mounted_volume_t* volume, int* entry_count){
    int root_dir_sectors = ((volume->bootsector->root_entry_count * 32) + (volume->bootsector->bytes_per_sector - 1)) / volume->bootsector->bytes_per_sector;
    void* root_dir = malloc(root_dir_sectors * volume->bootsector->bytes_per_sector);
    uint32_t first_root_dir_sector = volume->bootsector->reserved_sector_count + (volume->bootsector->table_size_16 * volume->bootsector->table_count);
    fat_volume_read_sectors(volume,first_root_dir_sector,root_dir_sectors,root_dir);
    *entry_count = volume->bootsector->root_entry_count;
    return root_dir;    
}

void* fat_read_directory(fat_mounted_volume_t* volume,  uint32_t directory_cluster,int* entry_count){
    if(directory_cluster == 0 && volume->fat_version != FAT_VERSION_32) 
        return legacy_fat_read_rootdir(volume, entry_count);
    
    void* buffer = NULL;
    uint64_t buffer_size = 0;
    uint32_t current_cluster = directory_cluster;
    int i = 0;
    while(!is_end_of_chain(volume,current_cluster)){
        buffer_size +=(volume->bootsector->sectors_per_cluster * volume->bootsector->bytes_per_sector);
        buffer = realloc(buffer,buffer_size);
        fat_volume_read_sectors(volume,cluster_to_lba(volume,current_cluster),volume->bootsector->sectors_per_cluster,buffer + ((volume->bootsector->sectors_per_cluster * volume->bootsector->bytes_per_sector) * i));
        current_cluster = get_next_cluster(volume,current_cluster);
        i++;
    }
    *entry_count = (volume->bootsector->sectors_per_cluster * 16) * i;
    return buffer;
}

void fat_read_bootsector(fat_mounted_volume_t* volume){
    volume->bootsector = malloc(512);
    fat_volume_read_sectors(volume,0,1,volume->bootsector);
}

uint32_t fat_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    if(offset >= node->length) return 0;
    if(offset >= node->length - 1) return 0;
    if(offset + size >= node->length){
        size = node->length - offset;
    }
    
    fat_mounted_volume_t* volume = (fat_mounted_volume_t*)node->impl;
    struct fat_node_info* fatinfo = volume->fileinfo[node->inode];
    uint32_t current_cluster = fatinfo->starting_cluster;

    uint32_t cluster_offset = offset / (volume->bootsector->bytes_per_sector * volume->bootsector->sectors_per_cluster);
    uint32_t byte_offset = offset % (volume->bootsector->bytes_per_sector * volume->bootsector->sectors_per_cluster);
    for(int i = 0; i < cluster_offset; i++){
        current_cluster =  get_next_cluster(volume,current_cluster);
        if(is_end_of_chain(volume,current_cluster)){
            return 0;
        }
    }
    uint64_t cluster_count = ((offset + size) + ((volume->bootsector->sectors_per_cluster * volume->bootsector->bytes_per_sector) - 1)) / (volume->bootsector->sectors_per_cluster * volume->bootsector->bytes_per_sector);
    void* clusterbuffer = malloc((volume->bootsector->bytes_per_sector * volume->bootsector->sectors_per_cluster) * cluster_count);

    int i = 0;
    while(!is_end_of_chain(volume,current_cluster) && i < cluster_count){
        fat_volume_read_sectors(volume,cluster_to_lba(volume,current_cluster),volume->bootsector->sectors_per_cluster,clusterbuffer + (i * (volume->bootsector->bytes_per_sector * volume->bootsector->sectors_per_cluster)));
        i++;
        current_cluster = get_next_cluster(volume,current_cluster);
    }

    memcpy(buffer,clusterbuffer + byte_offset,size);

    free(clusterbuffer);
    return size;
}

uint32_t fat_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    return 0;
}

static struct dirent dirent;

static struct dirent* fat_readdir(fs_node_t *node, uint32_t index){
    fat_mounted_volume_t* volume = (fat_mounted_volume_t*)node->impl;
    struct fat_node_info* fatinfo = volume->fileinfo[node->inode];
    for(int i = 0; i < fatinfo->no_child; i++){
        if(i == index){
            memcpy(dirent.name,fatinfo->children[i]->name,128);
            dirent.ino = fatinfo->children[i]->inode;
            return &dirent;
        }
    }
    return NULL;
}

fs_node_t* fat_finddir(fs_node_t *node, char *name){
    if(name == NULL) return NULL;
    if(*name == 0) return NULL;
    fat_mounted_volume_t* volume = (fat_mounted_volume_t*)node->impl;
    struct fat_node_info* fatinfo = volume->fileinfo[node->inode];
    for(int i = 0; i < fatinfo->no_child; i++){
        if(memcmp(name,fatinfo->children[i]->name,strlen(name) + 1) == 0){
            return fatinfo->children[i];
        }
    }
    return NULL;
}

void populate_directory(fat_mounted_volume_t* volume,fs_node_t* node, fs_node_t* parent, uint32_t dir_cluster){
    int entry_count = 0;
    int num_files = 2;
    struct fat_dirent* dir = fat_read_directory(volume, dir_cluster, &entry_count);
    
    for(int i = 0; i < entry_count; i++){
        if(dir[i].name[0] != 0 && dir[i].name[0] != 0xE5 && dir[i].name[0] != '.' && dir[i].attr != FAT_ATTR_VOLUME_ID) num_files++;
    }
    struct fat_node_info* fat_info = volume->fileinfo[node->inode];
    fat_info->starting_cluster = dir_cluster;
    fat_info->no_child = num_files;
    fat_info->children = malloc(sizeof(void*) * num_files);

    fs_node_t* current_dir = malloc(sizeof(fs_node_t));
    memset(current_dir,0,sizeof(fs_node_t));
    strcpy(current_dir->name,".");
    current_dir->inode = node->inode;
    current_dir->impl = (uint64_t)volume;
    current_dir->mask = 0777;
    current_dir->flags = FS_DIRECTORY;
    
    fs_node_t* prev_dir = malloc(sizeof(fs_node_t));
    memset(prev_dir,0,sizeof(fs_node_t));
    strcpy(prev_dir->name,"..");
    if(parent != NULL) prev_dir->inode = parent->inode;
    else prev_dir->inode = node->inode;
    prev_dir->impl = (uint64_t)volume;
    prev_dir->mask = 0777;
    prev_dir->flags = FS_DIRECTORY;
    
    fat_info->children[0] = current_dir;
    fat_info->children[1] = prev_dir;

    int i = 2;
    for(int x = 0; x < entry_count; x++){
        if(dir[x].name[0] == 0 || dir[x].name[0] == 0xE5 || dir[x].name[0] == '.' || dir[x].attr == FAT_ATTR_VOLUME_ID) continue;

        fs_node_t* new_node = malloc(sizeof(fs_node_t));
        memset(new_node->name,0,128);
        to_normal_filename(dir[x].name,new_node->name);
        new_node->mask = 0777;
        new_node->uid = new_node->gid = 0;
        new_node->impl = (uint64_t)volume;
        if(dir[x].attr == FAT_ATTR_DIRECTORY ){
            new_node->length = 0;
            new_node->flags = FS_DIRECTORY;
            new_node->read = 0;
            new_node->write = 0;
            new_node->open = 0;
            new_node->close = 0;
            new_node->ioctl = 0;
            new_node->readdir = &fat_readdir;
            new_node->finddir = &fat_finddir;
            new_node->ptr = 0;
            new_node->inode = volume->next_inode++;
            void* fs_extra = malloc(sizeof(struct fat_node_info));
            volume->fileinfo = realloc(volume->fileinfo,volume->next_inode * sizeof(void*));
            volume->fileinfo[new_node->inode] = fs_extra;
            uint32_t starting_cluster = dir[x].cluster_low | ((uint64_t)dir[x].cluster_high << 16);
            fat_info->children[i] = new_node;
            populate_directory(volume,new_node,node,starting_cluster);
        }else{
            new_node->length = dir[x].file_size;
            new_node->flags = FS_FILE;
            new_node->read = &fat_read;
            new_node->write = &fat_write;
            new_node->ioctl = 0;
            new_node->readdir = 0;
            new_node->finddir = 0;
            new_node->open = 0;
            new_node->close = 0;
            new_node->ptr = 0;
            new_node->inode = volume->next_inode++;
            struct fat_node_info* fat_extras = malloc(sizeof(struct fat_node_info));
            fat_extras->starting_cluster = dir[x].cluster_low | ((uint64_t)dir[x].cluster_high << 16);
            fat_extras->children = 0;
            fat_extras->no_child = 0;
            fat_info->children[i] = new_node;
            volume->fileinfo = realloc(volume->fileinfo,volume->next_inode * sizeof(void*));
            volume->fileinfo[new_node->inode] = fat_extras;
        }
        i++;
    }
    free(dir);
}

fs_node_t* fat_mount_partition(int disk_no, int partition_lba){
    memset(&dirent,0,sizeof(struct dirent));
    fat_mounted_volume_t* volume = malloc(sizeof(fat_mounted_volume_t));
    
    volume->disk_no = disk_no;
    volume->partition_offset = partition_lba;
    fat_read_bootsector(volume);
    volume->next_inode = 1;
    volume->fat_index = -1;
    volume->fat_buffer = NULL;
    
    uint32_t total_sectors;
    if(volume->bootsector->total_sectors_16 != 0) total_sectors = volume->bootsector->total_sectors_16;
    else total_sectors = volume->bootsector->total_sectors_32;
    uint32_t total_clusters = total_sectors / volume->bootsector->sectors_per_cluster;
    
    if(total_clusters < 4085) {
        volume->fat_version = FAT_VERSION_12;
    } else if(total_clusters < 65525) {
        volume->fat_version = FAT_VERSION_16;
    } else{
        volume->fat_version = FAT_VERSION_32;
    }
    
    fs_node_t* vol_root = malloc(sizeof(fs_node_t));
    memcpy(vol_root->name,"fat",4);
    vol_root->mask = 0777;
    vol_root->uid = vol_root->gid = vol_root->length = 0;
    vol_root->flags = FS_DIRECTORY;
    vol_root->read = 0;
    vol_root->write = 0;
    vol_root->open = 0;
    vol_root->close = 0;
    vol_root->readdir = &fat_readdir;
    vol_root->finddir = &fat_finddir;
    vol_root->ptr = 0;
    vol_root->inode = volume->next_inode++;
    vol_root->impl = (uint64_t)volume;
    volume->fileinfo = realloc(volume->fileinfo,volume->next_inode * sizeof(void*));
    void* fileinfo = malloc(sizeof(struct fat_node_info));
    volume->fileinfo[vol_root->inode] = fileinfo;
    volume->root = vol_root;

    uint32_t root_dir_start_cluster;
    if(volume->fat_version == FAT_VERSION_32){
        struct fatbs_ext32* extended_bs = (struct fatbs_ext32*)volume->bootsector->extended_section;
        memcpy(volume->volume_name,extended_bs->volume_label,11);
        volume->volume_name[11] = 0;
        root_dir_start_cluster = extended_bs->root_cluster;
    }else{
        struct old_fatbs_ext* extended_bs = (struct old_fatbs_ext*)volume->bootsector->extended_section;
        memcpy(volume->volume_name,extended_bs->volume_label,11);
        volume->volume_name[11] = 0;
        root_dir_start_cluster = 0;
    }
    populate_directory(volume,vol_root,NULL,root_dir_start_cluster);
    return vol_root;
}