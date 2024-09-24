#include "mem/zalloc/zalloc.h"
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

    /* NOTE: no need to calculate a timeslice, since this gets done anyway when a thread gets added to the scheduler */

    /* Pre-set the sthread slot */
    t->sthread = ret;

    return ret;
}

void destroy_sthread(struct scheduler* s, sthread_t* st)
{
    zfree_fixed(s->sthread_allocator, st);
}
