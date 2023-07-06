#include "list_test.h"
#include "tests.h"
#include <malloc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define __ANIVA_STDDEF__
#include <src/aniva/libk/data/linkedlist.h>

LIGHTOS_TEST_RESULT_t list_add_test(LIGHTOS_TEST_t* test);
LIGHTOS_TEST_RESULT_t list_remove_test(LIGHTOS_TEST_t* test);

LIGHTOS_SUBTEST_t subtests[subtest_count] = {
  [0] = {
    .subTestName = "list_add",
    .subTestEntry = list_add_test,
  },
  [1] = {
    .subTestName = "list remove",
    .subTestEntry = list_remove_test,
  }
};

LIGHTOS_TEST_RESULT_t test_aniva_lists(LIGHTOS_TEST_t* test) {

  LIGHTOS_TEST_RESULT_t result = SUCCESSFUL;

  for (int i = 0; i < subtest_count; i++) {
    const LIGHTOS_SUBTEST_t* subtest = &subtests[i];

    LIGHTOS_TEST_RESULT_t result = subtest->subTestEntry(test);

    if (result == SUCCESSFUL) {
      mark_test_successful();
    } else {
      mark_test_failed();
      result = FAILED;
    }

    log_test_result(subtest->subTestName, result, true);
  }
 
  log_test_result(test->name, result, false);
  return SUCCESSFUL;
}

int test_aniva_lists_init(LIGHTOS_TEST_t* test) {

  
  return 0;
}

/*
*
*/
LIGHTOS_TEST_RESULT_t list_add_test(LIGHTOS_TEST_t* test) {

  const uintptr_t value = 696969;
  uintptr_t data = value;

  list_t* list = malloc(sizeof(list_t));
  node_t* node = malloc(sizeof(node_t));
  node->data = (void*)&data;
  node->next = NULL;
  node->prev = NULL;
  list->m_length++;
  
  if (list->head == NULL || list->end == NULL) {
    list->head = node;
    list->end = node;
    goto test_check;
  }

  list->end->next = node;
  node->prev = list->end;
  list->end = node;

test_check:

  if (*(uint64_t*)list->head->data == value && list->head->data == &data) {
    free(list);
    free(node);
    return SUCCESSFUL;
  }

  free(list);
  free(node);
  return FAILED;
}

LIGHTOS_TEST_RESULT_t list_remove_test(LIGHTOS_TEST_t* test) {

  /* setup */
  uint64_t test_data_1 = 654;
  uint64_t test_data_2 = 660;
  uint64_t test_data_3 = 655;

  list_t* list = malloc(sizeof(list_t));

  // 3 nodes...
  node_t node1;
  node1.data = &test_data_1;
  list->m_length++;
  node_t node2;
  node2.data = &test_data_2;
  list->m_length++;
  node_t node3;
  node3.data = &test_data_3;
  list->m_length++;

  // connect em
  list->head = &node1;

  node1.next = &node2;
  node2.next = &node3;
  node3.next = NULL;

  node1.prev = NULL;
  node2.prev = &node1;
  node3.prev = &node2;

  list->end = &node3;

  // lets remove the middle one here
  const uint64_t index = 1;

  // our algorithm in list_remove

  uint32_t current_index = 0;
  for (node_t* (i) = (list)->head; (i) != 0; (i) = (i)->next) {
    if (current_index == index) {
      node_t* next = i->next;
      node_t* prev = i->prev;

      if (next == NULL && prev == NULL) {
        // we are the only one ;-;
        list->head = NULL;
        list->end = NULL;
      } else if (next == NULL) {
        // we are at the tail
        list->end = prev;
      } else if (prev == NULL) {
        // we are at the head
        list->head = next;
      }
      // pointer magic
      next->prev = prev;
      prev->next = next;

      list->m_length--;
      goto test_check;
    }
    current_index++;
  }

test_check:

  if (node1.next == &node3 && node3.prev == &node1 && list->m_length != 3) {
    return SUCCESSFUL;
  }

  return FAILED;
}
