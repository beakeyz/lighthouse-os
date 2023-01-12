#include "thread.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include <libk/string.h>

// TODO: move somewhere central
// FIXME: we now use 4 Kib as stack size, since I dont yet have a method to map page ranges where they fit in kmem_manager, so thats a TODO
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

  uintptr_t stack = (uintptr_t)kmem_kernel_alloc(kmem_get_page_base(kmem_request_pysical_page().m_ptr), SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_GET_MAKE);

  idle->m_stack_top = stack;

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
