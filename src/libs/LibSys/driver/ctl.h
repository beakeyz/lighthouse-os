#ifndef __LIGHTENV_LIBSYS_DRIVER_CTL__
#define __LIGHTENV_LIBSYS_DRIVER_CTL__

#include "LibSys/handle_def.h"
#include <sys/types.h>

/*
 * Info block about a specific driver
 */
typedef struct drv_info {
  const char* description;
  const char* type_str;
  uint32_t flags;
  uint32_t class;
} drv_info_t;

/*
 * Kind of driver ctl
 * Driver messages don't count as ctl
 */
enum DRV_CTL_MODE {
  DRV_CTL_LOAD,
  DRV_CTL_UNLOAD,
  DRV_CTL_RELOAD,
  DRV_CTL_INFO,
};

enum DRV_CTL_ACCESS_MODE {
  DRV_ACCESS_HANDLE,
  DRV_ACCESS_PATH,
  DRV_ACCESS_FILEPATH,
};

#endif // !__LIGHTENV_LIBSYS_DRIVER_CTL__
