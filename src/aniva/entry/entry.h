#ifndef __KMAIN__
#define __KMAIN__
#include "dev/driver.h"
#include "proc/proc.h"
#include <mem/pg.h>
#include <libk/stddef.h>
#include <libk/multiboot.h>

struct aniva_driver;
struct processor;

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;

extern uintptr_t kstack_top;
extern uintptr_t kstack_bottom;

extern struct aniva_driver* _kernel_pcdrvs_start[];
extern struct aniva_driver* _kernel_pcdrvs_end[];

extern struct aniva_driver* _kernel_core_drvs_start[];
extern struct aniva_driver* _kernel_core_drvs_end[];

extern pml_entry_t boot_pml4t[512];
extern pml_entry_t boot_pdpt[512];
extern pml_entry_t boot_pd0[512];
extern pml_entry_t boot_pd0_p[512 * 32];

/* TODO: automatic version bumping */
extern driver_version_t kernel_version;

#define FOREACH_CORE_DRV(i) for (struct aniva_driver** i = _kernel_core_drvs_start; i < _kernel_core_drvs_end; i++)

#define FOREACH_PCDRV(i) for (struct aniva_driver** i = _kernel_pcdrvs_start; i < _kernel_pcdrvs_end; i++)

#define SYSFLAGS_HAS_FRAMEBUFFER    (0x00000001)
#define SYSFLAGS_HAS_EARLY_TTY      (0x00000002)
#define SYSFLAGS_NO_INTERRUPTS      (0x00000004)
#define SYSFLAGS_HAS_RMM            (0x00000008) /* Do we have resource management? */

/* What's the maximum amount of processors we support? */
#define SYS_MAX_CPU                 32

/*
 * Global system variables, that should be known throughout the 
 * entire kernel
 * TODO: find a better place for all this stuff
 */
typedef struct {
  /* The physical multiboot address gets set very early during boot */
  paddr_t phys_multiboot_addr;
  /* The virtual multiboot address is computed right after the physical address is set (with HIGH_MAP_BASE) */
  vaddr_t virt_multiboot_addr;
  /*
   * The total multiboot size is set when initializing multiboot early. The memory
   * is reserved after the final kernel pagemap is created 
   */
  size_t total_multiboot_size;

  /*
   * The multiboot tags that we want to cache and use
   */
  struct multiboot_tag_new_acpi* xsdp;
  struct multiboot_tag_old_acpi* rsdp;
  struct multiboot_tag_framebuffer* firmware_fb;
  struct multiboot_tag_module* ramdisk;

  struct processor* processors[SYS_MAX_CPU];

  /* Allow for 128 UTF-8 characters of cmd line */
  char cmdline[128];

  uint32_t sys_flags;
  uint32_t kernel_version;

  vaddr_t kernel_start_addr;
  vaddr_t kernel_end_addr;
  
  /*
   * The amount of memory that is reserved directly above the kernel 
   * Calculated in multiboot.c
   */
  vaddr_t post_kernel_reserved_size; 
} system_info_t;

extern system_info_t g_system_info;

#endif // !__KMAIN__
