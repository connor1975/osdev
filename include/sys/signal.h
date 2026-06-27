#ifndef SYS_SIGNAL_H
#define SYS_SIGNAL_H

#include <stdint.h>
#include <sys/types.h>

#define SIGHUP           1
#define SIGINT           2
#define SIGQUIT          3
#define SIGILL           4
#define SIGTRAP          5
#define SIGABRT          6
#define SIGIOT           6
#define SIGBUS           7
#define SIGFPE           8
#define SIGKILL          9
#define SIGUSR1         10
#define SIGSEGV         11
#define SIGUSR2         12
#define SIGPIPE         13
#define SIGALRM         14
#define SIGTERM         15
#define SIGSTKFLT       16
#define SIGCHLD         17
#define SIGCONT         18
#define SIGSTOP         19
#define SIGTSTP         20
#define SIGTTIN         21
#define SIGTTOU         22
#define SIGURG          23
#define SIGXCPU         24
#define SIGXFSZ         25
#define SIGVTALRM       26
#define SIGPROF         27
#define SIGWINCH        28
#define SIGIO           29
#define SIGPOLL         29
#define SIGPWR          30
#define SIGSYS          31
#define SIGUNUSED       31

#define NSIG 32

typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);
typedef sighandler_t _sig_func_ptr;

typedef struct {
    int      si_signo; 
    int      si_errno; 
    int      si_code;  
    pid_t    si_pid;   
    uid_t    si_uid;   
    void    *si_addr;  
    int      si_status;
} siginfo_t;

struct sigaction {
    void     (*sa_handler)(int);
    void     (*sa_sigaction)(int, siginfo_t *, void *);
    sigset_t   sa_mask;
    int        sa_flags;
    void     (*sa_restorer)(void);
};

union sigval {
  int    sival_int;
  void  *sival_ptr;
};

int sigpending (sigset_t *);
int sigsuspend (const sigset_t *);
int sigwait (const sigset_t *, int *);

#define sigaddset(what,sig) (*(what) |= ((sigset_t)1<<(sig)), 0)
#define sigdelset(what,sig) (*(what) &= ~((sigset_t)1<<(sig)), 0)
#define sigemptyset(what)   (*(what) = 0, 0)
#define sigfillset(what)    (*(what) = ~((sigset_t)0), 0)
#define sigismember(what,sig) (((*(what)) & ((sigset_t)1<<(sig))) != 0)

#define SA_NOCLDSTOP   0x00000001
#define SA_NOCLDWAIT   0x00000002
#define SA_SIGINFO     0x00000004
#define SA_RESTART     0x10000000
#define SA_NODEFER     0x40000000
#define SA_RESETHAND   0x80000000
#define SA_ONSTACK     0x08000000
#define SA_RESTORER    0x04000000

#define SIG_SETMASK 0
#define SIG_BLOCK 1
#define SIG_UNBLOCK 2

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

int raise(int sig);
int sigaction (int signum, const struct sigaction * act, struct sigaction * oldact);
int sigprocmask (int how, const sigset_t * set, sigset_t * oldset);
sighandler_t signal(int signum, sighandler_t handler);
int kill(pid_t pid, int sig);

#endif