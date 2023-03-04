#ifndef __ANIVA_DEV_MANIFEST__
#define __ANIVA_DEV_MANIFEST__

#include "dev/driver.h"

struct aniva_driver;

typedef struct dev_manifest {
  aniva_driver_t* m_handle;
  aniva_driver_t** m_dependencies;

  dev_url_t m_url;

  size_t m_dep_count;
  size_t m_url_length;

  driver_identifier_t m_check_ident;
  driver_version_t m_check_version;

  // timestamp
  // hash
  // vendor
  // XML load info
  // driver handle
  // binary validator
} dev_manifest_t;

#endif // !__ANIVA_DEV_MANIFEST__
