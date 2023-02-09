#include "driver.h"
#include "mem/kmalloc.h"
#include <libk/string.h>

aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  driver_identifier_t identifier,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  ANIVA_DRIVER_DRV_MSG drv_msg,
  DEV_TYPE_t type
) {
  aniva_driver_t* ret = kmalloc(sizeof(aniva_driver_t));

  strcpy((char*)ret->m_name, name);
  strcpy((char*)ret->m_descriptor, descriptor);

  ret->m_version = version;

  // TODO: check if this identifier isn't taken
  ret->n_ident = identifier;
  ret->f_init = init;
  ret->f_exit = exit;
  ret->f_drv_msg = drv_msg;
  ret->m_type = type;

  return ret;
}

bool validate_driver(aniva_driver_t* driver) {
  if (driver == nullptr || driver->f_init == nullptr || driver->f_exit == nullptr) {
    return false;
  }

  return true;
}
