#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/process.h"
#include "mem/kmem.h"
#include "mem/page_dir.h"
#include "mem/tracker/tracker.h"
#include "oss/object.h"
#include "proc/core.h"
#include "proc/handle.h"
#include <libk/string.h>

struct penv;
struct mutex;
struct thread;
struct kresource;
struct proc_image;
struct user_profile;

/*
 * proc.h
 *
 * We define our blueprint for any process in the kernel here. Processes know about
 * a bunch of things like how many threads they have and which, what page directory
 * it uses, what resources its claiming, ect.
 * A bunch of things should be layed out a bit further:
 *
 * 1) resources
 * Every process owns a resource map. This is essensially an array that is indexed
 * based on resource type and contains linked lists to all the resources, which are sorted
 * on address hight (a memory resource with the address 0x1fff gets placed before a memory
 * resource with the address 0xffff). Since resources also need to know if they are shared,
 * every resource also keeps a linked list of owners
 */

/*
 * This struct may get quite big, so let's make sure to put
 * frequently used fields close to eachother in order to minimize
 * cache-misses
 *
 * TODO: proc should prob be protected by atleast a few mutexes or spinlocks
 */
typedef struct proc {
    /* Name of this process. Points to the objects key */
    const char* name;
    /* Key of the execution method. Can only be set once */
    const char* exec_key;
    struct oss_object* obj;

    u32 flags;
    /*
     * How many seconds it has been since boot that this process has been launched
     * This guy only overflows if the system has been running for 4000+ days and there is a new
     * process launch.. I think we should be fine
     */
    u32 proc_launch_time_rel_boot_sec;

    /* This is used to compare a processes status in relation to other processes */
    struct user_profile* profile;
    struct mutex* lock;

    /* Resource tracking */
    page_dir_t m_root_pd;
    khandle_map_t m_handle_map;
    page_tracker_t m_virtual_tracker;

    /* A couple of static pointers to certain threads */
    struct thread* main_thread;

    /* Simple linked list that contains all our threads */
    list_t* threads;

    size_t ticks_elapsed;
} proc_t;

static inline error_t proc_set_exec_method(proc_t* proc, const char* key)
{
    if (proc->exec_key)
        return -EALREADY;

    proc->exec_key = key;
    return 0;
}

static inline size_t proc_get_nr_threads(proc_t* proc)
{
    return proc->threads->m_length;
}

static inline size_t proc_get_ticks_elapsed(proc_t* proc)
{
    return proc->ticks_elapsed;
}

proc_t* create_proc(const char* name, struct user_profile* profile, u32 flags);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);
void destroy_proc(proc_t* proc);

// proc_t* proc_exec(const char* cmd, sysvar_vector_t vars, struct user_profile* profile, u32 flags);

/* Block until the process has ended execution */
int proc_schedule_and_await(proc_t* proc, struct user_profile* profile, const char* cmd, const char* stdio_path, HANDLE_TYPE stdio_type, enum SCHEDULER_PRIORITY prio);
int proc_schedule(proc_t* proc, struct user_profile* profile, const char* cmd, const char* stdio_path, HANDLE_TYPE stdio_type, enum SCHEDULER_PRIORITY prio);
const char* proc_try_get_symname(proc_t* proc, uintptr_t addr);

kerror_t proc_add_thread(proc_t* proc, struct thread* thread);
kerror_t proc_remove_thread(proc_t* proc, struct thread* thread);

kerror_t try_terminate_process(proc_t* proc);
kerror_t try_terminate_process_ex(proc_t* proc, bool defer_yield);

/* Heh? */
void terminate_process(proc_t* proc);
void proc_exit();

static inline bool proc_can_schedule(proc_t* proc)
{
    if (!proc || (proc->flags & PF_FINISHED) == PF_FINISHED || (proc->flags & PF_IDLE) == PF_IDLE)
        return false;

    if (!proc->threads || !proc_get_nr_threads(proc))
        return false;

    /* If none of the conditions above are met, it seems like we can schedule */
    return true;
}

/*
 * This means that the process will be removed from the scheduler queue
 * and will only be emplaced back once an external source has requested
 * that is will be
 */
void stall_process(proc_t* proc);

/*!
 * @brief: Checks if @proc is the process that runs the kernel
 */
static inline bool is_kernel(proc_t* proc)
{
    return (strncmp(proc->name, PROC_CORE_PROCESS_NAME, strlen(PROC_CORE_PROCESS_NAME)) == 0);
}

/*!
 * @brief: Checks if @proc is a process managed by the kernel
 */
static inline bool is_kernel_proc(proc_t* proc)
{
    return ((proc->flags & PF_KERNEL) == PF_KERNEL);
}

static inline bool is_driver_proc(proc_t* proc)
{
    return ((proc->flags & PF_DRIVER) == PF_DRIVER);
}

#endif // !__ANIVA_PROC__
