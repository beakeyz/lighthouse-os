#ifndef __ANIVA_KEVENT_CONTEXT__
#define __ANIVA_KEVENT_CONTEXT__

#include "libk/error.h"
#include <libk/stddef.h>

#define E_CONTEXT_FLAG_CANCELED     (0x00000001)
#define E_CONTEXT_FLAG_IMMUTABLE    (0x00000002)
#define E_CONTEXT_FLAG_INVALID      (0x00000004)

/*
 * NOTE: in regular scenarios, kevent_contex should NEVER be
 * heap-allocated, since it is never a persistent object
 */
typedef struct kevent_contex {
  uint32_t m_flags;
  uint32_t m_crc;
  void* m_data;
} kevent_contex_t;

kevent_contex_t create_clean_context();
kevent_contex_t create_context(void* data, uint32_t flags);

/*
 * Check if the context has been altered
 */
ErrorOrPtr verify_context(kevent_contex_t* context, uint32_t original_crc);

#endif // !__ANIVA_KEVENT_CONTEXT__
