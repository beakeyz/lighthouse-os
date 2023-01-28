//
// Created by joost on 1/28/23.
//

#ifndef __LIGHTHOUSE_OS_TESTS__
#define __LIGHTHOUSE_OS_TESTS__
#include <stddef.h>
#include <stdint.h>

#define test_count 1

struct LIGHTOS_SUBTEST;
struct LIGHTOS_TEST;

enum lightos_tests {
  list
};

typedef enum LIGHTOS_TEST_RESULT {
  FAILED = 0,
  SUCCESSFUL,
  SUCCESS_WITH_WARNING
} LIGHTOS_TEST_RESULT_t;

typedef struct LIGHTOS_TEST_DIAGNOSTICS {
  int total_tests;
  int success_count;
  int failure_count;
} LIGHTOS_TEST_DIAGNOSTICS_t;

typedef enum LIGHTOS_TEST_TYPE {
  LIGHTENV = 0,
  ANIVA_LIB,
  ANIVA_DRIVER,
} LIGHTOS_TEST_TYPE_t;

typedef LIGHTOS_TEST_RESULT_t (*LIGHT_TEST_ENTRY) (struct LIGHTOS_TEST*);

typedef int (*LIGHT_TEST_INIT) (struct LIGHTOS_TEST*);

typedef struct LIGHTOS_SUBTEST {
  const char* subTestName;
  LIGHT_TEST_ENTRY subTestEntry;
} LIGHTOS_SUBTEST_t;

typedef struct LIGHTOS_TEST {
  const char* name;
  LIGHTOS_TEST_TYPE_t type;
  LIGHT_TEST_ENTRY testEntry;
  LIGHT_TEST_INIT testInit;
  const LIGHTOS_SUBTEST_t* subTests;
} LIGHTOS_TEST_t;

LIGHTOS_TEST_RESULT_t log_test_result(const char* testName, LIGHTOS_TEST_RESULT_t result, int isSubtest);

extern LIGHTOS_TEST_t tests[test_count];
extern LIGHTOS_TEST_DIAGNOSTICS_t g_diagnostics;

inline void mark_test_successful() {
  g_diagnostics.total_tests++;
  g_diagnostics.success_count++;
}
inline void mark_test_failed() {
  g_diagnostics.total_tests++;
  g_diagnostics.failure_count++;
}

#endif //__LIGHTHOUSE_OS_TESTS__
