#include <bootloader/common.h>
#include <bootloader.h>
#include <stdint.h>
#include <string.h>
#include <vbe.h>
#include <stdio.h>

extern struct vbe_info_structure* get_vbe_info();
void get_vbe_mode_info(uint16_t mode, struct vbe_mode_info* info);
extern void set_vbe_mode(uint16_t mode);

struct vbe_mode_info modeinfo;

struct vbe_mode_info* vbe_init(int target_width, int target_height, int target_bpp){
    struct vbe_info_structure* vbeinfo = get_vbe_info();
	if(memcmp(vbeinfo->signature,"VESA",4)){
		printf("VBE not supported!");
		while(1==1);
	}
	uint16_t* modelist = segoffset_to_linear(vbeinfo->video_modes);
	uint16_t target_mode;
	while(*modelist != 0xFFFF){
		get_vbe_mode_info(*modelist, &modeinfo);
		if(modeinfo.width == target_width && modeinfo.height == target_height && modeinfo.bpp == target_bpp){
			target_mode = *modelist;
			break;
		}
		modelist++;
	}
	set_vbe_mode(target_mode);
	void* framebufferinfo = malloc(sizeof(struct vbe_mode_info));
    memcpy(framebufferinfo,&modeinfo,sizeof(struct vbe_mode_info));
    return framebufferinfo;
}