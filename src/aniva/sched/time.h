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
 * A timeslice is simply a 32-bit number telling the scheduler how many ns
 * a certain thread has left on the schedule
 *
 * the scheduler tick routine should get called every 1ms and timeslices are
 * in nanoseconds.
 */
typedef i64 stimeslice_t;

/* A particular task may enjoy this as minimum number of ns of scheduler time */
#define STIMESLICE_MIN 1
/* Maximum number of nanoseconds a single thread may have */
#define STIMESLICE_MAX 5000
/* Steps of 1ms */
#define STIMESLICE_STEPPING 1000
/* After how much time should a thread get switched away from */
#define STIMESLICE_GRANULARITY (4 * STIMESLICE_STEPPING)

/* Calculate the timeslice for a given sthread */
#define STIMESLICE(st) (STIMESLICE_MIN + (((STIMESLICE_MAX - STIMESLICE_MIN) * ((stimeslice_t)st->actual_prio)) >> SCHEDULER_PRIORITY_MAX_LOG2))

#endif // !__ANIVA_SCHED_TIMESLICES_H__
