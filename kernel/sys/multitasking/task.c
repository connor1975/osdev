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

extern task_t* task_list;

int get_task_state(int pid){
    task_t* task = find_task(pid);
    return task->state;
}

void task_add(task_t* task){
    task->state = TASK_READY;

    // append the task to the end of the task list
    task->next = NULL;
    task_t* cur = task_list;
    while(cur->next != NULL) cur = cur->next;
    cur->next = task;
}

task_t* task_create_child(){
    uint64_t flags = irq_disable_save();
    task_t* new_task = malloc(sizeof(task_t));
    memset(new_task, 0, sizeof(task_t));
    new_task->id = alloc_pid();
    new_task->children = NULL;
    new_task->child_count = 0;
    new_task->wait_handled = 0;
    new_task->state = TASK_STARTING;
    memset(new_task->fxsave_region,0,512);
    memset((void*)&new_task->context,0,sizeof(struct interrupt_frame));

    new_task->parent = (task_t*)current_task;
    current_task->children = realloc(current_task->children,(current_task->child_count + 1) * sizeof(task_t*));
    current_task->children[current_task->child_count] = new_task;
    current_task->child_count++;
    irq_restore(flags);
    return new_task;
}

int create_kernel_task(void* func){
    uint64_t flags = irq_disable_save();
    task_t* new_task = task_create_child();

    clone_file_descriptors(new_task,(task_t*)current_task);

    new_task->context.cs = KERNEL_CODE;
    new_task->context.ss = KERNEL_DATA;
    new_task->context.rflags = 0x200; // Interrupt enable
    new_task->context.rsp = (uint64_t)phys_to_virt(allocate_frames(DEFAULT_STACK_SIZE_PAGES)) + (DEFAULT_STACK_SIZE_PAGES * 4096);
    new_task->context.rip = (uint64_t)func;
    new_task->user = 0;
    new_task->rsp0 = 0;
    new_task->cr3 = (uint64_t)allocate_new_pd();
    new_task->user = 0;
    new_task->cwd = malloc(2);
    new_task->pgid = new_task->id;
    memcpy(new_task->cwd,"/\0",2);

    task_add(new_task);
    irq_restore(flags);
    return new_task->id;
}

int spawn_elf(fs_node_t* file,char** argv, char** envp){
    if(argv == NULL || file == NULL) return -EINVAL;
    
    void* buffer = malloc(file->length);
    read_fs(file,0,file->length,buffer);
    if(verify_elf(buffer)){
        free(buffer);
        return -ENOEXEC;
    }

    uint64_t flags = irq_disable_save();

    task_t* newtask = task_create_child();
    int ret = exec_elf(newtask,buffer,argv,envp);
    if(ret < 0) return ret;

    uint64_t cwdlen = strlen(current_task->cwd) + 1;
    newtask->cwd = malloc(cwdlen);
    memcpy(newtask->cwd,current_task->cwd,cwdlen);

    clone_file_descriptors(newtask,(task_t*)current_task);

    newtask->fs_base = 0;
    newtask->exit_code = 0;
    newtask->pgid = newtask->id;

    task_add(newtask);
    irq_restore(flags);
    return newtask->id;
}

int task_fork(){
    task_t* new_task = task_create_child();
    new_task->user = 1;

    new_task->cwd = malloc(strlen(current_task->cwd) + 1);
    memcpy(new_task->cwd,current_task->cwd,strlen(current_task->cwd) + 1);
    
    clone_file_descriptors(new_task,(task_t*)current_task);
    
    memcpy(new_task->fxsave_region,((task_t*)current_task)->fxsave_region,512);

    new_task->mmap_ptr = current_task->mmap_ptr;
    new_task->brk = current_task->brk;
    new_task->brk_start = current_task->brk_start;
    new_task->brk_next_page = current_task->brk_next_page;
    
    memset((void*)&new_task->context,0,sizeof(struct interrupt_frame));
    memcpy((void*)&new_task->context,(void*)syscall_context,sizeof(struct interrupt_frame));
    new_task->rsp0 = (uint64_t)phys_to_virt(allocate_frames(DEFAULT_STACK_SIZE_PAGES)) + (4096 * DEFAULT_STACK_SIZE_PAGES); // setup kernel stack
    new_task->context.cs = syscall_context->cs;
    new_task->context.ss = syscall_context->ss;
    new_task->context.rflags = 0x200; // Interrupt enable
    new_task->context.rsp = syscall_context->rsp;
    new_task->context.rip = syscall_context->rip;
    new_task->context.rax = 0;
    new_task->pgid = current_task->pgid;

    void* newpd = allocate_new_pd();
    clone_pd(phys_to_virt(newpd));
    new_task->cr3 = (uint64_t)newpd;
    
    task_add(new_task);
    return new_task->id;
}

void kill_task(int pid, int exit_code){
    irq_disable();
    task_t* task = find_task(pid);
    task->state = TASK_DEAD;
    if(task->user) free_process_memory(task->cr3);
    task->exit_code = exit_code;
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        task_close_file(task,i);
    }
    wait_queue_wake_all(&task->exit_waiters);
    wait_queue_wake_all(&task->parent->child_event_waiters);
    if(current_task->id == pid) yield();
}

void task_exit(int exit_code){
    kill_task(current_task->id,exit_code);
}

int kill(int pid, int sig){
    if(pid == 0){
        task_t* task = task_list;
        while(task != NULL){
            if(task->pgid == current_task->pgid && task->id != current_task->id){
                kill_task(task->id,sig);
            }
            task = task->next;
        }
        return 0;
    }
    if(pid < 0){
        task_t* task = task_list;
        while(task != NULL){
            if(task->pgid == -pid){
                kill_task(task->id,sig);
            }
            task = task->next;
        }
        return 0;
    }
    task_t* task = find_task(pid);
    if(task == NULL) return -ESRCH;
    kill_task(task->id,sig);
    return 0;
}

void task_open_stdio(){
    task_open_file(find_file("/dev/stdin"),(task_t*)current_task, O_RDONLY);
    task_open_file(find_file("/dev/stdout"),(task_t*)current_task, O_WRONLY);
    task_open_file(find_file("/dev/stderr"),(task_t*)current_task, O_WRONLY);
}

void multitasking_init(){
    uint64_t cr3;
    asm volatile ("movq %%cr3, %0" : "=r" (cr3));
    
    static task_t kernel_task;
    task_list = &kernel_task;
    current_task = &kernel_task;
    kernel_task.next = NULL;
    kernel_task.cr3 = cr3;
    kernel_task.user = 0;
    kernel_task.rsp0 = 0;
    kernel_task.id = alloc_pid();
    kernel_task.state = TASK_RUNNING;
    kernel_task.pgid = kernel_task.id;
    
    kernel_task.cwd = malloc(2);
    kernel_task.cwd[0] = '/';
    kernel_task.cwd[1] = '\0';
    
    kernel_task.child_count = 0;
    kernel_task.children = NULL;
    kernel_task.parent = NULL;
    kernel_task.wait_handled = 0;

    memset(&kernel_task.open_files,0,sizeof(struct file_descriptor*) * MAX_OPEN_FILES);

    timer_init();
}