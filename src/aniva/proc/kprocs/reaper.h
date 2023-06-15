#ifndef __ANIVA_REAPER_THREAD__
#define __ANIVA_REAPER_THREAD__

#include "libk/error.h"
#include "proc/core.h"
#include "proc/proc.h"

/*
 * Attach a reaper thread to a process. This should be a (or the) kernel process,
 * so it should never get reaped
 */
ErrorOrPtr init_reaper(proc_t* proc);

/*
 * Enqueue a process into the reaper queue.
 * NOTE: we require the process we want to reap to already be 
 * 'terminated' but not destroyed
 */
ErrorOrPtr reaper_register_process(proc_t* proc);

#endif // !__ANIVA_REAPER_THREAD__
