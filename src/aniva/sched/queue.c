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

    /* Prepare the maximum cycles per thread vector */
    for (u32 i = 0; i < N_SCHED_PRIO; i++)
        out->vec_threads[i].max_cycles = (i + 1);

    return 0;
}

void squeue_enqueue(scheduler_queue_t* queue, struct sthread* st)
{
    sthread_list_head_t* lh = &queue->vec_threads[st->base_prio];

    /* Increase the threads count preemptively */
    queue->n_sthread++;

    /* Set the threads queue pointer */
    st->c_queue = queue;

    /* If there are no threads in this ring yet, no need to do weird shit */
    if (!lh->list) {

        /* Let the thread point to itself to complete the ring */
        st->next = st;
        st->previous = st;

        lh->list = st;
        return;
    }

    /* Set the threads own pointers */
    st->next = lh->list;
    st->previous = lh->list->previous;

    /* Let the threads back neighbor know its existance */
    st->previous->next = st;

    /* Set the current list heads previous pointer */
    lh->list->previous = st;
}

void squeue_remove(scheduler_queue_t* queue, struct sthread* t)
{
    sthread_list_head_t* lh = &queue->vec_threads[t->base_prio];

    /* Make sure this bastard is in the scheduler */
    ASSERT(sthread_is_in_scheduler(t));

    t->previous->next = t->next;
    t->next->previous = t->previous;

    /* Cycle the list */
    if (t->next != t)
        lh->list = lh->list->next;
    else
        lh->list = nullptr;

    /* Clear the threads own next and previous pointers */
    t->next = nullptr;
    t->previous = nullptr;

    /* Set the threads queue pointer */
    t->c_queue = nullptr;

    /* Decrease the threads count */
    queue->n_sthread--;
}

/*!
 * @brief: Selects the next priority to be scheduled
 *
 * @returns: True if there is a new priority that has threads, false if there are no threads found
 */
static inline bool __squeue_next_prio(squeue_t* q)
{
    for (u32 i = 0; i < N_SCHED_PRIO; i++) {
        /* Go to the next lowest prio level */
        if (q->active_prio)
            q->active_prio--;
        else
            q->active_prio = SCHED_PRIO_MAX;

        /* If this thread list has any threads in them, break */
        if (q->vec_threads[q->active_prio].list)
            return true;
    }

    return false;
}

/*!
 * @brief: Handle an empty thread list
 *
 * Called when we try to select a new thread to schedule, but the list we're scheduling from
 * is empty. This can occur when the last thread from this list ran out of timeslice, got deactivated
 * or even completely removed from the scheduler
 */
static inline sthread_list_head_t* __squeue_handle_empty_tl(squeue_t* q)
{
    /* Reset the number of cycles */
    q->vec_threads[q->active_prio].nr_cycles = 0;

    ASSERT_MSG(__squeue_next_prio(q), "Failed to find a new scheduler priority to run!");

    return &q->vec_threads[q->active_prio];
}

static inline bool __squeue_need_switch_next_tl(sthread_list_head_t* lh)
{
    /* Check wether we need to switch to a new thread ring */
    return (!lh->list || lh->nr_cycles >= lh->max_cycles);
}

/*!
 * @brief: Select the next thread in a scheduler queue
 *
 * It is assumed that @q is the current active queue in the scheduler
 * It is also assumed that there are threads left to run
 */
sthread_t* squeue_get_next_thread(squeue_t* q)
{
    sthread_t* ret;
    sthread_list_head_t* lh;

    lh = &q->vec_threads[q->active_prio];

    /* No threads left in this list, find a new active prio */
    if (__squeue_need_switch_next_tl(lh))
        lh = __squeue_handle_empty_tl(q);

    /* Cycle the list */
    lh->list = lh->list->next;
    lh->nr_cycles++;

    /* Grab our new thread */
    ret = lh->list;

    // KLOG_DBG("Next t: %s\n", ret->t->m_name);

    return ret;
}
