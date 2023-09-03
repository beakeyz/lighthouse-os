#ifndef __ANIVA_DEV_MANIFEST__
#define __ANIVA_DEV_MANIFEST__

#include "dev/driver.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "sync/mutex.h"
#include "system/resource.h"

/*
 * We let drivers implement a few functions that are mostly meant to
 * simulate the file opperations like read, write, seek, ect. that some 
 * linux / unix drivers would implement.
 */
typedef struct driver_ops {
  int (*f_write) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
  int (*f_read) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
} driver_ops_t;

typedef struct dev_manifest {
  aniva_driver_t* m_handle;
  list_t* m_dependency_manifests;
  size_t m_dep_count;

  uint32_t m_flags;
  driver_version_t m_check_version;

  /* Resources that this driver has claimed */
  kresource_bundle_t m_resources;
  mutex_t m_lock;

  /* Url of the installed driver */
  dev_url_t m_url;
  size_t m_url_length;

  /* Path to the binary of the driver, only on external drivers */
  const char* m_driver_file_path;

  /* Any data that's specific to the kind of driver this is */
  void* m_private;

  driver_ops_t m_ops;
  // timestamp
  // hash
  // vendor
  // XML load info
  // driver handle
  // binary validator
} dev_manifest_t;

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle);
void destroy_dev_manifest(dev_manifest_t* manifest);

void manifest_gather_dependencies(dev_manifest_t* manifest);
bool ensure_dependencies(dev_manifest_t* manifest);

bool is_manifest_valid(dev_manifest_t* manifest);

ErrorOrPtr manifest_emplace_handle(dev_manifest_t* manifest, aniva_driver_t* handle);

bool driver_manifest_write(struct aniva_driver* manifest, int(*write_fn)());
bool driver_manifest_read(struct aniva_driver* manifest, int(*read_fn)());

bool install_private_data(struct aniva_driver* driver, void* data);

#endif // !__ANIVA_DEV_MANIFEST__
