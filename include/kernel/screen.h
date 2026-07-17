#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include <bootloader.h>

#define BLUE    0x000000FF
#define GREEN   0x0000FF00
#define RED     0x00FF0000
#define YELLOW  0x00FFFF00
#define MAGENTA 0x00FF00FF
#define CYAN    0x0000FFFF
#define WHITE RED | GREEN | BLUE
#define GREY    0x00AAAAAA
#define BLACK   0

extern int cursor_x;
extern int cursor_y;
extern int font_width;
extern int font_height;
extern int framebuffer_width;
extern int framebuffer_height;
extern int framebuffer_pitch;
extern void* framebuffer;

void screen_init(bootinfo_t*);
static inline void putpixel(uint32_t colour, int x, int y){
    uint32_t* row = (uint32_t*)((uint8_t*)framebuffer + y * framebuffer_pitch);
    row[x] = colour;
}
void render_psf_char(uint32_t fg, uint32_t bg, int pos_x, int pos_y, char c);

#endif