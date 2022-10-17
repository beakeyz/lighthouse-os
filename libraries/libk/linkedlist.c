#include "linkedlist.h"
#include "arch/x86/mem/kmem_manager.h"
#include "libk/stddef.h"
#include <arch/x86/dev/debug/serial.h>
#include <libk/string.h>

list_t* init_list() {
    return nullptr;
}

void add_node(list_t *list, void *data) {
    void* n =  kmem_alloc(SMALL_PAGE_SIZE);
    node_t* node = (node_t*)&n;
    node->data = data;
    
    // With this println, it seems to print the correct value,
    // otherwise it gives junk (?)
    // FIXME
    if (!list->head) {
        // this one
        list->head = node;
        list->end = node;
    }

    node->prev = list->end;
    list->end->next = node;

}