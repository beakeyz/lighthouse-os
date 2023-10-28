#include "queue.h"
#include "libk/flow/error.h"

/*!
 * @brief: Initialize a scheduler queue
 *
 * Since we never directly allocate a queue on the heap, we dont need a create function
 */
ANIVA_STATUS init_scheduler_queue(scheduler_queue_t* out)
{
  kernel_panic("TODO: init_scheduler_queue");
}

/*!
 * @brief: Flush all the frames from a scheduler queue
 *
 * Called on queue teardown, when we want to get rid of any remaining scheduler frames in 
 * the queue
 */
ANIVA_STATUS scheduler_queue_clear(scheduler_queue_t* queue)
{
  kernel_panic("TODO: scheduler_queue_clear");
}

/*!
 * @brief: Add a frame to the queue
 *
 * Links a new frame at the back of this queue, or depending on the frames prio,
 * somewhere else
 */
ANIVA_STATUS scheduler_queue_enqueue(scheduler_queue_t* queue, struct sched_frame* frame)
{
  kernel_panic("TODO: scheduler_queue_enqueue");
}

/*!
 * @brief: Add a frame to the front of the queue
 */
ANIVA_STATUS scheduler_queue_enqueue_front(scheduler_queue_t* queue, struct sched_frame* frame)
{
  kernel_panic("TODO: scheduler_queue_enqueue_front");
}

/*!
 * @brief: Add a frame right behind a certain @target in the queue
 *
 * After this function, when the target gets dequeued, the @entry will be the new head of the queue
 */
ANIVA_STATUS scheduler_queue_enqueue_behind(scheduler_queue_t* queue, struct sched_frame* target, struct sched_frame* entry)
{
  kernel_panic("TODO: scheduler_queue_enqueue_behind");
}

/*!
 * @brief: Remove a frame from the head of the queue
 *
 * @returns: The frame we found ad the head
 */
struct sched_frame* scheduler_queue_dequeue(scheduler_queue_t* queue)
{
  kernel_panic("TODO: scheduler_queue_dequeue");
}

/*!
 * @brief: Removes a certain @frame from the queue and enqueues it again
 * 
 * @returns: a few aniva error codes based on the error type
 * 1) ANIVA_SUCCESS: on successful requeue
 * 2) ANIVA_FAIL: if there whas an error during the requeue
 */
ANIVA_STATUS scheduler_queue_requeue(scheduler_queue_t* queue, struct sched_frame* frame)
{
  kernel_panic("TODO: scheduler_queue_requeue");
}

/*!
 * @brief: Return the entry in the queue that is next to be dequeued
 */
struct sched_frame* scheduler_queue_peek(scheduler_queue_t* queue)
{
  kernel_panic("TODO: scheduler_queue_peek");
}

/*!
 * @brief: Remove a @frame from the queue
 */
ANIVA_STATUS scheduler_queue_remove(scheduler_queue_t* queue, struct sched_frame* frame)
{
  kernel_panic("TODO: scheduler_queue_remove");
}
