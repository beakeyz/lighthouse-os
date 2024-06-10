#ifndef __ANIVA_LIBK_QUEUE__
#define __ANIVA_LIBK_QUEUE__

#include "libk/flow/error.h"

typedef struct queue_entry {
    struct queue_entry* m_preceding_entry;
    void* m_data;
} queue_entry_t;

// NOTE: when m_max_entries is 0, this
// implementation treats the capacity as
// theoretically infinite
typedef struct queue {
    queue_entry_t* m_head_ptr;
    queue_entry_t* m_tail_ptr;
    size_t m_entries;
    size_t m_max_entries;
} queue_t;

queue_t* create_queue(size_t capacity);
queue_t* create_limitless_queue();
void initialize_queue(queue_t* queue, size_t capacity);

ANIVA_STATUS destroy_queue(queue_t* queue, bool eliminate_entries);

/*
 * add an entry to the end of the queue (first-in-first out)
 */
void queue_enqueue(queue_t* queue, void* data);

/*
 * remove the first entry in the queue and return its data
 * thus making the entry behind the popped entry the new head of the queue
 */
void* queue_dequeue(queue_t* queue);

/*
 * get a pointer to the data that is first in the queue, without removing the entry
 */
void* queue_peek(queue_t* queue);

int queue_remove(queue_t* queue, void* item);

/*
 * checks to see that the maximum capacity is not exceeded and set correctly
 */
ANIVA_STATUS queue_ensure_capacity(queue_t* queue, size_t capacity);

#endif //__ANIVA_LIBK_QUEUE__
