#include <common.h>
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

#define TASK_TIME_SLICE 30 // milliseconds

volatile uint64_t ticks = 0;

task_t* task_list = NULL;
volatile task_t* current_task = NULL;

int next_pid = 0;

struct sleep_list_entry{
    struct sleep_list_entry* prev;
    task_t* task;
    uint64_t sleep_target;
    struct sleep_list_entry* next;
};

struct sleep_list_entry* sleep_list = NULL;

int alloc_pid(){
    return next_pid++;
}

task_t* sleep_list_wake(struct sleep_list_entry* entry){
    uint64_t flags = irq_disable_save();
    entry->task->state = TASK_READY;
    task_t* task = entry->task;
    if(entry->prev != NULL && entry->next != NULL){
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
    }
    if(entry->prev != NULL && entry->next == NULL){
        entry->prev->next = NULL;
    }
    if(entry->prev == NULL && entry->next != NULL){
        sleep_list = entry->next;
        entry->next->prev = NULL;
    }
    if(entry->prev == NULL && entry->next == NULL){
        sleep_list = NULL;
    }
    free(entry);
    irq_restore(flags);
    return task;
}

void sleep_list_append(task_t* task, uint64_t sleep_target){
    uint64_t flags = irq_disable_save();
    struct sleep_list_entry* entry = malloc(sizeof(struct sleep_list_entry));
    entry->task = task;
    entry->sleep_target = sleep_target;
    entry->next = NULL;
    if(sleep_list == NULL){
        entry->prev = NULL;
        sleep_list = entry;
    }else{
        struct sleep_list_entry* ptr = sleep_list;
        while(ptr->next != NULL){
            ptr = ptr->next;
        }
        ptr->next = entry;
        entry->prev = ptr;
    }
    task->state = TASK_SLEEPING;
    irq_restore(flags);
}

void wake_sleeping_tasks(){
    struct sleep_list_entry* entry = sleep_list;
    while(entry != NULL){
        struct sleep_list_entry* next = entry->next;
        if(entry->sleep_target <= ticks) {
            sleep_list_wake(entry);
        }
        entry = next;
    }
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
    wake_sleeping_tasks();
    if(!(ticks % TASK_TIME_SLICE) || current_task->state != TASK_RUNNING){
        schedule(regs);
    }
}

task_t* find_task(int id){
    task_t* cur = task_list;
    do{
        if(cur->id == id) return cur;
        cur = cur->next;
    }while(cur != NULL);
    return NULL;
}

void wait_queue_wake_one(struct wait_queue* q){
    uint64_t flags = irq_disable_save();
    if(q->tail == NULL){
        irq_restore(flags);
        return;
    }

    if(q->head == q->tail){
        if(q->tail->task->state != TASK_DEAD)
            q->tail->task->state = TASK_READY;
        free(q->tail);
        q->head = NULL;
        q->tail = NULL;
    }else{
        if(q->head->task->state != TASK_DEAD)
            q->head->task->state = TASK_READY;
        struct wait_queue_entry* next = q->head->next;
        free(q->head);
        q->head = next;
    }

    irq_restore(flags);
}

void wait_queue_wake_all(struct wait_queue* q){
    uint64_t flags = irq_disable_save();

    struct wait_queue_entry* entry = q->head;
    while(entry != NULL){
        if(entry->task->state != TASK_DEAD)
            entry->task->state = TASK_READY;
        entry = entry->next;
    }
    q->head = NULL;
    q->tail = NULL;

    irq_restore(flags);
}

void wait_queue_sleep(struct wait_queue* q){    
    uint64_t flags = irq_disable_save();
    struct wait_queue_entry* entry = malloc(sizeof(struct wait_queue_entry));
    entry->next = NULL;
    entry->task = (task_t*)current_task;
    
    if(q->head == NULL){
        q->head = entry;
        q->tail = entry;
    }else{
        q->tail->next = entry;
        q->tail = entry;
        
    }
    current_task->state = TASK_SLEEPING;
    irq_restore(flags);
    yield();
}

void initialise_wait_queue(struct wait_queue* q){
    q->head = NULL;
    q->tail = NULL;
}

void sleep(uint64_t ms){
    sleep_list_append(current_task,ticks+ms);
    yield();
}

void timer_init(){
    int divisor = 1193180 / 1000;
    outb(0x43, 0x36);
    outb(0x40, divisor & 0xFF);
    outb(0x40, divisor >> 8);
    register_irq_handler(0,&timer_irq);
}