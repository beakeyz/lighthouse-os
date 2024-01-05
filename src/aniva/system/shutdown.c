#include "kevent/event.h"
#include "libk/flow/error.h"
#include "sched/scheduler.h"
#include "system.h"

/*!
 * @brief: Check the validity of a shutdown call
 */
static inline void do_pre_shutdown_checks()
{
  /* TODO: check permission of the current process to shutdown the system */
}

/*!
 * @brief: Initiate a system shutdown
 *
 * Calls shutdown event(s)
 * Tries to do a graceful shutdown via ACPI
 * otherwise just asks the user to press the power button via any means we have left xD (Probably via a kernel_panic)
 */
void aniva_shutdown()
{
  do_pre_shutdown_checks();

  /* 
   * Call shutdown event 
   * There is no special context tied to this event
   */
  kevent_fire("shutdown", NULL, NULL);

  /* TODO: shutdown =) */
}
