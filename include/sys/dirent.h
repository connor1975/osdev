#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>

typedef struct{
    int fd;
    int off;
} DIR;

struct dirent
{
    char d_name[128];
    uint32_t d_ino;
};

struct linux_dirent64{
    uint64_t d_ino;
    int64_t d_off;
    uint16_t d_reclen;
    uint8_t d_type;
    char d_name[];
};

int getdents64(unsigned int fd, struct dirent *dirp,unsigned int count);

#endif