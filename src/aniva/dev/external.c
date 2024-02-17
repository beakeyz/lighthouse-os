#include "external.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "oss/obj.h"

extern_driver_t* create_external_driver(uint32_t flags)
{
  extern_driver_t* drv;

  drv = kzalloc(sizeof(extern_driver_t));

  if (!drv)
    return nullptr;

  memset(drv, 0, sizeof(extern_driver_t));

  drv->m_flags = flags;

  if ((flags & EX_DRV_OLDMANIFEST) != EX_DRV_OLDMANIFEST) {
    drv->m_manifest = create_drv_manifest(nullptr); 
    drv->m_manifest->m_flags |= (DRV_IS_EXTERNAL);
  }

  return drv;
}

/*
 * TODO: move this resource clear routine to the resource context subsystem
 *
 * NOTE: caller should have the manifests mutex held
 */
void destroy_external_driver(extern_driver_t* driver)
{
  drv_manifest_t* manifest;

  manifest = driver->m_manifest;

  /*
   * We need to let the manifest know that it does not have a driver anymore xD
   */
  if (manifest) {
    manifest->m_handle = nullptr;
    manifest->m_external = nullptr;

    manifest->m_flags &= ~DRV_HAS_HANDLE;
    manifest->m_flags |= DRV_DEFERRED_HNDL;

    /* Make sure we also deallocate our load base, just in case */
    if (driver->m_load_size)
      Must(__kmem_dealloc(nullptr, manifest->m_resources, driver->m_load_base, driver->m_load_size));
  }

  kzfree(driver, sizeof(extern_driver_t));
}

