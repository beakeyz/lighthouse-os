#ifndef __ANIVA_ERROR_WRAPPER__
#define __ANIVA_ERROR_WRAPPER__

#include <libk/stddef.h>
#include <logging/log.h>

typedef enum _ANIVA_STATUS {
  ANIVA_FAIL = 0,
  ANIVA_WARNING = 1,
  ANIVA_SUCCESS = 2,
  ANIVA_NOMEM,
  ANIVA_NODEV,
  ANIVA_NOCONN,
  ANIVA_FULL,
  ANIVA_EMPTY,
  ANIVA_BUSY,
  ANIVA_INVAL,
  // TODO: more types?
} ANIVA_STATUS;

#define EOP_ERROR 0
#define EOP_NOMEM 1
#define EOP_INVAL 2
#define EOP_BUSY  3
#define EOP_NODEV 4
#define EOP_NOCON 5
#define EOP_NULL  6

static inline bool status_is_ok(ANIVA_STATUS status)
{
  return (status == ANIVA_SUCCESS);
}

/*
 * FIXME: it seems like adding any more fields here 
 * breaks a lot of stuff. Why? idk. Should figure that out ig
 */
typedef struct _ErrorOrPtr {
  ANIVA_STATUS m_status;
  uintptr_t m_ptr;
} ErrorOrPtr;

NORETURN void __kernel_panic();
NORETURN void kernel_panic(const char* panic_message);

#define ASSERT(condition) ((condition) ? (void)0 : kernel_panic("Assertion failed! (In: " __FILE__ ") TODO: stacktrace!"))
#define ASSERT_MSG(condition, msg) do { if (!(condition)) { print("Assertion failed (In: " __FILE__ "): "); kernel_panic(msg); } } while(0)

// TODO: Add error messages
static ALWAYS_INLINE ErrorOrPtr Error() 
{
  ErrorOrPtr e = {
    .m_ptr = NULL,
    .m_status = ANIVA_FAIL,
  };
  return e;
}

static ALWAYS_INLINE ErrorOrPtr ErrorWithVal(uintptr_t val) 
{
  ErrorOrPtr e = {
    .m_ptr = val,
    .m_status = ANIVA_FAIL,
  };
  return e;
}

static ALWAYS_INLINE bool IsError(ErrorOrPtr e) 
{
  return (e.m_status == ANIVA_FAIL);
}

static ALWAYS_INLINE ErrorOrPtr Warning() 
{
  ErrorOrPtr e = {
    .m_ptr = NULL,
    .m_status = ANIVA_WARNING
  };
  return e;
}

static ALWAYS_INLINE ErrorOrPtr Success(uintptr_t value) 
{
  ErrorOrPtr e = {
    .m_ptr = value,
    .m_status = ANIVA_SUCCESS,
  };
  return e;
}

// for lazy people
static ALWAYS_INLINE uintptr_t Release(ErrorOrPtr eop) 
{
  if (eop.m_status == ANIVA_SUCCESS) {
    return eop.m_ptr;
  }
  return NULL;
}

static ALWAYS_INLINE uintptr_t Must(ErrorOrPtr eop) 
{
  if (eop.m_status != ANIVA_SUCCESS) {
    kernel_panic("ErrorOrPtr: Must(...) failed!");
  }
  return eop.m_ptr;
}

static ALWAYS_INLINE ANIVA_STATUS GetStatus(ErrorOrPtr eop)
{
  return eop.m_status;
}

#define TRY(result, err_or_ptr)                 \
  ErrorOrPtr result##_status = err_or_ptr;      \
  uintptr_t result = result##_status.m_ptr;     \
  (void)result;                                 \
  if (result##_status.m_status == ANIVA_FAIL) { \
    return result##_status;                     \
  }                                         

#endif // !__ANIVA_ERROR_WRAPPER__
