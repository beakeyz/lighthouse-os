#ifndef __ANIVA_EXTERNAL_DRIVER__
#define __ANIVA_EXTERNAL_DRIVER__

#include "dev/driver.h"
#include "dev/manifest.h"
#include "fs/file.h"
#include "proc/proc.h"
#include "system/resource.h"

#define EX_DRV_LOAD_FAILURE (0x00000001)
#define EX_DRV_USER         (0x00000002)
#define EX_DRV_NOPROC       (0x00000004)

typedef struct extern_driver {

  uint32_t m_flags;
  uint32_t m_res0;

  vaddr_t m_load_base;
  size_t m_load_size;

  /* How many kernelsymbols does this driver use */
  size_t m_ksymbol_count;

  /* File this driver was loaded from */
  file_t* m_file;

  /* Process that this driver got */
  proc_t* m_process;

  /* This drivers manifest */
  dev_manifest_t* m_manifest;
} extern_driver_t;

extern_driver_t* create_external_driver(uint32_t flags);
void destroy_external_driver(extern_driver_t* driver);

#endif // !__ANIVA_EXTERNAL_DRIVER__
