#include "driver.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "proc/proc.h"
#include "proc/socket.h"
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
  SocketOnPacket drv_msg,
  DEV_TYPE type
) {
  aniva_driver_t* ret = kmalloc(sizeof(aniva_driver_t));

  strcpy((char*)ret->m_name, name);
  strcpy((char*)ret->m_descriptor, descriptor);

  ret->m_version = version;

  // TODO: check if this identifier isn't taken
  ret->m_ident = identifier;
  ret->f_init = init;
  ret->f_exit = exit;
  ret->f_drv_msg = drv_msg;
  ret->m_type = type;
  ret->m_port = 0;

  return ret;
}

void destroy_driver(aniva_driver_t* driver) {
  // TODO: when a driver holds on to resources, we need to clean them up too (if they are not being referenced ofcourse)
  kfree(driver);
}

bool validate_driver(aniva_driver_t* driver) {
  if (driver == nullptr || driver->f_init == nullptr || driver->f_exit == nullptr) {
    return false;
  }

  return true;
}

ErrorOrPtr bootstrap_driver(aniva_driver_t* driver) {

  // NOTE: if the drivers port is not valid, the subsystem will verify 
  // it and provide a new port, so we won't have to wory about that here
  thread_t* driver_socket = create_thread_as_socket(sched_get_kernel_proc(), driver->f_init, (FuncPtr)driver->f_exit, driver->f_drv_msg, (char*)driver->m_name, driver->m_port);

  proc_add_thread(sched_get_kernel_proc(), driver_socket);

  return Success(0);
}
