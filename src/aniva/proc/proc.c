#include "proc.h"
#include "core.h"
#include "dev/core.h"
#include "entry/entry.h"
#include "fs/file.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "kevent/types/thread.h"
#include "libk/bin/elf.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "lightos/driver/loader.h"
#include "lightos/syscall.h"
#include "lightos/sysvar/shared.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "oss/path.h"
#include "proc/env.h"
#include "proc/handle.h"
#include "proc/kprocs/reaper.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sys/types.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/resource.h"
#include "thread.h"
#include "time/core.h"
#include <libk/string.h>
#include <mem/heap.h>

static void __destroy_proc(proc_t* p);

static int _proc_init_pagemap(proc_t* proc)
{
    // Only create new page dirs for non-kernel procs
    if (!is_kernel_proc(proc) && !is_driver_proc(proc)) {
        return kmem_create_page_dir(&proc->m_root_pd, KMEM_CUSTOMFLAG_CREATE_USER, 0);
    }

    proc->m_root_pd.m_root = nullptr;
    proc->m_root_pd.m_phys_root = (paddr_t)kmem_get_krnl_dir();
    proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
    proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
    return 0;
}

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
proc_t* create_proc(proc_t* parent, struct user_profile* profile, char* name, FuncPtr entry, uintptr_t args, uint32_t flags)
{
    proc_t* proc;
    /* NOTE: ->m_init_thread gets set by proc_add_thread */
    thread_t* init_thread;
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
    proc->m_parent = parent;
    proc->m_flags = flags | PROC_UNRUNNED;
    proc->m_thread_count = 1;
    proc->m_threads = init_list();
    proc->m_lock = create_mutex(NULL);
    proc->obj = create_oss_obj(name);
    proc->m_name = proc->obj->name;

    /* Set the process launch time */
    if (time_get_system_time(&time) == 0)
        proc->m_dt_since_boot = time.s_since_boot;

    /* Register ourselves */
    oss_obj_register_child(proc->obj, proc, OSS_OBJ_TYPE_PROC, __destroy_proc);

    /* Create a pagemap */
    _proc_init_pagemap(proc);

    /* Create a handle map */
    init_khandle_map(&proc->m_handle_map, KHNDL_DEFAULT_ENTRIES);

    /* Okay to pass a reference, since resource bundles should NEVER own this memory */
    proc->m_resource_bundle = create_resource_bundle(&proc->m_root_pd);
    proc->m_env = create_penv(proc->m_name, proc, NULL, NULL);

    init_thread = create_thread_for_proc(proc, entry, args, "main");

    ASSERT_MSG(init_thread, "Failed to create main thread for process!");

    ASSERT(proc_add_thread(proc, init_thread) == 0);

    /* Yikes */
    if (proc_register(proc, profile)) {
        destroy_proc(proc);
        return nullptr;
    }

    return proc;
}

extern void NORETURN __userproc_entry_wrapper(FuncPtr entry);

static int _userproc_wrap_entry(proc_t* p, FuncPtr entry)
{
    int error;
    thread_t* init_thread;
    void* entry_buffer;
    vaddr_t k_entry_buffer;
    paddr_t p_entry_buffer;

    /* Allocate a userbuffer where we can put the entry wrapper */
    error = kmem_user_alloc_range(&entry_buffer, p, SMALL_PAGE_SIZE, NULL, KMEM_FLAG_WRITABLE);

    if (error)
        return error;

    /* Grab the physical address of the buffer */
    p_entry_buffer = kmem_to_phys(p->m_root_pd.m_root, (vaddr_t)entry_buffer);
    k_entry_buffer = kmem_from_phys(p_entry_buffer, KERNEL_MAP_BASE);

    /* Map this fucker to the kernel */
    kmem_map_page(NULL, k_entry_buffer, p_entry_buffer, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    /* Copy the entry wrapper stub into the buffer */
    memcpy((void*)k_entry_buffer, __userproc_entry_wrapper, SMALL_PAGE_SIZE);

    /* Unmap the page from the kernel */
    kmem_unmap_page(NULL, k_entry_buffer);

    /* Create a thread that starts at the entry wrapper stub */
    init_thread = create_thread_for_proc(p, entry_buffer, (u64)entry, "main");

    ASSERT_MSG(init_thread, "Failed to create main thread for cloned process!");

    ASSERT(proc_add_thread(p, init_thread) == 0);

    return 0;
}

/*!
 * @brief: Create a duplicate process
 *
 * Make sure that there are no threads in this process that hold any mutexes, since the clone
 * can't hold these at the same time.
 *
 * NOTE: Only copies process state (handles, pagemap, resourcemaps, ect.)
 */
int proc_clone(proc_t* p, FuncPtr clone_entry, const char* clone_name, proc_t** clone)
{
    proc_t* cloneproc;

    /* Create new 'clone' process */
    /* Copy handle map */
    /* Copy resource list */
    /* Copy environment */
    /* Copy thread state */
    /* ... */

    if (!p || !clone_entry || !clone_name || !clone)
        return -KERR_INVAL;

    cloneproc = kmalloc(sizeof(*cloneproc));

    if (!cloneproc)
        return -KERR_NOMEM;

    memset(cloneproc, 0, sizeof(proc_t));

    /* TODO: move away from the idea of idle threads */
    cloneproc->m_parent = p->m_parent;
    cloneproc->m_flags = (cloneproc->m_flags | PROC_UNRUNNED) & ~PROC_FINISHED;
    cloneproc->m_thread_count = 1;
    cloneproc->m_threads = init_list();
    cloneproc->m_lock = create_mutex(NULL);
    cloneproc->obj = create_oss_obj(clone_name);
    cloneproc->m_name = cloneproc->obj->name;

    /* Register ourselves */
    oss_obj_register_child(cloneproc->obj, cloneproc, OSS_OBJ_TYPE_PROC, __destroy_proc);

    /* Copy the pagemap */
    kmem_copy_page_dir(&cloneproc->m_root_pd, &p->m_root_pd);

    /* Copy the handle map */
    copy_khandle_map(&cloneproc->m_handle_map, &p->m_handle_map);

    /* Yoink the resource bundle */
    cloneproc->m_resource_bundle = share_resource_bundle(p->m_resource_bundle);

    /* Wrap the entry in a healthy environment and create a thread for it */
    _userproc_wrap_entry(cloneproc, clone_entry);

    /* Yikes */
    if ((proc_register(cloneproc, p->m_env->profile))) {
        destroy_proc(cloneproc);
        return -KERR_INVAL;
    }

    /* Export the bitch */
    *clone = cloneproc;
    return 0;
}

/* This should probably be done by kmem lmao */
#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args)
{
    user_profile_t* admin;

    /* TODO: check */
    if (!IS_KERNEL_FUNC(entry))
        return nullptr;

    /* Kernel processes always run inside the admin profile */
    admin = get_admin_profile();

    /* TODO: don't limit to one name */
    return create_proc(nullptr, admin, PROC_CORE_PROCESS_NAME, entry, args, PROC_KERNEL);
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
proc_t* proc_exec(const char* cmd, sysvar_vector_t vars, struct user_profile* profile, u32 flags)
{
    int error;
    proc_t* p;
    file_t* file;
    oss_path_t oss_path = { 0 };

    /* Parse the path on it's spaces */
    error = oss_parse_path_ex(cmd, &oss_path, ' ');

    /* Failed to parse */
    if (error || !oss_path.n_subpath)
        return nullptr;

    /* Open the file which hopefully houses our executable */
    file = file_open(oss_path.subpath_vec[0]);

    if (!file)
        return nullptr;

    /* Create a process from this file */
    p = elf_exec_64(file, (flags & PROC_KERNEL) == PROC_KERNEL);

    /* Close the file */
    file_close(file);

    /* Fuck, could not create process */
    if (!p)
        return nullptr;

    /* Add the vars to the environments vectors */
    if (vars)
        penv_add_vector(p->m_env, vars);

    if ((flags & PROC_SYNC) == PROC_SYNC)
        error = proc_schedule_and_await(p, profile, cmd, NULL, NULL, SCHED_PRIO_HIGH);
    else
        error = proc_schedule(p, profile, cmd, NULL, NULL, SCHED_PRIO_HIGH);

    if (error) {
        destroy_proc(p);
        return nullptr;
    }

    return p;
}

/*!
 * @brief: Try to set the entry of a process
 */
kerror_t proc_set_entry(proc_t* p, FuncPtr entry, uintptr_t arg0, uintptr_t arg1)
{
    if (!p || !p->m_init_thread)
        return -KERR_INVAL;

    mutex_lock(p->m_init_thread->m_lock);

    p->m_init_thread->f_entry = (f_tentry_t)entry;

    thread_set_entrypoint(p->m_init_thread, entry, arg0, arg1);

    mutex_unlock(p->m_init_thread->m_lock);
    return KERR_NONE;
}

static void __proc_clear_shared_resources(proc_t* proc)
{
    /* 1) Loop over all the allocated resources by this process */
    /* 2) detach from every resource and then the resource manager should
     *    automatically destroy resources and merge their address ranges
     *    into neighboring resources
     */

    if (!proc->m_resource_bundle)
        return;

    /* Destroy the entire bundle, which deallocates the structures */
    destroy_resource_bundle(proc->m_resource_bundle);
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

        if (!current_handle->reference.kobj || current_handle->index == KHNDL_INVALID_INDEX)
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
    FOREACH(i, proc->m_threads)
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
    destroy_list(proc->m_threads);
}

/*
 * Caller should ensure proc != zero
 */
void __destroy_proc(proc_t* proc)
{
    /* Yeet threads */
    __proc_kill_threads(proc);

    /* Yeet handles */
    __proc_clear_handles(proc);

    /* Free everything else */
    destroy_mutex(proc->m_lock);

    destroy_khandle_map(&proc->m_handle_map);

    KLOG_DBG("Doing shared resources\n");

    /* loop over all the resources of this process and release them by using kmem_dealloc */
    __proc_clear_shared_resources(proc);

    KLOG_DBG("Doing page dir\n");
    /*
     * Kill the root pd if it has one, other than the currently active page dir.
     * We already kinda expect this to only be called from kernel context, but
     * you never know... For that we simply allow every page directory to be
     * killed as long as we are not currently using it :clown:
     */
    if (proc->m_root_pd.m_root != get_current_processor()->m_page_dir)
        kmem_destroy_page_dir(proc->m_root_pd.m_root);

    KLOG_DBG("Doing penv\n");

    /* Kill the environment */
    destroy_penv(proc->m_env);

    /* Clear it out */
    proc->m_env = nullptr;

    kfree(proc);
}

void destroy_proc(proc_t* proc)
{
    /* Unregister from the global register store */
    ASSERT_MSG(proc_unregister(proc) == 0, "Failed to unregister proc");

    /* Calls __destroy_proc */
    destroy_oss_obj(proc->obj);
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
    const char* hook_name;

    /*
     * If we can't find the process here, that probably means its already
     * terminated even before we could make this call (How the fuck could that have happend???)
     */
    if (!proc)
        return 0;

    /* Pause the scheduler so we don't get fucked while registering the hook */
    pause_scheduler();

    hook_name = oss_obj_get_fullpath(proc->obj);

    kevent_add_poll_hook("proc", hook_name, _await_proc_term_hook_condition, proc);

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
    error = kevent_await_hook_fire("proc", hook_name, NULL, NULL);

remove_hook_and_fail:
    /* Remove the hook */
    kevent_remove_hook("proc", hook_name);

    /* Free the hook name */
    kfree((void*)hook_name);

    return error;
}

/*!
 * @brief: Adds a process to the scheduler
 *
 * Pretty much a wrapper around sched_add_proc
 */
int proc_schedule(proc_t* proc, struct user_profile* profile, const char* cmd, const char* stdio_path, HANDLE_TYPE stdio_type, enum SCHEDULER_PRIORITY prio)
{
    oss_node_t* env_node;

    if (!proc || !proc->m_env)
        return -KERR_INVAL;

    /* Default to the null device in this case */
    if (!stdio_path) {
        stdio_path = "Dev/Null";
        stdio_type = HNDL_TYPE_DEVICE;
    }

    if (!cmd)
        cmd = proc->m_name;

    env_node = proc->m_env->node;

    if (!env_node)
        return -KERR_INVAL;

    /* Add the process to a profile, if this hasn't been done yet */
    if (profile && profile != proc->m_env->profile)
        profile_add_penv(profile, proc->m_env);

    sysvar_vector_t sys_vec = {
        SYSVAR_VEC_STR(SYSVAR_CMDLINE, cmd),
        SYSVAR_VEC_STR(SYSVAR_PROCNAME, proc->m_name),
        SYSVAR_VEC_STR(SYSVAR_STDIO, stdio_path),
        SYSVAR_VEC_DWORD(SYSVAR_STDIO_HANDLE_TYPE, stdio_type),
        SYSVAR_VEC_END,
    };

    /* Add this vec */
    penv_add_vector(proc->m_env, sys_vec);

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

    if ((proc->m_flags & PROC_FINISHED) == PROC_FINISHED)
        return KERR_HANDLED;

    this_thread = get_current_scheduling_thread();

    /* Check every thread to see if there are any pending syscalls */
    FOREACH(i, proc->m_threads)
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
    proc->m_flags |= PROC_FINISHED;

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

    mutex_lock(proc->m_lock);

    if ((proc->m_flags & PROC_FINISHED) == PROC_FINISHED)
        goto unlock_and_exit;

    /* We just need to know that this thread is not yet added to the process */
    error = list_indexof(proc->m_threads, NULL, thread);

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
    if (proc->m_threads->m_length == 0 && proc->m_init_thread == nullptr) {
        proc->m_init_thread = thread;
        /* Ensure the schedulers picks up on this fact */
        proc->m_flags |= PROC_UNRUNNED;
    }

    /* Add the thread to the processes list (NOTE: ->m_thread_count has already been updated at this point) */
    list_append(proc->m_threads, thread);

unlock_and_exit:
    mutex_unlock(proc->m_lock);
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

    mutex_lock(proc->m_lock);

    if (!list_remove_ex(proc->m_threads, thread))
        goto unlock_and_exit;

    proc->m_thread_count--;

    thread_ctx.thread = thread;
    thread_ctx.type = THREAD_EVENTTYPE_DESTROY;
    /* TODO: smp */
    thread_ctx.new_cpu_id = 0;
    thread_ctx.old_cpu_id = 0;

    /* Fire the create event */
    (void)kevent_fire("thread", &thread_ctx, sizeof(thread_ctx));

unlock_and_exit:
    mutex_unlock(proc->m_lock);
    return error;
}

void proc_add_async_task_thread(proc_t* proc, FuncPtr entry, uintptr_t args)
{
    // TODO: generate new unique name
    // list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
    kernel_panic("TODO: implement async task threads");
}

const char* proc_try_get_symname(proc_t* proc, uintptr_t addr)
{
    const char* proc_path;
    const char* buffer;
    kerror_t error;

    if (!proc || !addr)
        return nullptr;

    proc_path = oss_obj_get_fullpath(proc->obj);

    dynldr_getfuncname_msg_t msg_block = {
        .proc_path = proc_path,
        .func_addr = (void*)addr,
    };

    error = driver_send_msg_a(
        DYN_LDR_URL,
        DYN_LDR_GET_FUNC_NAME,
        &msg_block,
        sizeof(msg_block),
        &buffer,
        sizeof(char*));

    /* Free the path */
    kfree((void*)proc_path);

    if ((error) || !buffer || !strlen(buffer))
        return nullptr;

    return buffer;
}
