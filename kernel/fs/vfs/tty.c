#include <kernel/common.h>
#include <kernel/fs/vfs.h>
#include <kernel/keyboard.h>
#include <kernel/tty.h>
#include <kernel/screen.h>
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

void clear_cursor(tty_t* tty){
    if(tty->cursor_x >= tty->width || tty->cursor_y >= tty->height) return;
    tty_render_cell(current_tty, (tty->cursor_y * tty->width) + tty->cursor_x);
}

void redraw_tty_screen(tty_t* tty){
    for(int i = 0; i < tty->cell_count; i++){
        tty_render_cell(tty,i);
    }
    draw_cursor(tty);
}

void handle_newline(tty_t* tty){
    if(tty == current_tty && tty->echo && tty->cursor_visible){
        clear_cursor(current_tty);
    } 
    
    tty->cursor_x = 0;
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
        if(tty == current_tty && tty->echo) redraw_tty_screen(tty);
        return;
    }
}

void tty_writechar(tty_t* tty,char c){
    switch(c){
        case '\n':
            handle_newline(tty);
            if(tty == current_tty && tty->echo) draw_cursor(tty);
            return;
    }
    
    if(tty->cursor_x >= tty->width){
        handle_newline(tty);
    }

    int index = (tty->cursor_y * tty->width) + tty->cursor_x;
    tty->cells[index].c = c;
    tty->cells[index].fg = tty->default_fg;
    tty->cells[index].bg = tty->default_bg;
    tty->cursor_x++;
    if(tty == current_tty && tty->echo){
        tty_render_cell(tty,index);
        draw_cursor(tty);
    }
}

void putchar(int c){
    tty_writechar(current_tty,(char)c);
}

void tty_handle_input(input_event_t input_event){

}

#define TIOCGWINSZ 0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

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
    }
    return -EINVAL;
}

/*
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
    node->read = 0;
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
    node->read = 0;
    node->write = tty_write;
    node->readdir = 0;
    node->finddir = 0;
    node->ioctl = tty_ioctl;
    return node;
}    
*/

void tty_fs_init(){
    //dev_add_node(create_stdin_device());
    //dev_add_node(create_stdout_device());
    //dev_add_node(create_stderr_device());
    //dev_add_node(create_tty_device());
}

void tty_init(){
    num_ttys = 2;
    ttys = calloc(2,sizeof(tty_t*));

    for(int i = 0; i < num_ttys; i++){
        ttys[i] = malloc(sizeof(tty_t));    
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