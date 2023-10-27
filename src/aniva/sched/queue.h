#ifndef __ANIVA_SCHED_QUEUE__
#define __ANIVA_SCHED_QUEUE__

/*
 * This file manages the scheduler process queue
 */

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct sched_frame;

typedef struct scheduler_queue {
  struct sched_frame* dequeue;
  struct sched_frame* enqueue;
  uint32_t count;
} scheduler_queue_t;

ANIVA_STATUS init_scheduler_queue(scheduler_queue_t* out);
ANIVA_STATUS scheduler_queue_clear(scheduler_queue_t* queue);

ANIVA_STATUS scheduler_queue_enqueue(scheduler_queue_t* queue, struct sched_frame* frame);
ANIVA_STATUS scheduler_queue_enqueue_front(scheduler_queue_t* queue, struct sched_frame* frame);
struct sched_frame* scheduler_queue_dequeue(scheduler_queue_t* queue);
ANIVA_STATUS scheduler_queue_requeue(scheduler_queue_t* queue, struct sched_frame* frame);
struct sched_frame* scheduler_queue_peek(scheduler_queue_t* queue);

ANIVA_STATUS scheduler_queue_remove(scheduler_queue_t* queue, struct sched_frame* frame);

#endif // !__ANIVA_SCHED_QUEUE__
