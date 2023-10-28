#include "queue.h"
#include <libk/string.h>
#include "libk/flow/error.h"
#include "scheduler.h"

/*!
 * @brief: Initialize a scheduler queue
 *
 * Since we never directly allocate a queue on the heap, we dont need a create function
 */
ANIVA_STATUS init_scheduler_queue(scheduler_queue_t* out)
{
  if (!out)
    return ANIVA_FAIL;

  memset(out, 0, sizeof(*out));
  return ANIVA_SUCCESS;
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

  if (!queue || !frame)
    return ANIVA_FAIL;

  if (queue->enqueue)
    queue->enqueue->previous = frame;

  frame->previous = nullptr;
  queue->enqueue = frame;

  /* First entry to go in, also link the dequeue pointer */
  if (!queue->dequeue)
    queue->dequeue = frame;

  queue->count++;

  return ANIVA_SUCCESS;
}

/*!
 * @brief: Add a frame to the front of the queue
 */
ANIVA_STATUS scheduler_queue_enqueue_front(scheduler_queue_t* queue, struct sched_frame* frame)
{
  if (!queue || !frame)
    return ANIVA_FAIL;

  frame->previous = queue->dequeue;
  queue->dequeue = frame;

  if (!queue->enqueue)
    queue->enqueue = frame;
  
  queue->count++;

  return ANIVA_SUCCESS;
}

/*!
 * @brief: Add a frame right behind a certain @target in the queue
 *
 * After this function, when the target gets dequeued, the @entry will be the new head of the queue
 * This functions also walks the queue to see if the target exists in it
 */
ANIVA_STATUS scheduler_queue_enqueue_behind(scheduler_queue_t* queue, struct sched_frame* target, struct sched_frame* entry)
{
  struct sched_frame* current;

  if (!queue || !target || !entry)
    return ANIVA_FAIL;

  current = queue->dequeue;

  /* Try to find the target */
  while (current && current != target)
    current = current->previous;

  if (!current)
    return ANIVA_FAIL;

  /* Make sure that we update ->enqueue when @entry gets put at the back of the queue */
  if (queue->enqueue == current)
    queue->enqueue = entry;

  entry->previous = current->previous;
  current->previous = entry;

  queue->count++;

  return ANIVA_SUCCESS;
}

/*!
 * @brief: Remove a frame from the head of the queue
 *
 * @returns: The frame we found ad the head
 */
struct sched_frame* scheduler_queue_dequeue(scheduler_queue_t* queue)
{
  struct sched_frame* ret;

  if (!queue || !queue->dequeue)
    return nullptr;

  ret = queue->dequeue;

  queue->dequeue = ret->previous;
  ret->previous = nullptr;

  queue->count--;

  return ret;
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
  ANIVA_STATUS res;

  res = scheduler_queue_remove(queue, frame);

  if (res != ANIVA_SUCCESS)
    return res;

  return scheduler_queue_enqueue(queue, frame);
}

/*!
 * @brief: Return the entry in the queue that is next to be dequeued
 */
struct sched_frame* scheduler_queue_peek(scheduler_queue_t* queue)
{
  return queue->dequeue;
}

/*!
 * @brief: Remove a @frame from the queue
 */
ANIVA_STATUS scheduler_queue_remove(scheduler_queue_t* queue, struct sched_frame* frame)
{
  struct sched_frame* current;
  struct sched_frame* next;

  current = queue->dequeue;
  next = nullptr;

  /* Try to find the target */
  while (current && current != frame) {
    next = current;
    current = current->previous;
  }

  if (!current)
    return ANIVA_FAIL;

  if (next)
    next->previous = current->previous;

  if (queue->dequeue == current)
    queue->dequeue = current->previous;

  if (queue->enqueue == current)
    queue->enqueue = next;

  current->previous = nullptr;

  queue->count--;

  return ANIVA_SUCCESS;
}
