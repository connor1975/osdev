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
    irq_enable();
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    struct file_descriptor* file_descriptor = current_task->open_files[fd];
    if(file_descriptor == NULL) return -EBADF;
    int mode = file_descriptor->flags & 0x3;
    if(mode == O_RDONLY) return -EBADF;

    if(file_descriptor->flags & O_APPEND){
        file_descriptor->offset = file_descriptor->file->length;
    }

    fs_node_t* file = file_descriptor->file;
    if(buffer == NULL) return -EFAULT;
    uint32_t ret = write_fs(file,file_descriptor->offset,count,(uint8_t*)buffer);
    file_descriptor->offset+=ret;
    
    return ret;
}

uint64_t sys_read(int fd, char* buffer, uint64_t count){
    irq_enable();
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    struct file_descriptor* file_descriptor = current_task->open_files[fd];
    if(file_descriptor == NULL) return -EBADF;
    int mode = file_descriptor->flags & 0x3;
    if(mode == O_WRONLY) return -EBADF;
    
    fs_node_t* file = file_descriptor->file;
    if(buffer == NULL) return -EFAULT;
    uint32_t ret = read_fs(file,file_descriptor->offset,count,(void*)buffer);
    file_descriptor->offset+=ret;

    return ret;
}

uint64_t sys_open(char* path, int flags){
    irq_enable();
    if(flags & O_CREAT && find_file(path) == NULL){
        create_file_fs(find_file_dir(path),get_filename_from_path(path));
    }
    return task_open_file(find_file(path),(task_t*)current_task,flags);
}

uint64_t sys_close(int fd){
    irq_enable();
    return task_close_file(fd);
}

uint64_t sys_truncate(const char* path, uint64_t length){
    fs_node_t* file = find_file((char*)path);
    if(file == NULL) return -ENOENT;
    truncate_fs(file,length);
    return 0;
}

uint64_t sys_ftruncate(uint32_t fd, uint64_t length){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    struct file_descriptor* file_descriptor = current_task->open_files[fd];
    if(file_descriptor == NULL) return -EBADF;
    truncate_fs(file_descriptor->file,length);
    return 0;
}

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

uint64_t sys_lseek(int fd_num, uint64_t offset, uint64_t whence){
    irq_enable();
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
    irq_enable();
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
    irq_enable();
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
    irq_enable();
    fs_node_t* node = find_file(path);
    if(node == NULL) return -ENOENT;
    struct stat* stat = buf;
    return stat_internal(node,stat);
}

uint64_t sys_lstat(char* path, void* buf){
    irq_enable();
    fs_node_t* node = find_file(path);
    if(node == NULL) return -ENOENT;
    struct stat* stat = buf;
    return stat_internal(node,stat);
}

uint64_t sys_fstat(int fd, void* buf){
    irq_enable();
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
    irq_enable();
    int count = 0;
    for(int i = 0; i < iovcnt; i++){
        int len = sys_write(fd,iov[i].iov_base,iov[i].iov_len);
        if(len < 0) return len;
        count+=len;
    }
    return count;
}

uint64_t sys_readdir(int fd, int index, struct dirent* buffer){
    irq_enable();
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
    irq_enable();
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    fs_node_t* node = current_task->open_files[fd]->file;
    uint64_t ret = ioctl_fs(node,request,argp);
    return ret;
}

uint64_t sys_dup(int oldfd){
    irq_enable();
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
    irq_enable();
    if(oldfd == newfd) return -EINVAL;
    if(newfd >= MAX_OPEN_FILES) return -EBADF;
    if(oldfd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[oldfd] == NULL) return -EBADF;

    current_task->open_files[newfd] = current_task->open_files[oldfd];
    current_task->open_files[newfd]->refcount++;
    open_fs(current_task->open_files[newfd]->file,1,1);
    return newfd;
}