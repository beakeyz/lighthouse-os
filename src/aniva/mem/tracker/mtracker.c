#include "mtracker.h"
#include "mem/zalloc/zalloc.h"
#include "sync/spinlock.h"

error_t init_page_tracker(page_tracker_t* tracker)
{
    if (!tracker)
        return -ENULL;

    memset(tracker, 0, sizeof(*tracker));

    /* Initialize the spinloc */
    init_spinlock(&tracker->lock, NULL);
}
