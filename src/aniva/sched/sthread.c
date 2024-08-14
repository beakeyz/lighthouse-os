#include "mem/zalloc/zalloc.h"
#include "sched/time.h"
#include "scheduler.h"

sthread_t* create_sthread(struct scheduler* s, thread_t* t, enum SCHEDULER_PRIORITY p)
{
    sthread_t* ret;

    ret = zalloc_fixed(s->sthread_allocator);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->t = t;
    ret->base_prio = p;
    /* Calculate the initial scheduler priority */
    ret->actual_prio = SCHEDULER_CALC_PRIO(ret);
    /* Calculate the initial timeslice */
    ret->tslice = STIMESLICE(ret);

    /* Pre-set the sthread slot */
    t->sthread = ret;
    t->sthread_slot = &t->sthread;

    return ret;
}

void destroy_sthread(struct scheduler* s, sthread_t* st)
{
    zfree_fixed(s->sthread_allocator, st);
}
