#include <stdint.h>
#include <multitasking.h>
#include <errno.h>
#include <sys/signal.h>

uint64_t sys_kill(int id, int sig){
    return kill(id, sig);
}

uint64_t sys_sigaction(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9){
    return -ENOSYS;
}

uint64_t sys_sigprocmask(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9){
    return -ENOSYS;
}

uint64_t sys_sigreturn(uint64_t rdi, uint64_t rsi, uint64_t rdx, uint64_t r10, uint64_t  r8, uint64_t r9){
    return -ENOSYS;
}