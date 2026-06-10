#include <common.h>
#include <fs/vfs.h>
#include <stdint.h>
#include <string.h>

uint32_t read_null(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    return 0;
}

uint32_t write_null(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    return size;
}

uint32_t write_zero(fs_node_t* node, uint32_t offsest, uint32_t size, uint8_t* buffer){
    return size;
}

uint32_t read_zero(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    memset(buffer,0,size);
    return size;
}

fs_node_t* create_zero_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    memset(node,0,sizeof(fs_node_t));
    strcpy(node->name,"zero");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = read_zero;
    node->write = write_zero;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = 0;
    return node;
}

fs_node_t* create_null_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    memset(node,0,sizeof(fs_node_t));
    strcpy(node->name,"null");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = read_null;
    node->write = write_null;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = 0;
    return node;
}

void zero_init(){
    dev_add_node(create_null_device());
    dev_add_node(create_zero_device());
}