#include "device.h"
#include "dev/core.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "proc/core.h"

/*!
 * @brief Register a video device with its underlying aniva_driver
 *
 * @driver: the driver to register to
 * @device: the device to register
 */
void register_video_device(aniva_driver_t* driver, video_device_t* device)
{
  bool result;
  dev_manifest_t* manifest = try_driver_get(driver, NULL);

  if (!manifest)
    return;

  result = install_private_data(manifest->m_handle, device);

  if (!result)
    return;

  device->manifest = manifest;
}

/*!
 * @brief Unregister the video device form its driver
 *
 * @device: the device to unregister
 */
void unregister_video_device(struct video_device* device)
{
  dev_manifest_t* manifest = device->manifest;

  mutex_lock(&manifest->m_lock);

  manifest->m_private = nullptr;

  mutex_unlock(&manifest->m_lock);

  destroy_video_device(device);
}

/*!
 * @brief Get the current active video device
 *
 * Since there can only be one active driver at a time, this is insane lol
 */
struct video_device* get_active_video_device()
{
  /* NOTE: the first graphics driver (also hopefully the only) is always the active driver */
  dev_manifest_t* manifest = get_active_driver_from_type(DT_GRAPHICS);
  ASSERT(manifest);

  if (!manifest)
    return nullptr;

  /* The private data SHOULD contain the video device struct lol */
  return manifest->m_private;
}

/*!
 * @brief Try to make us the new active video driver
 *
 * Since there can only be one active video driver at a time, we need infrastructure to manage
 * which drivers get to be loaded and which dont. For this we have precedence. Video drivers with
 * a higher precedence than the active video device driver will pass this function. Those who have lower 
 * precedence won't. drivers should call this function as early as possible to avoid unnecessary allocations.
 *
 * @device: the video device to activate
 */
int try_activate_video_device(video_device_t* device)
{
  video_device_t* c_active_device;

  if (!device)
    return -1;

  c_active_device = get_active_video_device();

  (void)c_active_device;
  /* TODO: */
  kernel_panic("TODO: try_activate_video_device");

  return 0;
}

int destroy_video_device(video_device_t* device)
{
  kernel_panic("TODO: destroy_video_device");
}
