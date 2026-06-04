#ifndef ANSI_H
#define ANSI_H

#include <tty.h>
#include <stdint.h>

#define ANSI_STATE_NORMAL  0
#define ANSI_STATE_ESCAPE  1
#define ANSI_STATE_CSI     2

int handle_ansi(tty_t* tty, uint8_t c);

#endif