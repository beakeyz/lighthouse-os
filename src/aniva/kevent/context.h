#ifndef __ANIVA_KEVENT_CONTEXT__
#define __ANIVA_KEVENT_CONTEXT__

#include "libk/flow/error.h"
#include <libk/stddef.h>

#define E_CONTEXT_FLAG_CANCELED     (0x00000001)
#define E_CONTEXT_FLAG_IMMUTABLE    (0x00000002)
#define E_CONTEXT_FLAG_INVALID      (0x00000004)
#define E_CONTEXT_FLAG_ERROR        (0x00000008)

/*
 * NOTE: in regular scenarios, kevent_contex should NEVER be
 * heap-allocated, since it is never a persistent object
 */
typedef struct kevent_contex {
  uint32_t m_flags;
  uint32_t m_crc;
  void* m_data;
} kevent_contex_t;

#define CTX_OK(ctx) (ctx.m_flags)

int init_ke_context(kevent_contex_t* context);


#endif // !__ANIVA_KEVENT_CONTEXT__
