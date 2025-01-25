#include "list.h"
#include <stdlib.h>

list_t* create_list()
{
    list_t* ret;

    ret = malloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    ret->head = nullptr;
    ret->p_enqueue = &ret->head;

    return ret;
}

void destroy_list(list_t* list)
{
    list_entry_t *cur, *next;

    cur = list->head;

    /* Free all list entries */
    while (cur) {
        next = cur->next;

        free(cur);

        cur = next;
    }

    free(list);
}

static list_entry_t* __create_list_entry(void* data)
{
    list_entry_t* entry;

    entry = malloc(sizeof(*entry));

    if (!entry)
        return nullptr;

    entry->data = data;
    entry->next = nullptr;

    return entry;
}

static void __destroy_list_entry(list_entry_t* entry)
{
    free(entry);
}

error_t list_add(list_t* list, void* data)
{
    list_entry_t* entry;

    entry = __create_list_entry(data);

    if (!entry)
        return -ENOMEM;

    /* Enqueue this guy at the enqueue pointer */
    *list->p_enqueue = entry;
    list->p_enqueue = &entry->next;

    return 0;
}

error_t list_remove(list_t* list, void* data)
{
    list_entry_t** p_entry;
    list_entry_t* to_remove;

    /* Try to find our guy */
    for (p_entry = &list->head; *p_entry; p_entry = &(*p_entry)->next) {
        if ((*p_entry)->data == data)
            break;
    }

    /* Store the entry we need to remove */
    to_remove = *p_entry;
    /* Solve the link */
    *p_entry = (*p_entry)->next;

    /* This is the end of the list, we need to set the enqueue pointer here */
    if (!(*p_entry))
        list->p_enqueue = p_entry;

    __destroy_list_entry(to_remove);

    return 0;
}
