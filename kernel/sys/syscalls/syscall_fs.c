#include <kernel/common.h>
#include <kernel/multitasking.h>
#include <kernel/interrupts.h>
#include <kernel/fs/vfs.h>
#include <kernel/keyboard.h>
#include <kernel/mm.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <errno.h>

uint64_t sys_write(int fd, char* buffer, uint64_t count){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* file = current_task->open_files[fd]->file;
    if(buffer == NULL) return -EFAULT;
    uint32_t ret = write_fs(file,current_task->open_files[fd]->offset,count,(uint8_t*)buffer);
    current_task->open_files[fd]->offset+=ret;
    return ret;
}

uint64_t sys_read(int fd, char* buffer, uint64_t count){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* file = current_task->open_files[fd]->file;
    if(buffer == NULL) return -EFAULT;
    uint32_t ret = read_fs(file,current_task->open_files[fd]->offset,count,(void*)buffer);
    current_task->open_files[fd]->offset+=ret;
    return ret;
}

uint64_t sys_open(char* path){
    return task_open_file(kopen(path),(task_t*)current_task);
}

uint64_t sys_close(int fd){
    return task_close_file(fd);
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

uint64_t sys_lseek(int fd_num, uint64_t offset, uint64_t whence){
    if(fd_num >= MAX_OPEN_FILES) return -EBADF;
    struct file_descriptor* fd = current_task->open_files[fd_num];
    if(fd == NULL) return -EBADF;
    switch(whence){
        case SEEK_SET:
        fd->offset = offset;
        break;
        case SEEK_CUR:
        fd->offset+= offset;
        break;
        case SEEK_END:
        fd->offset = fd->file->length;
        break;
    }
    return fd->offset;
}

uint64_t sys_getcwd(char* buffer, uint64_t size){
    if(size == 0) return -EINVAL;
    if(buffer == NULL) return -EFAULT;
    int length = strlen(current_task->cwd) + 1;
    if(size < length) return -ERANGE;
    if(strlen(current_task->cwd) + 1 <= size){
        memcpy(buffer,current_task->cwd,length);
        return length;
    }
    return (uint64_t)buffer;
}

uint64_t sys_chdir(char* path){
    if(path == NULL) return -EFAULT;
    return task_chdir(path);
}

struct stat{
    uint64_t   st_dev;
    uint64_t   st_ino;
    uint32_t   st_mode;
    uint32_t   st_nlink;
    uint32_t   st_uid;
    uint32_t   st_gid;
    uint64_t   st_rdev;
    uint64_t   st_size;
    uint32_t   st_blksize;
    uint64_t   st_blocks;
};

uint64_t stat_internal(fs_node_t* node,struct stat* stat){
    if(node == NULL || stat == NULL) return -EFAULT;
    stat->st_dev = 0;
    stat->st_ino = node->inode;
    stat->st_nlink = 1;
    stat->st_uid = node->uid;
    stat->st_gid = node->gid;
    stat->st_rdev = 0;
    stat->st_size = node->length;
    stat->st_blksize = 0;
    stat->st_blocks = 0;    
    stat->st_mode = node->mask;
    
    switch(node->flags){
        case FS_DIRECTORY:
        stat->st_mode |= S_IFDIR;
        break;
        case FS_MOUNTPOINT | FS_DIRECTORY:
        stat->st_mode |= S_IFDIR;
        break;
        case FS_CHARDEVICE:
        stat->st_mode |= S_IFCHR;
        break;
        case FS_FILE:
        stat->st_mode |= S_IFREG;
        break;
        case FS_BLOCKDEVICE:
        stat->st_mode |= S_IFBLK;
        break;
        case FS_PIPE:
        stat->st_mode |= S_IFIFO;
        break;
    }
    return 0;
}

uint64_t sys_stat(char* path, void* buf){
    fs_node_t* node = find_file(path);
    if(node == NULL) return -ENOENT;
    struct stat* stat = buf;
    return stat_internal(node,stat);
}

uint64_t sys_lstat(char* path, void* buf){
    fs_node_t* node = find_file(path);
    if(node == NULL) return -ENOENT;
    struct stat* stat = buf;
    return stat_internal(node,stat);
}

uint64_t sys_fstat(int fd, void* buf){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* node = current_task->open_files[fd]->file;
    struct stat* stat = buf;
    return stat_internal(node,stat);
}

struct iovec {
    void   *iov_base;
    uint64_t  iov_len;
};

uint64_t sys_writev(int fd, const struct iovec *iov, int iovcnt){
    int count = 0;
    for(int i = 0; i < iovcnt; i++){
        int len = sys_write(fd,iov[i].iov_base,iov[i].iov_len);
        if(len < 0) return len;
        count+=len;
    }
    return count;
}

uint64_t sys_readdir(int fd, int index, struct dirent* buffer){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* node = current_task->open_files[fd]->file;
    if(buffer == NULL) return -1;

    struct dirent* dirent = readdir_fs(node,index);

    if(dirent != NULL){
        buffer->ino = dirent->ino;
        strcpy(buffer->name,dirent->name);
        return (uint64_t)buffer;
    }
    return 0;
}

uint64_t sys_ioctl(int fd, unsigned long request, void* argp){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* node = current_task->open_files[fd]->file;
    uint64_t ret = ioctl_fs(node,request,argp);
    return ret;
}

uint64_t sys_dup(int oldfd){
    if(oldfd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[oldfd] == NULL) return -EBADF;

    for(int i = 0; i < MAX_OPEN_FILES; i++){
        if(current_task->open_files[i] == NULL){
            current_task->open_files[i] = current_task->open_files[oldfd];
            current_task->open_files[i]->refcount++;
            return i;
        }
    }
    return -EMFILE;
}

uint64_t sys_dup2(int oldfd, int newfd){
    if(oldfd == newfd) return -EINVAL;
    if(newfd >= MAX_OPEN_FILES) return -EBADF;
    if(oldfd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[oldfd] == NULL) return -EBADF;

    current_task->open_files[newfd] = current_task->open_files[oldfd];
    current_task->open_files[newfd]->refcount++;
    open_fs(current_task->open_files[newfd]->file,1,1);
    return newfd;
}