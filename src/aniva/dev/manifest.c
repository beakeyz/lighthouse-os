#include "manifest.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include <libk/string.h>

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle, void** deps, size_t deps_count, uint8_t flags) {

  if (!handle) {
    return nullptr;
  }

  dev_manifest_t* ret = kmalloc(sizeof(dev_manifest_t));

  if (!ret)
    return nullptr;

  ret->m_handle = handle;

  ret->m_dependencies = (aniva_driver_t**)deps;
  ret->m_dep_count = deps_count;

  ret->m_type = handle->m_type;
  ret->m_check_version = handle->m_version;
  ret->m_check_ident = handle->m_ident;

  ret->m_flags = flags;

  const char* driver_type_url = get_driver_type_url(handle->m_type);
  const size_t dtu_length = strlen(driver_type_url);
  ret->m_url_length = strlen(driver_type_url) + 1 + strlen(handle->m_name);
  // TODO: concat
  ret->m_url = kmalloc(ret->m_url_length + 1);
  memset((char*)ret->m_url, 0, ret->m_url_length + 1);
  memcpy((char*)ret->m_url, driver_type_url, dtu_length);
  *(char*)&ret->m_url[dtu_length] = DRIVER_URL_SEPERATOR;
  memcpy((char*)ret->m_url + dtu_length + 1, handle->m_name, strlen(handle->m_name));

  return ret;
}

void destroy_dev_manifest(dev_manifest_t* manifest) {
  // TODO: figure out if we need to take the driver handle with us...
  kfree((void*)manifest->m_url);
  kfree(manifest);
}
