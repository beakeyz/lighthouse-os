#include "device.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include <libk/string.h>
#include "mem/heap.h"

/*!
 * @brief Register a video device with its underlying aniva_driver
 *
 * @driver: the driver to register to
 * @device: the device to register
 */
int register_video_device(video_device_t* vdev)
{
  dev_manifest_t* manifest;

  if (!vdev || !vdev->device)
    return -1;

  /* Get the core video driver */
  manifest = get_core_driver(DT_GRAPHICS);

  if (!manifest)
    return -1;

  return manifest_add_device(manifest, vdev->device);
}

/*!
 * @brief Unregister the video device form its driver
 *
 * @device: the device to unregister
 */
int unregister_video_device(struct video_device* vdev)
{
  device_t* _dev;

  if (!vdev)
    return -1;

  _dev = vdev->device;

  if (!_dev)
    return -1;

  return manifest_remove_device(_dev->parent, _dev->device_path);
}

/*!
 * @brief Get the current main video device
 *
 * Since there should realistically only be one active driver at a time, this is insane lol
 */
struct video_device* get_main_video_device()
{
  /* NOTE: the first graphics driver (also hopefully the only) is always the active driver */
  device_t* maindev;
  dev_manifest_t* manifest = get_core_driver(DT_GRAPHICS);

  if (!manifest)
    return nullptr;

  maindev = nullptr;

  /* Try to find the main device on the  */
  if (manifest_find_device(manifest, &maindev, VIDDEV_MAINDEVICE) || !maindev)
    return nullptr;

  /* 'maindev' may not contain anything other than a video device */
  return maindev->private;
}

/*!
 * @brief Try to deactivate the current active video driver
 *
 * Try to find a 'maindev' device on the core driver and unload its driver
 */
int video_deactivate_current_driver()
{
  dev_manifest_t* core_manifest;
  dev_manifest_t* class_manifest;
  device_t* maindev;

  core_manifest = get_core_driver(DT_GRAPHICS);

  if (!core_manifest)
    return -1;

  maindev = nullptr;

  /* If we can't find a video device, we're good */
  if (manifest_find_device(core_manifest, &maindev, VIDDEV_MAINDEVICE) || !maindev)
    return 0;

  class_manifest = maindev->parent;

  /* FIXME: what to do when this fails? */
  logln("Trying to unload current video driver!");
  Must(unload_driver(class_manifest->m_url));
  logln(" Unloaded current video driver!");

  return 0;
}

/*!
 * @brief: Allocate memory for a video device
 *
 * Also allocates a generic device object
 */
video_device_t* create_video_device(struct aniva_driver* driver, struct video_device_ops* ops)
{
  video_device_t* ret;

  ret = nullptr;

  if (!driver || !ops)
    return nullptr;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    goto exit;

  memset(ret, 0, sizeof(*ret));

  ret->device = create_device(driver, VIDDEV_MAINDEVICE);
  ret->ops = ops;

  ret->device->private = ret;

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
