#include <heap.h>
#include <interrupts.h>
#include <gdt.h>
#include <mm.h>
#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include <string.h>
#include <multitasking.h>
#include <fs/vfs.h>
#include <errno.h>

void clone_file_descriptors(task_t* new_task, task_t* old_task){
    memset(new_task->open_files,0,sizeof(struct file_descriptor*) * MAX_OPEN_FILES);
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        if(old_task->open_files[i] != NULL){
            new_task->open_files[i] = malloc(sizeof(struct file_descriptor));
            memcpy(new_task->open_files[i],old_task->open_files[i],sizeof(struct file_descriptor));
            new_task->open_files[i]->refcount++;
            open_fs(new_task->open_files[i]->file,1,1);
        }
    }
}

int task_chdir(char* path){
    if(path == NULL) return -EFAULT;
    char* newdir = vfs_absolute_path(current_task->cwd,path);
    fs_node_t* node = kopen(newdir);
    if(node == NULL) return -ENOENT;
    if(!(node->flags & FS_DIRECTORY)) return -ENOTDIR;
    current_task->cwd = realloc(current_task->cwd,strlen(newdir) + 1);
    strcpy(current_task->cwd,newdir);
    free(newdir);
    return 0;
}

int task_open_file(fs_node_t* file, task_t* task, int flags){
    if(file == NULL) return -ENOENT;
    int i = 0;
    for(i = 0; i < MAX_OPEN_FILES; i++){
        if(task->open_files[i] == NULL) break;
    }
    open_fs(file,flags & O_RDONLY, flags & O_WRONLY);
    task->open_files[i] = malloc(sizeof(struct file_descriptor));
    task->open_files[i]->file = file;
    task->open_files[i]->offset = 0;
    task->open_files[i]->refcount = 1;
    task->open_files[i]->flags = flags;
    if(flags & O_TRUNC){
        truncate_fs(file,0); 
    }
    return i;
}

int task_close_file(task_t* task, int fd){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(task->open_files[fd] == NULL) return -EBADF;
    struct file_descriptor* file_descriptor = task->open_files[fd];
    if(file_descriptor->refcount > 1){
        file_descriptor->refcount--;
        return 0;
    }
    close_fs(file_descriptor->file);
    free(file_descriptor);
    task->open_files[fd] = NULL;
    return 0;
}