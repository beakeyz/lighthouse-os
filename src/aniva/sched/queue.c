#include "libk/flow/error.h"
#include "scheduler.h"
#include <libk/string.h>

kerror_t init_squeue(scheduler_queue_t* out)
{
    if (!out)
        return -KERR_INVAL;

    /* Clear queue memory */
    memset(out, 0, sizeof(*out));

    /* Start at the highest priority */
    out->active_prio = SCHED_PRIO_7;
    out->n_sthread = 0;

    for (u32 i = 0; i < N_SCHED_PRIO; i++)
        out->vec_threads[i].enq = &out->vec_threads[i].list;

    return 0;
}

kerror_t squeue_clear(scheduler_queue_t* queue);
