#ifndef TTY_H
#define TTY_H

#include <stdint.h>

typedef struct{
    char c;
    uint32_t bg;
    uint32_t fg;
} tty_screen_cell_t;

#define TTY_CANONICAL 1
#define TTY_RAW 2

typedef struct {
    tty_screen_cell_t* cells;
    uint32_t cell_count;
    
    /// These values are cells not pixel dimensions
    uint32_t width; 
    uint32_t height; 
    uint32_t cursor_x;
    uint32_t cursor_y;

    uint32_t default_bg;
    uint32_t default_fg;

    int mode;
    int echo;
    int cursor_visible;
} tty_t;


void redraw_tty_screen(tty_t* tty);
void tty_init();
void tty_fs_init();
void tty_handle_input(input_event_t input);

tty_t* current_tty;
tty_t** ttys;
int num_ttys;

#endif 