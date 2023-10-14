
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include <dev/core.h>
#include <dev/driver.h>

int lightenv_init();
int lightenv_exit();

uint64_t lightenv_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

EXPORT_DRIVER(env_driver) = {
  .m_name = "login",
  .m_type = DT_SERVICE,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = nullptr,
  .f_exit = nullptr,
  .f_msg = nullptr,
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};

/*
 * What is logins purpose?
 * - manage profile logins
 * - launch the correct environment (either graphical or terminal)
 */
int lightenv_init()
{
  fb_color_t color = {
    .raw_clr = RGBA32(0, 0, 0, 0xff),
  };
  viddev_blt_t blt = {
    .x = 0,
    .y = 0,
    .width = (uint32_t)-1,
    .height = (uint32_t)-1,
    .mode = VIDDEV_BLT_MODE_COLOR,
    .buffer = &color
  };

  /* Send a BLT message to the video driver to clear the screen */
  driver_send_msg("core/video", VIDDEV_DCC_BLT, &blt, sizeof(blt));

  /* ??? */

  return 0;
}

int lightenv_exit()
{
  return 0;
}

uint64_t lightenv_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return 0;
}
