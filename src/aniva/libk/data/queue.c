#include "queue.h"
#include <mem/heap.h>

// FIXME: the entire capacity concept of queues is not used at the moment, let's use it
queue_t *create_queue(size_t capacity) {
  queue_t *queue_ptr; 

  queue_ptr = kmalloc(sizeof(queue_t));

  if (!queue_ptr)
    return nullptr;

  queue_ptr->m_entries = 0;
  queue_ptr->m_head_ptr = nullptr;
  queue_ptr->m_tail_ptr = nullptr;
  queue_ptr->m_max_entries = capacity;
  return queue_ptr;
}

queue_t *create_limitless_queue() {
  return create_queue(0);
}

void initialize_queue(queue_t* queue_ptr, size_t capacity) {
  if (queue_ptr == nullptr) {
    return;
  }
  queue_ptr->m_entries = 0;
  queue_ptr->m_head_ptr = nullptr;
  queue_ptr->m_tail_ptr = nullptr;
  queue_ptr->m_max_entries = capacity;
}

ANIVA_STATUS destroy_queue(queue_t* queue, bool eliminate_entries) 
{
  void *entry = queue_dequeue(queue);

  for (; entry != nullptr; entry = queue_dequeue(queue)) {
    if (eliminate_entries)
      kfree(entry);
  }

  kfree(queue);
  return ANIVA_SUCCESS;
}

void queue_enqueue(queue_t *queue, void* data) {
  queue_entry_t *new_entry = kmalloc(sizeof(queue_entry_t));

  if (new_entry == nullptr) {
    return;
  }

  queue->m_entries++;
  new_entry->m_data = data;
  new_entry->m_preceding_entry = nullptr;

  if (queue->m_head_ptr == nullptr || queue->m_tail_ptr == nullptr) {
    queue->m_head_ptr = new_entry;
    queue->m_tail_ptr = new_entry;

  } else {
    queue->m_tail_ptr->m_preceding_entry = new_entry;
  }

  queue->m_tail_ptr = new_entry;

}

void* queue_dequeue(queue_t *queue) {
  if (queue->m_head_ptr == nullptr) {
    return nullptr;
  }

  queue_entry_t *entry_to_remove = queue->m_head_ptr;
  void *ret = entry_to_remove->m_data;

  if (entry_to_remove->m_preceding_entry != nullptr) {
    queue->m_head_ptr = entry_to_remove->m_preceding_entry;
  } else {
    // empty
    queue->m_head_ptr = nullptr;
    queue->m_tail_ptr = nullptr;
  }
  queue->m_entries--;

  kfree(entry_to_remove);
  return ret;
}

void* queue_peek(queue_t* queue) {
  if (!queue || !queue->m_head_ptr) {
    return nullptr;
  }

  return queue->m_head_ptr->m_data;
}

ANIVA_STATUS queue_ensure_capacity(queue_t* queue, size_t capacity) {
  if (queue->m_entries > capacity) {
    // TODO: truncate?
    return ANIVA_FAIL;
  }

  queue->m_max_entries = capacity;
  return ANIVA_SUCCESS;
}
