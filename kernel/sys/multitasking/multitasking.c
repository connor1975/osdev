#include <kernel/common.h>
#include <kernel/interrupts.h>
#include <kernel/gdt.h>
#include <kernel/mm.h>
#include <stdint.h>
#include <stddef.h>
#include <elf.h>
#include <string.h>
#include <kernel/multitasking.h>
#include <kernel/fs/vfs.h>
#include <errno.h>

uint64_t ticks = 0;

task_t* task_list = NULL;
volatile task_t* current_task = NULL;

int next_pid = 0;

int alloc_pid(){
    return next_pid++;
}

void get_next_ready_task(){
    while(1){
        if(current_task->next != NULL){
            current_task = current_task->next;
        }else{
            current_task = task_list;
        }
        if(current_task->state == TASK_READY) return;
    }
}

void schedule(struct interrupt_frame* regs){
    memcpy((void*)&current_task->context, (void*)regs, sizeof(struct interrupt_frame));
    asm volatile("fxsave %0 "::"m"(current_task->fxsave_region));
    if(current_task->state == TASK_RUNNING) current_task->state = TASK_READY; 

    get_next_ready_task();

    current_task->state = TASK_RUNNING;
    memcpy((void*)regs,(void*)&current_task->context,sizeof(struct interrupt_frame));
    switch_pml4((void*)current_task->cr3);
    tss_set_kernel_stack((void*)current_task->rsp0);
    asm volatile("fxrstor  %0 "::"m"(current_task->fxsave_region));
    wrmsr(MSR_FSBASE,(uint64_t)current_task->fs_base);
}

void timer_irq(struct interrupt_frame* regs){
    ticks++;
    schedule(regs);
}

int get_task_state(int pid){
    task_t* task = find_task(pid);
    return task->state;
}

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

int task_open_file(fs_node_t* file, task_t* task){
    if(file == NULL) return -ENOENT;
    int i = 0;
    for(i = 0; i < MAX_OPEN_FILES; i++){
        if(task->open_files[i] == NULL) break;
    }
    task->open_files[i] = malloc(sizeof(struct file_descriptor));
    task->open_files[i]->file = file;
    task->open_files[i]->offset = 0;
    task->open_files[i]->refcount = 1;
    return i;
}

int task_close_file(int fd){
    if(fd >= MAX_OPEN_FILES) return -EBADF;
    if(current_task->open_files[fd] == NULL) return -EBADF;
    struct file_descriptor* file_descriptor = current_task->open_files[fd];
    if(file_descriptor->refcount > 1){
        file_descriptor->refcount--;
        return 0;
    }
    close_fs(file_descriptor->file);
    free(file_descriptor);
    current_task->open_files[fd] = NULL;
    return 0;
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
    task_t* new_task = malloc(sizeof(task_t));
    memset(new_task, 0, sizeof(task_t));
    new_task->id = alloc_pid();
    new_task->children = NULL;
    new_task->child_count = 0;
    new_task->wait_handled = 0;
    new_task->parent = (task_t*)current_task;
    current_task->children = realloc(current_task->children,(current_task->child_count + 1) * sizeof(task_t*));
    current_task->children[current_task->child_count] = new_task;
    current_task->child_count++;
    return new_task;
}

int create_kernel_task(void* func){
    irq_disable();
    task_t* new_task = task_create_child();

    clone_file_descriptors(new_task,(task_t*)current_task);

    memset(new_task->fxsave_region,0,512);
    memset((void*)&new_task->context,0,sizeof(struct interrupt_frame));
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
    memcpy(new_task->cwd,"/\0",2);

    task_add(new_task);
    irq_enable();
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

    irq_disable();

    task_t* newtask = task_create_child();
    int ret = exec_elf(newtask,buffer,argv,envp);
    if(ret < 0) return ret;

    uint64_t cwdlen = strlen(current_task->cwd) + 1;
    newtask->cwd = malloc(cwdlen);
    memcpy(newtask->cwd,current_task->cwd,cwdlen);

    clone_file_descriptors(newtask,(task_t*)current_task);

    newtask->fs_base = 0;
    newtask->exit_code = 0;

    task_add(newtask);
    irq_enable();
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

    void* newpd = allocate_new_pd();
    clone_pd(phys_to_virt(newpd));
    new_task->cr3 = (uint64_t)newpd;
    
    task_add(new_task);
    return new_task->id;
}

task_t* find_task(int id){
    task_t* cur = task_list;
    do{
        if(cur->id == id) return cur;
        cur = cur->next;
    }while(cur != NULL);
    return NULL;
}

void sleep(uint64_t ms){
    uint64_t target = ticks + ms;
    while(ticks < target);
}

void kill_task(int pid, int exit_code){
    irq_disable();
    task_t* task = find_task(pid);
    task->state = TASK_DEAD;
    if(task->user) free_process_memory(task->cr3);
    task->exit_code = exit_code;
    for(int i = 0; i < MAX_OPEN_FILES; i++){
        task_close_file(i);
    }
    irq_enable();
    if(current_task->id == pid) while(1)asm volatile("hlt"); // wait for schedule
}

void task_exit(int exit_code){
    kill_task(current_task->id,exit_code);
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
    kernel_task.state = TASK_READY;
    
    kernel_task.cwd = malloc(2);
    kernel_task.cwd[0] = '/';
    kernel_task.cwd[1] = '\0';
    
    kernel_task.child_count = 0;
    kernel_task.children = NULL;
    kernel_task.parent = NULL;
    kernel_task.wait_handled = 0;

    memset(&kernel_task.open_files,0,sizeof(struct file_descriptor*) * MAX_OPEN_FILES);
    task_open_file(kopen("/dev/stdin"),&kernel_task);
    task_open_file(kopen("/dev/stdout"),&kernel_task);
    task_open_file(kopen("/dev/stderr"),&kernel_task);

    int divisor = 1193180 / 1000;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
    register_irq_handler(0,timer_irq);

}