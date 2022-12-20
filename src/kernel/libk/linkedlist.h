#ifndef __LINKEDLIST__
#define __LINKEDLIST__
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

// hihi
#define ITTERATE(list) node_t* itterator = list->head; \
                       while (itterator) {

#define SKIP_ITTERATION() itterator = itterator->next; \
                          continue

#define ENDITTERATE(list) itterator = itterator->next; }

// TODO: finish linkedlist (double link)
list_t* init_list ();
void list_append(list_t*, void*);
void list_append_before(list_t*, void*, uint32_t);
void list_remove(list_t*, uint32_t);

#endif // !__LINKEDLIST__


