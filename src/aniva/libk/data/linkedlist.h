#ifndef __ANIVA_LINKEDLIST__
#define __ANIVA_LINKEDLIST__

#include <libk/flow/error.h>
#include <libk/stddef.h>

typedef struct node {
    void* data;
    struct node* prev;
    struct node* next;
} node_t;

typedef struct list {
    node_t* head;
    node_t* end;
    size_t m_length;
} list_t;

#define FOREACH(i, list) for (struct node * (i) = (list)->head; (i) != nullptr; (i) = (i)->next)

// TODO: finish linkedlist (double link)
list_t* init_list();
list_t create_list();
void destroy_list(list_t*);
void list_append(list_t*, void*);
void list_prepend(list_t* list, void* data);
void list_append_before(list_t*, void*, uint32_t);
void list_append_after(list_t*, void*, uint32_t);
bool list_remove(list_t*, uint32_t); // I would like to return this entry, but its kinda hard when you need to kfree a bunch of shit
bool list_remove_ex(list_t*, void*);
void* list_get(list_t*, uint32_t);
int list_indexof(list_t* list, uint32_t* p_idx, void* data);

#endif // !__LINKEDLIST__
