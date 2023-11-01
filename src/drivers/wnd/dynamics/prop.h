#ifndef __ANIVA_LWND_DYNAMIC_PROP__
#define __ANIVA_LWND_DYNAMIC_PROP__

#include <libk/stddef.h>

#define LWND_PROP_TYPE_SPRITE 0x0u
#define LWND_PROP_TYPE_WIDGET 0x1u
/* Always-on-top island */
#define LWND_PROP_TYPE_AOT_ISLAND 0x2u

struct lwnd_screen;

/*
 * Dumb prop
 *
 * Acts as a generic framework on top of which we can build things like 
 * widgets or cursors
 */
typedef struct lwnd_dynamic_prop {

  uint8_t type;

  /* Heap-allocated */
  void* private;

  struct lwnd_screen* parent;

  /* Link through to the next prop */
  struct lwnd_dynamic_prop* next;
} lwnd_dynamic_prop_t;

#endif // !__ANIVA_LWND_DYNAMIC_PROP__
