#ifndef __ANIVA_VID_DEV_FRAMEBUFFER__
#define __ANIVA_VID_DEV_FRAMEBUFFER__

#include "libk/flow/reference.h"
#include "libk/stddef.h"

struct fb_ops;
struct vdfb_ops;
struct video_device;

#define FB_PIXELTYPE_PACKED 0

#define FB_ACCEL_NONE 0
#define FB_ACCEL_2D   1
#define FB_ACCEL_3D   2

/* Standardized rgb(a) */
#define RGBA32(r, g, b, a) (uint32_t)(((r) & 0xFF) << 24 || ((g) & 0xFF) << 16 || ((b) & 0xFF) << 8 || ((a) & 0xFF))

#define TO_RED(rgba)    (((rgba) >> 24) & 0xFF)
#define TO_GREEN(rgba)  (((rgba) >> 16) & 0xFF)
#define TO_BLUE(rgba)   (((rgba) >> 8)  & 0xFF)
#define TO_ALPHA(rgba)  ((rgba)         & 0xFF)

typedef union fb_color {
  struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
  } components;
  uint32_t raw_clr;
} fb_color_t;

/*
 * The fb_info struct for in-kernel tracking of the attributes of 
 * a framebuffer device
 */
typedef struct fb_info {

  /* Generic fb stuff */
  paddr_t addr;
  size_t size;
  uint32_t width, height, pitch;
  uint8_t bpp;
  uint8_t pixeltype;
  uint16_t accel;

  /* Layout of the 4 color chanels in the pixels */
  struct {
    struct {
      uint32_t offset, length;
    } red;
    struct {
      uint32_t offset, length;
    } green;
    struct {
      uint32_t offset, length;
    } blue;
    struct {
      uint32_t offset, length;
    } alpha;
  } colors;

  /* The parent device */
  struct video_device* parent;

  /* Opperations defined by the video device */
  struct fb_ops* ops;

  /* video devices private data */
  uint8_t priv_data[];
} fb_info_t;

fb_info_t* create_fb_info(struct video_device* device, size_t priv_data_size);

/* Clear the framebuffer memory, deallocate other shit */
void destroy_fb_info(fb_info_t* info);

typedef struct fb_ops {
  int (*f_destroy) (fb_info_t* info);

  /* Transforms the standard framebuffer color into a packed DWORD for the framebuffer */
  uint32_t (*f_clr_transform) (fb_color_t clr);

  /* Draw a simple rectangle */
  int (*f_draw_rect)(fb_info_t* info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr);
  /* Draw a glyph from a bitmap */
  int (*f_draw_glyph)(fb_info_t* info, uint32_t x, uint32_t y, uint8_t* glyph_bm, uint32_t glyph_bm_len, fb_color_t clr);

  /* TODO: create a helper struct for any virtual mapping / tranfer */
  int (*f_mmap)(fb_info_t* info, void* p_buffer, size_t* p_size);
  /* ... */
} fb_ops_t;

/*
 * The vd_framebuffer is for external 'users' of the video device to 
 * perform rendering in a more orchestrated way. This is less technical 
 * than fb_info, since that contains information about how framebuffer memory
 * is structured
 */
typedef struct vd_framebuffer {
  struct video_device* device;
  struct vdfb_ops* ops;

  refc_t ref;

  uint32_t pitch;

  /*
   * Logical width and height of the framebuffer
   */
  uint32_t width, height;

  /*
   * link into the ->framebuffers list in video_device
   */
  struct vd_framebuffer* next;

  vaddr_t dma_buffer;

} vd_framebuffer_t;

vd_framebuffer_t* create_vdfb(struct video_device* device);
void destroy_vdfb(vd_framebuffer_t* framebuffer);
void release_vdfb(vd_framebuffer_t* framebuffer);

typedef struct vdfb_ops {
  vd_framebuffer_t* (*f_create)(struct video_device* device);
  void (*f_destroy)(vd_framebuffer_t* buffer);
} vdfb_ops_t;

#endif // !__ANIVA_VID_DEV_FRAMEBUFFER__
