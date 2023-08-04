#include "external.h"
#include "dev/manifest.h"
#include "fs/vobj.h"
#include "mem/zalloc.h"
#include "system/resource.h"

extern_driver_t* create_external_driver(uint32_t flags)
{
  extern_driver_t* drv;

  drv = kzalloc(sizeof(extern_driver_t));

  if (!drv)
    return nullptr;

  memset(drv, 0, sizeof(extern_driver_t));

  drv->m_process = NULL;
  drv->m_file = NULL;
  drv->m_flags = flags;

  drv->m_manifest = create_dev_manifest(nullptr); 

  return drv;
}

/*
 * TODO: move this resource clear routine to the resource context subsystem
 */
void destroy_external_driver(extern_driver_t* driver)
{
  /* We can now close the file if it has one */
  if (driver->m_file)
    vobj_close(driver->m_file->m_obj);

  if (driver->m_manifest)
    destroy_dev_manifest(driver->m_manifest);

  kzfree(driver, sizeof(extern_driver_t));
}

