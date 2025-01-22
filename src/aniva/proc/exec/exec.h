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
proc_t* aniva_exec(oss_object_t* object, u32 flags);
proc_t* aniva_exec_by(struct aniva_exec_method* method, oss_object_t* object, u32 flags);

/* Function prototype for the execute function */
typedef proc_t* (*f_AnivaExecMethod_Execute)(oss_object_t* object);

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
