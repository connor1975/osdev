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

#define INPUT_BUFFER_SIZE 4096

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

    // raw mode ring buffer
    uint8_t ring_buffer[INPUT_BUFFER_SIZE];
    uint32_t head; // write index
    uint32_t tail; // read index

    // Canonical mode line buffer - rough prototype, only support buffering 1 line ahead while nothings reading
    uint8_t line_buffer[INPUT_BUFFER_SIZE];
    int line_buffer_write_index;
    int line_buffer_read_index;
    int line_ready;
} tty_t;


void redraw_tty_screen(tty_t* tty);
void tty_init();
void tty_fs_init();
void tty_handle_input(input_event_t input);
uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);

tty_t* current_tty;
tty_t** ttys;
int num_ttys;

#endif 