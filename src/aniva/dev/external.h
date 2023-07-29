#ifndef __ANIVA_EXTERNAL_DRIVER__
#define __ANIVA_EXTERNAL_DRIVER__

#include "dev/driver.h"
#include "dev/manifest.h"
#include "fs/file.h"
#include "proc/proc.h"

#define EX_DRV_LOAD_FAILURE (0x00000001)
#define EX_DRV_USER         (0x00000002)

typedef struct extern_driver {

  uint32_t m_sibling_count; /* Amount of external drivers that got loaded from the same file */
  uint32_t m_flags;

  size_t m_missing_symbol_count;

  /* File this driver was loaded from */
  file_t* m_file;

  /* Process that this driver got */
  proc_t* m_process;

  /* This drivers manifest */
  dev_manifest_t* m_manifest;
} extern_driver_t;

#endif // !__ANIVA_EXTERNAL_DRIVER__
