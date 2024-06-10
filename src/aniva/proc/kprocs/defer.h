#ifndef __ANIVA_DEFER__
#define __ANIVA_DEFER__

#include "libk/stddef.h"

typedef struct defered_call {
    FuncPtr m_call;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    struct defered_call* m_next;
} defered_call_t;

void init_defered_calls();

void register_defered_call(FuncPtr call, uint64_t arg0, uint64_t arg1, uint64_t arg2);
void unregister_defered_call(FuncPtr call);

#endif // !__ANIVA_DEFER__
