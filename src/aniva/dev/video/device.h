#ifndef __ANIVA_VID_DEVICE__
#define __ANIVA_VID_DEVICE__

#include "dev/core.h"
#include "dev/video/framebuffer.h"
#include "dev/video/precedence.h"

#define VIDDEV_FLAG_FB          (0x01)
#define VIDDEV_FLAG_GPU         (0x02)
#define VIDDEV_FLAG_VIRTUAL     (0x04)
#define VIDDEV_FLAG_FIRMWARE    (0x08)

struct video_device_ops;
struct aniva_driver;

/*
 * Fun little idea:
 *
 * Force video drivers to implement certain controlcodes, and also enforce certain
 * control-returnvalues to allow the kernel to check availability of the codes at 
 * runtime
 */
/* driver controlcodes / ioctl codes */
#define VIDDEV_DCC_PLACEHOLDER 0 /* TODO */

/*
 * Structure for any video devices like gpus or fb devices
 *
 * These devices are meant to actually beam pixels on the screen, handle 2D/3D accel
 * or sometimes even compute paralel for us. 
 *
 * /How does communication work between the outside world and video drivers?
 * 
 */
typedef struct video_device {
  video_dev_precedence_t precedence;
  uint8_t res0;
  uint16_t flags;
  uint32_t res1;
  fb_info_t* fbinfo;

  struct dev_manifest* manifest;
  struct video_device_ops* ops;
} video_device_t;

int destroy_video_device(video_device_t* device);

/*
 * NEVER use these function raw
 * we have wrappers for them that do essensial preperations and cleanup. This 
 * goes for most _ops in the system
 */
typedef struct video_device_ops {
  int (*f_mmap) (video_device_t* device, void* p_buffer, size_t* p_size);
  int (*f_msg) (video_device_t* device, driver_control_code_t code);
  int (*f_destroy)(video_device_t* device);
} video_device_ops_t;

int video_device_mmap(video_device_t* device, void* p_buffer, size_t* p_size);
int video_device_msg(video_device_t* device, driver_control_code_t code);

#endif // !__ANIVA_VID_DEVICE__
