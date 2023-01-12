#include "scheduler.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "proc/proc.h"

static list_t* s_thread_list = nullptr;

static void test_kernel_thread_func () {
  for (;;) {}
}

LIGHT_STATUS init_scheduler() {
  s_thread_list = init_list();

  
  proc_t* proc_ptr = nullptr;
  LIGHT_STATUS status = create_kernel_proc(proc_ptr, test_kernel_thread_func);


  return LIGHT_FAIL;
}

LIGHT_STATUS enter_scheduler() {


  return LIGHT_FAIL;
} 

LIGHT_STATUS exit_scheduler() {

  return LIGHT_FAIL;
}

void sched_tick(registers_t*);

thread_t* next_thread();
proc_t* next_proc();

LIGHT_STATUS sched_add_proc(proc_t*, proc_id);
LIGHT_STATUS sched_add_thead(thread_t*);

LIGHT_STATUS sched_remove_proc(proc_t*);
LIGHT_STATUS sched_remove_proc_by_id(proc_id);
LIGHT_STATUS sched_remove_thread(thread_t*);

