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

void squeue_enqueue(scheduler_queue_t* queue, struct sthread* st)
{
    ASSERT_MSG(!st->c_queue, "TRIED TO ENQUEUE while in a q");

    /* Set the threads next pointer */
    st->next = nullptr;
    st->c_queue = queue;
    /* Update the threads scheduler thread link */
    st->t->sthread_slot = queue->vec_threads[st->base_prio].enq;

    /* Put the thread inside the correct list */
    *queue->vec_threads[st->base_prio].enq = st;
    queue->vec_threads[st->base_prio].enq = &st->next;

    /* Increase the threads count */
    queue->n_sthread++;
}

void squeue_remove(scheduler_queue_t* queue, struct sthread** t)
{
    sthread_t* drf_thread = *t;

    ASSERT_MSG(drf_thread->c_queue, "TRIED TO DEQUEUE while not in a queue");

    /* If *t->next is null, we know that the queue enqueue pointer points to it */
    if (!drf_thread->next)
        queue->vec_threads[drf_thread->base_prio].enq = t;
    else
        /* Make sure the link of the next thread doesn't break */
        drf_thread->next->t->sthread_slot = t;

    /* Close the link */
    *t = drf_thread->next;

    /* Update the threads queue status */
    drf_thread->c_queue = nullptr;
    drf_thread->t->sthread_slot = nullptr;

    /* Decrease the threads count */
    queue->n_sthread--;
}
