#ifndef __ANIVA_LINKEDLIST__
#define __ANIVA_LINKEDLIST__

#include <libk/flow/error.h>
#include <libk/stddef.h>

typedef struct node {
    void* data;
    struct node* next;
} node_t;

typedef struct list {
    node_t* head;
    node_t** p_enqueue;
    size_t m_length;
} list_t;

#define FOREACH(i, list) for (struct node * (i) = (list)->head; (i) != nullptr; (i) = (i)->next)
#define FOREACH_P(i, list) for (struct node** (i) = &(list)->head; (*i) != nullptr; (i) = &(*i)->next)

// TODO: finish linkedlist (double link)
list_t* init_list();
void destroy_list(list_t*);
error_t list_append(list_t*, void*);
error_t list_prepend(list_t* list, void* data);
error_t list_append_before(list_t*, void*, uint32_t);
bool list_remove(list_t*, uint32_t); // I would like to return this entry, but its kinda hard when you need to kfree a bunch of shit
bool list_remove_ex(list_t*, void*);
void* list_get(list_t*, uint32_t);
int list_indexof(list_t* list, uint32_t* p_idx, void* data);

enum LIST_COMP_MODE {
    BIG_TO_SMALL,
    SMALL_TO_BIG,
};

enum LIST_COMP_RESULT {
    LIST_COMP_BIGGER = 1,
    LIST_COMP_EQUAL = 0,
    LIST_COMP_SMALLER = -1,
    LIST_COMP_INVALID = -2,
};

/*
 * Definition for a list comparator function
 * Given two nodes, this function can tell list_sort, whether to 
 * see one node as bigger or smaller than the other
 */
typedef enum LIST_COMP_RESULT (*F_LIST_COMPARATOR) (node_t* a, node_t* b);

error_t list_sort(list_t* list, F_LIST_COMPARATOR f_comparator, enum LIST_COMP_MODE mode);
error_t list_reverse(list_t* list);

#endif // !__LINKEDLIST__
