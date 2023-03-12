#ifndef __ANIVA_ERROR_WRAPPER__
#define __ANIVA_ERROR_WRAPPER__
#include <libk/stddef.h>

typedef enum _ANIVA_STATUS {
  ANIVA_FAIL = 0,
  ANIVA_WARNING = 1,
  ANIVA_SUCCESS = 2,
  // TODO: more types?
} ANIVA_STATUS;

typedef struct _ErrorOrPtr {
  ANIVA_STATUS m_status;
  uintptr_t m_ptr;
} ErrorOrPtr;

NORETURN void __kernel_panic();
NORETURN void kernel_panic(const char* panic_message);

#define ASSERT(condition) ((condition) ? (void)0 : kernel_panic("Assertion failed! TODO: stacktrace!"))
#define ASSERT_MSG(condition, msg) if (!(condition)) { print("Assertion failed: "); kernel_panic(msg); }

ALWAYS_INLINE ErrorOrPtr Error() {
  ErrorOrPtr e = {
    .m_ptr = NULL,
    .m_status = ANIVA_FAIL,
  };
  return e;
}

ALWAYS_INLINE ErrorOrPtr Warning() {
  ErrorOrPtr e = {
    .m_ptr = NULL,
    .m_status = ANIVA_WARNING
  };
  return e;
}

ALWAYS_INLINE ErrorOrPtr Success(uintptr_t value) {
  ErrorOrPtr e = {
    .m_ptr = value,
    .m_status = ANIVA_SUCCESS,
  };
  return e;
}

// for lazy people
ALWAYS_INLINE uintptr_t Release(ErrorOrPtr eop) {
  if (eop.m_status == ANIVA_SUCCESS) {
    return eop.m_ptr;
  }
  return NULL;
}

ALWAYS_INLINE uintptr_t Must(ErrorOrPtr eop) {
  if (eop.m_status == ANIVA_FAIL) {
    kernel_panic("ErrorOrPtr: Must(...) failed!");
  }
  return eop.m_ptr;
}

#define TRY(result_var, err_or_ptr)         \
  ErrorOrPtr result = err_or_ptr;           \
  if (result.m_status == ANIVA_FAIL) {      \
    return result;                          \
  }                                         \
  uintptr_t result_var = result.m_ptr

#endif // !__ANIVA_ERROR_WRAPPER__
