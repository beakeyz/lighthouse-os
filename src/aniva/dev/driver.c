#include "driver.h"
#include "dev/core.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "sync/mutex.h"
#include <mem/heap.h>
#include <libk/string.h>

int generic_driver_entry(drv_manifest_t* driver);

bool driver_is_ready(drv_manifest_t* manifest) 
{
  const bool active_flag = (manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE;

  return active_flag && !mutex_is_locked(manifest->m_lock);
}

bool driver_is_busy(drv_manifest_t* manifest)
{
  if (!manifest)
    return false;

  return (mutex_is_locked(manifest->m_lock));
}

/*
 * Quick TODO: create a way to validate pointer origin
 */
int drv_read(drv_manifest_t* manifest, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  int result;
  aniva_driver_t* driver;

  if (!manifest ||!buffer || !buffer_size)
    return DRV_STAT_INVAL;

  driver = manifest->m_handle;

  if (!driver || !manifest->m_ops.f_read)
    return DRV_STAT_NOMAN;

  if (driver_is_busy(manifest))
    return DRV_STAT_BUSY;

  mutex_lock(manifest->m_lock);

  result = manifest->m_ops.f_read(driver, buffer, buffer_size, offset);

  mutex_unlock(manifest->m_lock);

  return result;
}

int drv_write(drv_manifest_t* manifest, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  int result;
  aniva_driver_t* driver;

  if (!manifest ||!buffer || !buffer_size)
    return DRV_STAT_INVAL;

  driver = manifest->m_handle;

  if (!driver || !manifest->m_ops.f_write)
    return DRV_STAT_NOMAN;

  if (driver_is_busy(manifest))
    return DRV_STAT_BUSY;

  mutex_lock(manifest->m_lock);

  result = manifest->m_ops.f_write(driver, buffer, buffer_size, offset);

  mutex_unlock(manifest->m_lock);

  return result;
}

int generic_driver_entry(drv_manifest_t* manifest)
{
  if ((manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE)
    return -1;

  /* NOTE: The driver can also mark itself as ready */
  int error = manifest->m_handle->f_init();

  if (!error) {
    /* Init finished, the driver is ready for messages */
    manifest->m_flags |= DRV_ACTIVE;
  } else {
    manifest->m_flags |= DRV_FAILED;
  }

  /* Get any failures through to the bootstrap */
  return error;
}

ErrorOrPtr bootstrap_driver(drv_manifest_t* manifest) 
{
  int error;

  if (!manifest)
    return Error();

  if (manifest->m_flags & DRV_FS) {
    /* TODO: redo */
  }

  /* Preemptively set the driver to inactive */
  manifest->m_flags &= ~DRV_ACTIVE;
  manifest->m_flags &= ~DRV_FAILED;

  // NOTE: if the drivers port is not valid, the subsystem will verify 
  // it and provide a new port, so we won't have to wory about that here
  error = generic_driver_entry(manifest);

  if (error)
    return Error();

  return Success(0);
}
