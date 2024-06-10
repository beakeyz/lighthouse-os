#ifndef __ANIVA_KEVENT_PROC_EVENT__
#define __ANIVA_KEVENT_PROC_EVENT__

#include <libk/stddef.h>

struct proc;

enum KEVENT_PROC_EVENTTYPE {
    /* Process creation */
    PROC_EVENTTYPE_CREATE,
    /* Process destruction */
    PROC_EVENTTYPE_DESTROY,
    /* Process rescheduling to another CPU */
    PROC_EVENTTYPE_RESCHEDULE
};

typedef struct kevent_proc_ctx {
    struct proc* process;

    enum KEVENT_PROC_EVENTTYPE type;

    uint32_t old_cpuid;
    uint32_t new_cpuid;
} kevent_proc_ctx_t;

#endif // !__ANIVA_KEVENT_PROC_EVENT__
