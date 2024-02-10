#include "manifest.h"
#include "dev/core.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
#include "mem/heap.h"
#include "oss/node.h"
#include "oss/obj.h"
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
  ret->m_handle = handle;

  ret->m_flags = NULL;
  ret->m_dep_count = NULL;
  ret->m_dependency_manifests = init_list();

  /* Reset the manifest opperations */
  memset(&ret->m_ops, 0, sizeof(ret->m_ops));

  if (handle) {
    ret->m_check_version = handle->m_version;
    ret->m_url_length = get_driver_url_length(handle);
    // TODO: concat
    ret->m_url = get_driver_url(handle);

    ret->m_obj = create_oss_obj(handle->m_name);

    /* Make sure our object knows about us */
    oss_obj_register_child(ret->m_obj, ret, OSS_OBJ_TYPE_DRIVER, destroy_dev_manifest);
  } else {
    ret->m_flags |= DRV_DEFERRED_HNDL;
  }

  /* 
   * NOTE: drivers use the kernels page dir, which is why we can pass NULL 
   * This might be SUPER DUPER dumb but like idc and it's extra work to isolate
   * drivers from the kernel, so yea fuckoff
   */
  ret->m_resources = create_resource_bundle(NULL);

  return ret;
}

ErrorOrPtr manifest_emplace_handle(dev_manifest_t* manifest, aniva_driver_t* handle)
{
  if (manifest->m_handle || !(manifest->m_flags & DRV_DEFERRED_HNDL))
    return Error();

  ASSERT_MSG(manifest->m_obj == nullptr, "Tried to emplace a handle on a manifest which already has an object");

  /* Mark the manifest as non-deferred */
  manifest->m_flags &= ~DRV_DEFERRED_HNDL;

  /* Emplace the handle and its data */
  manifest->m_handle = handle;
  manifest->m_check_version = handle->m_version;
  manifest->m_url_length = get_driver_url_length(handle);
  manifest->m_url = get_driver_url(handle);

  manifest->m_obj = create_oss_obj(handle->m_name);

  /* Make sure our object knows about us */
  oss_obj_register_child(manifest->m_obj, manifest, OSS_OBJ_TYPE_DRIVER, destroy_dev_manifest);
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
  /*
   * An attached driver can be destroyed in two ways:
   * 1) Using this function directly. In this case we need to check if we are still attached to OSS and if we are
   * that means we need to destroy ourselves through that mechanism
   * 2) Through OSS. When we're attached to OSS, we have our own oss object that is attached to an oss node. When
   * the manifest was created we registered our destruction method there, which means that all dev_manifest memory
   * is owned by the oss_object
   */
  if (manifest->m_obj && manifest->m_obj->priv == manifest) {
    /* Calls destroy_dev_manifest again with ->priv cleared */
    destroy_oss_obj(manifest->m_obj);
    return;
  }

  destroy_mutex(manifest->m_lock);
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
