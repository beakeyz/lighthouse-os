#include "driver.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/manifest.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <mem/heap.h>
#include <libk/string.h>

int generic_driver_entry(dev_manifest_t* driver);

/*
 * We use goto's here to make sure that node->m_lock is unlocked when this function exits
 */
static int driver_fs_msg(vnode_t* node, driver_control_code_t code, void* buffer, size_t size) {

  int result;
  aniva_driver_t* driver;
  mutex_lock(node->m_lock);

  if (!node) {
    result = -1;
    goto generic_exit;
  }


  if (!vnode_has_driver(node)) {
    result = -1;
    goto generic_exit;
  }

  /* Fetch the driver from our node */
  driver = node->m_drv;

  if (!driver) {
    result = -1;
    goto generic_exit;
  }

  node->m_access_count++;

  result = driver->f_msg(driver, code, buffer, size, NULL, NULL);

generic_exit:
  mutex_unlock(node->m_lock);
  return result;
}

/* TODO: finish checkups and preps for fs */
vnode_t* create_fs_driver(dev_manifest_t* manifest) {

  aniva_driver_t* driver;

  if (!(manifest->m_flags & DRV_FS))
    return nullptr;

  driver = manifest->m_handle;

  /* 
   * There isn't really anything special about fs_driver nodes. 
   * They simply need some IO so make driver interaction easy, so
   * just modifying a generic vnode is fine for now
   *
   * Driver nodes are also marked as frozen, since they are not meant 
   * to be written to
   */
  vnode_t* node = create_generic_vnode(driver->m_name, VN_SYS | VN_FROZEN);

  vnode_set_type(node, VNODE_DRIVER);

  /* Add dev handle */
  node->m_drv = driver;

  /* Add msg hook */
  node->m_ops->f_msg = driver_fs_msg;

  return node;
}

void detach_fs_driver(vnode_t* node) {
  kernel_panic("TODO: implement detach_fs_driver");
}

bool driver_is_ready(dev_manifest_t* manifest) {

  const bool active_flag = (manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE;

  return active_flag && !mutex_is_locked(manifest->m_lock);
}

bool driver_is_busy(dev_manifest_t* manifest)
{
  if (!manifest)
    return false;

  return (mutex_is_locked(manifest->m_lock));
}

/*
 * Quick TODO: create a way to validate pointer origin
 */
int drv_read(dev_manifest_t* manifest, void* buffer, size_t* buffer_size, uintptr_t offset)
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

int drv_write(dev_manifest_t* manifest, void* buffer, size_t* buffer_size, uintptr_t offset)
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

int generic_driver_entry(dev_manifest_t* manifest)
{
  if ((manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE)
    return -1;

  FOREACH(i, manifest->m_dependency_manifests) {
    dev_manifest_t* dep_manifest = i->data;

    if (!dep_manifest || !is_driver_loaded(dep_manifest)) {
      /* TODO: check if the dependencies allow for dynamic loading and if so, load them anyway */
      kernel_panic("dependencies are not loaded!");

      /* Mark the driver as failed */
      manifest->m_flags |= DRV_FAILED;
      return -1;
    }
  }

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

ErrorOrPtr bootstrap_driver(dev_manifest_t* manifest) 
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
