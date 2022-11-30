#ifndef __LINKEDLIST__
#define __LINKEDLIST__

typedef struct node {
  void* data;
  struct node* prev;
  struct node* next;
} node_t;

typedef struct list {
  node_t* head;
  node_t* end;
} list_t;

// TODO: finish linkedlist (double link)
list_t* init_list ();
void add_node (list_t*, void*);

#endif // !__LINKEDLIST__


