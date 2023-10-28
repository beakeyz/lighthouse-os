#include "linkedlist.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include <dev/debug/serial.h>
#include <libk/string.h>

list_t* init_list() {
  list_t* ret = kmalloc(sizeof(list_t));
  list_t list = create_list();

  *ret = list;
  return ret;
}

list_t create_list() {
  list_t ret = {
    .head = nullptr,
    .end = nullptr,
    .m_length = 0
  };
  return ret;
}

void destroy_list(list_t* list) {
  node_t* next, *current;

  current = list->head;

  while (current) {
    next = current->next;

    kfree(current);

    current = next;
  }

  kfree(list);
}

void list_append(list_t *list, void *data) {
  node_t* node = kmalloc(sizeof(node_t));
  node->data = data;
  node->next = nullptr;
  node->prev = nullptr;
  list->m_length++;
  
  if (list->head == nullptr || list->end == nullptr) {
    list->head = node;
    list->end = node;
    return;
  }

  list->end->next = node;
  node->prev = list->end;
  list->end = node;
}

void list_prepend(list_t *list, void *data) {
  node_t* node = kmalloc(sizeof(node_t));
  node->data = data;
  node->next = nullptr;
  node->prev = nullptr;
  list->m_length++;

  if (list->head == nullptr || list->end == nullptr) {
    list->head = node;
    list->end = node;
    return;
  }

  list->head->prev = node;
  node->next = list->head;
  list->head = node;
}

// TODO: fix naming
void list_append_before(list_t *list, void *data, uint32_t index) {
  uint32_t loop_idx = 0;
  node_t* next = list->head;
  while (next) {

    if (loop_idx == index) {
      // found
      node_t* old_next = next->next;

      node_t* new = kmalloc(sizeof(node_t));
      new->data = data;
      new->prev = nullptr;
      new->next = nullptr;

      if (old_next == nullptr) {
        old_next = list->end;

        new->prev = old_next->prev;
        old_next->next = new;
        old_next = new;
        list->m_length ++;
        return;
      }

      old_next->prev = new;
      next->next = new;
      new->next = old_next;
      new->prev = next;
      list->m_length++;
      return;
    }

    loop_idx++;
    next = next->next;
  }
  // hasn't found shit =/
  // FIXME: return error
}

void list_append_after(list_t* list, void* data, uint32_t index) {
  uint32_t loop_idx = 0;

  if (list->m_length <= 1) {
    list_append(list, data);
    return;
  }

  node_t* new = kmalloc(sizeof(node_t));
  new->data = data;
  new->next = nullptr;
  new->prev = nullptr;

  node_t* next = list->head;
  while (next) {

    if (loop_idx == index) {
      // found
      node_t* next_to_new_node = next->next;
      node_t* prev_to_new_node = next;

      new->prev = prev_to_new_node;
      new->next = next_to_new_node;

      if (next_to_new_node)
        next_to_new_node->prev = new;

      prev_to_new_node->next = new;

      list->m_length++;
      return;
    }

    loop_idx++;
    next = next->next;
  }
}

bool list_remove(list_t* list, uint32_t index) {
  uint32_t current_index = 0;
  FOREACH(i, list) {
    if (current_index == index) {
      node_t* next = i->next;
      node_t* prev = i->prev;

      if (next == nullptr && prev == nullptr) {
        // we are the only one ;-;
        list->head = nullptr;
        list->end = nullptr;
      } else if (next == nullptr) {
        // we are at the tail
        list->end = prev;
        prev->next = nullptr;
      } else if (prev == nullptr) {
        // we are at the head
        list->head = next;
        next->prev = nullptr;
      } else {
        // we are somewhere in between, use pointer magic
        next->prev = prev;
        prev->next = next;
      }

      kfree(i);
      list->m_length--;
      return true;
    }
    current_index++;
  }
  return false;
}

bool list_remove_ex(list_t* list, void* item) {
  
  FOREACH(i, list) {
    if (i->data != item)
      continue;

    if (list->head == i)
      list->head = i->next;

    if (list->end == i)
      list->end = i->prev;

    if (i->prev)
      i->prev->next = i->next;

    if (i->next)
      i->next->prev = i->prev;

    kfree(i);
    list->m_length--;
    return true;
  }
  return false;
}

// hihi linear scan :clown:
void* list_get(list_t* list, uint32_t index) {

  if (index >= list->m_length)
    return nullptr;

  uint32_t current_index = 0;
  FOREACH(i, list) {
    if (current_index == index) {
      return i->data;
    }
    current_index++;
  }
  return nullptr;
}

// NOTE: ErrorOrPtr returns an uint64_t, while linkedlist indexing uses uint32_t here
ErrorOrPtr list_indexof(list_t* list, void* data) {
  uint32_t idx = 0;
  FOREACH(i, list) {
    if (i->data == data) {
      return Success(idx);
    }
    idx++;
  }
  return Error();
}
