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

void generic_driver_entry(dev_manifest_t* driver);

/*
 * We use goto's here to make sure that node->m_lock is unlocked when this function exits
 */
static int driver_fs_msg(vnode_t* node, driver_control_code_t code, void* buffer, size_t size) {

  int result;
  mutex_lock(node->m_lock);

  if (!node) {
    result = -1;
    goto generic_exit;
  }

  node->m_access_count++;

  if (!vnode_has_driver(node)) {
    result = -1;
    goto generic_exit;
  }

  /* Fetch the driver from our node */
  aniva_driver_t* driver = node->m_drv;

  /* Find the assigned socket for that driver */
  threaded_socket_t* socket = find_registered_socket(driver->m_port);

  if (!socket) {
    result = -1;
    goto generic_exit;
  }

  packet_payload_t payload;
  packet_response_t* response;

  init_packet_payload(&payload, nullptr, buffer, size, code);

  /* Lock the mutex to ensure we are clear to use all of the drivers resources */
  mutex_lock(socket->m_packet_mutex);

  result = (int)driver->f_drv_msg(payload, &response);

  mutex_unlock(socket->m_packet_mutex);

  /* When dealing with fs messages, we don't really care about responses */
  if (response) {
    destroy_packet_response(response);
  }

  destroy_packet_payload(&payload);

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

  return active_flag && !mutex_is_locked(&manifest->m_lock);
}

bool driver_is_busy(dev_manifest_t* manifest)
{
  if (!manifest)
    return false;

  return (mutex_is_locked(&manifest->m_lock));
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

  mutex_lock(&manifest->m_lock);

  result = manifest->m_ops.f_read(driver, buffer, buffer_size, offset);

  mutex_unlock(&manifest->m_lock);

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

  mutex_lock(&manifest->m_lock);

  result = manifest->m_ops.f_write(driver, buffer, buffer_size, offset);

  mutex_unlock(&manifest->m_lock);

  return result;
}

void generic_driver_entry(dev_manifest_t* manifest) {

  /* Just make sure this driver is marked as inactive */
  manifest->m_flags &= ~DRV_ACTIVE;

  FOREACH(i, manifest->m_dependency_manifests) {
    dev_manifest_t* dep_manifest = i->data;

    if (!dep_manifest || !is_driver_loaded(dep_manifest)) {
      /* TODO: check if the dependencies allow for dynamic loading and if so, load them anyway */
      kernel_panic("dependencies are not loaded!");

      /* Mark the driver as failed */
      manifest->m_flags |= DRV_FAILED;
      return;
    }
  }

  /* NOTE: The driver can also mark itself as ready */
  int result = manifest->m_handle->f_init();

  if (result < 0) {
    // Unload the driver
    kernel_panic("Driver failed to init, TODO: implement graceful driver unloading");
  }

  /* Init finished, the driver is ready for messages */
  manifest->m_flags |= DRV_ACTIVE;
}

ErrorOrPtr bootstrap_driver(dev_manifest_t* manifest) {

  if (!manifest)
    return Error();

  if (manifest->m_flags & DRV_FS) {
    /* TODO: redo */
  }

  /* Preemptively set the driver to inactive */
  manifest->m_flags &= ~DRV_ACTIVE;

  // NOTE: if the drivers port is not valid, the subsystem will verify 
  // it and provide a new port, so we won't have to wory about that here
  generic_driver_entry(manifest);

  return Success(0);
}
