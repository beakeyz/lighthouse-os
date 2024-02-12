#ifndef __ANIVA_VID_CORE__
#define __ANIVA_VID_CORE__

#include <libk/stddef.h>
#include "framebuffer.h"

struct device;

void init_video();

bool dev_is_vdev(struct device* device);

/* 
 * Routines to interact with the framebuffer of a videodevice
 *
 * device.c
 */

extern uint32_t vdev_get_fb_width(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_height(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_pitch(struct device* device, fb_handle_t fb);
extern uint8_t vdev_get_fb_bpp(struct device* device, fb_handle_t fb);
extern size_t vdev_get_fb_size(struct device* device, fb_handle_t fb);

extern uint32_t vdev_get_fb_red_offset(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_green_offset(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_blue_offset(struct device* device, fb_handle_t fb);

extern uint32_t vdev_get_fb_red_length(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_green_length(struct device* device, fb_handle_t fb);
extern uint32_t vdev_get_fb_blue_length(struct device* device, fb_handle_t fb);

extern int vdev_get_mainfb(struct device* device, fb_handle_t* fb);

extern ssize_t vdev_map_fb(struct device* device, fb_handle_t fb, vaddr_t base);
extern int vdev_map_fb_ex(struct device* device, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, vaddr_t base);
extern int vdev_unmap_fb(struct device* device, fb_handle_t fb, vaddr_t base);

#endif // !__ANIVA_VID_CORE__
