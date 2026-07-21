#include <stdint.h>
#include <bootloader.h>
#include <string.h>
#include <screen.h>
#include <mm.h>
#include <debug.h>

struct vbe_mode_info* framebuffer_info;
void* framebuffer;

int framebuffer_width;
int framebuffer_height;
int framebuffer_pitch;

#define PSF1_HEADER_SIZE 4

extern int _binary_font_psf_start;
void* psf_font = &_binary_font_psf_start;

int font_width;
int font_height;

void screen_init(bootinfo_t* bootinfo){
    framebuffer_info = phys_to_virt(bootinfo->framebuffer);
    framebuffer = phys_to_virt((void*)(uint64_t)bootinfo->framebuffer->framebuffer);
    framebuffer_width = bootinfo->framebuffer->width;
    framebuffer_height = bootinfo->framebuffer->height;
    framebuffer_pitch = bootinfo->framebuffer->pitch;
    font_width = 8;
    font_height = 16;

    kprintf(KPRINTF_INFO,"kernel given framebuffer at address %p with resolution of %dx%d\n",bootinfo->framebuffer->framebuffer,framebuffer_width,framebuffer_height);
}

void clear_char(uint32_t bg, int pos_x, int pos_y){
    for(int y = 0; y < 16;  y++){
        for(int x = 0; x < 8; x++){
            putpixel(bg,pos_x + x, pos_y + y);
        }
    }
}


void render_psf_char(uint32_t fg, uint32_t bg, int pos_x, int pos_y, char c){
    if(c == 0){
        clear_char(bg,pos_x,pos_y);
        return;
    }

    unsigned char* glpyh = psf_font + PSF1_HEADER_SIZE + (c * 16);
    for(int y = 0; y < 16; y++){
        for(int x = 0; x < 8; x++){
            if(glpyh[y] & 0b10000000 >> x){
                putpixel(fg,x + pos_x,y + pos_y);
            } else{
                putpixel(bg,x + pos_x,y + pos_y);
            }
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