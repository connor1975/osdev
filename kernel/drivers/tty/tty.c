#include <kernel/common.h>
#include <kernel/fs/vfs.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/screen.h>
#include <kernel/ansi.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

tty_t* current_tty = NULL;
tty_t** ttys = NULL;
int num_ttys;

void tty_render_cell(tty_t* tty, uint32_t index){
    char c = tty->cells[index].c;
    render_psf_char(tty->default_fg, tty->default_bg,(index % tty->width) * font_width, (index / tty->width) * font_height,c);
}

void draw_cursor(tty_t* tty){
    if(tty->cursor_x >= tty->width || tty->cursor_y >= tty->height) return;
    for(int i = 0; i < 16; i++){
        putpixel(GREY,tty->cursor_x * font_width,(tty->cursor_y * font_height) + i);
    }
}

void clear_cursor(){
    if(current_tty->cursor_x >= current_tty->width || current_tty->cursor_y >= current_tty->height) return;
    tty_render_cell(current_tty, (current_tty->cursor_y * current_tty->width) + current_tty->cursor_x);
}

void redraw_tty_screen(tty_t* tty){
    for(int i = 0; i < tty->cell_count; i++){
        tty_render_cell(tty,i);
    }
    if(current_tty->cursor_visible) draw_cursor(tty);
}

void handle_newline(tty_t* tty){
    if(tty == current_tty && tty->cursor_visible){
        clear_cursor();
    } 
    
    tty->cursor_y++;
    if(tty->cursor_y >= tty->height){
        memmove(tty->cells, (char*)tty->cells + (tty->width * sizeof(tty_screen_cell_t)), (sizeof(tty_screen_cell_t) * tty->cell_count) - (tty->width * sizeof(tty_screen_cell_t)));
        tty_screen_cell_t* bottom_row = (void*)((char*)tty->cells + ((sizeof(tty_screen_cell_t) * tty->width) * (tty->height - 1)));
        for(int i = 0; i < tty->width; i++){
            bottom_row[i].bg = tty->default_bg;
            bottom_row[i].fg = 0;
            bottom_row[i].c = 0;
        }
        tty->cursor_y = tty->height - 1;
        if(tty == current_tty) redraw_tty_screen(tty);
        return;
    }
}

void tty_writechar(tty_t* tty,uint8_t c){
    switch(c){
        case '\n':
            handle_newline(tty);
            if(tty->mode == TTY_CANONICAL)
                tty_writechar(tty,'\r');
            if(tty == current_tty && tty->cursor_visible) 
                draw_cursor(tty);
            return;
        case '\r':
            if(tty == current_tty && tty->cursor_visible)
                clear_cursor();
            tty->cursor_x = 0;
            if(tty == current_tty && tty->cursor_visible) 
                draw_cursor(tty);
            return;
    }
    
    if(tty->cursor_x >= tty->width){
        handle_newline(tty);
        tty_writechar(current_tty,'\r');
    }

    int index = (tty->cursor_y * tty->width) + tty->cursor_x;
    tty->cells[index].c = c;
    tty->cells[index].fg = tty->default_fg;
    tty->cells[index].bg = tty->default_bg;
    tty->cursor_x++;
    if(tty == current_tty){
        tty_render_cell(tty,index);
        if(tty->cursor_visible)
            draw_cursor(tty);
    }
}

void tty_putchar(tty_t* tty,uint8_t c){
    if(handle_ansi(tty,c))
        return;
    tty_writechar(tty,c);
}

void putchar(int c){
    tty_putchar(current_tty,(char)c);
}

int tty_ring_push(tty_t* tty, uint8_t c) {
    uint32_t next = (tty->head + 1) % INPUT_BUFFER_SIZE;

    if (next == tty->tail){
        return -1;
    }

    tty->ring_buffer[tty->head] = c;
    tty->head = next;
    return 0;
}

int tty_ring_pop(tty_t* tty, uint8_t* out) {
    if (tty->head == tty->tail){
        return -1;
    }

    *out = tty->ring_buffer[tty->tail];
    tty->tail = (tty->tail + 1) % INPUT_BUFFER_SIZE;
    return 0;
}

void tty_clear_line_from_cursor(tty_t* tty){
    uint32_t index = (tty->cursor_y * tty->width) + tty->cursor_x;
    for(int i = index; i < ((tty->cursor_y + 1) * tty->width); i++){
        tty->cells[i].c = 0;
        tty->cells[i].fg = tty->default_fg;
        tty->cells[i].bg = tty->default_bg;
        tty_render_cell(tty,i);
    }
}

void tty_clear_line(tty_t* tty){
    uint32_t index = (tty->cursor_y * tty->width);
    for(int i = index; i < index + tty->width; i++){
        tty->cells[i].c = 0;
        tty->cells[i].fg = tty->default_fg;
        tty->cells[i].bg = tty->default_bg;
        tty_render_cell(tty,i);
    }
}

void tty_raw_input_send_escape(uint8_t byte){
    tty_ring_push(current_tty, 0x1b);
    tty_ring_push(current_tty, 91);
    tty_ring_push(current_tty,byte);
}

static inline void raw_print_arrows(uint8_t byte){
    tty_writechar(current_tty,'^');
    tty_writechar(current_tty,'[');
    tty_writechar(current_tty,'[');
    tty_writechar(current_tty,byte);
}

void tty_handle_input_raw(input_event_t input_event){
    if(input_event.ascii != 0){
        if(input_event.ctrl_pressed) {
            input_event.ascii = input_event.ascii & 0x1f; // Convert to control code
        }
        tty_ring_push(current_tty,input_event.ascii);
        if(current_tty->echo){
            if(input_event.ascii == '\r'){
                tty_writechar(current_tty,'^');
                tty_writechar(current_tty,'M');
            }else{
                tty_writechar(current_tty,input_event.ascii);
            }
        }
    }else{
        switch(input_event.scancode){
            case ARROW_DOWN_PRESSED:
            tty_raw_input_send_escape('B');
            if(current_tty->echo) raw_print_arrows('B');
            break;
            case ARROW_UP_PRESSED:
            tty_raw_input_send_escape('A');
            if(current_tty->echo) raw_print_arrows('A');
            break;
            case ARROW_RIGHT_PRESSED:
            tty_raw_input_send_escape('C');
            if(current_tty->echo) raw_print_arrows('C');
            break;
            case ARROW_LEFT_PRESSED:
            tty_raw_input_send_escape('D');
            if(current_tty->echo) raw_print_arrows('D');
            break;
        }
    }
}

void tty_handle_input_canonical(input_event_t input_event){
    if(current_tty->line_ready) return;
    if(input_event.ascii != 0){
        char c = input_event.ascii;
        
        switch(c){
            case 0x7f:
            if(current_tty->line_buffer_write_index > 0){
                if(current_tty->cursor_visible && current_tty->echo) clear_cursor();
                current_tty->line_buffer_write_index--;
                current_tty->line_buffer[current_tty->line_buffer_write_index] = 0;
                
                current_tty->cursor_x--;
                current_tty->cells[(current_tty->cursor_y * current_tty->width) + current_tty->cursor_x].c = 0;
                if(current_tty->echo){
                    tty_render_cell(current_tty,(current_tty->cursor_y * current_tty->width) + current_tty->cursor_x);
                    if(current_tty->cursor_visible) draw_cursor(current_tty);
                }
                return;
            }
            return;            
            case '\r':
            current_tty->line_buffer[current_tty->line_buffer_write_index] = '\n';
            current_tty->line_buffer[current_tty->line_buffer_write_index + 1] = 0;
            current_tty->line_ready = 1;
            break;
            default:
            current_tty->line_buffer[current_tty->line_buffer_write_index] = c;
            current_tty->line_buffer_write_index++;
            break;
        }

        if(current_tty->echo){
            if(input_event.scancode == ENTER_PRESSED) tty_writechar(current_tty,'\n');
            tty_writechar(current_tty,c);
        }
    }
}

void tty_handle_input(input_event_t input_event){
    if(current_tty->mode == TTY_RAW){
        tty_handle_input_raw(input_event);
    }else{
        tty_handle_input_canonical(input_event);
    }
}

void tty_move_cursor(tty_t* tty, int x, int y){
    if(x >= tty->width || y >= tty->height || y < 0 || x < 0) return;
    if(tty == current_tty && tty->cursor_visible) clear_cursor();

    tty->cursor_x = x;
    tty->cursor_y = y;
    if(tty->cursor_visible && tty == current_tty){
        draw_cursor(current_tty);
    }    
}

void tty_set_cursor_visibility(tty_t* tty, int visible){
    if(tty == current_tty){
        if(visible && !tty->cursor_visible){
            draw_cursor(current_tty);
        }else if(!visible && tty->cursor_visible){
            clear_cursor();
        }
    }
    tty->cursor_visible = visible;
}

void tty_clear_screen(tty_t* tty){
    for(int i = 0; i < tty->cell_count; i++){
        tty->cells[i].c = 0;
        tty->cells[i].fg = tty->default_fg;
        tty->cells[i].bg = tty->default_bg;
    }
    if(tty == current_tty) redraw_tty_screen(tty);
}

int tty_ioctl(fs_node_t *node, unsigned long request, void * argp){
    switch(request){
        case TIOCGWINSZ:
            if(argp == NULL) return -EINVAL;
            struct winsize* winsize = argp;
            winsize->ws_row = current_tty->height;
            winsize->ws_col = current_tty->width;
            winsize->ws_xpixel = framebuffer_width;
            winsize->ws_ypixel = framebuffer_height;
            return 0;
        break;
        case TCGETS:
            if(argp == NULL) return -EINVAL;
            struct termios* termios_p = argp;
            termios_p->c_iflag = 0;
            termios_p->c_oflag = 0;
            termios_p->c_cflag = 0;
            termios_p->c_lflag = 0 | (current_tty->mode == TTY_CANONICAL ? ICANON : 0) | (current_tty->echo ? ECHO : 0);
            if (termios_p->c_lflag & ICANON) {
                termios_p->c_iflag |= ICRNL;
                termios_p->c_oflag |= OPOST;
            }   

            if (!(termios_p->c_lflag & ICANON)) {
                termios_p->c_iflag &= ~(ICRNL | IXON | BRKINT);
                termios_p->c_oflag &= ~OPOST;
            }
            return 0;
        break;
        case TCSETS:
            if(argp == NULL) return -EINVAL;
            struct termios* new_termios = argp;
            if((new_termios->c_lflag & ICANON) != 0){
                current_tty->mode = TTY_CANONICAL;
            }else{
                current_tty->mode = TTY_RAW;
            }
            if((new_termios->c_lflag & ECHO) != 0){
                current_tty->echo = 1;
            }else{
                current_tty->echo = 0;
            }
            return 0;
    }
    return -EINVAL;
}

uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    tty_t* tty = current_tty;
    for(int i = 0; i < size; i++){
        tty_putchar(tty,buffer[i]);
    }
    return size;
}

uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer){
    tty_t* tty = current_tty;
    if(tty->mode == TTY_RAW){
        int bytes_read = 0;
        uint8_t c;
        while(tty->tail == tty->head); // Wait for input
        while(bytes_read < size && tty_ring_pop(tty,&c) == 0){
            buffer[bytes_read] = c;
            bytes_read++;
        }
        return bytes_read;
    }else{
        int bytes_read = 0;
        while(!tty->line_ready);
        for(int i = 0; i < size; i++){
            buffer[i] = tty->line_buffer[tty->line_buffer_read_index];
            bytes_read++;
            tty->line_buffer_read_index++;
            if(tty->line_buffer[tty->line_buffer_read_index] == 0){
                tty->line_buffer_write_index = 0;
                tty->line_buffer_read_index = 0;
                tty->line_ready = 0;
                return bytes_read;
            } 
        }
        return bytes_read;
    }
}

fs_node_t* create_stdin_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stdin");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = tty_read;
    node->write = 0;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_stdout_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stdout");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = 0;
    node->write = tty_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_stderr_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"stderr");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = 0;
    node->write = tty_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}

fs_node_t* create_tty_device(){
    fs_node_t* node = malloc(sizeof(fs_node_t));
    strcpy(node->name,"tty");
    node->mask = 0666;
    node->uid = 0;
    node->gid = 0;
    node->flags = FS_CHARDEVICE;
    node->inode = 0;
    node->impl = 0;
    node->length = 0;
    node->ptr = 0;
    node->read = tty_read;
    node->write = tty_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}    

void tty_fs_init(){
    dev_add_node(create_stdin_device());
    dev_add_node(create_stdout_device());
    dev_add_node(create_stderr_device());
    dev_add_node(create_tty_device());
}

void tty_init(){
    num_ttys = 2;
    ttys = calloc(2,sizeof(tty_t*));

    for(int i = 0; i < num_ttys; i++){
        ttys[i] = calloc(sizeof(tty_t),1);    
        tty_t* tty = ttys[i];
        tty->width = framebuffer_width / font_width;
        tty->height = framebuffer_height / font_height;
        tty->cell_count = ((framebuffer_width / font_width) * (framebuffer_height / font_height));
        tty->cells = calloc(tty->cell_count, sizeof(tty_screen_cell_t));
        tty->cursor_x = 0;
        tty->cursor_y = 0;
        tty->default_bg = BLACK;
        tty->default_fg = WHITE;
        tty->mode = TTY_CANONICAL;
        tty->cursor_visible = 1;
        tty->echo = 1;
    }
    current_tty = ttys[0];
}