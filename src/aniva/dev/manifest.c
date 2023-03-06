#include "manifest.h"
#include "mem/heap.h"
#include <libk/string.h>

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle, void** deps, size_t deps_count, dev_url_t url, uint8_t flags) {

  if (!handle || strlen(url) == 0) {
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
  ret->m_check_ident = handle->n_ident;

  ret->m_flags = flags;

  ret->m_url_length = strlen(url);
  ret->m_url = url;

  return ret;
}
