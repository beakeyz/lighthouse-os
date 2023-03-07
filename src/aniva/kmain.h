#ifndef __KMAIN__
#define __KMAIN__

#include "sync/mutex.h"
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

typedef struct {
  uintptr_t m_multiboot_addr;
  size_t m_total_multiboot_size;

  Processor_t m_bsp_processor;
  Processor_t* m_current_core;
} GlobalSystemInfo_t;

extern GlobalSystemInfo_t g_GlobalSystemInfo;

extern mutex_t* g_test_mutex;

#endif // !__KMAIN__
