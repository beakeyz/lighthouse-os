#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "sync/mutex.h"

static const char* _current_active_path;
static mutex_t* _vid_core_lock;

int init_video();
int exit_video();

uint64_t msg_video(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  switch (code) {
    case VID_CORE_DCC_REGISTER_ACTIVE:
      {
        dev_manifest_t* active = buffer;
        _current_active_path = active->m_url;
        break;
      }
  }
  return 0;
}

aniva_driver_t vid_core = {
  .m_name = "video",
  .m_descriptor = "Manage video drivers",
  .f_init = init_video,
  .f_exit = exit_video,
  .f_msg = msg_video,
  .m_type = DT_CORE,
};


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


EXPORT_CORE_DRIVER(vid_core);
