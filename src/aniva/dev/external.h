#ifndef __ANIVA_EXTERNAL_DRIVER__
#define __ANIVA_EXTERNAL_DRIVER__

#include "dev/manifest.h"

#define EX_DRV_LOAD_FAILURE (0x00000001)
#define EX_DRV_USER         (0x00000002)
#define EX_DRV_NOPROC       (0x00000004)
#define EX_DRV_OLDMANIFEST  (0x00000008)

typedef struct extern_driver {

  uint32_t m_flags;
  uint32_t m_res0;

  vaddr_t m_load_base;
  size_t m_load_size;

  /* How many kernelsymbols does this driver use */
  size_t m_ksymbol_count;

  /* Symbols exported by this driver */
  hashmap_t* m_exp_symmap;

  /* This drivers manifest */
  drv_manifest_t* m_manifest;
} extern_driver_t;

void init_external_drivers();

extern_driver_t* create_external_driver(uint32_t flags);
void destroy_external_driver(extern_driver_t* driver);

void set_exported_drvsym(extern_driver_t* driver, char* name, vaddr_t addr);
vaddr_t get_exported_drvsym(char* name);

#endif // !__ANIVA_EXTERNAL_DRIVER__
