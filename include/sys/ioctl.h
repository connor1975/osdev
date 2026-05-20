#ifndef IOCTL_H
#define IOCTL_H

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TIOCGWINSZ	0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

int ioctl(int fd,int request, ...);

#endif