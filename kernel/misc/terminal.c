#include <kernel/common.h>
#include <kernel/screen.h>
#include <stdint.h>
#include <string.h>

#define PSF1_HEADER_SIZE 4

extern int _binary_font_psf_start;
void* psf_font = &_binary_font_psf_start;

int cursor_x = 0;
int cursor_y = 0;

int font_width;
int font_height;

uint32_t text_colour;
uint32_t clear_colour;

void render_psf_char(uint32_t colour, int pos_x, int pos_y, char c){
    unsigned char* glpyh = psf_font + PSF1_HEADER_SIZE + (c * 16);
    for(int y = 0; y < 16; y++){
        for(int x = 0; x < 8; x++){
            if(glpyh[y] & 0b10000000 >> x){
                putpixel(colour,x + pos_x,y + pos_y);
            }
        }
    }
}

void clear_char(int pos_x, int pos_y){
    for(int y = 0; y < 16;  y++){
        for(int x = 0; x < 8; x++){
            putpixel(clear_colour,pos_x + x, pos_y + y);
        }
    }
}

void render_cursor(uint32_t colour, int posx, int posy){
    for(int x = 0; x < 8; x++){
        for(int y = 0; y < 16; y++){
            putpixel(colour,posx + x, posy + y);
        }
    }
}

void clear_screen(uint32_t colour){
    clear_colour = colour;
    clear_framebuffer(colour);
}

void draw_cursor(int x, int y){
    render_cursor(WHITE,cursor_x,cursor_y);
}

void set_cursor_pos(int x, int y){
    clear_char(cursor_x,cursor_y);
    cursor_x = x;
    cursor_y = y;
    draw_cursor(cursor_x,cursor_y);
}

void set_text_colour(uint32_t colour){
    text_colour = colour;
}

void putchar_newline(){
    if(cursor_y + ((font_height * 2) - 1) > framebuffer_height){
        memcpy(framebuffer,framebuffer + (framebuffer_pitch * font_height),(framebuffer_pitch * framebuffer_height) - (framebuffer_pitch * font_height));
        cursor_x = 0;
        memset(framebuffer + (framebuffer_pitch * (framebuffer_height - font_height)),0,framebuffer_pitch * font_height);
        cursor_y = framebuffer_height - font_height;
    }else{
        cursor_y+=font_height;
        cursor_x = 0;
    }
}

void print_char(char c){
    clear_char(cursor_x,cursor_y);
    switch(c){
        case '\f':
            cursor_x = 0;
            cursor_y = 0;
            clear_screen(0);
        break;
        case '\n':
            putchar_newline();
        break;
        case '\b':
            if(cursor_x - font_width < 0){
                if(cursor_y == 0) break;
                cursor_y -= font_height;
                cursor_x = framebuffer_width;
            }
            cursor_x-=font_width;
            clear_char(cursor_x,cursor_y);
        break;
        default:
            render_psf_char(text_colour,cursor_x,cursor_y,c);   
            cursor_x+=font_width;    
            if(cursor_x + (font_width - 1) > framebuffer_width){
                putchar_newline();
            }
        break;
    }
    draw_cursor(cursor_x, cursor_y);
}

void putchar(int c){
    print_char(c);
}

void terminal_init(){
    set_text_colour(WHITE);
    clear_colour = BLACK;
    font_height = 16;
    font_width = 8;
}