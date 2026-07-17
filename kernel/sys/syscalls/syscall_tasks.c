#include <kernel/multitasking.h>
#include <kernel/msr.h>
#include <kernel/interrupts.h>
#include <kernel/fs/vfs.h>
#include <elf.h>
#include <kernel/heap.h>
#include <kernel/gdt.h>
#include <kernel/mm.h>
#include <string.h>
#include <elf.h>
#include <errno.h>
#include <stdint.h>

uint64_t sys_fork(){
    return task_fork();
}

char** copy_args_to_kernel(char** argv){
    if(argv == NULL) return NULL;
    int argc = 0;
    while(argv[argc] != NULL) argc++;
    
    char** kargv = malloc((argc + 1) * sizeof(char*));
    for(int i = 0; i < argc; i++){
        kargv[i] = malloc(strlen(argv[i]) + 1);
        strcpy(kargv[i], argv[i]);
    }
    kargv[argc] = NULL;
    return kargv;
}

void free_args(char** argv){
    if(argv == NULL) return;
    int i = 0;
    while(argv[i] != NULL){
        free(argv[i]);
        i++;
    }
    free(argv);
}

uint64_t sys_execve(char* pathname, char** argv, char** envp){
    char** kargv = copy_args_to_kernel(argv);
    char** kenvp = copy_args_to_kernel(envp);

    fs_node_t* file = kopen(pathname);
    if(file == NULL) {
        return -ENOENT;
    }
    if(argv == NULL) return -EFAULT;
    task_t* task = (task_t*)current_task;

    void* buffer = malloc(file->length);
    read_fs(file,0,file->length,buffer);
    int ret = exec_elf(task,buffer,kargv,kenvp);
    if(ret < 0) return ret;

    memcpy((void*)syscall_context,(void*)&task->context,sizeof(struct interrupt_frame));
    switch_pml4((void*)task->cr3);
    tss_set_kernel_stack((void*)task->rsp0);

    free_args(kargv);
    free_args(kenvp);
    return 0;
}

uint64_t sys_wait4(int64_t pid, int * status, int options, void * rusage){
    if(pid > 0) {
        task_t* task = find_task(pid);
        if(task == NULL) return -ESRCH;

        while(task->state != TASK_DEAD){
            wait_queue_sleep(&task->exit_waiters);
        }
        
        if(status != NULL) *status = (task->exit_code & 0xff) << 8;
        return pid;
    }else{
        while(1){
            for(int i = 0; i < current_task->child_count; i++){
                if(current_task->children[i]->state == TASK_DEAD && current_task->children[i]->wait_handled == 0){
                    current_task->children[i]->wait_handled = 1;
                    task_t* task = find_task(current_task->children[i]->id);
                    if(status != NULL) *status = (task->exit_code & 0xff) << 8;
                    return current_task->children[i]->id;
                }
            }
            wait_queue_sleep(&((task_t*)current_task)->child_event_waiters);
        }
    }
}

uint64_t sys_get_pid(){
    return current_task->id;
}

uint64_t sys_getpgid(int pid){
    if(pid == 0){
        return current_task->pgid;
    }else{
        task_t* task = find_task(pid);
        if(task == NULL){
            return -ESRCH;
        }
        return task->pgid;
    }
}

uint64_t sys_setpgid(int pid, int pgid){
    if(pgid < 0)
        return -EINVAL;

    if(pid == 0)
        pid = current_task->id;

    if(pgid == 0)
        pgid = pid;

    task_t *task = find_task(pid);
    if(task == NULL)
        return -ESRCH;

    task->pgid = pgid;
    return 0;
}
uint64_t sys_prctl(int code, void* addr){
    switch(code){
        case 0x1002:
            current_task->fs_base = addr;
            wrmsr(MSR_FSBASE,(uint64_t)addr);
            return 0;
        break;
    }
    return -EINVAL;
}

void sys_exit(int exit_code){
    task_exit(exit_code);
}

void sys_exit_group(int exit_code){
    sys_exit(exit_code);
}

uint64_t sys_set_tid_address(int* tidptr){
    *tidptr = current_task->id;
    return current_task->id;
}

void sys_sched_yield(){
    schedule((struct interrupt_frame*)syscall_context);
}