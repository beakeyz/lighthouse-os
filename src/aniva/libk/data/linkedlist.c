#include "linkedlist.h"
#include "libk/flow/error.h"
#include "sys/types.h"
#include <dev/debug/serial.h>
#include <libk/string.h>
#include <mem/heap.h>

list_t* init_list()
{
    list_t* ret;

    ret = kmalloc(sizeof(list_t));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(list_t));

    /* Make sure the enqueue pointer is set to it's own head */
    ret->p_enqueue = &ret->head;

    return ret;
}

void destroy_list(list_t* list)
{
    node_t *next, *current;

    current = list->head;

    while (current) {
        next = current->next;

        kfree(current);

        current = next;
    }

    kfree(list);
}

error_t list_append(list_t* list, void* data)
{
    node_t* node = kmalloc(sizeof(node_t));

    if (!node)
        return -ENOMEM;

    /* Clear the node struct */
    memset(node, 0, sizeof(node_t));

    /* Set the data pointer */
    node->data = data;

    /* Enqueue at the tail */
    *list->p_enqueue = node;
    list->p_enqueue = &node->next;

    list->m_length++;

    return 0;
}

error_t list_prepend(list_t* list, void* data)
{
    node_t* node = kmalloc(sizeof(node_t));

    if (!node)
        return -ENOMEM;

    memset(node, 0, sizeof(node_t));

    node->data = data;
    node->next = list->head;

    list->m_length++;

    return 0;
}

static inline node_t** __list_get_node_slot_by_data(list_t* list, void* data)
{
    node_t** walker;

    walker = &list->head;

    /* Walk to the entry of the index */
    while (*walker) {
        if ((*walker)->data == data)
            break;

        walker = &(*walker)->next;
    }

    return walker;
}

static inline node_t** __list_get_node_slot(list_t* list, u32 index)
{
    node_t** walker;

    /* Index needs to be in range */
    if (index >= list->m_length)
        return nullptr;

    walker = &list->head;

    /* Walk to the entry of the index */
    while (*walker) {
        if (!index)
            break;

        walker = &(*walker)->next;
        index--;
    }

    /* At this point, we should ALWAYS have depleted @index */
    ASSERT(!index);

    return walker;
}

// TODO: fix naming
error_t list_append_before(list_t* list, void* data, uint32_t index)
{
    node_t** slot;
    node_t* new_node;

    /* Index needs to be in range */
    if (index >= list->m_length)
        return -ERANGE;

    slot = __list_get_node_slot(list, index);

    if (!slot)
        return -ENOMEM;

    /* Reached the end. We can simply append */
    if (!(*slot))
        return list_append(list, data);

    /* Create a new node */
    new_node = kmalloc(sizeof(node_t));

    if (!new_node)
        return -ENOMEM;

    /* Clear it's buffer */
    memset(new_node, 0, sizeof(node_t));

    /* Initialize the fields */
    new_node->data = data;
    new_node->next = *slot;

    /* Link this node where walker used to be, such that we place new_node before *walker */
    *slot = new_node;

    /* Add length */
    list->m_length++;

    return 0;
}

static inline bool __list_remove_slot(list_t* list, node_t** slot)
{
    node_t* entry;

    if (!slot || !(*slot))
        return false;

    /* Extract the entry */
    entry = *slot;

    /* Be sure to check wether or not we need to reset the enqueue pointer */
    if (list->p_enqueue == &entry->next)
        list->p_enqueue = slot;

    /* Close the link */
    *slot = entry->next;

    /* Free the entries memory */
    kfree(entry);

    /* Bye */
    list->m_length--;

    return true;
}

bool list_remove(list_t* list, uint32_t index)
{
    node_t** slot;

    if (!list || !list->m_length)
        return false;

    /* Get a slot by this index */
    slot = __list_get_node_slot(list, index);

    /* Remove the entry at this slot */
    return __list_remove_slot(list, slot);
}

bool list_remove_ex(list_t* list, void* item)
{
    node_t** slot;

    if (!list || !list->m_length)
        return false;

    /* Try to get a slot for this fucker */
    slot = __list_get_node_slot_by_data(list, item);

    return __list_remove_slot(list, slot);
}

// hihi linear scan :clown:
void* list_get(list_t* list, uint32_t index)
{
    if (index >= list->m_length)
        return nullptr;

    /* Walk the entire list */
    FOREACH(i, list)
    {
        if (!index)
            return i->data;

        /* Bye */
        index--;
    }
    return nullptr;
}

// NOTE: ErrorOrPtr returns an uint64_t, while linkedlist indexing uses uint32_t here
int list_indexof(list_t* list, uint32_t* p_idx, void* data)
{
    uint32_t idx = 0;
    FOREACH(i, list)
    {
        if (i->data == data) {
            *p_idx = idx;
            return 0;
        }
        idx++;
    }
    return -KERR_NOT_FOUND;
}

static inline void __switch_pointers(void** a, void** b)
{
    if (!a || !b)
        return;

    *a = (void*)((u64)(*a) ^ (u64)(*b));
    *b = (void*)((u64)(*b) ^ (u64)(*a));
    *a = (void*)((u64)(*a) ^ (u64)(*b));
}

static inline void __list_swap(node_t** a, node_t** b)
{
    /* Swap the pointers */
    __switch_pointers((void**)a, (void**)b);

    /* Switch the internal pointers */
    __switch_pointers((void**)&(*a)->next, (void**)&(*b)->next);
}

static inline bool __should_swap(enum LIST_COMP_RESULT result, enum LIST_COMP_MODE mode)
{
    return (result == LIST_COMP_BIGGER && mode == SMALL_TO_BIG) || (result == LIST_COMP_SMALLER && mode == BIG_TO_SMALL);
}

/*!
 * @brief: Sorts a linked list
 *
 * This currently implements bubble sort (yuck)
 * TODO: implement a quicker sorting algorithm (quicksort?)
 */
error_t list_sort(list_t* list, F_LIST_COMPARATOR f_comparator, enum LIST_COMP_MODE mode)
{
    node_t** current;
    bool is_sorted;
    enum LIST_COMP_RESULT result;

    if (!list || !f_comparator)
        return -EINVAL;

    do {
        is_sorted = true;
        current = &list->head;
        
        while (*current) {

            result = f_comparator(*current, (*current)->next);

            /* Check if we should swap these two */
            if (__should_swap(result, mode)) {
                __list_swap(current, &(*current)->next);
                is_sorted = false;
            }

            current = &(*current)->next;
        }

    } while (!is_sorted);

    return 0;
}

static node_t* __list_reverse(list_t* list, node_t* node)
{
    node_t* newhead;

    /* If we've found the last node in the unreversed list, se that as the new head */
    if (!node->next) {
        list->head = node;
        return node;
    }

    /* Perform a recursive walk of the link, where we end at the last entry */
    newhead = __list_reverse(list, node->next);

    /* The new head will now point to us */
    newhead->next = node;

    /* Set this bois next to null preemtively */
    node->next = NULL;

    return node;
}

error_t list_reverse(list_t* list)
{
    /* A list without a head is easily reversable */
    if (!list->head)
        return 0;

    /* Set this fucker first */
    list->p_enqueue = &list->head->next;

    /* Then do recursive reversing */
    __list_reverse(list, list->head);

    return 0;
}
