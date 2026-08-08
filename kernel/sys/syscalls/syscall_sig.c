#include <stdint.h>
#include <multitasking.h>
#include <errno.h>
#include <sys/signal.h>
#include <string.h>

uint64_t sys_kill(int id, int sig){
    return kill(id, sig);
}

uint64_t sys_sigaction(int signum, const struct sigaction* act, struct sigaction* oldact){
    if(oldact != NULL)
        memcpy(oldact,&((task_t*)current_task)->signal_state.handlers[signum],sizeof(struct sigaction));
    if(act != NULL)
        memcpy(&((task_t*)current_task)->signal_state.handlers[signum],(void*)act,sizeof(struct sigaction));
    return 0;
}

uint64_t sys_sigprocmask(int how, const sigset_t* set, sigset_t* oldset){
    if(oldset != NULL)
        *oldset = current_task->signal_state.masked;

    switch(how){
        case SIG_UNBLOCK:
            current_task->signal_state.masked &= ~(*set);
            break;
        case SIG_BLOCK:
            current_task->signal_state.masked |= *set;
            break;
        case SIG_SETMASK:
            current_task->signal_state.masked = *set;
            break;   
    }
    return 0;
}

uint64_t sys_sigreturn(){
    memcpy(&((task_t*)current_task)->context,&((task_t*)current_task)->saved_signal_context,sizeof(struct interrupt_frame));
    memcpy((struct interrupt_frame*)current_task->syscall_context,&((task_t*)current_task)->context,sizeof(struct interrupt_frame));
    current_task->signal_interrupted = 0;
    return 0;
}