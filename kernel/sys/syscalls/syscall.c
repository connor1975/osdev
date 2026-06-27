#include <common.h>
#include <interrupts.h>
#include <msr.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef uint64_t (*syscall)(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);

extern uint64_t sys_exit(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_write(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_read(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_open(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_lseek(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_brk(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_get_pid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_kill(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_close(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_fork(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_execve(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getcwd(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_stat(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_fstat(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_chdir(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_mmap(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_munmap(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_wait4(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_prctl(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_writev(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_exit_group(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_set_tid_address(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getdents(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_readdir(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_ioctl(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_gettimeofday(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_pipe(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_sched_yield(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_dup(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_dup2(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_lstat(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_uname(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_truncate(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_ftruncate(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_unlink(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_link(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_mkdir(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_rmdir(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_rename(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_access(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_geteuid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getgid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getegid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getuid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_sigaction(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_sigprocmask(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_sigreturn(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_getpgid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_setpgid(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);
extern uint64_t sys_nanosleep(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9);

syscall syscall_table[] = {
    (syscall)sys_exit, // 0
    (syscall)sys_write, // 1
    (syscall)sys_read, // 2
    (syscall)sys_open, // 3
    (syscall)sys_lseek, // 4
    (syscall)sys_brk, // 5
    (syscall)sys_get_pid, // 6
    (syscall)sys_kill, // 7
    (syscall)sys_close, // 8
    (syscall)sys_fork, // 9
    (syscall)sys_execve, // 10
    (syscall)sys_getcwd, // 11
    (syscall)sys_stat, // 12
    (syscall)sys_fstat, // 13
    (syscall)sys_chdir, // 14
    (syscall)sys_mmap, // 15
    (syscall)sys_munmap, // 16
    (syscall)sys_sched_yield, // 17
    (syscall)sys_wait4, // 18
    (syscall)sys_prctl, // 19
    (syscall)sys_writev, // 20
    (syscall)sys_exit_group, // 21
    (syscall)sys_set_tid_address, // 22
    (syscall)sys_readdir, // 23
    (syscall)sys_ioctl, // 24
    (syscall)sys_gettimeofday, // 25
    (syscall)sys_pipe, // 26
    (syscall)sys_dup, // 27
    (syscall)sys_dup2, // 28
    (syscall)sys_lstat, // 29
    (syscall)sys_uname, // 30
    (syscall)sys_truncate, // 31
    (syscall)sys_ftruncate, // 32
    (syscall)sys_unlink, // 33
    (syscall)sys_link, // 34
    (syscall)sys_mkdir, // 35
    (syscall)sys_rmdir, // 36
    (syscall)sys_rename, // 37
    (syscall)sys_access, // 38
    (syscall)sys_geteuid, //39
    (syscall)sys_getgid, //40
    (syscall)sys_getegid, //41
    (syscall)sys_getuid, // 42
    (syscall)sys_sigaction, // 43
    (syscall)sys_sigprocmask, // 44
    (syscall)sys_sigreturn, // 45
    (syscall)sys_getpgid, // 46
    (syscall)sys_setpgid, // 47
    (syscall)sys_nanosleep, // 48
    0
};

int num_syscalls;

volatile struct interrupt_frame* syscall_context = NULL;

void syscall_handler(struct interrupt_frame* regs){
    syscall_context = regs;
    if(regs->rax < num_syscalls){
        regs->rax = syscall_table[regs->rax](regs->rdi,regs->rsi,regs->rdx,regs->r10,regs->r8,regs->r9);
        return;
    }
    else{
        printf("Invalid Syscall number %llu",regs->rax);
        regs->rax = -ENOSYS;
        return;
    }
}

extern void syscall_entry();

void syscall_install(){
    int i = 0;
    while(syscall_table[i] != (void*)0){
        i++;
    }
    num_syscalls = i;
    register_isr_handler(0x80,syscall_handler);
}