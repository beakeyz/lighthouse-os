#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <dev/video/device.h>

static mutex_t* _vid_core_lock;

int init_video();
int exit_video();

uint64_t msg_video(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  ErrorOrPtr result = Success(0);
  dev_manifest_t* active;

  mutex_lock(_vid_core_lock);

  /* NOTE: by default, we simply pass on any messages for the active graphics driver through here */
  active = get_main_driver_from_type(DT_GRAPHICS);

  switch (code) {
    default:
      {
        result = driver_send_msg_ex(active, code, buffer, size, out_buffer, out_size);
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

  return 0;
}

int exit_video()
{
  destroy_mutex(_vid_core_lock);

  unregister_core_driver(&vid_core);

  return 0;
}

