#ifndef SCREEN_H
#define SCREEN_H

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
void clear_framebuffer(uint32_t colour);
void putpixel(uint32_t colour, int x, int y);

#endif