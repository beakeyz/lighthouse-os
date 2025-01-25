#ifndef __LSDK_LIST_H__
#define __LSDK_LIST_H__

#include "lightos/types.h"

typedef struct lsdk_list_entry {
    void* data;
    struct lsdk_list_entry* next;
} list_entry_t;

typedef struct lsdk_list {
    list_entry_t* head;
    list_entry_t** p_enqueue;
} list_t;

#define foreach_list(i, list) \
    for (list_entry_t* i = (list)->head; i != nullptr; i = (i)->next)

list_t* create_list();
void destroy_list(list_t* list);

error_t list_add(list_t* list, void* data);
error_t list_remove(list_t* list, void* data);

#endif // !__LSDK_LIST_H__
