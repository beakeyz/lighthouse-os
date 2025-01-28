#ifndef __ANIVA_PROC_ENVIR_H__
#define __ANIVA_PROC_ENVIR_H__

#include "lightos/types.h"

struct proc;
struct oss_object;

typedef struct proc_envir {
    struct oss_object* woh;
} proc_envir_t;

error_t init_process_envir(struct proc* proc);

/*
 * FIXME: Is this even needed? It seems like the environment gets dealt with when the process exits
 * and the process object is destroyed.
 */
void destroy_process_envir(struct proc* proc);

error_t process_envir_connect_wo(struct proc* proc, struct oss_object* new_wo);
error_t process_envir_disconnect_wo(struct proc* proc, struct oss_object* old_wo);

#endif // !__ANIVA_PROC_ENVIR_H__
