#include "libk/flow/error.h"
#include "scheduler.h"
#include <libk/string.h>

kerror_t init_squeue(scheduler_queue_t* out);
kerror_t squeue_clear(scheduler_queue_t* queue);

kerror_t squeue_enqueue(scheduler_queue_t* queue, struct thread* t);
struct sched_frame* squeue_dequeue(scheduler_queue_t* queue);
struct sched_frame* squeue_peek(scheduler_queue_t* queue);
