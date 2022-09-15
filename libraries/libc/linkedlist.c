#include "linkedlist.h"
#include "libc/stddef.h"
#include <arch/x86/dev/debug/serial.h>

list_t* init_list() {
    return nullptr;
}

void add_node(list_t *list, void *data) {
    node_t* node = (node_t*) data;
    
    if (list->head == NULL) {
        list->head = node;
        list->end = node;
    }

    node->prev = list->end;
    list->end->next = node;

}
