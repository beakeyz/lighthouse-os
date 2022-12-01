#include "linkedlist.h"
#include "kernel/mem/kmem_manager.h"
#include "libk/stddef.h"
#include "mem/kmalloc.h"
#include <kernel/dev/debug/serial.h>
#include <libk/string.h>

list_t* init_list() {
  // TODO
  return nullptr;
}

void list_append(list_t *list, void *data) {
  node_t* node = kmalloc(sizeof(node_t));
  node->data = data;
  list->m_length++;
  
  if (!list->head) {
    list->head = node;
    list->end = node;
    return;
  }

  list->end->next = node;
  node->prev = list->end;
  list->end = node;
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
