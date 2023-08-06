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

int video_device_mmap(video_device_t* device, void* p_buffer, size_t* p_size)
{
  int error;

  if (!device->ops || !device->ops->f_mmap)
    return -1;

  dev_manifest_t* prev = get_current_driver();
  set_current_driver(device->manifest);

  error = device->ops->f_mmap(device, p_buffer, p_size);

  set_current_driver(prev);

  return error;
}

int video_device_msg(video_device_t* device, driver_control_code_t code)
{
  int error;

  if (!device->ops || !device->ops->f_msg)
    return -1;

  dev_manifest_t* prev = get_current_driver();
  set_current_driver(device->manifest);

  error = device->ops->f_msg(device, code);

  set_current_driver(prev);

  return error;
}

int destroy_video_device(video_device_t* device)
{
  return device->ops->f_destroy(device);
}
