#include "core.h"
#include "libk/flow/doorbell.h"
#include <dev/core.h>
#include <dev/driver.h>

static int io_core_init();
static int io_core_exit();
static uint64_t io_core_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

//static kdoorbell_t* event_db;

aniva_driver_t io_core = {
  .m_name = "io",
  .m_type = DT_CORE,
  .f_init = io_core_init,
  .f_exit = io_core_exit,
  .f_msg = io_core_msg,
  .m_version = DEF_DRV_VERSION(0, 0, 1),
};
EXPORT_CORE_DRIVER(io_core);

static int io_core_init()
{
  return 0;
}

static int io_core_exit()
{
  return 0;
}

static uint64_t io_core_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  return DRV_STAT_OK;
}
