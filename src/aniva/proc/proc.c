#include "proc.h"
#include "core.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "kevent/types/thread.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/api/process.h"
#include "lightos/api/sysvar.h"
#include "lightos/error.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "mem/phys.h"
#include "mem/tracker/tracker.h"
#include "oss/core.h"
#include "oss/object.h"
#include "oss/path.h"
#include "proc/exec/exec.h"
#include "proc/handle.h"
#include "proc/kprocs/reaper.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sys/types.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include "thread.h"
#include "time/core.h"
#include <libk/string.h>
#include <mem/heap.h>

#define PROC_DEFAULT_PAGE_TRACKER_BSIZE 0x8000

static int __destroy_proc(oss_object_t* object);

static int _proc_init_pagemap(proc_t* proc)
{
    // Only create new page dirs for non-kernel procs
    if (!is_kernel_proc(proc) && !is_driver_proc(proc)) {
        return kmem_create_page_dir(&proc->m_root_pd, KMEM_CUSTOMFLAG_CREATE_USER, 0);
    }

    return kmem_get_kernel_addrspace(&proc->m_root_pd);
}

static inline int __proc_init_page_tracker(proc_t* proc, size_t bsize)
{
    error_t error;
    void* range_cache_buf;

    /* Try to allocate a buffer for our tracker allocations */
    error = kmem_kernel_alloc_range(&range_cache_buf, bsize, NULL, KMEM_FLAG_WRITABLE);

    if (error)
        return error;

    /* Initialize the page tracker */
    error = init_page_tracker(&proc->m_virtual_tracker, range_cache_buf, bsize, (u64)-1);

    if (error)
        goto dealloc_and_exit;

    /* Pre-allocate the first 1 Mib of pages, so the NULL address can always stay un-allocated */
    /* TODO: have this work */
    // error = page_tracker_alloc(&proc->m_virtual_tracker, 0, 256, PAGE_RANGE_FLAG_UNBACKED);

    // if (IS_FATAL(error))
    // goto dealloc_and_exit;

    /* Algood, let's exit */
    return error;

dealloc_and_exit:
    kmem_kernel_dealloc((u64)range_cache_buf, bsize);

    return error;
}

static inline int __proc_destroy_page_tracker(proc_t* proc)
{
    size_t bsize;
    void* buffer;

    if (!proc)
        return -EINVAL;

    /* Cache this size real quick */
    bsize = proc->m_virtual_tracker.sz_cache_buffer;
    buffer = proc->m_virtual_tracker.cache_buffer;

    /* Clear the virtual page tracker */
    ASSERT(IS_OK(kmem_phys_dealloc_from_tracker(proc->m_root_pd.m_root, &proc->m_virtual_tracker)));

    /* Destroy the tracker and it's trackings */
    destroy_page_tracker(&proc->m_virtual_tracker);

    /* Also fuck the buffer we created */
    return kmem_kernel_dealloc((u64)buffer, bsize);
}

static oss_object_ops_t proc_oss_ops = {
    .f_Destroy = __destroy_proc,
};

/*!
 * @brief Allocate memory for a process and prepare it for execution
 *
 * This creates:
 * - the processes page-map if needed
 * - a main thread
 * - some structures to do process housekeeping
 *
 * TODO: remove sockets from existing
 */
proc_t* create_proc(const char* name, struct user_profile* profile, u32 flags)
{
    proc_t* proc;
    /* NOTE: ->m_init_thread gets set by proc_add_thread */
    system_time_t time;

    if (!name)
        return nullptr;

    if (!profile)
        profile = get_active_profile();

    /* TODO: create proc cache */
    proc = kmalloc(sizeof(proc_t));

    if (!proc)
        return nullptr;

    memset(proc, 0, sizeof(proc_t));

    /* TODO: move away from the idea of idle threads */
    proc->flags = flags;
    proc->threads = init_list();
    proc->lock = create_mutex(NULL);
    proc->obj = create_oss_object(name, OF_NO_DISCON, OT_PROCESS, &proc_oss_ops, proc);
    proc->name = proc->obj->key;
    proc->profile = profile;

    /* Set the process launch time */
    if (time_get_system_time(&time) == 0)
        proc->proc_launch_time_rel_boot_sec = time.s_since_boot;

    /* Create a pagemap */
    _proc_init_pagemap(proc);

    /* Create a handle map */
    init_khandle_map(&proc->m_handle_map, KHNDL_DEFAULT_ENTRIES);

    /* Initialize this fucker */
    __proc_init_page_tracker(proc, PROC_DEFAULT_PAGE_TRACKER_BSIZE);

    /* Yikes */
    if (IS_OK(proc_register(proc, profile)))
        return proc;

    destroy_proc(proc);
    return nullptr;
}

extern void NORETURN __userproc_entry_wrapper(FuncPtr entry);

/* This should probably be done by kmem lmao */
#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args)
{
    proc_t* ret;
    thread_t* main_thread;
    user_profile_t* admin;

    /* TODO: check */
    if (!IS_KERNEL_FUNC(entry))
        return nullptr;

    /* Kernel processes always run inside the admin profile */
    admin = get_admin_profile();

    /* TODO: don't limit to one name */
    ret = create_proc(PROC_CORE_PROCESS_NAME, admin, PF_KERNEL);

    if (!ret)
        return nullptr;

    main_thread = create_thread(entry, args, "main", ret, true);

    if (!main_thread) {
        destroy_proc(ret);
        return 0;
    }

    /* Add the main thread */
    proc_add_thread(ret, main_thread);

    return ret;
}

/*!
 * @brief: Execute an executable at @path
 *
 * FIXME: We are giving away a reference to the process with this function, even though the process might get
 * absolutely astrosmacked (aka terminated and destroyed) at any time. When this happens, we still have this
 * reference floating around, which may cause very nasty bugs. Fix this ASAP, either with reference counting,
 * or buffer objects
 *
 * @cmd: The path to the executable to execute. Also includes the
 * command parameters.
 * @profile: The profile to which we want to register this process
 * @flags: Flags to be passed to the process
 */
proc_t* proc_exec(const char* cmd, const char* pname, sysvar_vector_t vars, struct user_profile* profile, u32 flags)
{
    int error;
    proc_t* p;
    oss_object_t* object = nullptr;
    oss_path_t oss_path = { 0 };

    /* Try to create a process */
    p = create_proc(pname, profile, flags);

    if (!p)
        return nullptr;

    /* Parse the path on it's spaces */
    error = oss_parse_path_ex(cmd, &oss_path, ' ');

    /* Failed to parse */
    if (error || !oss_path.n_subpath)
        goto error_and_exit;

    /* Open the file which hopefully houses our executable */
    error = oss_open_object(oss_path.subpath_vec[0], &object);

    /* Destroy the parsed path, since we don't need that guy anymore */
    oss_destroy_path(&oss_path);

    if (error)
        goto error_and_exit;

    /* The exec call will bind the contents of the object to a thread complex for the process */
    error = aniva_exec(object, p, NULL);

    /* Bruh =( */
    if (error)
        goto error_and_exit;

    /* Close the file */
    oss_object_close(object);

    if ((flags & PF_SYNC) == PF_SYNC)
        error = proc_schedule_and_await(p, profile, cmd, NULL, NULL, SCHED_PRIO_HIGH);
    else
        error = proc_schedule(p, profile, cmd, NULL, NULL, SCHED_PRIO_HIGH);

    if (error) {
        destroy_proc(p);
        return nullptr;
    }

    return p;

error_and_exit:
    if (object)
        oss_object_close(object);

    destroy_proc(p);

    return nullptr;
}

/*
 * Loop over all the open handles of this process and clear out the
 * lingering references and objects
 */
static void __proc_clear_handles(proc_t* proc)
{
    khandle_map_t* map;
    khandle_t* current_handle;

    if (!proc)
        return;

    map = &proc->m_handle_map;

    for (uint32_t i = 0; i < map->max_count; i++) {
        current_handle = &map->handles[i];

        if (!current_handle->kobj || current_handle->index == KHNDL_INVALID_INDEX)
            continue;

        destroy_khandle(current_handle);
    }
}

static inline void __proc_kill_threads(proc_t* proc)
{
    list_t* temp_tlist;

    if (!proc)
        return;

    /* Create a temporary trampoline list */
    temp_tlist = init_list();

    /* Put all threads on a seperate temporary list */
    FOREACH(i, proc->threads)
    {
        /* Kill every thread */
        list_append(temp_tlist, i->data);
    }

    /*  */
    FOREACH(i, temp_tlist)
    {
        /* Make sure we remove the thread from the processes queue */
        proc_remove_thread(proc, i->data);

        /* Murder the bitch */
        destroy_thread(i->data);
    }

    destroy_list(temp_tlist);
    destroy_list(proc->threads);
}

/*!
 * @brief: The oss function for when a process gets destroyed
 *
 * Only gets called when the process object runs out of references.
 */
static int __destroy_proc(oss_object_t* object)
{
    proc_t* proc;

    if (!object || object->type != OT_PROCESS || !object->private)
        return -EINVAL;

    proc = object->private;

    /* Yeet handles */
    __proc_clear_handles(proc);

    /* Yeet threads */
    __proc_kill_threads(proc);

    /* Free everything else */
    destroy_mutex(proc->lock);

    destroy_khandle_map(&proc->m_handle_map);

    /* loop over all the resources of this process and release them by using kmem_dealloc */
    __proc_destroy_page_tracker(proc);

    /*
     * Kill the root pd if it has one, other than the currently active page dir.
     * We already kinda expect this to only be called from kernel context, but
     * you never know... For that we simply allow every page directory to be
     * killed as long as we are not currently using it :clown:
     */
    if (get_current_processor()->m_page_dir != &proc->m_root_pd)
        kmem_destroy_page_dir(proc->m_root_pd.m_root);

    kfree(proc);
    return 0;
}

void destroy_proc(proc_t* proc)
{
    /*
     * Unregister from the global register store
     * This releases the object reference that the process core has gained
     * when a process gets registered
     */
    ASSERT_MSG(proc_unregister(proc) == 0, "Failed to unregister proc");
}

static bool _await_proc_term_hook_condition(kevent_ctx_t* ctx, void* param)
{
    proc_t* param_proc = param;
    kevent_proc_ctx_t* proc_ctx;

    if (ctx->buffer_size != sizeof(*proc_ctx))
        return false;

    proc_ctx = ctx->buffer;

    /* Check if this is our process */
    if (proc_ctx->type != PROC_EVENTTYPE_DESTROY || proc_ctx->process != param_proc)
        return false;

    /* Yes! Fire the hook */
    return true;
}

/*!
 * @brief Wait for a process to be terminated
 *
 *
 */
int proc_schedule_and_await(proc_t* proc, struct user_profile* profile, const char* cmd, const char* stdio_path, HANDLE_TYPE stdio_type, enum SCHEDULER_PRIORITY prio)
{
    int error;

    /*
     * If we can't find the process here, that probably means its already
     * terminated even before we could make this call (How the fuck could that have happend???)
     */
    if (!proc)
        return 0;

    /* Pause the scheduler so we don't get fucked while registering the hook */
    pause_scheduler();

    kevent_add_poll_hook("proc", proc->name, _await_proc_term_hook_condition, proc);

    /* Do an instant rescedule */
    error = proc_schedule(proc, profile, cmd, stdio_path, stdio_type, prio);

    /* Fuck */
    if (error) {
        resume_scheduler();
        goto remove_hook_and_fail;
    }

    /* Resume the scheduler so we don't die */
    resume_scheduler();

    /* Wait for the process to be bopped */
    error = kevent_await_hook_fire("proc", proc->name, NULL, NULL);

remove_hook_and_fail:
    /* Remove the hook */
    kevent_remove_hook("proc", proc->name);

    return error;
}

/*!
 * @brief: Adds a process to the scheduler
 *
 * Pretty much a wrapper around sched_add_proc
 */
int proc_schedule(proc_t* proc, struct user_profile* profile, const char* cmd, const char* stdio_path, HANDLE_TYPE stdio_type, enum SCHEDULER_PRIORITY prio)
{
    oss_object_t* proc_obj;

    if (!proc)
        return -KERR_INVAL;

    /* Default to the null device in this case */
    if (!stdio_path) {
        stdio_path = "Devices/Null";
        stdio_type = HNDL_TYPE_OBJECT;
    }

    if (!cmd)
        cmd = proc->name;

    proc_obj = proc->obj;

    if (!proc_obj)
        return -KERR_INVAL;

    if (!profile)
        profile = proc->profile;

    /* Attack the most basic sysvars to the process */
    sysvar_attach_ex(proc_obj, SYSVAR_CMDLINE, profile->attr.ptype, SYSVAR_TYPE_STRING, NULL, (void*)cmd, strlen(cmd));
    sysvar_attach_ex(proc_obj, SYSVAR_STDIO, profile->attr.ptype, SYSVAR_TYPE_STRING, NULL, (void*)stdio_path, strlen(stdio_path));
    sysvar_attach_ex(proc_obj, SYSVAR_STDIO_HANDLE_TYPE, profile->attr.ptype, SYSVAR_TYPE_DWORD, NULL, (void*)&stdio_type, sizeof(stdio_type));

    /* Try to add all threads of this process to the scheduler */
    return scheduler_add_proc(proc, prio);
}

/*
 * Terminate the process, which means that we
 * 1) Remove it from the global register store, so that the id gets freed
 * 2) Queue it to the reaper thread so It can be safely be disposed of
 *
 * NOTE: don't remove from the scheduler here, but in the reaper
 */
kerror_t try_terminate_process(proc_t* proc)
{
    return try_terminate_process_ex(proc, false);
}

/*!
 * @brief: Queues a process for termination
 *
 * Order of opperation (+ lockings)
 * 1) Freeze the scheduler so we don't get fucked out here (this function might
 *    get called from inside a process, which means this function needs to always
 *    return)
 * 2) Make sure the targeted process does not get scheduled again
 * 3) Register the process to the reaper process, so it can get destroyed
 * 4) Unpause the scheduler
 */
kerror_t try_terminate_process_ex(proc_t* proc, bool defer_yield)
{
    thread_t* c_thread;
    thread_t* this_thread;
    kerror_t error;

    if (!proc)
        return -1;

    if ((proc->flags & PF_FINISHED) == PF_FINISHED)
        return KERR_HANDLED;

    this_thread = get_current_scheduling_thread();

    /* Check every thread to see if there are any pending syscalls */
    FOREACH(i, proc->threads)
    {
        c_thread = i->data;

        /* We already know we're in a syscall if we meet ourselves */
        if (c_thread == this_thread)
            continue;

        KLOG_DBG("Thread %s %s in syscall: %d\n", c_thread->m_name, SYSID_IS_VALID(c_thread->m_c_sysid) ? "is" : "was last", c_thread->m_c_sysid);

        /* This thread is OK, just murder it */
        if (c_thread->m_c_sysid == SYSID_EXIT || !SYSID_IS_VALID(c_thread->m_c_sysid))
            goto disable_scheduling_and_go_next;

        /* Wait until the thread finishes it's syscall and stops itself */
        while (c_thread->m_current_state == RUNNABLE || c_thread->m_current_state == RUNNING) {
            // KLOG_DBG("Waiting for syscall... %d\n", c_thread->m_c_sysid);

            /* Make the thread yield when it exits this syscall */
            SYSID_SET_VALID(c_thread->m_c_sysid, false);

            scheduler_yield();
        }

    disable_scheduling_and_go_next:
        /* Not in a syscall, yay */
        thread_stop(c_thread);
    }

    /* Pause the scheduler to make sure we're not fucked while doing this */
    pause_scheduler();

    /* Mark as finished, since we know we won't be seeing it again after we return from this call */
    proc->flags |= PF_FINISHED;

    /*
     * Register to the reaper
     * NOTE: this also pauses the scheduler
     */
    error = reaper_register_process(proc);

    /* Remove from the scheduler (Pauses it) */
    (void)scheduler_remove_proc(proc);

    resume_scheduler();

    /* Yield to catch any terminates from within a process */
    if (!defer_yield)
        scheduler_yield();
    return error;
}

/*!
 * @brief Exit the current process
 *
 * Nothing to add here...
 */
void proc_exit()
{
    try_terminate_process(get_current_proc());
    scheduler_yield();
}

/*!
 * @brief: Add a thread to @proc
 *
 * Locks the process and fails if we try to add a thread to a finished process
 */
kerror_t proc_add_thread(proc_t* proc, struct thread* thread)
{
    kerror_t error;
    kevent_thread_ctx_t thread_ctx = { 0 };

    if (!thread || !proc)
        return -1;

    error = -1;

    mutex_lock(proc->lock);

    if ((proc->flags & PF_FINISHED) == PF_FINISHED)
        goto unlock_and_exit;

    /* We just need to know that this thread is not yet added to the process */
    error = list_indexof(proc->threads, NULL, thread);

    if (error == 0)
        goto unlock_and_exit;

    error = (0);

    thread_ctx.thread = thread;
    thread_ctx.type = THREAD_EVENTTYPE_CREATE;
    /* TODO: smp */
    thread_ctx.new_cpu_id = 0;
    thread_ctx.old_cpu_id = 0;

    /* Fire the create event */
    kevent_fire("thread", &thread_ctx, sizeof(thread_ctx));

    /* If this is the first thread, we need to make sure it gets ran first */
    if (proc->threads->m_length == 0 && proc->main_thread == nullptr)
        proc->main_thread = thread;

    /* Add the thread to the processes list (NOTE: ->m_thread_count has already been updated at this point) */
    list_append(proc->threads, thread);

unlock_and_exit:
    mutex_unlock(proc->lock);
    return error;
}

/*!
 * @brief: Remove a thread from the process
 */
kerror_t proc_remove_thread(proc_t* proc, struct thread* thread)
{
    kerror_t error;
    kevent_thread_ctx_t thread_ctx = { 0 };

    error = -KERR_INVAL;

    mutex_lock(proc->lock);

    if (!list_remove_ex(proc->threads, thread))
        goto unlock_and_exit;

    thread_ctx.thread = thread;
    thread_ctx.type = THREAD_EVENTTYPE_DESTROY;
    /* TODO: smp */
    thread_ctx.new_cpu_id = 0;
    thread_ctx.old_cpu_id = 0;

    /* Fire the create event */
    (void)kevent_fire("thread", &thread_ctx, sizeof(thread_ctx));

unlock_and_exit:
    mutex_unlock(proc->lock);
    return error;
}

const char* proc_try_get_symname(proc_t* proc, uintptr_t addr)
{
    aniva_exec_method_t* method;

    /* Try to get the execution method that handled this process */
    method = aniva_exec_method_get(proc->exec_key);

    if (!method || !method->f_get_func_symbol)
        return nullptr;

    return method->f_get_func_symbol(proc, addr);
}
