#include "dev/video/core.h"
#include "dev/video/device.h"
#include "kevent/event.h"

void init_video()
{
  init_vdevice();

  /* Any events related to video devices */
  ASSERT(KERR_OK(add_kevent(VDEV_EVENTNAME, KE_DEVICE_EVENT, NULL, 128)));
}
