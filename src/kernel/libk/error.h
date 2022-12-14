#ifndef __LIGHT_ERROR_WRAPPER__
#define __LIGHT_ERROR_WRAPPER__
#include "mem/kmalloc.h"
#include <libk/stddef.h>
#include <interupts/interupts.h>

typedef enum {
  LIGHT_FAIL = 0,
  LIGHT_SUCCESS = 1,
  // TODO: more types?
} ErrorType_t;

typedef struct _ErrorOrPtr {
  ErrorType_t m_status;
  uintptr_t m_ptr;
} ErrorOrPtr;

NORETURN void __kernel_panic();
NORETURN void kernel_panic(const char* panic_message);

#define ASSERT(condition) (condition ? (void)0 : kernel_panic("Assertion failed! TODO: stacktrace!"))

#endif // !__LIGHT_ERROR_WRAPPER__
