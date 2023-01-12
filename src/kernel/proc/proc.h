#ifndef __LIGHT_PROC__
#define __LIGHT_PROC__
#include "libk/error.h"
#include "libk/linkedlist.h"

typedef size_t proc_id;

typedef struct {
  char m_name[32];
  proc_id m_id;

  list_t* m_threads;

  size_t m_ticks_used;
} proc_t;

LIGHT_STATUS create_kernel_proc (proc_t* proc);

#endif // !__LIGHT_PROC__
