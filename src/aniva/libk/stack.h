#ifndef __ANIVA_STACK_HELPER__
#define __ANIVA_STACK_HELPER__

#include <libk/stddef.h>

#define STACK_PUSH(stack, type, value)    \
    do {                                  \
        stack -= sizeof(type);            \
        *((volatile type*)stack) = value; \
    } while (0)

#define STACK_PUSH_ZERO(stack, type, value) \
    stack -= sizeof(type);                  \
    type* value = ((volatile type*)stack)

#define STACK_POP(stack, type, value)     \
    do {                                  \
        value = *((volatile type*)stack); \
        stack += sizeof(type);            \
    } while (0)

typedef struct {
    //
} stacktrace_t;

stacktrace_t* dump_stack(uintptr_t rsp);

#endif // !__ANIVA_STACK_HELPER__
