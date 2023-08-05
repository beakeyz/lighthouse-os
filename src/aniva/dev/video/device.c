#include "device.h"
#include "dev/manifest.h"
#include "proc/core.h"

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
