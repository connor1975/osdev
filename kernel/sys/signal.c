#include <multitasking.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

extern task_t* task_list;

#define SIG_DFL_IGNORE 0
#define SIG_DFL_TERM 1

const uint8_t signal_default_actions[NSIG] = {
    SIG_DFL_IGNORE,
    [SIGHUP] = SIG_DFL_TERM,
    [SIGINT] = SIG_DFL_TERM,
    [SIGQUIT] = SIG_DFL_TERM,
    [SIGILL] = SIG_DFL_TERM,
    [SIGTRAP] = SIG_DFL_TERM,
    [SIGABRT] = SIG_DFL_TERM,
    [SIGBUS] = SIG_DFL_TERM,
    [SIGFPE] = SIG_DFL_TERM,
    [SIGKILL] = SIG_DFL_TERM,
    [SIGUSR1] = SIG_DFL_TERM,
    [SIGSEGV] = SIG_DFL_TERM,
    [SIGUSR2] = SIG_DFL_TERM,
    [SIGPIPE] = SIG_DFL_TERM,
    [SIGALRM] = SIG_DFL_TERM,
    [SIGTERM] = SIG_DFL_TERM,
    [SIGSTKFLT] = SIG_DFL_TERM,
    [SIGCHLD] = SIG_DFL_IGNORE,
    [SIGCONT] = SIG_DFL_IGNORE,
    [SIGSTOP] = SIG_DFL_IGNORE,
    [SIGTSTP] = SIG_DFL_IGNORE,
    [SIGTTIN] = SIG_DFL_IGNORE,
    [SIGTTOU] = SIG_DFL_IGNORE,
    [SIGURG] = SIG_DFL_IGNORE,
    [SIGXCPU] = SIG_DFL_IGNORE,
    [SIGXFSZ] = SIG_DFL_TERM,
    [SIGVTALRM] = SIG_DFL_TERM,
    [SIGPROF] = SIG_DFL_TERM,
    [SIGWINCH] = SIG_DFL_IGNORE,
    [SIGIO] = SIG_DFL_IGNORE,
    [SIGPWR] = SIG_DFL_IGNORE,
    [SIGSYS] = SIG_DFL_TERM
};

int signal_perform_default(task_t* task, int signum){
    uint8_t default_action = signal_default_actions[signum];
    if(default_action == SIG_DFL_TERM){
        kill_task(task->id,0x80 + signum);
        return 1;
    }
    
    return 0;
}

int deliver_signal(task_t* task, int signum){
    task->signal_state.pending = (task->signal_state.pending & ~(1ULL << signum));
    task->signal_interrupted = 1;
    memcpy(&task->saved_signal_context,&task->context,sizeof(struct interrupt_frame));
    
    struct sigaction* signal = &task->signal_state.handlers[signum];
    
    if(signal->sa_handler == SIG_DFL){
        return signal_perform_default(task,signum);
    }
    if(signal->sa_handler == SIG_IGN){
        task->signal_interrupted = 0;
        return 0;
    }
    task->context.rip = (uintptr_t)signal->sa_handler;
    push_to_stack(task,&signal->sa_restorer,sizeof(void*));
    return 0;
}

int get_next_pending_signal(task_t* task){
    uint64_t pending = task->signal_state.pending;
    uint64_t masked = task->signal_state.masked;
    for(int i = 1; i < NSIG; i++){
        if(pending & (1ULL << i) && !(masked & (1ULL << i))){
            return i;
        }
    }
    return 0;
}

int send_signal(int pid, int sig){
    task_t* task = find_task(pid);

    if(task == NULL)
        return -ESRCH;

    if(sig == 0)
        return 0;

    if(sig < NSIG && sig > 0){
        if(task->signal_state.handlers[sig].sa_handler == SIG_DFL){
            task->signal_interrupted = 1;
            signal_perform_default(task,sig);
        }else{
            task->signal_state.pending |= (1ULL << sig);
            if(task->state != TASK_DEAD) task->state = TASK_READY;
        }
        return 0;
    }
    else{
        return -EINVAL;
    }
}

int kill(int pid, int sig){
    if(pid == 0){
        task_t* task = task_list;
        while(task != NULL){
            if(task->pgid == current_task->pgid && task->id != current_task->id){
                send_signal(task->id,sig);
            }
            task = task->next;
        }
        return 0;
    }
    if(pid < 0){
        task_t* task = task_list;
        while(task != NULL){
            if(task->pgid == -pid){
                send_signal(task->id,sig);
            }
            task = task->next;
        }
        return 0;
    }
    task_t* task = find_task(pid);
    if(task == NULL) return -ESRCH;
    return send_signal(task->id,sig);
}

void reset_signal_handlers(task_t* task){
    for(int i = 0; i < NSIG; i++){
        if(task->signal_state.handlers[i].sa_handler != SIG_DFL && task->signal_state.handlers[i].sa_handler != SIG_IGN){
            task->signal_state.handlers[i].sa_handler = SIG_DFL;
        }
    }
    task->signal_state.pending = 0;
}