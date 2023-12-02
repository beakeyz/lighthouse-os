#include "manifest.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/string.h>

/* 
 * A single driver can't manage more devices than this. If for some reason this number is
 * exceeded for good reason, we should create multiplexer drivers or something 
 */
#define DM_MAX_DEVICES 512

size_t get_driver_url_length(aniva_driver_t* handle) 
{
  const char* driver_type_url = get_driver_type_url(handle->m_type);
  return strlen(driver_type_url) + 1 + strlen(handle->m_name);
}

const char* get_driver_url(aniva_driver_t* handle) 
{
  /* Prerequisites */
  const char* dtu = get_driver_type_url(handle->m_type);
  size_t dtu_length = strlen(dtu);

  size_t size = get_driver_url_length(handle);
  const char* ret = kmalloc(size + 1);
  memset((char*)ret, 0, size + 1);
  memcpy((char*)ret, dtu, dtu_length);
  *(char*)&ret[dtu_length] = DRIVER_URL_SEPERATOR;
  memcpy((char*)ret + dtu_length + 1, handle->m_name, strlen(handle->m_name));

  return ret;
}


bool driver_manifest_write(aniva_driver_t* driver, int(*write_fn)())
{
  dev_url_t path;
  dev_manifest_t* manifest;

  if (!driver || !write_fn)
    return false;

  path = get_driver_url(driver);

  if (!path)
    return false;
  
  manifest = get_driver(path);

  if (!manifest)
    return false;

  kfree((void*)path);

  mutex_lock(manifest->m_lock);

  manifest->m_ops.f_write = write_fn;

  mutex_unlock(manifest->m_lock);

  return true;
}

bool driver_manifest_read(aniva_driver_t* driver, int(*read_fn)())
{
  dev_url_t path;
  dev_manifest_t* manifest;

  if (!driver || !read_fn)
    return false;

  path = get_driver_url(driver);

  if (!path)
    return false;
  
  manifest = get_driver(path);

  kfree((void*)path);

  if (!manifest)
    return false;

  mutex_lock(manifest->m_lock);

  manifest->m_ops.f_read = read_fn;

  mutex_unlock(manifest->m_lock);

  return true;
}

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle)
{
  dev_manifest_t* ret;

  ret = allocate_dmanifest();

  ASSERT(ret);

  memset(ret, 0, sizeof(*ret));

  ret->m_lock = create_mutex(NULL);
  ret->m_device_lock = create_mutex(NULL);
  ret->m_handle = handle;

  ret->m_flags = NULL;
  ret->m_dep_count = NULL;
  ret->m_dependency_manifests = init_list();
  ret->m_device_map = create_hashmap(DM_MAX_DEVICES, HASHMAP_FLAG_SK | HASHMAP_FLAG_FS);

  /* Reset the manifest opperations */
  memset(&ret->m_ops, 0, sizeof(ret->m_ops));

  if (handle) {
    ret->m_check_version = handle->m_version;
    ret->m_url_length = get_driver_url_length(handle);
    // TODO: concat
    ret->m_url = get_driver_url(handle);
  } else {
    ret->m_flags |= DRV_DEFERRED_HNDL;
  }

  create_resource_bundle(&ret->m_resources);

  return ret;
}

ErrorOrPtr manifest_emplace_handle(dev_manifest_t* manifest, aniva_driver_t* handle)
{
  if (manifest->m_handle || !(manifest->m_flags & DRV_DEFERRED_HNDL))
    return Error();

  /* Mark the manifest as non-deferred */
  manifest->m_flags &= ~DRV_DEFERRED_HNDL;

  /* Emplace the handle and its data */
  manifest->m_handle = handle;
  manifest->m_check_version = handle->m_version;
  manifest->m_url_length = get_driver_url_length(handle);
  manifest->m_url = get_driver_url(handle);

  return Success(0);
}

/*!
 * @brief Destroy a manifest and deallocate it's resources
 *
 * This assumes that the underlying driver has already been taken care of
 *
 * NOTE: does not destroy the devices on this manifest. Caller needs to make sure this is 
 * done. The driver behind this manifest needs to implement functions that handle the propper 
 * destruction of the devices, since it is the one who owns this memory
 */
void destroy_dev_manifest(dev_manifest_t* manifest) 
{
  destroy_mutex(manifest->m_lock);
  destroy_mutex(manifest->m_device_lock);

  destroy_hashmap(manifest->m_device_map);
  destroy_list(manifest->m_dependency_manifests);

  destroy_resource_bundle(manifest->m_resources);

  kfree((void*)manifest->m_url);

  if (manifest->m_driver_file_path)
    kfree((void*)manifest->m_driver_file_path);

  free_dmanifest(manifest);
}

/*!
 * @brief: 'serialize' the given driver paths into the manifest
 *
 * TODO: redo this once the dependency rework has gone through
 */
void manifest_gather_dependencies(dev_manifest_t* manifest)
{
  aniva_driver_t* handle = manifest->m_handle;

  /* We should not have any dependencies on the manifest at this point */
  ASSERT(!manifest->m_dep_count);
  ASSERT(manifest->m_dependency_manifests);

  /*
   * We kinda trust too much in the fact that m_dep_count is 
   * correct...
   */
  for (uintptr_t i = 0; i < handle->m_dep_count; i++) {

    /* TODO: we can check if this address is located in a 
     used resource in oder to validate it */
    dev_url_t url = handle->m_dependencies[i];
    
    if (!url)
      continue;

    /*
     * Get the driver from the installed pool
     */
    dev_manifest_t* dep_manifest = get_driver(url);

    // if we can't load this drivers dependencies, we just terminate this entire thing
    if (!dep_manifest) {
      return;
    }

    list_append(manifest->m_dependency_manifests, dep_manifest);

    manifest->m_dep_count++;
  }
}

bool ensure_dependencies(dev_manifest_t* manifest)
{
  return (manifest->m_handle && manifest->m_dep_count == manifest->m_handle->m_dep_count);
}

/*!
 * @brief: Get the amount of driver that are attached on this manifest
 */
int manifest_get_device_count(dev_manifest_t* manifest, uint32_t* count)
{
  if (!manifest || !manifest_is_active(manifest))
    return -1;

  if (!count)
    return -2;

  *count = manifest->m_device_map->m_size;
  return 0;
}

/*!
 * @brief: Add a device to this manifest
 */
int manifest_add_device(dev_manifest_t* manifest, device_t* device)
{
  ErrorOrPtr res;

  if (!manifest || !device)
    return -1;

  mutex_lock(manifest->m_device_lock);

  res = hashmap_put(manifest->m_device_map, (hashmap_key_t)device->device_path, device);

  mutex_unlock(manifest->m_device_lock);

  if (IsError(res))
    return -2;

  device->link = manifest;

  return 0;
}

/*!
 * @brief: Remove a device from this manifest
 */
int manifest_remove_device(dev_manifest_t* manifest, const char* device)
{
  device_t* dev;

  if (!manifest || !manifest_is_active(manifest))
    return -1;

  if (!device)
    return -2;

  mutex_lock(manifest->m_device_lock);

  dev = hashmap_remove(manifest->m_device_map, (hashmap_key_t)device);

  mutex_unlock(manifest->m_device_lock);

  if (!dev)
    return -3;

  dev->link = nullptr;
  return 0;
}

/*!
 * @brief: Get a device from the manifest
 *
 * Caller needs to verify @dev_buffer
 */
int manifest_find_device(dev_manifest_t* manifest, device_t** dev_buffer, const char* device_name)
{
  device_t* t;

  if (!manifest || !manifest_is_active(manifest))
    return -1;

  if (!dev_buffer || !device_name)
    return -2;

  mutex_lock(manifest->m_device_lock);

  t = hashmap_get(manifest->m_device_map, (hashmap_key_t)device_name);

  mutex_unlock(manifest->m_device_lock);

  if (!t)
    return -3;

  *dev_buffer = t;
  return 0;
}
