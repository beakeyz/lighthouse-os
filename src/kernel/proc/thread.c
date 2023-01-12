#include "thread.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include "system/processor/fpu/state.h"
#include <libk/string.h>

// TODO: move somewhere central
#define DEFAULT_STACK_SIZE 16 * Kib

LIGHT_STATUS create_thread(thread_t* thread, FuncPtr entry, const char* name, bool kthread) { // make this sucka

  thread_t* idle = kmalloc(sizeof(thread_t));
  memcpy(idle->m_name, name, strlen(name));
  idle->m_cpu = g_GlobalSystemInfo.m_current_core->m_cpu_num;
  idle->m_parent_proc = nullptr;
  idle->m_ticks_elapsed = 0;
  idle->m_current_state = RUNNABLE;

  idle->m_context.rip = (uintptr_t)entry;
  idle->m_context.rdi = NULL;

  uintptr_t stack = Must(kmem_kernel_alloc_range(DEFAULT_STACK_SIZE, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL));

  idle->m_stack_top = stack;

  store_fpu_state(&idle->m_fpu_state);

  return LIGHT_FAIL;
} // make this sucka

LIGHT_STATUS kill_thread(thread_t* thread) {
  return LIGHT_FAIL;
} // kill thread and prepare for context swap to kernel
LIGHT_STATUS kill_thread_if(thread_t* thread, bool condition) {
  return LIGHT_FAIL;
} // kill if condition is met
LIGHT_STATUS clean_thread(thread_t* thread) {
  return LIGHT_FAIL;
} // clean resources thead used

LIGHT_STATUS switch_context_to(thread_t* thread) {
  return LIGHT_FAIL;
}

void reset_thread_fpu_state(thread_t* thread) {
}
