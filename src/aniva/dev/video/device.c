#include "device.h"
#include "core.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/group.h"
#include "dev/driver.h"
#include "dev/video/events.h"
#include "dev/video/framebuffer.h"
#include "kevent/event.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <libk/string.h>

static dgroup_t* _video_group;

/*!
 * @brief Register a video device with its underlying aniva_driver
 *
 * @driver: the driver to register to
 * @device: the device to register
 */
int register_video_device(video_device_t* vdev)
{
    if (!vdev || !vdev->device)
        return -1;

    vdev_event_ctx_t vdev_ctx = {
        .type = VDEV_EVENT_REGISTER,
        .device = vdev,
    };

    kevent_fire(VDEV_EVENTNAME, &vdev_ctx, sizeof(vdev_ctx));

    /* Try to add the device to our video group */
    return dev_group_add_device(_video_group, vdev->device);
}

/*!
 * @brief Unregister the video device form its driver
 *
 * @device: the device to unregister
 */
int unregister_video_device(struct video_device* vdev)
{
    vdev_event_ctx_t vdev_ctx = {
        .type = VDEV_EVENT_REMOVE,
        .device = vdev,
    };

    kevent_fire(VDEV_EVENTNAME, &vdev_ctx, sizeof(vdev_ctx));

    return dev_group_remove_device(_video_group, vdev->device);
}

/*!
 * @brief Get the current main video device
 *
 * Since there should realistically only be one active driver at a time, this is insane lol
 */
struct video_device* get_active_vdev()
{
    /* NOTE: the first graphics driver (also hopefully the only) is always the active driver */
    int error;
    device_t* maindev;

    error = dev_group_get_device(_video_group, VIDDEV_MAINDEVICE, &maindev);

    /* If the device does not have an video endpoint we're fucked lmao */
    if (error || !maindev)
        return nullptr;

    /* 'maindev' may not contain anything other than a video device */
    return maindev->private;
}

/*!
 * @brief Try to deactivate the current active video driver
 *
 * Try to find a 'maindev' device on the video group and remove the device. If the attached
 * driver has only one device attached, this will prompt its unload
 */
int video_deactivate_current_driver()
{
    int error;
    device_t* maindev;

    error = dev_group_get_device(_video_group, VIDDEV_MAINDEVICE, &maindev);

    /* No main device = no current driver =) */
    if (error)
        return 0;

    kernel_panic("video_deactivate_current_driver: Bruh");
    return 0;
}

/*!
 * @brief: Allocate memory for a video device
 *
 * Also allocates a generic device object
 */
video_device_t* create_video_device(struct driver* driver, const char* name, device_ctl_node_t* ctl_list)
{
    video_device_t* ret;

    ret = nullptr;

    if (!driver)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        goto exit;

    memset(ret, 0, sizeof(*ret));

    /* Create a device with our own endpoints */
    ret->device = create_device_ex(driver, (char*)name, ret, NULL, ctl_list);

exit:
    return ret;
}

int vdev_init_fb_helper(video_device_t* device, uint32_t fb_capacity, struct fb_helper_ops* ops)
{
    fb_helper_t* helper;

    if (!device || !fb_capacity)
        return -1;

    helper = kmalloc(sizeof(*helper));

    if (!helper)
        return -2;

    memset(helper, 0, sizeof(*helper));

    helper->ops = ops;
    helper->main_fb = NULL;
    helper->fb_capacity = fb_capacity;
    helper->framebuffers = kmalloc(sizeof(fb_info_t) * fb_capacity);

    memset(helper->framebuffers, 0, sizeof(fb_info_t) * fb_capacity);

    device->fb_helper = helper;
    return 0;
}

fb_info_t* fb_helper_get(fb_helper_t* helper, fb_handle_t fb)
{
    if (fb >= helper->fb_capacity)
        return nullptr;

    return &helper->framebuffers[fb];
}

/*!
 * @brief: Deallocate video device memory
 */
int destroy_video_device(video_device_t* vdev)
{
    if (!vdev)
        return -1;

    destroy_device(vdev->device);
    kfree(vdev);

    return 0;
}

/*!
 * @brief: Grab the main framebuffer handle from a videodevice
 */
int vdev_get_mainfb(struct device* device, fb_handle_t* fb)
{
    video_device_t* dev;

    if (!device)
        return -1;

    dev = device->private;

    if (!dev || !dev->fb_helper || !dev->fb_helper->ops || !dev->fb_helper->ops->f_get_main_fb)
        return -1;

    return dev->fb_helper->ops->f_get_main_fb(device, fb);
}

static inline fb_info_t* _get_fb_info(device_t* device, fb_handle_t fb)
{
    video_device_t* dev;

    if (!device)
        return nullptr;

    dev = device->private;

    if (!dev || !dev->fb_helper)
        return nullptr;

    return fb_helper_get(dev->fb_helper, fb);
}

int vdev_get_fbinfo(struct device* device, fb_handle_t fb, vaddr_t fb_map_addr, fb_info_t* binfo)
{
    ssize_t map_error;
    fb_info_t dummy = { 0 };
    fb_info_t* info = _get_fb_info(device, fb);

    if (!info)
        return -KERR_NOT_FOUND;

    /* Copy the info */
    memcpy(&dummy, info, sizeof(*info));

    dummy.addr = fb_map_addr;
    dummy.kernel_addr = fb_map_addr;

    /* Map the thing */
    map_error = vdev_map_fb(device, fb, fb_map_addr);

    if (map_error < 0)
        return (kerror_t)map_error;

    /* Copy over the dummy */
    memcpy(binfo, &dummy, sizeof(*binfo));
    return 0;
}

#define DEF_VDEV_GET_FB_(type, thing)                               \
    type vdev_get_fb_##thing(struct device* device, fb_handle_t fb) \
    {                                                               \
        fb_info_t* info = _get_fb_info(device, fb);                 \
        if (!info)                                                  \
            return 0;                                               \
        return info->thing;                                         \
    }

#define DEF_VDEV_GET_FB_CLR(type, color)                                     \
    type vdev_get_fb_##color##_offset(struct device* device, fb_handle_t fb) \
    {                                                                        \
        fb_info_t* info = _get_fb_info(device, fb);                          \
        if (!info)                                                           \
            return 0;                                                        \
        return info->colors.color.offset_bits;                               \
    }                                                                        \
    type vdev_get_fb_##color##_length(struct device* device, fb_handle_t fb) \
    {                                                                        \
        fb_info_t* info = _get_fb_info(device, fb);                          \
        if (!info)                                                           \
            return 0;                                                        \
        return info->colors.color.length_bits;                               \
    }

/*
 * This is where all the fields of fb_info_t
 * are exposed
 */

DEF_VDEV_GET_FB_(uint32_t, width);
DEF_VDEV_GET_FB_(uint32_t, height);
DEF_VDEV_GET_FB_(uint32_t, pitch);
DEF_VDEV_GET_FB_(uint8_t, bpp);
DEF_VDEV_GET_FB_(size_t, size);

DEF_VDEV_GET_FB_CLR(uint32_t, red);
DEF_VDEV_GET_FB_CLR(uint32_t, green);
DEF_VDEV_GET_FB_CLR(uint32_t, blue);

ssize_t vdev_map_fb(struct device* device, fb_handle_t fb, vaddr_t base)
{
    video_device_t* vdev;
    fb_helper_t* helper;

    if (!device || !base)
        return -1;

    vdev = device->private;

    if (!vdev)
        return -2;

    helper = vdev->fb_helper;

    if (!helper || !helper->ops || !helper->ops->f_map)
        return -3;

    return helper->ops->f_map(device, fb, NULL, NULL, NULL, NULL, base);
}

int vdev_unmap_fb(struct device* device, fb_handle_t fb, vaddr_t base)
{
    video_device_t* vdev;
    fb_helper_t* helper;

    if (!device || !base)
        return -1;

    vdev = device->private;

    if (!vdev)
        return -2;

    helper = vdev->fb_helper;

    if (!helper || !helper->ops || !helper->ops->f_unmap)
        return -3;

    return helper->ops->f_unmap(device, fb, base);
}

/*!
 * @brief: Initialize the videodevice management code
 */
void init_vdevice()
{
    _video_group = register_dev_group(DGROUP_TYPE_VIDEO, "video", NULL, NULL);

    ASSERT_MSG(_video_group, "Failed to create video group!");
}
