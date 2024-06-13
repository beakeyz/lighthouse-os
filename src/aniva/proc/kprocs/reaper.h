#ifndef __ANIVA_REAPER_THREAD__
#define __ANIVA_REAPER_THREAD__

#include "libk/flow/error.h"
#include "proc/proc.h"

/*
 * Attach a reaper thread to a process. This should be a (or the) kernel process,
 * so it should never get reaped
 */
kerror_t init_reaper(proc_t* proc);

/*
 * Enqueue a process into the reaper queue.
 * NOTE: we require the process we want to reap to already be
 * 'terminated' but not destroyed
 */
kerror_t reaper_register_process(proc_t* proc);
int reaper_register_thread(thread_t* thread);

#endif // !__ANIVA_REAPER_THREAD__
