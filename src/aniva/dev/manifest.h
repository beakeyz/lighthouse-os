#ifndef __ANIVA_DEV_MANIFEST__
#define __ANIVA_DEV_MANIFEST__

#include "dev/driver.h"
#include "libk/data/linkedlist.h"
#include "libk/stddef.h"
#include "sync/mutex.h"
#include "system/resource.h"

#define DMAN_FLAG_WANT_PROC     (0x00000001)
#define DMAN_FLAG_HAD_DEP       (0x00000002)
#define DMAN_FLAG_PSEUDO        (0x00000004)
#define DMAN_FLAG_SYSTEM        (0x00000008)
#define DMAN_FLAG_ORPHAN        (0x00000010)
#define DMAN_FLAG_IS_EXTERNAL   (0x00000020)
#define DMAN_FLAG_HAS_MSG_FUNC  (0x00000040)
#define DMAN_FLAG_DEFERRED_HNDL (0x00000080)

typedef struct driver_ops {
  int (*f_write) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
  int (*f_read) (aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
} driver_ops_t;

typedef struct dev_manifest {
  aniva_driver_t* m_handle;

  list_t* m_dependency_manifests;

  /* Resources that this driver has claimed */
  kresource_bundle_t m_resources;

  driver_ops_t m_ops;

  mutex_t m_lock;

  /* Url of the installed driver */
  dev_url_t m_url;

  /* Path to the binary of the driver, only on external drivers */
  const char* m_driver_file_path;

  size_t m_dep_count;
  size_t m_url_length;

  driver_version_t m_check_version;

  uint32_t m_flags;
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

#endif // !__ANIVA_DEV_MANIFEST__
