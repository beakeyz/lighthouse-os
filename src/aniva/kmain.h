#ifndef __KMAIN__
#define __KMAIN__

#include "system/processor/processor.h"
#include "libk/queue.h"
#include <mem/PagingComplex.h>
#include <libk/stddef.h>
#include <libk/multiboot.h>

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;

extern uintptr_t kstack_top;
extern uintptr_t kstack_bottom;

extern PagingComplex_t boot_pml4t[512];
extern PagingComplex_t boot_pdpt[512];
extern PagingComplex_t boot_pd0[512];
extern PagingComplex_t boot_pd0_p[512 * 32];

typedef struct multiboot_color _color_t;

typedef struct {
  uintptr_t m_multiboot_addr;
  size_t m_total_multiboot_size;

  Processor_t m_bsp_processor;
  Processor_t* m_current_core;
} GlobalSystemInfo_t;

__attribute__((noreturn)) void aniva_task(queue_t* buffer);

extern GlobalSystemInfo_t g_GlobalSystemInfo;

#endif // !__KMAIN__
