#include <stdint.h>
#include <bootloader.h>
#include <string.h>
#include <kernel/common.h>

struct vbe_mode_info* framebuffer_info;
void* framebuffer;

int framebuffer_width;
int framebuffer_height;
int framebuffer_pitch;

void clear_framebuffer(uint32_t colour){
    cursor_x = 0;
    cursor_y = 0;
    uint32_t* ptr = framebuffer;
    for(int i = 0; i < framebuffer_width * framebuffer_height; i++){
        ptr[i] = colour;
    }
}

void putpixel(uint32_t colour, int x, int y){
    *((uint32_t*)framebuffer + x + (y * framebuffer_width)) = colour;
}

void screen_init(bootinfo_t* bootinfo){
    framebuffer_info = phys_to_virt(bootinfo->framebuffer);
    framebuffer = phys_to_virt((void*)(uint64_t)bootinfo->framebuffer->framebuffer);
    framebuffer_width = bootinfo->framebuffer->width;
    framebuffer_height = bootinfo->framebuffer->height;
    framebuffer_pitch = bootinfo->framebuffer->pitch;
}