#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <dev/video/device.h>

static mutex_t* _vid_core_lock;
static dev_manifest_t* _vid_core_manifest;

int init_video();
int exit_video();

uint64_t msg_video(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  ErrorOrPtr result = Success(0);
  dev_manifest_t* act_drv;
  device_t* act_device;

  mutex_lock(_vid_core_lock);

  /* NOTE: by default, we simply pass on any messages for the active graphics driver through here */
  act_device = nullptr;

  if (manifest_find_device(_vid_core_manifest, &act_device, VIDDEV_MAINDEVICE) || !act_device)
    return DRV_STAT_INVAL;

  /* The active driver is the one attached to the main device */
  act_drv = act_device->parent;

  /* TODO: cache this request when there is no active video device */
  ASSERT(act_drv);

  switch (code) {
    default:
      {
        result = driver_send_msg_ex(act_drv, code, buffer, size, out_buffer, out_size);
        break;
      }
  }

  mutex_unlock(_vid_core_lock);

  /* Since driver_send_msg_ex will pass us the error code from the underlying driver, we're fine here =) */
  return Release(result);
}

/*
 * The core graphics driver (video)
 *
 * This driver acts as a gateway for any other entity to the graphics subsystem. When we load 
 * the internal drivers of the kernel, the graphics driver whith the highest precedence gets marked as
 * active and that will be the driver we will be providing for 
 *
 * TODO: refactor the core video driver to use driver devices
 * The current way we do this is some wonky shit with a private field on the drivers manifest, 
 * which sucks. A video device should be attached to the core video driver under the name 'maindev'
 * which will result in the complete path 'Dev/core/video/maindev'. The video core will assume the
 * device at this path is one managed by only one driver. Drivers that wish to replace a current (probably
 * generic) video driver, must issue a detach and cleanup of the 'maindev' device and driver to the
 * core video driver. It will then ask the current video driver to clean up its shit and unload it. At 
 * this point there is no video device for the rest of the system. This means we need to cache all the users
 * of the video device, before we remove it, so we can notify them of this device change (It should not 
 * matter much, just for modesetting and device properties). Simplefied, this would look something like this:
 *  - (More advanced) video driver gets loaded
 *  - Cache the handles to the current video device
 *  - Remove the current video device and it's driver
 *  - Let the new video device do it's initialization
 *  - Send a notify to all the handles of the video device at 'Dev/core/video/maindev'
 * At this point handle holders may send new messages to the video device. In order to ensure no accedental access
 * to a non-exsistant video device, we can do either of two things:
 *  - Issue a system-wide pause in the form of a scheduler pause and interrupt catching
 *  - Simply cache any messages going to the would-be video device and then send them one after the other after 
 *    the new device has been initialized
 *
 * Another TODO: create message contexts so we can track the origin and validity of any message
 */
aniva_driver_t vid_core = {
  .m_name = "video",
  .m_descriptor = "Manage video drivers",
  .f_init = init_video,
  .f_exit = exit_video,
  .f_msg = msg_video,
  .m_type = DT_CORE,
};
EXPORT_CORE_DRIVER(vid_core);


int init_video()
{
  int result;

  println("Initialized video core");

  _vid_core_lock = create_mutex(NULL);

  result = register_core_driver(&vid_core, DT_GRAPHICS);

  if (result)
    return result;

  _vid_core_manifest = try_driver_get(&vid_core, NULL);

  ASSERT(_vid_core_manifest);
  return 0;
}

int exit_video()
{
  destroy_mutex(_vid_core_lock);

  unregister_core_driver(&vid_core);

  return 0;
}

