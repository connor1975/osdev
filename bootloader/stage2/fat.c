#include <bootloader/common.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern void bios_read_disk(uint32_t dap_addr, uint8_t drive_num);
uint8_t bios_drive_num;

char sector_buffer[1024];

void read_sectors(uint32_t lba, uint16_t sector_count, void* buffer, uint8_t drive_num) {
    uint16_t i = 0;
    while (i < sector_count) {
        uint16_t sectors_to_read = (sector_count - i) > 1 ? 2 : 1;
        struct dap dap;
        dap.reserved = 0;
        dap.size = 0x10;
        dap.sector_count = sectors_to_read;
        dap.buffer = linear_to_segoff((uint32_t)(uint64_t)sector_buffer);
        dap.lba = lba + i;
        bios_read_disk((uint32_t)(uint64_t)&dap, drive_num);
        memcpy(buffer + (512 * i), sector_buffer, 512 * sectors_to_read);
        i += sectors_to_read;
    }
}

struct fat_bootsector* bootsector;

unsigned int cluster_to_lba(unsigned int cluster){
    return (bootsector->hidden_sector_count + bootsector->reserved_sector_count + (bootsector->table_size_32 * bootsector->table_count)) + ((cluster - 2) * bootsector->sectors_per_cluster);
}

uint8_t bios_drive_num;
void* fat_sector_buffer = NULL;
int last_fat_chunk_offset = 0;

unsigned int get_next_cluster(unsigned int cluster){
    unsigned int fat_chunk_offset = (cluster * 4) / 1024;
    unsigned int fat_offset = (cluster * 4) % 1024;
    if(last_fat_chunk_offset != fat_chunk_offset){
        read_sectors(bootsector->hidden_sector_count + bootsector->reserved_sector_count + fat_chunk_offset,2,fat_sector_buffer, bios_drive_num);
        last_fat_chunk_offset = fat_chunk_offset;
    }
    unsigned int next_cluster = *(unsigned int*)(fat_sector_buffer + fat_offset);
    return next_cluster;
}

char to_upper(char string){
    if(string > 96){
        return string-=32;
    }
    return string;
}

char fat_filename[11];
char* to_fat_filename(char* filename){
    memset(fat_filename,' ', 11);
    for(int i = 0; i < 8; i++){
        if(*filename == '.' || *filename == 0 || *filename == '/'){
            break;
        }
        fat_filename[i] = to_upper(*filename);
        filename++;
    }
    if(filename[0] == '.'){
        fat_filename[8] = to_upper(filename[1]);
        fat_filename[9] = to_upper(filename[2]);
        fat_filename[10] = to_upper(filename[3]);
    }
    return fat_filename;
}

struct fat_dirent* search_directory(char* unformatted_filename,struct fat_dirent* directory){
    char* filename = to_fat_filename(unformatted_filename);
    static struct fat_dirent dirent;
    void* dir_sector = malloc(512 * bootsector->sectors_per_cluster);
    uint32_t current_cluster;
    if(directory != NULL){
        current_cluster = (uint64_t)directory->cluster_low; 
    }else{
        current_cluster = bootsector->root_cluster;
    }

    void* ptr = dir_sector;
    while(current_cluster < 0xffffff8){
        ptr = dir_sector;
        read_sectors(cluster_to_lba(current_cluster),bootsector->sectors_per_cluster,dir_sector,bios_drive_num);
        for(int i = 0; i < 16 * bootsector->sectors_per_cluster; i++){
            if(memcmp(filename, ptr, 11) == 0){
                memcpy((void*)&dirent,ptr,sizeof(struct fat_dirent));
                free(512 * bootsector->sectors_per_cluster);
                return &dirent;
            }
            ptr+=32;
        }
        current_cluster = get_next_cluster(current_cluster);
    }
    free(512 * bootsector->sectors_per_cluster);
    return NULL;
}

struct fat_dirent* find_file(char* path){
    if(*path == '/')path++;
    struct fat_dirent* dirent = search_directory(path,NULL);
    while(*path){
        if(*path == '/'){
            dirent = search_directory(path + 1 , dirent);
        }
        path++;
    }
    return dirent;
}

void* read_file(struct fat_dirent* file){
    uint64_t cluster_count = (((bootsector->sectors_per_cluster * 512 ) - 1) + file->file_size) / (bootsector->sectors_per_cluster * 512);
    void* clusterbuffer = malloc((512 * bootsector->sectors_per_cluster) * cluster_count);
    void* bufferptr = clusterbuffer;
    uint32_t current_cluster = (uint32_t)file->cluster_low | ((uint32_t)file->cluster_high << 16);
    for(int i = 0; i < cluster_count; i++){
        read_sectors(cluster_to_lba(current_cluster),bootsector->sectors_per_cluster,bufferptr,bios_drive_num);
        current_cluster = get_next_cluster(current_cluster);
        bufferptr+=(bootsector->sectors_per_cluster * 512); 
    }
    return clusterbuffer;
}

void fat_init(uint32_t partition_lba, uint8_t drive_num){
    bios_drive_num = drive_num;
    bootsector = malloc(512);
    read_sectors(partition_lba,1,bootsector,drive_num);
    fat_sector_buffer = malloc(1024); 
    read_sectors(bootsector->hidden_sector_count + bootsector->reserved_sector_count,2,fat_sector_buffer, drive_num);
    last_fat_chunk_offset = 0;    
}