#ifndef IOCTL_H
#define IOCTL_H

#define TCGETS		1
#define TCSETS		2
#define TIOCGWINSZ	3
#define TIOCSPGRP	4
#define TIOCGPGRP	5

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd,int request, ...);

#endif