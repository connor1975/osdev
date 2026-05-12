#include <kernel/common.h>
#include <kernel/fs/vfs.h>
#include <kernel/keyboard.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define TIOCGWINSZ 0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

uint32_t stdin_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    if(size == 0) return 0;
    return keyboard_read((void*)buffer,size);    
}

uint32_t stdout_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    irq_disable();
    for(int i = 0; i < size; i++){
        printf("%c",((char*)buffer)[i]);
    }
    irq_enable();
    return size;
}

int tty_ioctl(fs_node_t *node, unsigned long request, void * argp){
    switch(request){
        case TIOCGWINSZ:
            if(argp == NULL) return -EINVAL;
            struct winsize* winsize = argp;
            winsize->ws_row = framebuffer_height / font_height;
            winsize->ws_col = framebuffer_width / font_width;
            winsize->ws_xpixel = framebuffer_width;
            winsize->ws_ypixel = framebuffer_height;
            return 0;
        break;
    }
    return -EINVAL;
}

fs_node_t* create_stdin_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stdin");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = stdin_read;
    node->write = 0;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_stdout_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stdout");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = 0;
    node->write = stdout_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_stderr_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stderr");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = 0;
    node->write = stdout_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_tty_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"tty");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = stdin_read;
    node->write = stdout_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

void tty_init(){
    dev_add_node(create_stdin_device());
    dev_add_node(create_stdout_device());
    dev_add_node(create_stderr_device());
    dev_add_node(create_tty_device());
}