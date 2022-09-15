#include "linkedlist.h"
#include "libc/stddef.h"
#include <arch/x86/dev/debug/serial.h>

list_t* init_list() {
    return nullptr;
}

void add_node(list_t *list, void *data) {
    node_t* node = (node_t*) data;
    node->next = nullptr;
    node->prev = nullptr;

    if (list == nullptr) {
        println("looping");
        list->head = node;
        list->end = node;
        return;
    }

    if (list->head == nullptr) {

        list->head = node;
    }

    list->end->next = node;
    node->prev = list->end;
    list->end = node;
}
