#ifndef __LIGHT_STACK_HELPER__
#define __LIGHT_STACK_HELPER__

#define STACK_PUSH(stack, type, value) do {    \
  stack -= sizeof(type);                       \
  *((volatile type *)stack) = value;           \
} while(0)

#define STACK_PUSH_ZERO(stack, type, value)  \
  stack -= sizeof(type);                     \
  type value = *((volatile type *)stack)

#define STACK_POP(stack, type, value) do {    \
  value = *((volatile type *)stack);          \
  stack += sizeof(type);                      \
} while(0)

#endif // !__LIGHT_STACK_HELPER__
