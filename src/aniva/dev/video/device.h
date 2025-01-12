#ifndef __ANIVA_VID_DEVICE__
#define __ANIVA_VID_DEVICE__

#include "dev/core.h"
#include "dev/device.h"
#include "dev/video/connector.h"
#include "lightos/api/device.h"

#define VIDDEV_FLAG_FB (0x01)
#define VIDDEV_FLAG_GPU (0x02)
#define VIDDEV_FLAG_VIRTUAL (0x04)
#define VIDDEV_FLAG_FIRMWARE (0x08)

#define VIDDEV_MAINDEVICE "maindev"

struct device;
struct fb_info;
struct fb_helper;
struct vdev_info;
struct aniva_driver;
struct fb_helper_ops;

union fb_color;

typedef struct device_video_endpoint {

    int (*f_enable_engine)(struct device* dev);
    int (*f_disable_engine)(struct device* dev);

    int (*f_get_info)(struct device* dev, struct vdev_info* info);

    int (*f_await_vblank)(struct device* dev);

} device_video_endpoint_t;

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
    struct device* device;

    uint32_t flags;
    uint16_t max_connector_count;
    uint16_t current_connector_count;

    void* priv;

    struct fb_helper* fb_helper;
    struct vdev_info* info;
} video_device_t;

void init_vdevice();

video_device_t* create_video_device(struct driver* driver, const char* name, enum DEVICE_CTYPE type, device_ctl_node_t* ctl_list);
int destroy_video_device(video_device_t* device);

int vdev_init_fb_helper(video_device_t* device, uint32_t fb_capacity, struct fb_helper_ops* ops);

int register_video_device(struct video_device* device);
int unregister_video_device(struct video_device* device);

struct video_device* get_active_vdev();
int video_deactivate_current_driver();

/*
 * Info about a certain video device
 *
 * Info itself is ignorant about the device it comes from
 * Any info is gathered by ->f_get_info, which is provided by the video driver
 */
typedef struct vdev_info {
    vdev_connector_t* connectors[VCONNECTOR_MAX];
} vdev_info_t;

#endif // !__ANIVA_VID_DEVICE__
