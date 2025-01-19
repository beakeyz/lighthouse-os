#include "core.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "oss/connection.h"
#include "oss/core.h"
#include "oss/object.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/profile/runtime.h"
#include "system/sysvar/var.h"
#include <libk/data/hashmap.h>

// TODO: fix this mechanism, it sucks
static zone_allocator_t* __thread_allocator;
static oss_object_t* __proc_root_obj;

struct thread* spawn_thread(char* name, enum SCHEDULER_PRIORITY prio, FuncPtr entry, uint64_t arg0)
{
    proc_t* current;
    thread_t* thread;

    /* Don't be dumb lol */
    if (!entry)
        return nullptr;

    current = get_current_proc();

    /* No currently scheduling proc means we are fucked lol */
    if (!current)
        return nullptr;

    thread = create_thread_for_proc(current, entry, arg0, name);

    if (!thread)
        return nullptr;

    if ((proc_add_thread(current, thread))) {
        /* Sadge */
        destroy_thread(thread);
        return nullptr;
    }

    scheduler_add_thread(thread, prio);
    return thread;
}

/*!
 * @brief: Find a process through oss
 *
 * Processes live in oss under the Proc root object. They are simply attached to this node
 * by name and any sysvars will be directly connected to them
 */
proc_t* find_proc(const char* path)
{
    proc_t* ret;
    oss_object_t* obj;

    /* Try to find this object in oss */
    if (oss_open_object(path, &obj))
        return nullptr;

    ret = oss_object_unwrap(obj, OT_PROCESS);

    if (!ret) {
        oss_object_close(obj);
        return nullptr;
    }

    /*
     * This object is a process object! Return the private field which
     * (hopefully still) contains our process object
     */
    return ret;
}

uint32_t get_proc_count()
{
    return runtime_get_proccount();
}

/*!
 * @brief: Loop over all the current processes and calls @f_callback
 *
 * If @f_callback returns true, the loop is broken and the process that did it will be put in @presult
 * TODO: Implement this lmao
 *
 * All processes live in their own environment inside a profile in %/Runtime. We'll first have to itterate all
 * profiles and then all objects inside these profiles.
 *
 * @returns: Wether we were able to walk the entire vector
 */
bool foreach_proc(bool (*f_callback)(struct proc*), struct proc** presult)
{
    bool result;
    oss_connection_t* conn;

    mutex_lock(__proc_root_obj->lock);

    /* Loop over all this guys connections */
    FOREACH(i, __proc_root_obj->connections)
    {
        conn = i->data;

        /* Skip any upstream connections (How thefuck would that happen but sure) */
        if (oss_connection_is_upstream(conn, __proc_root_obj))
            continue;

        if (conn->child->type != OT_PROCESS)
            continue;

        /* Call the callback here */
        result = f_callback(conn->child->private);

        /* Just dip if the callback returns false */
        if (!result)
            break;
    }

    mutex_unlock(__proc_root_obj->lock);

    return result;
}

thread_t* find_thread(proc_t* proc, thread_id_t tid)
{

    thread_t* ret;

    if (!proc)
        return nullptr;

    FOREACH(i, proc->m_threads)
    {
        ret = i->data;

        if (ret->m_tid == tid)
            return ret;
    }

    return nullptr;
}

/*!
 * @brief: Register a process to the kernel
 */
kerror_t proc_register(struct proc* proc, user_profile_t* profile)
{
    int error;
    processor_t* cpu;
    kevent_proc_ctx_t ctx;

    mutex_lock(proc->m_lock);

    error = oss_object_connect(__proc_root_obj, proc->obj);

    proc->profile = profile;

    mutex_unlock(proc->m_lock);

    /* Oops */
    if (error)
        return -1;

    /* Add a count to the runtime proccount */
    runtime_add_proccount();

    cpu = get_current_processor();

    ctx.process = proc;
    ctx.type = PROC_EVENTTYPE_CREATE;
    ctx.new_cpuid = cpu->m_cpu_num;
    ctx.old_cpuid = cpu->m_cpu_num;

    /* Fire a funky kernel event */
    kevent_fire("proc", &ctx, sizeof(ctx));

    return (0);
}

/*!
 * @brief: Unregister a process from the kernel
 */
kerror_t proc_unregister(struct proc* proc)
{
    processor_t* cpu;
    kevent_proc_ctx_t ctx;

    /* Add a count to the runtime proccount */
    runtime_remove_proccount();

    mutex_lock(proc->m_lock);

    /* Disconnect the process */
    oss_object_disconnect(__proc_root_obj, proc->obj);

    cpu = get_current_processor();

    ctx.process = proc;
    ctx.type = PROC_EVENTTYPE_DESTROY;
    ctx.new_cpuid = cpu->m_cpu_num;
    ctx.old_cpuid = cpu->m_cpu_num;

    kevent_fire("proc", &ctx, sizeof(ctx));

    mutex_unlock(proc->m_lock);

    return 0;
}

/*!
 * @brief: Check if the current process is the kernel process
 *
 * NOTE: this checks wether the current process has the ID of the
 * kernel process!
 */
bool current_proc_is_kernel()
{
    proc_t* curr = get_current_proc();
    return is_kernel(curr);
}

/*!
 * @brief: Allocate a thread on the dedicated thread allocator
 */
thread_t* allocate_thread()
{
    return zalloc_fixed(__thread_allocator);
}

/*!
 * @brief: Deallocate a thread from the dedicated thread allocator
 */
void deallocate_thread(thread_t* thread)
{
    zfree_fixed(__thread_allocator, thread);
}

/*
 * Initialize:
 *  - socket registry
 *  - proc_id generation
 *  - tspacket caching
 *  - TODO: process caching with its own zone allocator
 */
ANIVA_STATUS init_proc_core()
{
    __thread_allocator = create_zone_allocator(128 * Kib, sizeof(thread_t), NULL);

    oss_open_object("Proc", &__proc_root_obj);

    /*
     * System variables are an integral part of the process core,
     * although they can be classified as part of multiple subsystems, as they
     * cover a wide range of applications. We initialize their subsystem here
     * however, since in order to run processes, we NEED sysvars and userprofiles.
     * The process core also gets initialized really early and always before any
     * real system configuration needs to be done, so initializing this here
     * should not be an issue
     */
    init_sysvars();
    init_user_profiles();

    /*
     * Initialize the hdrv subsystem
     * These drivers are second order drivers (with first order drivers
     * being device drivers), which are in charge of driving software-to-software
     * services inside the OSes ABI. With these drivers, we're able to configure
     * the software interaction with system objects.
     */
    init_khandle_drivers();

    return ANIVA_SUCCESS;
}
