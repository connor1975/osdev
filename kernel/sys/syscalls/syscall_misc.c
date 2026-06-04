#include <common.h>
#include <interrupts.h>
#include <multitasking.h>
#include <pipe.h>
#include <mm.h>
#include <time.h>
#include <stdint.h>
#include <errno.h>

struct timeval {
    uint64_t tv_sec;	
    uint32_t tv_usec;
};

uint64_t sys_gettimeofday(struct timeval* tv, void* tz){
    tv->tv_usec = 0;
    tv->tv_sec = get_unix_time();
    return 0;
}

uint64_t sys_pipe(int* fildes){
    irq_enable();
    if(fildes == NULL) return -EINVAL;

    fs_node_t* pipe = create_pipe(DEFAULT_PIPE_SIZE);
    int read_fd = task_open_file(pipe,(task_t*)current_task,O_RDONLY);
    int write_fd = task_open_file(pipe,(task_t*)current_task,O_WRONLY);
    fildes[0] = read_fd;
    fildes[1] = write_fd;
    return 0;
}

char sysname[] = "ConnorOS";
char nodename[] = "connoros";
char release[] = "0.0.1";
char version[] = "ConnorOS Kernel Version 0.0.1";
char machine[] = "x86_64";

struct old_utsname{
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
};

uint64_t sys_uname(struct old_utsname* buf){
    if(buf == NULL) return -EFAULT;
    memcpy(buf->sysname,sysname,sizeof(sysname));
    memcpy(buf->nodename,nodename,sizeof(nodename));
    memcpy(buf->release,release,sizeof(release));
    memcpy(buf->version,version, sizeof(version));
    memcpy(buf->machine,machine,sizeof(machine));
    return 0;
}

uint64_t sys_geteuid(){
    return 0;
}

uint64_t sys_getgid(){
    return 0;
}

uint64_t sys_getegid(){
    return 0;
}

uint64_t sys_getuid(){
    return 0;
}