#ifndef __ANIVA_DEV_MANIFEST__
#define __ANIVA_DEV_MANIFEST__

#include "dev/core.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/data/hashmap.h>

struct oss_obj;
struct extern_driver;

/*
 * We let drivers implement a few functions that are mostly meant to
 * simulate the file opperations like read, write, seek, ect. that some 
 * linux / unix drivers would implement.
 */
typedef struct driver_ops {
  int (*f_write) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
  int (*f_read) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
} driver_ops_t;

/*
 * TODO: rework driver dependencies to make every driver keep track of a list of 
 * other drivers that depend on them
 *
 * TODO: work out device attaching
 */
typedef struct dev_manifest {
  aniva_driver_t* m_handle;
  list_t* m_dependency_manifests;
  size_t m_dep_count;

  uint32_t m_flags;
  driver_version_t m_check_version;

  /* Resources that this driver has claimed */
  kresource_bundle_t* m_resources;
  mutex_t* m_lock;

  /* Url of the installed driver */
  dev_url_t m_url;
  size_t m_url_length;

  struct oss_obj* m_obj;
  /* Any data that's specific to the kind of driver this is */
  //void* m_private;

  /* Path to the binary of the driver, only on external drivers */
  const char* m_driver_file_path;
  struct extern_driver* m_external;

  driver_ops_t m_ops;
} dev_manifest_t;

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle);
void destroy_dev_manifest(dev_manifest_t* manifest);

void manifest_gather_dependencies(dev_manifest_t* manifest);
bool ensure_dependencies(dev_manifest_t* manifest);

bool is_manifest_valid(dev_manifest_t* manifest);

ErrorOrPtr manifest_emplace_handle(dev_manifest_t* manifest, aniva_driver_t* handle);

bool driver_manifest_write(struct aniva_driver* manifest, int(*write_fn)());
bool driver_manifest_read(struct aniva_driver* manifest, int(*read_fn)());

static inline bool manifest_is_active(dev_manifest_t* manifest)
{
  return (manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE;
}

#endif // !__ANIVA_DEV_MANIFEST__
