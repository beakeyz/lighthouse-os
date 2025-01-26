#ifndef __ANIVA_PROC_EXEC_H__
#define __ANIVA_PROC_EXEC_H__

/*
 * Aniva exec
 *
 * This framework tries to generefy process execution by implementing different execution methods, based
 * on the kind of object that gets passed to the exec function.
 */

#include <oss/object.h>
#include <proc/proc.h>

struct aniva_exec_method;

/* 'Executes' a task by creating a new process for it */
error_t aniva_exec(oss_object_t* object, proc_t* target, u32 flags);
error_t aniva_exec_by(struct aniva_exec_method* method, oss_object_t* object, proc_t* target, u32 flags);

/* Function prototype for the execute function */
typedef error_t (*f_AnivaExecMethod_Execute)(oss_object_t* object, proc_t* target);

/*
 * Supplementary functions that the kernel can use to gather information about processes it has running
 */
typedef vaddr_t (*f_AnivaExecMethod_GetFuncAddr)(proc_t* process, const char* symbol);
typedef const char* (*f_AnivaExecMethod_GetFuncSymbol)(proc_t* process, vaddr_t addr);

/*
 * A single aniva execution method
 *
 * These guys always have a static lifetime
 */
typedef struct aniva_exec_method {
    const char* key;
    /* Actually executes the object */
    f_AnivaExecMethod_Execute f_execute;
    /* Next method in the link */
    struct aniva_exec_method* next;
} aniva_exec_method_t;

int register_exec_method(aniva_exec_method_t* method);
int unregister_exec_method(const char* key);

aniva_exec_method_t** aniva_exec_method_get_slot(const char* key);
aniva_exec_method_t* aniva_exec_method_get(const char* key);

/* Initialize all the basic execution models of aniva */
void init_aniva_execution();

#endif // !__ANIVA_PROC_EXEC_H__
