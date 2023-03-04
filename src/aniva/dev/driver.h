#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__
#include <libk/stddef.h>
#include "core.h"

typedef void (*ANIVA_DRIVER_INIT) ();
typedef int (*ANIVA_DRIVER_EXIT) ();
typedef int (*ANIVA_DRIVER_DRV_MSG) (char*, ...);

typedef struct driver_version {
  uint8_t maj;
  uint8_t min;
  uint16_t bump;
} driver_version_t;

typedef struct driver_identifier {
  uint8_t m_minor;
  uint8_t m_major;
} driver_identifier_t;

typedef struct aniva_driver {
  const char m_name[MAX_DRIVER_NAME_LENGTH];
  const char m_descriptor[MAX_DRIVER_DESCRIPTOR_LENGTH];
  driver_version_t m_version;
  driver_identifier_t n_ident;

  ANIVA_DRIVER_INIT f_init;
  ANIVA_DRIVER_EXIT f_exit;
  // FIXME: what arguments are best to pass here?
  ANIVA_DRIVER_DRV_MSG f_drv_msg;

  DEV_TYPE_t m_type;
  // TODO:
} aniva_driver_t;

aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  driver_identifier_t identifier,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  ANIVA_DRIVER_DRV_MSG drv_msg,
  DEV_TYPE_t type
  );

bool validate_driver(aniva_driver_t* driver);

#endif //__ANIVA_DRIVER__
