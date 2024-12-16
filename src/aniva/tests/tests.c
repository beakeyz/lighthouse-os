#include "tests.h"
#include "libk/kopts/parser.h"
#include "logging/log.h"
#include <libk/string.h>

extern aniva_test_t* _aniva_tests_start[];
extern aniva_test_t* _aniva_tests_end[];

static u32 __aniva_test_nr;
static aniva_test_t** __aniva_test_vec;

/*!
 * @brief: initialize the aniva tests environment
 */
void init_aniva_tests()
{
    /* If we don't need to do tests, we don't need to init shit */
    if (!aniva_should_do_tests())
        return;

    if (!kopts_get_bool("do_aniva_tests"))
        return;

    /* We don't have any tests in this case for some reason lolz */
    if (&_aniva_tests_end[0] == &_aniva_tests_start[0])
        return;

    /* Calculate the number of tests we have */
    __aniva_test_nr = ((u64)&_aniva_tests_end[0] - (u64)&_aniva_tests_start[0]) / sizeof(aniva_test_t*);
    __aniva_test_vec = _aniva_tests_start;
}

/*!
 * @brief: Execute the aniva tests
 *
 * This currently always does all the tests and records some stats about this
 * run in the buffer @results
 *
 * TODO: Implement a way to:
 *  1) Choose which tests we want to do
 *  2) Exempt tests
 *  3) Check if any tests can be skipped (because we did
 *     them before, they didn't change and thay passed that time)
 */
error_t do_aniva_tests(aniva_tests_result_t* results)
{
    aniva_test_t* c_test;

    /* Just pretend like all tests succeeded if we don't need to do tests */
    if (!aniva_should_do_tests())
        return 0;

    if (!kopts_get_bool("do_aniva_tests"))
        return 0;

    if (!results)
        return -EINVAL;

    /* Clear the results buffer */
    memset(results, 0, sizeof(*results));

    /* Set the number of tests we're going to try to do */
    results->nr_tests = __aniva_test_nr;

    KLOG_INFO("Trying to perform kernel tests...\n");

    /* Do the tests */
    for (u32 i = 0; i < __aniva_test_nr; i++) {
        c_test = __aniva_test_vec[i];

        KLOG_INFO("[%d/%d] %s ... ", (i + 1), __aniva_test_nr, c_test->title ? c_test->title : "Unknown");

        /* If there was no test function specified,  */
        if (!c_test->tester || !c_test->test_func)
            continue;

        /* Clear the error description buffer, just in case */
        // memset(c_test->err_desc, 0, sizeof(c_test->err_desc));

        /* Store the result back into the aniva_test struct */
        c_test->result = aniva_test_do(c_test);

        /* Record the amount of tests that failed and passed */
        if (c_test->result == 0) {
            results->nr_pass++;
            KLOG("Passed\n");
        } else {
            results->nr_fail++;
            KLOG("Failed\n");
        }

        /* Count the amount of tests we actually did */
        results->nr_tests_done++;
    }

    /* Some tests failed */
    if (results->nr_fail)
        return -ETESTFAIL;

    /* Some tests were invalid ??? */
    if (results->nr_tests_done != results->nr_tests)
        return -ERANGE;

    return 0;
}
