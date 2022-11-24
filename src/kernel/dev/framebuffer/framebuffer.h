// TODO: for now this is just a temporary solution to make this work. In 
// the future I want to have a device drive to do this and make the code 
// more concise and scalable

#ifndef __FRAMEBUFFER__
#define __FRAMEBUFFER__
#include <kernel/libk/multiboot.h>
#include <libk/stddef.h>

// Colorcode translation

typedef struct fb_data {
    void *address;
    uint8_t bpp;
    uint32_t pitch;
    uint32_t memory_size;
    uint32_t width;
    uint32_t height;

    uint64_t phys_address;
} fb_data_t;

#define FRAMEBUFFER_VIRTUAL_BASE 0xffffffffbd000000

void init_fb (struct multiboot_tag_framebuffer* mb_fb);

fb_data_t get_data ();
void draw_pixel (uint64_t x, uint64_t y, uint32_t color);

#endif // !__FRAMEBUFFER__
