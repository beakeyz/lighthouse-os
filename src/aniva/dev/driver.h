#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__
#include <libk/stddef.h>
#include "core.h"

typedef struct driver_version {
  uint8_t m_minor;
  uint8_t m_major;
  uint8_t m_bump;
} driver_version_t;

typedef struct aniva_driver {
  const char m_name[MAX_DRIVER_NAME_LENGHT];
  driver_version_t m_version;

  // TODO:
} aniva_driver_t;

void init_aniva_driver_register();

#endif //__ANIVA_DRIVER__
