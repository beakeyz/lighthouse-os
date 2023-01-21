#include "linkedlist.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "mem/kmalloc.h"
#include <dev/debug/serial.h>
#include <libk/string.h>

list_t* init_list() {
  list_t* ret = kmalloc(sizeof(list_t));
  ret->m_length = 0;
  ret->end = nullptr;
  ret->head = nullptr;
  return ret;
}

void destroy_list(list_t* list) {
  for (uintptr_t i = 0; i < list->m_length; i++) {
    list_remove(list, i);
  }
  // yoink list
  kfree(list);
}

void list_append(list_t *list, void *data) {
  node_t* node = kmalloc(sizeof(node_t));
  node->data = data;
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

bool list_remove(list_t* list, uint32_t index) {
  uint32_t current_index = 0;
  FOREACH(i, list) {
    if (current_index == index) {
      node_t* next = i->next;
      node_t* prev = i->prev;

      // pointer magic
      next->prev = prev;
      prev->next = next;

      kfree(i->data);
      kfree(i);
      list->m_length--;
      return true;
    }
    current_index++;
  }
  return false;
}

// hihi linear scan :clown:
void* list_get(list_t* list, uint32_t index) {
  uint32_t current_index = 0;
  FOREACH(i, list) {
    if (current_index == index) {
      return i->data;
    }
    current_index++;
  }
  return nullptr;
}
