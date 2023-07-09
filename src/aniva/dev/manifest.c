#include "manifest.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sync/mutex.h"
#include <libk/string.h>

size_t get_driver_url_length(aniva_driver_t* handle) {
  const char* driver_type_url = get_driver_type_url(handle->m_type);
  const size_t dtu_length = strlen(driver_type_url);
  return strlen(driver_type_url) + 1 + strlen(handle->m_name);
}

const char* get_driver_url(aniva_driver_t* handle) {
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


bool manifest_write_fn(aniva_driver_t* driver, int(*write_fn)())
{
  dev_manifest_t* manifest;

  if (!driver || !write_fn)
    return false;
  
  manifest = driver->m_manifest;

  if (!manifest)
    return false;

  mutex_lock(&manifest->m_lock);

  manifest->m_ops.f_write = write_fn;

  mutex_unlock(&manifest->m_lock);

  return true;
}

bool manifest_read_fn(aniva_driver_t* driver, int(*read_fn)())
{
  dev_manifest_t* manifest;

  if (!driver || !read_fn)
    return false;
  
  manifest = driver->m_manifest;

  if (!manifest)
    return false;

  mutex_lock(&manifest->m_lock);

  manifest->m_ops.f_write = read_fn;

  mutex_unlock(&manifest->m_lock);

  return true;
}

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle, uint8_t flags) {

  if (!handle) {
    return nullptr;
  }

  /* If it already has one, no need to make it */
  if (handle->m_manifest) {
    return handle->m_manifest;
  }

  dev_manifest_t* ret = allocate_dmanifest();

  if (!ret)
    return nullptr;

  init_mutex(&ret->m_lock, NULL);

  ret->m_handle = handle;
  ret->m_dep_count = handle->m_dep_count;
  ret->m_dependency_manifests = init_list();

  ret->m_type = handle->m_type;
  ret->m_check_version = handle->m_version;
  ret->m_check_ident = handle->m_ident;

  ret->m_flags = flags;

  ret->m_url_length = get_driver_url_length(handle);
  // TODO: concat
  ret->m_url = get_driver_url(handle);

  /*
   * We kinda trust too much in the fact that m_dep_count is 
   * correct...
   */
  for (uintptr_t i = 0; i < ret->m_dep_count; i++) {

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
      destroy_dev_manifest(ret);
      return nullptr;
    }

    list_append(ret->m_dependency_manifests, dep_manifest);
  }

  return ret;
}

void destroy_dev_manifest(dev_manifest_t* manifest) {
  // TODO: figure out if we need to take the driver handle with us...

  clear_mutex(&manifest->m_lock);

  destroy_list(manifest->m_dependency_manifests);
  kfree((void*)manifest->m_url);
  free_dmanifest(manifest);
}
