#ifndef ANSI_H
#define ANSI_H

#include <tty.h>
#include <stdint.h>

static const uint32_t ansi_colours[16] = {
    0x00000000, // Black
    0x00AA0000, // Red
    0x0000AA00, // Green
    0x00AA5500, // Yellow
    0x000000AA, // Blue
    0x00AA00AA, // Magenta
    0x0000AAAA, // Cyan
    0x00AAAAAA, // White
    0x00555555, // Bright Black (Gray)
    0x00FF5555, // Bright Red
    0x0055FF55, // Bright Green
    0x00FFFF55, // Bright Yellow
    0x005555FF, // Bright Blue
    0x00FF55FF, // Bright Magenta
    0x0055FFFF, // Bright Cyan
    0x00FFFFFF  // Bright White
};

#define ANSI_STATE_NORMAL  0
#define ANSI_STATE_ESCAPE  1
#define ANSI_STATE_CSI     2

int handle_ansi(tty_t* tty, uint8_t c);

#endif