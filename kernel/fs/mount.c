#include <stdint.h>
#include <fs/fat.h>
#include <fs/ext2.h>
#include <disk.h>
#include <fs/vfs.h>
#include <string.h>
#include <errno.h>
#include <heap.h>
#include <debug.h>
#include <stdio.h>

struct mount{
    fs_node_t* node;
    char path[256];
    char device[256];
    char fs[32];
    struct mount* next;
};

static struct mount* mounts = NULL;
fs_node_t* mounts_vfs = NULL;
static void* mounts_vfs_buffer = NULL;
static uint64_t mounts_vfs_buffer_len = 0;

uint64_t read_mounts(fs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer){
    if(offset > mounts_vfs_buffer_len)
        return 0;
    if(offset+size > mounts_vfs_buffer_len)
        size = mounts_vfs_buffer_len-offset;
    memcpy(buffer,mounts_vfs_buffer+offset,size);
    return size;
}

void rebuild_mounts_vfs(){
    int len = 0;
    struct mount* current = mounts;
    while(current != NULL){
        len+=strlen(current->path)+strlen(current->device)+strlen(current->fs)+3;
        current = current->next;
    }
    mounts_vfs_buffer_len = len;
    mounts_vfs_buffer = realloc(mounts_vfs_buffer, mounts_vfs_buffer_len);
    current = mounts;
    int offset = 0;
    while(current != NULL){
        memcpy((char*)mounts_vfs_buffer+offset,current->device,strlen(current->device));
        offset+=strlen(current->device);
        ((char*)mounts_vfs_buffer)[offset++] = ' ';
        memcpy((char*)mounts_vfs_buffer+offset,current->path,strlen(current->path));
        offset+=strlen(current->path);
        ((char*)mounts_vfs_buffer)[offset++] = ' ';
        memcpy((char*)mounts_vfs_buffer+offset,current->fs,strlen(current->fs));
        offset+=strlen(current->fs);
        ((char*)mounts_vfs_buffer)[offset++] = '\n';
        current = current->next;
    }
    if(mounts_vfs != NULL)
        mounts_vfs->length = mounts_vfs_buffer_len;
}

void hook_mtab(){
    mounts_vfs = find_file("/etc/mtab");
    if(mounts_vfs == NULL){
        kprintf(KPRINTF_CRITICAL,"kernel cannot find /etc/mtab!\n");
        return;
    }

    mounts_vfs->mask = 0444;
    mounts_vfs->read = read_mounts;
    mounts_vfs->write = 0;
    mounts_vfs->length = 0;
    rebuild_mounts_vfs();
}

void register_mount(fs_node_t* node, char* device, char* path, char* fs){
    struct mount* current = mounts;
    if(current == NULL){
        mounts = malloc(sizeof(struct mount));
        memset(mounts,0,sizeof(struct mount));
        current = mounts;
    }else{
        while(current->next != NULL){
            current = current->next;
        }
        current->next = malloc(sizeof(struct mount));
        memset(current->next,0,sizeof(struct mount));
        current = current->next;
    }
    
    current->node = node;
    strcpy(current->path,path);
    
    if(fs == NULL)
        strcpy(current->fs,node->name);
    else
        strcpy(current->fs,fs);

    if(device == NULL)
        strcpy(current->device,node->name);
    else
        strcpy(current->device,device);

    kprintf(KPRINTF_INFO,"mount: mounting %s to path: %s of type %s\n",current->device,current->path,current->fs);
    rebuild_mounts_vfs();
}

void mount_vfs_remove_single(char* path){
    struct mount* current = mounts;
    struct mount* prev = NULL;
    while(current != NULL){
        if(strcmp(current->path,path) == 0){
            if(prev == NULL){
                mounts = current->next;
            }else{
                prev->next = current->next;
            }
            free(current);
            rebuild_mounts_vfs();
            return;
        }
        prev = current;
        current = current->next;
    }
}

void unmount_device(char* device){
    struct mount* current = mounts;
    while(current != NULL){
        struct mount* next = current->next;

        if(strcmp(current->device,device)==0)
            umount(current->path);
        current = next;
    }
}

int umount(char* target){
    fs_node_t* node = find_file(target);
    if(node->flags & FS_BLOCKDEVICE){
        unmount_device(target);
        return 0;
    }
    
    vfs_unmount(target);
    mount_vfs_remove_single(target);

    return 0;
}

void mount_virtual(char* path, fs_node_t* node){
    vfs_mount(path,node);
    register_mount(node,NULL,path,NULL);
}

int mount_internal(int disk, int partition, char* dev, char* path, char* fs){
    if((strcmp(fs, "ext2") == 0)){
        fs_node_t* root = ext2_mount_partition(disk,partition);
        if(root == NULL)
            return EINVAL;
        vfs_mount(path,root);
        register_mount(root,dev,path,fs);
        return 0;
    }
    if(strcmp(fs,"fat") == 0){
        fs_node_t* root = fat_mount_partition(disk,partition);
        if(root == NULL)
            return EINVAL;
        vfs_mount(path,root);
        register_mount(root,dev,path,fs);
        return 0;
    }
    return EINVAL;
}

int mount_device(char* dev, char* path, char* fs){
        if(dev == NULL || path == NULL || fs == NULL)
        return -EINVAL;

    fs_node_t* device = find_file(dev);
    if(device == NULL)
        return -ENODEV;

    int disk = device->impl & 0xffffffff;

    struct disk* disk_info = get_disk_info(disk);

    int partition = 0;
    if(disk_info->partition_count > 0){
        partition = (uint32_t)((uint64_t)device->impl >> 32);
    }

    int ret = mount_internal(disk,partition,dev,path,fs);

    return ret;
}