#include "driver.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include <mem/heap.h>
#include <libk/string.h>

aniva_driver_t* create_driver(
  const char* name,
  const char* descriptor,
  driver_version_t version,
  driver_identifier_t identifier,
  ANIVA_DRIVER_INIT init,
  ANIVA_DRIVER_EXIT exit,
  ANIVA_DRIVER_DRV_MSG drv_msg,
  DEV_TYPE_t type
) {
  aniva_driver_t* ret = kmalloc(sizeof(aniva_driver_t));

  strcpy((char*)ret->m_name, name);
  strcpy((char*)ret->m_descriptor, descriptor);

  ret->m_version = version;

  // TODO: check if this identifier isn't taken
  ret->n_ident = identifier;
  ret->f_init = init;
  ret->f_exit = exit;
  ret->f_drv_msg = drv_msg;
  ret->m_type = type;

  return ret;
}

bool validate_driver(aniva_driver_t* driver) {
  if (driver == nullptr || driver->f_init == nullptr || driver->f_exit == nullptr) {
    return false;
  }

  return true;
}

ErrorOrPtr bootstrap_driver(aniva_driver_t* driver) {

  // hihi we're fucking with the scheduler
  pause_scheduler();

  // TODO: fix port
  proc_add_thread(sched_get_kernel_proc(), create_thread_as_socket(sched_get_kernel_proc(), driver->f_init, (FuncPtr)driver->f_exit, (char*)driver->m_name, 0));

  resume_scheduler();

  return Success(0);
}
