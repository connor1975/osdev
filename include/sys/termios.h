#ifndef TERMIOS_H
#define TERMIOS_H

#include <stdint.h>

#define BRKINT 0x0001
#define ICRNL 0x0002
#define IGNBRK 0x0004
#define IGNCR 0x0008
#define IGNPAR 0x0010
#define INLCR 0x0020
#define INPCK 0x0040
#define ISTRIP 0x0080
#define IXOFF 0x0100
#define IXON 0x0200
#define PARMRK 0x0400

#define	OPOST 0x00000001

#define ECHO 0x0001
#define ECHOE 0x0002
#define ECHOK 0x0004
#define ECHONL 0x0008
#define ICANON 0x0010
#define IEXTEN 0x0020
#define ISIG 0x0040
#define NOFLSH 0x0080
#define TOSTOP 0x0100

typedef uint16_t tcflag_t;
typedef unsigned char cc_t;

#define NCCS 11
struct termios
{
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
};

int tcgetattr(int fd, struct termios *termptr);
int tcsetattr(int fd, int action, struct termios *termptr);

#endif
