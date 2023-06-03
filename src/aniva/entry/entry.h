#ifndef __KMAIN__
#define __KMAIN__
#include "system/processor/processor.h"
#include <mem/pg.h>
#include <libk/stddef.h>
#include <libk/multiboot.h>

struct aniva_driver;

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;

extern uintptr_t kstack_top;
extern uintptr_t kstack_bottom;

extern struct aniva_driver* _kernel_pcdrvs_start[];
extern struct aniva_driver* _kernel_pcdrvs_end[];

extern pml_entry_t boot_pml4t[512];
extern pml_entry_t boot_pdpt[512];
extern pml_entry_t boot_pd0[512];
extern pml_entry_t boot_pd0_p[512 * 32];

#define FOREACH_PCDRV(i) for (struct aniva_driver** i = _kernel_pcdrvs_start; i < _kernel_pcdrvs_end; i++)

/*
 * Global system variables, that should be known throughout the 
 * entire kernel
 * TODO: find a better place for all this stuff
 */
typedef struct {
  uintptr_t multiboot_addr;
  size_t total_multiboot_size;

  /* Allow for 128 UTF-8 characters of cmd line */
  char cmdline[128];
  //Processor_t m_bsp_processor;
  //Processor_t* m_current_core;

  /* We copy the multiboot framebuffer tag here, if it's available */
  bool has_framebuffer;

} system_info_t;

extern system_info_t g_system_info;

#endif // !__KMAIN__
