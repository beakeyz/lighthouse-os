#ifndef __ANIVA_PROC_EXEC_H__
#define __ANIVA_PROC_EXEC_H__

/*
 * Aniva exec
 *
 * This framework tries to generefy process execution by implementing different execution methods, based
 * on the kind of object that gets passed to the exec function.
 */

#include <oss/object.h>

struct proc;
struct aniva_exec_method;

/* Every execution method may have their own definition of 'library' */
typedef void process_library_t;

/* 'Executes' a task by creating a new process for it */
error_t aniva_exec(oss_object_t* object, struct proc* target, u32 flags);
error_t aniva_exec_by(struct aniva_exec_method* method, oss_object_t* object, struct proc* target, u32 flags);

/* Function prototype for the execute function */
typedef error_t (*f_AnivaExecMethod_Execute)(oss_object_t* object, struct proc* target);

/*
 * Supplementary functions that the kernel can use to gather information about processes it has running
 */
typedef vaddr_t (*f_AnivaExecMethod_GetFuncAddr)(struct proc* process, const char* symbol);
typedef const char* (*f_AnivaExecMethod_GetFuncSymbol)(struct proc* process, vaddr_t addr);

typedef process_library_t* (*f_AnivaExecMethod_LoadLib)(struct proc* process, oss_object_t* object);
typedef process_library_t* (*f_AnivaExecMethod_GetLib)(struct proc* process, const char* lib);

/*
 * A single aniva execution method
 *
 * These guys always have a static lifetime
 */
typedef struct aniva_exec_method {
    const char* key;
    /* Actually executes the object */
    f_AnivaExecMethod_Execute f_execute;
    f_AnivaExecMethod_GetFuncAddr f_get_func_addr;
    f_AnivaExecMethod_GetFuncSymbol f_get_func_symbol;
    f_AnivaExecMethod_GetLib f_get_lib;
    f_AnivaExecMethod_LoadLib f_load_lib;
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
