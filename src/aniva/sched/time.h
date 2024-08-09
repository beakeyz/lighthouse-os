#ifndef __ANIVA_SCHED_TIMESLICES_H__
#define __ANIVA_SCHED_TIMESLICES_H__

#include <libk/stddef.h>
#include <time/core.h>

/*
 * This header defines scheduler timeslice calculations
 *
 * The scheduler is powered by a ticksource, which may have variable fequencies. We want tasks to enjoy
 * consistant timeslices, regardless of the ticksource powering the scheduler.
 */

/*
 * A timeslice is simply a 32-bit number telling the scheduler how many ms
 * a certain thread has left on the schedule
 */
typedef i32 stimeslice_t;

/* A particular task may enjoy this as minimum number of ms of scheduler time */
#define STIMESLICE_MIN 2
/* and a maximum of however many ticks are stuffed into one second */
#define STIMESLICE_MAX 40000
#define STIMESLICE_STEPPING 40
/* After how much time should a thread get switched away from */
#define STIMESLICE_GRANULARITY (3 * STIMESLICE_STEPPING)

/* Calculate the timeslice for a given sthread */
#define STIMESLICE(st) (STIMESLICE_MIN + (((STIMESLICE_MAX) * ((stimeslice_t)st->actual_prio + 1)) >> SCHEDULER_PRIORITY_MAX_LOG2))

#endif // !__ANIVA_SCHED_TIMESLICES_H__
