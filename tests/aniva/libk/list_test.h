//
// Created by joost on 1/28/23.
//

#ifndef __LIGHTHOUSE_OS_LIST_TEST__
#define __LIGHTHOUSE_OS_LIST_TEST__
#include <tests.h>

#define subtest_count 2

extern LIGHTOS_SUBTEST_t subtests[subtest_count];

LIGHTOS_TEST_RESULT_t test_aniva_lists(LIGHTOS_TEST_t* test);

int test_aniva_lists_init(LIGHTOS_TEST_t* test);

#endif //__LIGHTHOUSE_OS_LIST_TEST__
