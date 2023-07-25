// TODO: for now this is just a temporary solution to make this work. In 
// the future I want to have a device driver to do this and make the code 
// more concise and scalable

#ifndef __FRAMEBUFFER__
#define __FRAMEBUFFER__
#include "dev/driver.h"
#include <libk/multiboot.h>
#include <libk/stddef.h>

//#define FB_DRV_SET_MB_TAG 8 /* FIXME: should drv messages be able to alter mb framebuffer tag? */
#define FB_DRV_SET_WIDTH 9
#define FB_DRV_SET_HEIGHT 10
#define FB_DRV_SET_FB 11
#define FB_DRV_MAP_FB 12
#define FB_DRV_RESET 13
#define FB_DRV_GET_FB_INFO 14

typedef struct fb_info {
  uint32_t width;
  uint32_t height;
  uint32_t bpp;
  uint32_t pitch;
  size_t size;
  uint32_t* volatile paddr; 
} fb_info_t;

extern aniva_driver_t g_base_fb_driver;

#endif // !__FRAMEBUFFER__
