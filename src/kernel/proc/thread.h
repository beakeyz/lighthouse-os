#ifndef __LIGHT_THREAD__
#define __LIGHT_THREAD__
#include "mem/PagingComplex.h"
#include "proc/context.h"
#include "system/processor/gdt.h"
#include <system/processor/registers.h>
#include <libk/stddef.h>

typedef enum {
  INVALID = 0,
  RUNNING,
  RUNNABLE,
  DYING,
  DEAD,
  STOPPED,
  BLOCKED,
} ThreadState;


typedef struct {
  kContext_t m_context;

  uint32_t m_cpu; // atomic?
  uint32_t m_ticks_elapsed;
  uint32_t m_max_ticks;

  uintptr_t m_stack_top;
} thread_t;


#endif // !__LIGHT_THREAD__
