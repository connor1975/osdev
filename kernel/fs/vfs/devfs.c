#include <fs/vfs.h>
#include <keyboard.h>
#include <heap.h>
#include <tty.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <disk.h>
#include <debug.h>

#define DEV_STARTING_INODE 2

static fs_node_t* dev_node;

static fs_node_t** devfs_entries = NULL;
static int devfs_entry_count = 0;
static int dev_next_ino = DEV_STARTING_INODE;

static struct dirent dirent;

static struct dirent* dev_readdir(fs_node_t *node, uint32_t index){
    if (index >= devfs_entry_count)
        return NULL;
    memcpy(dirent.name, devfs_entries[index]->name,strlen(devfs_entries[index]->name));
    dirent.name[strlen(devfs_entries[index]->name)] = 0;
    dirent.ino = devfs_entries[index]->inode;
    return &dirent;
}

fs_node_t* dev_finddir(fs_node_t *node, char *name){
   int i;
   for (i = 0; i < devfs_entry_count; i++)
       if (!strcmp(name, devfs_entries[i]->name))
           return devfs_entries[i];
   return NULL;
}

void dev_add_node(fs_node_t* node){
    int index = devfs_entry_count;
    devfs_entry_count++;
    devfs_entries = realloc(devfs_entries,sizeof(fs_node_t*) * devfs_entry_count);
    node->inode = dev_next_ino++;
    devfs_entries[index] = node;
}

void tty_init();
void task_open_stdio();

int remount_devfs(){
    fs_node_t* dev = find_file("/dev");
    if(dev == NULL){
        kprintf(KPRINTF_ERROR, "trying to remount devfs but /dev does not exist!\n");
        return 1;
    }
    mount_virtual("/dev",dev_node);
    return 0;
}

void devfs_init(){
    dev_node = malloc(sizeof(fs_node_t));
    memset(dev_node,0,sizeof(fs_node_t));
    strcpy(dev_node->name,"devfs");
    dev_node->uid = dev_node->gid = dev_node->length = 0;
    dev_node->inode = DEV_STARTING_INODE - 1;
    dev_node->mask = 0777;
    dev_node->flags = FS_DIRECTORY;
    dev_node->read = 0;
    dev_node->write = 0;
    dev_node->open = 0;
    dev_node->close = 0;
    dev_node->ioctl = 0;
    dev_node->readdir = &dev_readdir;
    dev_node->finddir = &dev_finddir;
    dev_node->ptr = 0;
    dev_node->impl = 0;
    //mount_virtual("/dev",dev_node);

    fs_node_t* this_node = malloc(sizeof(fs_node_t));
    memset(this_node,0,sizeof(fs_node_t));
    strcpy(this_node->name,".");
    this_node->mask = 0777;
    this_node->flags = FS_DIRECTORY;
    dev_add_node(this_node);

    fs_node_t* up_node = malloc(sizeof(fs_node_t));
    memset(up_node,0,sizeof(fs_node_t));
    strcpy(up_node->name,"..");
    up_node->mask = 0777;
    up_node->flags = FS_DIRECTORY;
    dev_add_node(up_node);

    zero_init();
    tty_fs_init();
    create_disk_devices();
    log_vfs_init();
}