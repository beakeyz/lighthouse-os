#ifndef __ANIVA_VID_DEVICE__
#define __ANIVA_VID_DEVICE__

#include "dev/core.h"
#include "libk/stddef.h"

#define VIDDEV_FLAG_FB          (0x01)
#define VIDDEV_FLAG_GPU         (0x02)
#define VIDDEV_FLAG_VIRTUAL     (0x04)
#define VIDDEV_FLAG_FIRMWARE    (0x08)

struct video_device_ops;
struct aniva_driver;

union fb_color;

/*
 * Fun little idea:
 *
 * Force video drivers to implement certain controlcodes, and also enforce certain
 * control-returnvalues to allow the kernel to check availability of the codes at 
 * runtime
 */
/* driver controlcodes / ioctl codes */
#define VIDDEV_DCC_BLT 100 

/*
 * Structure to be passed into a blt message
 */
typedef struct viddev_blt {
  uint32_t x, y;
  uint32_t width, height;
  union fb_color* buffer;
} viddev_blt_t;

#define VIDDEV_DCC_MAPFB 101

typedef struct viddev_mapfb {
  vaddr_t virtual_base;
  size_t* size;
} viddev_mapfb_t;

#define VIDDEV_DCC_GET_FBINFO 102

/*
 * Structure for any video devices like gpus or fb devices
 *
 * These devices are meant to actually beam pixels on the screen, handle 2D/3D accel
 * or sometimes even compute paralel for us. 
 *
 * /How does communication work between the outside world and video drivers?
 * Probably like IOCTLs / DMSG
 *
 * A video device most likely contains only one connector, but can contain up to
 * max_connector_count connectors. It is up to the driver to figure this out. A 
 * connector is a representation of the physical connection between the graphics 
 * hardware and the display. The connector structure knows about the kind of communication
 * that the hardware needs, so the video_device should go there for its transfers. 
 *
 * 
 */
typedef struct video_device {
  struct dev_manifest* manifest;
  
  uint32_t flags;
  uint16_t max_connector_count;
  uint16_t current_connector_count;

  struct video_device_ops* ops;
} video_device_t;

int destroy_video_device(video_device_t* device);

void register_video_device(struct aniva_driver* driver, struct video_device* device);
void unregister_video_device(struct video_device* device);

struct video_device* get_active_video_device();

int try_activate_video_device(video_device_t* device);

typedef struct video_device_ops {
  int (*f_remove) (video_device_t* dev);
} video_device_ops_t;

#endif // !__ANIVA_VID_DEVICE__
