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

void generic_driver_entry(aniva_driver_t* driver);

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

  packet_payload_t* payload = create_packet_payload(buffer, size, code);
  packet_response_t* response;

  /* Lock the mutex to ensure we are clear to use all of the drivers resources */
  mutex_lock(socket->m_packet_mutex);

  result = (int)driver->f_drv_msg(*payload, &response);

  mutex_unlock(socket->m_packet_mutex);

  /* When dealing with fs messages, we don't really care about responses */
  if (response) {
    destroy_packet_response(response);
  }

  destroy_packet_payload(payload);

generic_exit:
  mutex_unlock(node->m_lock);
  return result;
}

aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  driver_identifier_t identifier,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  SocketOnPacket drv_msg,
  DEV_TYPE type
) {
  aniva_driver_t* ret = kmalloc(sizeof(aniva_driver_t));

  strcpy((char*)ret->m_name, name);
  strcpy((char*)ret->m_descriptor, descriptor);

  ret->m_version = version;

  // TODO: check if this identifier isn't taken
  ret->m_ident = identifier;
  ret->f_init = init;
  ret->f_exit = exit;
  ret->f_drv_msg = drv_msg;
  ret->m_type = type;
  ret->m_port = 0;

  return ret;
}

/* TODO: finish checkups and preps for fs */
vnode_t* create_fs_driver(aniva_driver_t* driver) {
  if (!(driver->m_flags & DRV_FS))
    return nullptr;

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

void destroy_driver(aniva_driver_t* driver) {
  // TODO: when a driver holds on to resources, we need to clean them up too (if they are not being referenced ofcourse)
  kfree(driver);
}

bool validate_driver(aniva_driver_t* driver) {
  if (driver == nullptr || driver->f_init == nullptr || driver->f_exit == nullptr) {
    return false;
  }

  return true;
}

bool driver_is_ready(aniva_driver_t* driver) {

  const bool active_flag = driver->m_flags & DRV_ACTIVE;

  if (driver->m_flags & DRV_SOCK) {

    threaded_socket_t* socket = find_registered_socket(driver->m_port);

    if (!socket)
      return false;

    return active_flag && (socket_is_flag_set(socket, TS_ACTIVE) && socket_is_flag_set(socket, TS_READY));
  }

  return active_flag;
}

bool driver_is_busy(aniva_driver_t* driver)
{
  if (!driver || !driver->m_manifest)
    return false;

  return (mutex_is_locked(&driver->m_manifest->m_lock));
}

/*
 * Quick TODO: create a way to validata pointer origin
 */
int drv_read(aniva_driver_t* driver, void* buffer, size_t* buffer_size)
{
  int result;
  dev_manifest_t* manifest;

  if (!driver ||!buffer || !buffer_size)
    return DRV_STAT_INVAL;

  manifest = driver->m_manifest;

  if (!manifest || !manifest->m_ops.f_read)
    return DRV_STAT_NOMAN;

  if (driver_is_busy(driver))
    return DRV_STAT_BUSY;

  mutex_lock(&manifest->m_lock);

  result = manifest->m_ops.f_read(driver, buffer, buffer_size);

  mutex_unlock(&manifest->m_lock);

  return result;
}

int drv_write(aniva_driver_t* driver, void* buffer, size_t* buffer_size)
{
  int result;
  dev_manifest_t* manifest;

  if (!driver ||!buffer || !buffer_size)
    return DRV_STAT_INVAL;

  manifest = driver->m_manifest;

  if (!manifest || !manifest->m_ops.f_read)
    return DRV_STAT_NOMAN;

  if (driver_is_busy(driver))
    return DRV_STAT_BUSY;

  mutex_lock(&manifest->m_lock);

  result = manifest->m_ops.f_write(driver, buffer, buffer_size);

  mutex_unlock(&manifest->m_lock);

  return result;
}

void generic_driver_entry(aniva_driver_t* driver) {

  /* Just make sure this driver is marked as inactive */
  driver->m_flags &= ~DRV_ACTIVE;

  /* Ensure dependencies are loaded */
  dev_manifest_t* manifest = create_dev_manifest(driver, 0);

  FOREACH(i, manifest->m_dependency_manifests) {
    dev_manifest_t* dep_manifest = i->data;

    if (!dep_manifest || !is_driver_loaded(dep_manifest->m_handle)) {
      /* TODO: check if the dependencies allow for dynamic loading and if so, load them anyway */
      kernel_panic("dependencies are not loaded!");

      /* Mark the driver as failed */
      driver->m_flags |= DRV_FAILED;
      return;
    }
  }

  /* NOTE: The driver can also mark itself as ready */
  int result = driver->f_init();

  if (result < 0) {
    // Unload the driver
    kernel_panic("Driver failed to init, TODO: implement graceful driver unloading");
  }

  /* Init finished, the driver is ready for messages */
  driver->m_flags |= DRV_ACTIVE;
}

ErrorOrPtr bootstrap_driver(aniva_driver_t* driver, dev_url_t path) {

  ErrorOrPtr result;

  if (!driver)
    return Error();

  /* TODO: check if the driver fs is actually enabled */
  /* TODO: create an fs type to host all the driver shit */
  if (driver->m_flags & DRV_FS) {
    /* TODO: check if this path is already used by a driver */

    uintptr_t i;
    const char* parsed_driver_name;
    size_t path_length;

    path_length = strlen(path);
    i = path_length - 1;

    while (i && path[i] && path[i] != VFS_PATH_SEPERATOR) {
      i--;
    }

    /*
     * Reached the start of the path. We don't allow the mounting of devices
     * in the root of the VFS, so we'll return an error =/
     */
    if (!i) {
      return Error();
    }

    parsed_driver_name = &path[i+1];

    /* Invalid path */
    if (!memcmp(parsed_driver_name, driver->m_name, strlen(driver->m_name))
        || parsed_driver_name[strlen(driver->m_name)]) {
      return Error();
    }

    char path_copy[path_length];
    memcpy(path_copy, path, path_length);

    path_copy[i] = '\0';

    // println("Mounting driver");
    // println(path_copy);
    // println(parsed_driver_name);

    result = vfs_mount_driver(path_copy, driver);

    if (result.m_status != ANIVA_SUCCESS) {
      kernel_panic("TODO: check for errors while mounting driver to vfs!");
      return result;
    }
  }

  /* Preemptively set the driver to inactive */
  driver->m_flags &= ~DRV_ACTIVE;

  // NOTE: if the drivers port is not valid, the subsystem will verify 
  // it and provide a new port, so we won't have to wory about that here

  if (driver->m_flags & DRV_SOCK) {

    println("Creating socket driver");
    thread_t* driver_thread = create_thread_as_socket(sched_get_kernel_proc(), (FuncPtr)generic_driver_entry, (uintptr_t)driver, (FuncPtr)driver->f_exit, driver->f_drv_msg, (char*)driver->m_name, &driver->m_port);

    result = proc_add_thread(sched_get_kernel_proc(), driver_thread);

    println("Created socket driver");
  } else {
    generic_driver_entry(driver);
  }

  return result;
}
