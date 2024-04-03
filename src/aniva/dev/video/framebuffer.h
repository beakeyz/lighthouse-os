#ifndef __ANIVA_VID_DEV_FRAMEBUFFER__
#define __ANIVA_VID_DEV_FRAMEBUFFER__

#include "libk/stddef.h"

struct device;
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

#define BYTES_PER_PIXEL(bpp) ((bpp) >> 3)

typedef union fb_color {
  struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
  } components;
  uint32_t raw_clr;
} fb_color_t;

typedef uint32_t fb_handle_t;

/*
 * The fb_info struct for in-kernel tracking of the attributes of 
 * a framebuffer device
 */
typedef struct fb_info {
  /* Generic fb stuff */
  paddr_t addr;
  vaddr_t kernel_addr;
  size_t size;
  uint32_t width, height, pitch;
  uint8_t bpp;
  uint8_t pixeltype;
  uint16_t accel;

  /* Layout of the 4 color chanels in the pixels */
  struct {
    struct {
      uint32_t offset_bits, length_bits;
    } red;
    struct {
      uint32_t offset_bits, length_bits;
    } green;
    struct {
      uint32_t offset_bits, length_bits;
    } blue;
    struct {
      uint32_t offset_bits, length_bits;
    } alpha;
  } colors;

  fb_handle_t handle;

  /* The parent device */
  struct video_device* parent;

  /* video devices private data */
  uint8_t priv_data[];
} fb_info_t;

fb_info_t* create_fb_info(struct video_device* device, size_t priv_data_size);

/* Clear the framebuffer memory, deallocate other shit */
void destroy_fb_info(fb_info_t* info);

int generic_draw_rect(fb_info_t* info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr);

static inline uint32_t fb_get_fb_color(fb_info_t* info, fb_color_t color)
{
  return (((color.components.r >> info->colors.red.offset_bits) & info->colors.red.length_bits) ||
          ((color.components.g >> info->colors.green.offset_bits) & info->colors.green.length_bits) ||
          ((color.components.b >> info->colors.blue.offset_bits) & info->colors.blue.length_bits) || 
          ((color.components.a >> info->colors.alpha.offset_bits) & info->colors.alpha.length_bits));
}

typedef struct fb_helper_ops {
  int (*f_destroy)(struct device* dev, fb_handle_t fb);

  int (*f_get_main_fb)(struct device* dev, fb_handle_t* fb);
  int (*f_set_main_fb)(struct device* dev, fb_handle_t fb);
  int (*f_pagefilp)(struct device* dev);

  /* 
   * x, y, width and height being NULL means we map the entire framebuffer 
   * Returns: The positive size of the mapped (part of the) framebuffer when the mapping is
   *          successful, otherwise a negative error code
   */
  ssize_t (*f_map)(struct device* dev, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, vaddr_t base);
  ssize_t (*f_unmap)(struct device* dev, fb_handle_t fb, vaddr_t base);


  /* 
   * Transforms the standard framebuffer color into a packed DWORD for the framebuffer 
   * This assumes that the framebuffer is <= 32 bpp
   */
  uint32_t (*f_clr_transform)(struct device* dev, fb_handle_t fb, fb_color_t clr);

  /* Draw a simple rectangle */
  int (*f_draw_rect)(struct device* dev, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr);
  /* Draw a glyph from a bitmap */
  int (*f_draw_glyph)(struct device* dev, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint8_t* glyph_bm, fb_color_t clr);
  /* BLT =) */
  int (*f_blt)(struct device* dev, fb_handle_t fb, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t* image);
} fb_helper_ops_t;

typedef struct fb_mapping {
  uint32_t width;
  uint32_t height;
  uint32_t x;
  uint32_t y;
  size_t size;
  vaddr_t base;

  struct fb_mapping* next;
} fb_mapping_t;

/*
 * Optional extention to a video device that handles framebuffer creation,
 * access, manipulation, ect.
 */
typedef struct fb_helper {
  uint32_t fb_capacity;
  /* This is zero most of the time */
  fb_handle_t main_fb;

  fb_mapping_t* fb_mappings;
  fb_info_t* framebuffers;
  fb_helper_ops_t* ops;
} fb_helper_t;

int fb_helper_add_mapping(fb_helper_t* helper, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t size, vaddr_t base);
int fb_helper_remove_mapping(fb_helper_t* helper, vaddr_t base);
int fb_helper_get_mapping(fb_helper_t* helper, vaddr_t base, fb_mapping_t** bmapping);

extern fb_info_t* fb_helper_get(fb_helper_t* helper, fb_handle_t fb);

#endif // !__ANIVA_VID_DEV_FRAMEBUFFER__
