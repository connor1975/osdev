#ifndef MULTITASKING_H
#define MULTITASKING_H

#include <interrupts.h>
#include <stdint.h>
#include <fs/vfs.h>
#include <spinlock.h>

#define USER_HEAP_START (void*)0x200000000 // 8 GB into memory
#define USER_MMAP_START (void*)0x700000000000
#define USER_STACK_TOP  (void*)0x800000000000
#define DEFAULT_STACK_SIZE_PAGES 32

#define MAX_OPEN_FILES 256

struct wait_queue_entry;

struct wait_queue{
    struct wait_queue_entry* head;
    struct wait_queue_entry* tail;
};

typedef struct task{
    struct interrupt_frame context;
    __attribute__((aligned(16))) uint8_t fxsave_region[512];
    struct task* next;
    int user;
    uint64_t cr3;
    int id;
    uint64_t rsp0;
    void* brk_start;
    void* brk;
    void* brk_next_page;
    void* mmap_ptr;
    int state;
    char* cwd;
    struct file_descriptor* open_files[MAX_OPEN_FILES];
    int exit_code;
    void* fs_base;

    struct wait_queue child_event_waiters;
    struct wait_queue exit_waiters;  

    int wait_handled;
    int child_count;
    struct task** children;
    struct task* parent;
} task_t;

enum TASK_STATE{
    TASK_DEAD,
    TASK_READY,
    TASK_STARTING,
    TASK_RUNNING,
    TASK_SLEEPING
};

struct wait_queue_entry{
    task_t* task;
    struct wait_queue_entry* next;
};

void wait_queue_wake_one(struct wait_queue* q);
void wait_queue_wake_all(struct wait_queue* q);
void wait_queue_sleep(struct wait_queue* q);
void initialise_wait_queue(struct wait_queue* q);

void timer_init();
void clone_file_descriptors(task_t* new_task, task_t* old_task);
int alloc_pid();
void multitasking_init();
task_t* find_task(int id);
int create_task(void* func, int user, uint64_t cr3, int argc, char** argv, char** envp);
void schedule(struct interrupt_frame* regs);
int get_task_state(int pid);
void kill_task(int pid, int exit_code);
void push_args(task_t* task, int argc, char** argv,char** envp);
int task_chdir(char* path);
void task_exit(int exit_code);
int task_open_file(fs_node_t* file,task_t*, int flags);
int task_fork();
void add_task(task_t* task);
task_t* task_create_child();
int create_kernel_task(void* func);
int spawn_elf(fs_node_t* file,char** argv, char** envp);
int task_close_file(task_t* task, int fd);
extern void yield();
char** gen_argv(char* string);

extern volatile task_t* current_task;
extern volatile struct interrupt_frame* syscall_context;

#endif