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

void add_node(list_t *list, void *data) {
  node_t* node = kmalloc(sizeof(node_t));
  node->data = data;
  
  if (!list->head) {
    list->head = node;
    list->end = node;
  }

  node->prev = list->head;
  list->head->next = node;
  list->head = node;
}
