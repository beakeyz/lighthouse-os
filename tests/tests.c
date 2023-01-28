//
// Created by joost on 1/28/23.
//

#include "tests.h"
#include "aniva/libk/list_test.h"
#include <stdio.h>

const char* get_result_str(LIGHTOS_TEST_RESULT_t result);

LIGHTOS_TEST_DIAGNOSTICS_t g_diagnostics = {
  .failure_count = 0,
  .success_count = 0
};


LIGHTOS_TEST_t tests[test_count] = {
  [list] = {
    .name = "Aniva libk list_t",
    .type = ANIVA_LIB,
    .testEntry = test_aniva_lists,
    .testInit = test_aniva_lists_init
  }
};

// FIXME: find a way to use the internal libk funtions in this environment...
// when we inevatiabily change something in the library, we will have to change the 
// test internals as well, which is cumbersome
int main() {

  printf("\nWelcome to the LIGHTOS test suite!\n");

  for (int i = 0; i < test_count; i++) {

    LIGHTOS_TEST_t* test = &tests[i];

    test->testInit(test);

    test->testEntry(test);

  }

  int total_tests = (g_diagnostics.total_tests == 0) ? 1 : g_diagnostics.total_tests;
  float percentage_success = ((float)g_diagnostics.success_count / (float)total_tests) * 100;

  printf("Tests done! Here are the results: \n");
  printf(" Successes:              %d\n", g_diagnostics.success_count);
  printf(" Failures:               %d\n", g_diagnostics.total_tests);
  printf(" Percentage successful   %f%%\n", percentage_success);

  
  return 0;
}

LIGHTOS_TEST_RESULT_t log_test_result(const char* testName, LIGHTOS_TEST_RESULT_t result, int isSubtest) {
  
  if (isSubtest == 0) {
    printf("[*] %s: %s\n", testName, get_result_str(result));
  } else {
    printf("  *] %s: %s\n", testName, get_result_str(result));
  }
  
  return result;
}

const char* get_result_str(LIGHTOS_TEST_RESULT_t result) {
  switch (result) {
    case FAILED:
      return "\033[32;1mFailed\033[0m";
    case SUCCESSFUL:
      return "\033[32;2mSuccess\033[0m";
    case SUCCESS_WITH_WARNING:
      return "Success with warning (TODO: display)";
  }
  return "Unknown";
}
