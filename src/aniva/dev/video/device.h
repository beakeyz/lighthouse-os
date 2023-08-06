#ifndef __ANIVA_VID_DEVICE__
#define __ANIVA_VID_DEVICE__

#include "dev/core.h"

#define VIDDEV_FLAG_FB          (0x01)
#define VIDDEV_FLAG_GPU         (0x02)
#define VIDDEV_FLAG_VIRTUAL     (0x04)
#define VIDDEV_FLAG_FIRMWARE    (0x08)

struct video_device_ops;
struct aniva_driver;

struct vd_framebuffer;
struct fb_info;

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
  uint32_t flags;
  uint16_t max_connector_count;
  uint16_t current_connector_count;
  struct fb_info* fbinfo;

  struct vd_framebuffer* framebuffers;

  struct dev_manifest* manifest;
  struct video_device_ops* ops;
} video_device_t;

int destroy_video_device(video_device_t* device);

void register_video_device(struct aniva_driver* driver, struct video_device* device);
void unregister_video_device(struct video_device* device);

struct video_device* get_active_video_device();

struct vd_framebuffer* vid_dev_attach_vdfb(struct video_device* device);
void vid_dev_detach_vdfb(struct video_device* device, struct vd_framebuffer* framebuffer);

struct vd_framebuffer* vid_dev_vdfb_get(struct video_device* device, uint32_t index);

void vid_dev_register_connector();
void vid_dev_unregister_connector();

typedef struct video_device_ops {
  int (*f_mmap) (video_device_t* device, void* p_buffer, size_t* p_size);
  int (*f_msg) (video_device_t* device, driver_control_code_t code);

  int (*f_destroy)(video_device_t* device);
} video_device_ops_t;

int video_device_mmap(video_device_t* device, void* p_buffer, size_t* p_size);
int video_device_msg(video_device_t* device, driver_control_code_t code);

#endif // !__ANIVA_VID_DEVICE__
