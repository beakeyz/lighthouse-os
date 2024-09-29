#include "libk/flow/error.h"
#include "libk/stddef.h"
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
        out->vec_threads[i].max_cycles = 0xff;

    return 0;
}

static inline void __squeue_enqueue(scheduler_queue_t* queue, struct sthread* st)
{
    sthread_list_head_t* lh = &queue->vec_threads[st->base_prio];

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

    ASSERT_MSG(st->previous, "Why is there no list previous ???");

    /* Let the threads back neighbor know its existance */
    st->previous->next = st;

    /* Set the current list heads previous pointer */
    lh->list->previous = st;
}

static inline void __squeue_remove(scheduler_queue_t* queue, struct sthread* t)
{
    sthread_list_head_t* lh;

    lh = &queue->vec_threads[t->base_prio];

    /* Clear the list if this was the last thread */
    if (t->next == t)
        lh->cur_thread = lh->list = nullptr;
    else {
        if (lh->cur_thread == t)
            lh->cur_thread = lh->cur_thread->next;

        if (lh->list == t)
            lh->list = t->next;

        if (queue->next_thread == t)
            queue->next_thread = lh->cur_thread;

        t->previous->next = t->next;
        t->next->previous = t->previous;
    }

    /* Clear the threads own next and previous pointers */
    t->next = nullptr;
    t->previous = nullptr;
}

void squeue_enqueue(scheduler_queue_t* queue, struct sthread* st)
{
    /* Make sure this bastard is in the scheduler */
    ASSERT_MSG(!st->next && !st->previous, st->next->t->m_name);

    __squeue_enqueue(queue, st);

    /* Set the threads queue pointer */
    st->c_queue = queue;

    queue->n_sthread++;
}

void squeue_remove(scheduler_queue_t* queue, struct sthread* t)
{
    /* Make sure this bastard is in the scheduler */
    ASSERT(sthread_is_in_scheduler(t));

    __squeue_remove(queue, t);

    /* Set the threads queue pointer */
    t->c_queue = nullptr;

    /* Decrease the threads count */
    queue->n_sthread--;
}

void squeue_requeue(scheduler_queue_t* old, scheduler_queue_t* new, struct sthread* t)
{
    /* Make sure this bastard is in the scheduler */
    ASSERT(sthread_is_in_scheduler(t));

    __squeue_remove(old, t);
    __squeue_enqueue(new, t);

    old->n_sthread--;
    new->n_sthread++;

    t->c_queue = new;
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
    sthread_list_head_t* lh;

    if (!q->n_sthread)
        return nullptr;

    /* Grab the current active thread list */
    lh = &q->vec_threads[q->active_prio];

    /* Add a cycle to this boi */
    lh->nr_cycles++;

    /* No threads left in this list, find a new active prio */
    if (__squeue_need_switch_next_tl(lh))
        lh = __squeue_handle_empty_tl(q);

    /* Cycle the list */
    if (!lh->cur_thread)
        lh->cur_thread = lh->list;
    else
        lh->cur_thread = lh->cur_thread->next;

    // KLOG_DBG("Next t: %s\n", ret->t->m_name);

    return lh->cur_thread;
}
