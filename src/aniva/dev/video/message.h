#ifndef __ANIVA_VDEV_MESSAGE__
#define __ANIVA_VDEV_MESSAGE__

#include <libk/stddef.h>

/*
 * Define message structures for userspace to follow
 * FIXME: should these be part of a header in LibSys?
 */

/*
 * Fun little idea:
 *
 * Force video drivers to implement certain controlcodes, and also enforce certain
 * control-returnvalues to allow the kernel to check availability of the codes at 
 * runtime
 */
/* driver controlcodes / ioctl codes */
#define VIDDEV_DCC_BLT 100 

#define     VIDDEV_BLT_MODE_IMAGE 0
#define     VIDDEV_BLT_MODE_COLOR 1

/*
 * Structure to be passed into a blt message
 */
typedef struct viddev_blt {
  uint32_t x, y;
  uint32_t width, height;
  uint8_t mode;
  union fb_color* buffer;
} viddev_blt_t;

#define VIDDEV_DCC_MAPFB 101

typedef struct viddev_mapfb {
  vaddr_t virtual_base;
  size_t* size;
} viddev_mapfb_t;

#define VIDDEV_DCC_GET_FBINFO 102

#endif // !__ANIVA_VDEV_MESSAGE__
