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

/*!
 * @brief: Starts a full system reboot
 *
 * This initiates the same events as a shutdown, but instead of doing a powerdown, we try to do a
 * graceful reboot, through ACPI. If that does not work, we can always do a 'non-graceful' shutdown, by
 * letting the system tripplefault or something, which will trigger a reboot on most systems. In this case
 * it will be intentional, so any shutdown jobs will be done
 */
void aniva_reboot()
{
  kernel_panic("TODO: reboot");
}

void aniva_suspend()
{
  kernel_panic("TODO: suspend");
}
