#include "core.h"
#include "dev/core.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"


void register_video_device(video_device_t* device)
{
  bool result;
  dev_manifest_t* manifest = get_driver("this");

  if (!manifest)
    return;

  result = install_private_data(manifest->m_handle, device);

  if (!result)
    return;

  device->manifest = manifest;
}

void unregister_video_device(struct video_device* device)
{
  dev_manifest_t* manifest = device->manifest;

  mutex_lock(&manifest->m_lock);

  manifest->m_private = nullptr;

  mutex_unlock(&manifest->m_lock);

  destroy_video_device(device);
}

struct video_device* get_active_video_device()
{
  /* NOTE: the first graphics driver (also hopefully the only) is always the active driver */
  dev_manifest_t* manifest = get_driver_from_type(DT_GRAPHICS, 0);

  if (!manifest)
    return nullptr;

  /* The private data SHOULD contain the video device struct lol */
  return manifest->m_private;
}

void init_video()
{
  
}
