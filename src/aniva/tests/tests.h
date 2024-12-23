#ifndef __ANIVA_TESTS_H__
#define __ANIVA_TESTS_H__

#include "lightos/types.h"
#include <libk/stddef.h>

struct aniva_test;

typedef error_t (*f_ANIVA_TESTER_FUNC)(struct aniva_test* test);

enum ANIVA_TEST_TYPE {
    ANIVA_TEST_TYPE_ALGO,
    ANIVA_TEST_TYPE_MATH,
    ANIVA_TEST_TYPE_DEV,
    ANIVA_TEST_TYPE_DRV,
    ANIVA_TEST_TYPE_IO,
    ANIVA_TEST_TYPE_MEM,
};

/*
 * Structure that represents a single aniva test
 */
typedef struct aniva_test {
    /* The function we need to test */
    FuncPtr test_func;
    /* The function used to test this function */
    f_ANIVA_TESTER_FUNC tester;
    /* The type of test */
    enum ANIVA_TEST_TYPE type;
    /* The test result */
    error_t result;
} aniva_test_t;

static inline error_t aniva_test_do(aniva_test_t* test)
{
    return test->tester(test);
}

typedef struct aniva_tests_result {
    /* Number of passed tests */
    u32 nr_pass;
    /* Number of failed tests */
    u32 nr_fail;
    /* Total number of tests */
    u32 nr_tests;
    /*
     * Number of tests actually done (I know this is just the sum of nr_pass and nr_fail, but
     * we get this dword for free anyway so stop crying)
     */
    u32 nr_tests_done;
} aniva_tests_result_t;

void init_aniva_tests();
error_t do_aniva_tests(aniva_tests_result_t* results);

#ifdef ANIVA_COMPILE_TESTS
static inline bool aniva_should_do_tests()
{
    return (ANIVA_COMPILE_TESTS == 1);
}

#define ANIVA_REGISTER_TEST(__title, __test_func, __tester, __type) \
    static const USED aniva_test_t __anvtst_##__test_func = {       \
        .test_func = (FuncPtr)(__test_func),                        \
        .tester = (__tester),                                       \
        .type = (__type),                                           \
        .result = 0,                                                \
    };                                                              \
    static const USED SECTION(".aniva_tests") aniva_test_t* __p_anvtst_##__test_func = &__anvtst_##__test_func
#else
static inline bool aniva_should_do_tests()
{
    return false;
}
#define ANIVA_REGISTER_TEST(__test_func, __tester, __type)
#endif // !ANIVA_COMPILE_TESTS

#endif // !__ANIVA_TESTS_H__
