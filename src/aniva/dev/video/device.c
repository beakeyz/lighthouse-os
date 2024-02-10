#include "device.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/endpoint.h"
#include "dev/group.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include <libk/string.h>
#include "mem/heap.h"

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

  if (!dev_has_endpoint(vdev->device, ENDPOINT_TYPE_VIDEO))
    return -2;

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
  if (error || !maindev || !dev_has_endpoint(maindev, ENDPOINT_TYPE_VIDEO))
    return nullptr;

  /* 'maindev' may not contain anything other than a video device */
  return maindev->private;
}

/*!
 * @brief Try to deactivate the current active video driver
 *
 * Try to find a 'maindev' device on the video group and unload its driver
 * This *should* have the side-effect that the current video device gets removed,
 * if the driver is well behaved. If not, that could just be very fucky, so 
 * FIXME/TODO: check if this is a potential bug and fix it if it is
 */
int video_deactivate_current_driver()
{
  int error;
  device_t* maindev;
  dev_manifest_t* class_manifest;

  error = dev_group_get_device(_video_group, VIDDEV_MAINDEVICE, &maindev);

  /* No main device = no current driver =) */
  if (error)
    return 0;

  class_manifest = maindev->parent;

  /* FIXME: what to do when this fails? */
  logln("Trying to unload current video driver!");
  Must(unload_driver(class_manifest->m_url));
  logln(" Unloaded current video driver!");
  return 0;
}

/*!
 * @brief: Check if a collection of endpoints contains a video endpoint
 */
static inline bool _has_video_endpoint(struct device_endpoint* eps, uint32_t ep_count)
{
  for (uint32_t i = 0; i < ep_count; i++) {

    if (eps[i].type == ENDPOINT_TYPE_VIDEO)
      return true;
  }

  return false;
}

/*!
 * @brief: Allocate memory for a video device
 *
 * Also allocates a generic device object
 */
video_device_t* create_video_device(struct aniva_driver* driver, struct device_endpoint* eps, uint32_t ep_count)
{
  video_device_t* ret;

  ret = nullptr;

  if (!driver || !eps)
    return nullptr;

  /* This would suck big time lmao */
  if (!_has_video_endpoint(eps, ep_count))
    return nullptr;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    goto exit;

  memset(ret, 0, sizeof(*ret));

  /* Create a device with our own endpoints */
  ret->device = create_device_ex(driver, VIDDEV_MAINDEVICE, ret, NULL, eps, ep_count);

exit:
  return ret;
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
 * @brief: Initialize the videodevice management code
 */
void init_vdevice()
{
  _video_group = register_dev_group(DGROUP_TYPE_VIDEO, "video", NULL, NULL);

  ASSERT_MSG(_video_group, "Failed to create video group!");
}
