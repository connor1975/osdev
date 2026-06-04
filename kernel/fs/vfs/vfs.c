#include <fs/vfs.h>
#include <multitasking.h>
#include <common.h>
#include <stdio.h>
#include <string.h>

fs_node_t *fs_root = 0;

int ioctl_fs(fs_node_t *node, unsigned long request, void * argp){
    if(node == NULL) return 0;
    if(node->ioctl != 0)
        return node->ioctl(node,request,argp);
    else
        return 0;
}

uint32_t read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    if(node == NULL) return 0;
    if (node->read != 0)
        return node->read(node, offset, size, buffer);
    else
        return 0;
}

uint32_t write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer){
    if(node == NULL) return 0;
    if (node->write != 0)
        return node->write(node, offset, size, buffer);
    else
        return 0;
}

void open_fs(fs_node_t *node, uint8_t read, uint8_t write){
    if(node == NULL) return;
    if (node->open != 0)
        return node->open(node);
}

void close_fs(fs_node_t *node){
    if(node == NULL) return;
    if (node->close != 0)
        return node->close(node);
}

void truncate_fs(fs_node_t* node, int length){
    if(node == NULL) return;
    if(node->truncate != 0)
        node->truncate(node,length);
}

struct dirent *readdir_fs(fs_node_t *node, uint32_t index){
    if(node->flags & FS_MOUNTPOINT) node = node->ptr;
    if (node->flags == FS_DIRECTORY && node->readdir != 0 )
        return node->readdir(node, index);
    else
        return 0;
}

fs_node_t *finddir_fs(fs_node_t *node, char *name){
    if(node->flags & FS_MOUNTPOINT) node = node->ptr;
    if (node->flags & FS_DIRECTORY && node->finddir != 0 )
        return node->finddir(node, name);
    else
        return 0;
}

void create_file_fs(fs_node_t* node, char* name){
    if(node->flags & FS_MOUNTPOINT) node = node->ptr;
    if (node->flags & FS_DIRECTORY && node->create_file != 0)
        return node->create_file(node,name);
}

fs_node_t* find_file(char* file_path){
    char* path = malloc(strlen(file_path) + 1);
    strcpy(path,file_path);
    path[strlen(file_path)] = '\0';
    if(strcmp(path, "") == 0) return NULL;
    char* cwd;
    if(current_task != NULL) cwd = current_task->cwd;
    else cwd = "/";

    char* safepath = vfs_absolute_path(cwd,path);
    fs_node_t* cur = fs_root;
    char* saveptr;
    char* token = strtok_r(safepath,"/", &saveptr);
    while(token != NULL){
        cur = finddir_fs(cur,token);
        if(cur == NULL) {
            return NULL;
        }
        token = strtok_r(NULL,"/", &saveptr);
    }
    free(path);
    return cur;
}

fs_node_t* find_file_dir(char* file_path){
    if (!file_path || file_path[0] == '\0')
        return NULL;

    char* cwd;
    if(current_task != NULL) cwd = current_task->cwd;
    else cwd = "/";
    char* abs = vfs_absolute_path(cwd, file_path);

    fs_node_t* cur = fs_root;

    char* saveptr;
    char* token = strtok_r(abs, "/", &saveptr);

    while (token != NULL){
        fs_node_t* next = finddir_fs(cur, token);
        if (next == NULL)
            break;
        cur = next;
        token = strtok_r(NULL, "/", &saveptr);
    }
    free(abs);
    return cur;
}

char* get_filename_from_path(char* path){
    if (!path || path[0] == '\0')
        return NULL;

    char* last = path;
    for (char* p = path; *p; p++){
        if (*p == '/')
            last = p + 1;
    }
    return last;
}

fs_node_t* kopen(char* path){
    if(fs_root == NULL) return NULL;
    fs_node_t* node = find_file(path);
    if(node == NULL) return NULL;
    open_fs(node,1,1);
    return node;
}

int vfs_mount(char* path,fs_node_t* local_root){
    if(local_root == NULL) return 1;
    if(memcmp(path,"/",2) == 0){
        fs_root = local_root;
        return 0;
    }
    fs_node_t* mountpoint = kopen(path);
    if(mountpoint == NULL) return 1;

    mountpoint->flags |= FS_MOUNTPOINT;
    mountpoint->ptr = local_root;
    return 0;
}