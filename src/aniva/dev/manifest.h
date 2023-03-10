#ifndef __ANIVA_DEV_MANIFEST__
#define __ANIVA_DEV_MANIFEST__

#include "dev/driver.h"
#include "libk/linkedlist.h"

struct aniva_driver;

// TODO: add flags
#define DEV_MANIFEST_FLAGS_WANTS_PROCESS 1
#define DEV_MANIFEST_FLAGS_HAS_DEPENDENCIES 2

typedef struct dev_manifest {
  aniva_driver_t* m_handle;
  list_t* m_dependency_manifests;

  dev_url_t m_url;

  size_t m_dep_count;
  size_t m_url_length;

  driver_identifier_t m_check_ident;
  driver_version_t m_check_version;

  DEV_TYPE m_type;

  uint8_t m_flags;
  // timestamp
  // hash
  // vendor
  // XML load info
  // driver handle
  // binary validator
} dev_manifest_t;

dev_manifest_t* create_dev_manifest(aniva_driver_t* handle, uint8_t flags);
void destroy_dev_manifest(dev_manifest_t* manifest);

bool is_manifest_valid(dev_manifest_t* manifest);

#endif // !__ANIVA_DEV_MANIFEST__
