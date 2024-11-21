#include "fs/file.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "kevent/types/thread.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "priv.h"
#include "proc/core.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <lightos/handle_def.h>

/*
 * Driver to dynamically load programs that were linked against shared binaries
 *
 */

/* Where we store all the loaded_app structs */
static list_t* _loaded_apps;
static mutex_t* _dynld_lock;

/*!
 * @brief: Register an app that has been dynamically loaded
 *
 * Simply appends to the applist
 */
kerror_t register_app(loaded_app_t* app)
{
    if (!app)
        return -KERR_NULL;

    mutex_lock(_dynld_lock);

    list_append(_loaded_apps, app);

    mutex_unlock(_dynld_lock);
    return KERR_NONE;
}

/*!
 * @brief: Unregister an app that has been dynamically loaded
 *
 * Simply removes from the applist
 */
kerror_t unregister_app(loaded_app_t* app)
{
    bool result;

    if (!app)
        return -KERR_NULL;

    mutex_lock(_dynld_lock);

    result = list_remove_ex(_loaded_apps, app);

    mutex_unlock(_dynld_lock);

    return result ? KERR_NONE : -KERR_INVAL;
}

/*!
 * @brief: Find a loaded app from a process object
 */
static loaded_app_t* _get_app_from_proc(proc_t* proc)
{
    loaded_app_t* ret;

    FOREACH(i, _loaded_apps)
    {
        ret = i->data;

        if (ret->proc == proc)
            return ret;
    }

    return nullptr;
}

/*!
 * @brief: Hook for any process events
 */
static int _dyn_loader_proc_hook(kevent_ctx_t* _ctx, void* param)
{
    loaded_app_t* app;
    kevent_proc_ctx_t* ctx;

    if (_ctx->buffer_size != sizeof(*ctx))
        return -1;

    ctx = (kevent_proc_ctx_t*)_ctx->buffer;

    /* We're not interrested in these events */
    if (ctx->type != PROC_EVENTTYPE_DESTROY)
        return 0;

    /*
     * We've hit an interresting event
     * Now there is an issue, since we're inside an event context: we need to aquire
     * the loaders lock, but we can't. To solve this here is a lil todo,
     * TODO: Create workqueues that we can defer this kind of work to. This means we need to 'pause'
     * the actual event, because in the case of a proc destroy for example, we can't just defer the
     * event handler, continue to destory the process and THEN call the deferred handler, right after
     * we've already gotter rid of the process...
     */

    app = _get_app_from_proc(ctx->process);

    /* OK, we're not interrested */
    if (!app)
        return 0;

    unregister_app(app);

    destroy_loaded_app(app);
    return 0;
}

/*!
 * @brief: Initialize the drivers structures
 */
static int _loader_init()
{
    _loaded_apps = init_list();
    _dynld_lock = create_mutex(NULL);

    ASSERT(KERR_OK(kevent_add_hook("proc", DYN_LDR_NAME, _dyn_loader_proc_hook, NULL)));
    return 0;
}

/*!
 * @brief: Destruct the drivers structures
 *
 * TODO: Kill any processes that are still registered
 */
static int _loader_exit()
{
    loaded_app_t* app;

    ASSERT(KERR_OK(kevent_remove_hook("proc", DYN_LDR_NAME)));

    FOREACH(i, _loaded_apps)
    {
        app = i->data;

        destroy_loaded_app(app);
    }

    destroy_list(_loaded_apps);
    destroy_mutex(_dynld_lock);
    return 0;
}

/*!
 * @brief: Load a dynamically linked program directly from @file
 */
static kerror_t _loader_ld_appfile(file_t* file, proc_t** p_proc)
{
    kerror_t error;
    loaded_app_t* app;

    /* We've found the file, let's try to load this fucker */
    error = load_app(file, &app, p_proc);

    if (error || !app)
        return -DRV_STAT_INVAL;

    /* Everything went good, register it to ourself */
    register_app(app);

    return KERR_NONE;
}

/*!
 * @brief: Open @path and load the app that in the resulting file
 */
static kerror_t _loader_ld_app(const char* path, size_t pathlen, proc_t** p_proc)
{
    kerror_t error;
    file_t* file;

    /* Try to find the file (TODO: make this not just gobble up the input buffer) */
    file = file_open(path);

    if (!file)
        return -DRV_STAT_INVAL;

    error = _loader_ld_appfile(file, p_proc);

    file_close(file);

    return error;
}

static uint64_t _loader_msg(aniva_driver_t* driver, dcc_t code, void* in_buf, size_t in_bsize, void* out_buf, size_t out_bsize)
{
    file_t* in_file;
    proc_t* c_proc;
    const char* in_path;

    c_proc = get_current_proc();

    switch (code) {
    /*
     * Load an app with dynamic libraries
     * TODO: safety
     */
    case DYN_LDR_LOAD_APP:
        in_path = in_buf;

        if (!in_path)
            return DRV_STAT_INVAL;

        if (!out_buf || out_bsize != sizeof(proc_t*))
            return DRV_STAT_INVAL;

        /* FIXME: Safety here lmao */
        if (_loader_ld_app(in_path, in_bsize, (proc_t**)out_buf))
            return DRV_STAT_INVAL;

        break;
    case DYN_LDR_LOAD_APPFILE:
        in_file = in_buf;

        /* This would be kinda cringe */
        if (!in_file || in_bsize != sizeof(*in_file))
            return DRV_STAT_INVAL;

        if (!out_buf || out_bsize != sizeof(proc_t*))
            return DRV_STAT_INVAL;

        /*
         * Try to load this mofo directly.
         * NOTE: File ownership is in the hands of the caller,
         *       so we can't touch file lifetime
         */
        if (_loader_ld_appfile(in_file, (proc_t**)out_buf))
            return DRV_STAT_INVAL;

        break;
    /* Look through the loaded libraries of the current process to find the specified library */
    case DYN_LDR_GET_LIB:

        /* Can't give out a handle */
        if (out_bsize != sizeof(HANDLE))
            return DRV_STAT_INVAL;

        kernel_panic("TODO: DYN_LDR_GET_LIB");
        break;
    case DYN_LDR_LOAD_LIB: {
        loaded_app_t* target_app;
        dynamic_library_t* lib;

        if (!in_bsize || !in_buf || !out_bsize || !out_buf)
            return DRV_STAT_INVAL;

        target_app = _get_app_from_proc(c_proc);

        if (!target_app)
            return DRV_STAT_INVAL;

        if (load_dynamic_lib((const char*)in_buf, target_app, &lib))
            return DRV_STAT_INVAL;

        /* Wait for the libraries entry function to finish */
        await_lib_init(lib);

        *(dynamic_library_t**)out_buf = lib;
        break;
    }
    case DYN_LDR_GET_FUNC_ADDR: {
        loaded_app_t* target_app;
        loaded_sym_t* target_sym;
        const char* dyn_func;

        if (in_bsize != sizeof(const char*) || !in_buf || out_bsize != sizeof(vaddr_t*) || !out_buf)
            return DRV_STAT_INVAL;

        /* Yay, unsafe cast :clown: */
        dyn_func = (const char*)in_buf;

        target_app = _get_app_from_proc(c_proc);

        if (!target_app)
            return DRV_STAT_INVAL;

        target_sym = loaded_app_find_symbol(target_app, dyn_func);

        if (!target_sym)
            return DRV_STAT_INVAL;

        if ((target_sym->flags & LDSYM_FLAG_EXPORT) != LDSYM_FLAG_EXPORT)
            return DRV_STAT_INVAL;

        *(vaddr_t*)out_buf = target_sym->uaddr;
        break;
    }
    case DYN_LDR_GET_FUNC_NAME: {
        proc_t* target_proc;
        loaded_app_t* target_app;
        loaded_sym_t* sym;
        dynldr_getfuncname_msg_t* msgblock;

        if (in_bsize != sizeof(*msgblock) || !out_buf || out_bsize != sizeof(char**))
            return DRV_STAT_INVAL;

        msgblock = in_buf;

        /* Find the process we need to check */
        target_proc = find_proc(msgblock->proc_path);

        if (!target_proc)
            return DRV_STAT_INVAL;

        /* Check if we have a loaded app for this lmao */
        target_app = _get_app_from_proc(target_proc);

        if (!target_app)
            return DRV_STAT_INVAL;

        sym = loaded_app_find_symbol_by_addr(target_app, msgblock->func_addr);

        if (!sym || !sym->name)
            return DRV_STAT_INVAL;

        /* Found a symbol yay */
        *(const char**)out_buf = sym->name;
        break;
    }
    default:
        return DRV_STAT_INVAL;
    }

    return DRV_STAT_OK;
}

EXPORT_DRIVER(_dynamic_loader) = {
    .m_name = DYN_LDR_NAME,
    .m_descriptor = "Used to load dynamically linked apps",
    .m_type = DT_OTHER,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = _loader_init,
    .f_exit = _loader_exit,
    .f_msg = _loader_msg,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};

/*!
 * @brief: Event handler for catching library initialize events
 *
 * When an app wants to load a library at runtime, we first need to initialize that lib, so for that
 * we create an extra thread in the target process that does the library initialization. When this thread
 * is finished, we join with the main thread and continue execution
 */
static int __libinit_thread_eventhook(kevent_ctx_t* _ctx, void* param)
{
    loaded_app_t* app;
    dynamic_library_t* lib;
    kevent_thread_ctx_t* ctx;

    KLOG_DBG("__libinit_thread_eventhook triggered!\n");

    /* Kinda weird lmao */
    if (_ctx->buffer_size != sizeof(*ctx))
        return 0;

    ctx = _ctx->buffer;

    if (ctx->type != THREAD_EVENTTYPE_DESTROY)
        return 0;

    app = _get_app_from_proc(ctx->thread->m_parent_proc);

    if (!app)
        return 0;

    FOREACH(i, app->library_list)
    {
        lib = i->data;

        if (lib->lib_init_thread != ctx->thread)
            continue;

        ASSERT_MSG(lib->lib_wait_thread, "FUCK: got a library initialize thread, but no thread waiting for this initialize!");

        KLOG_DBG("Unblocking wait thread: %s\n", lib->lib_wait_thread->m_name);

        thread_unblock(lib->lib_wait_thread);

        lib->lib_init_thread = nullptr;
        lib->lib_wait_thread = nullptr;
        break;
    }

    return 0;
}

/*!
 * @brief: Initialize a dynamic library and wait for it's entry function to finish
 *
 * TODO: Create a timeout for libraries that take too long to init?
 * TODO: Let the loading app specify if they want to be able to run in paralel to the initialization thread?
 *
 * Called when loading a library inside an already running app
 */
kerror_t await_lib_init(dynamic_library_t* lib)
{
    proc_t* target_proc;
    thread_t* lib_entry_thread;
    thread_t* lib_wait_thread;
    vaddr_t libtramp_uvirt;
    vaddr_t libtramp_kvirt;

    target_proc = lib->app->proc;
    lib_wait_thread = get_current_scheduling_thread();

    if (!target_proc)
        return -KERR_NULL;

    /* Allocate a page for the trampoline code inside the process */
    ASSERT(!kmem_user_alloc_range((void**)&libtramp_uvirt, target_proc, SMALL_PAGE_SIZE, NULL, NULL));
    ASSERT(!kmem_get_kernel_address(&libtramp_kvirt, libtramp_uvirt, target_proc->m_root_pd.m_root));

    /* Copy the code into the process */
    memcpy((void*)libtramp_kvirt, __lib_trampoline, SMALL_PAGE_SIZE);

    /* Make sure it knows to leave physical memory alone */
    resource_apply_flags(libtramp_uvirt, SMALL_PAGE_SIZE, KRES_FLAG_MEM_KEEP_PHYS, target_proc->m_resource_bundle->resources[KRES_TYPE_MEM]);

    lib_entry_thread = create_thread_for_proc(target_proc, (FuncPtr)libtramp_uvirt, (uintptr_t)lib->entry, lib->name);

    if (!lib_entry_thread)
        return -KERR_NULL;

    /* Mark which thread is waiting on this sucker */
    lib->lib_wait_thread = lib_wait_thread;
    lib->lib_init_thread = lib_entry_thread;

    /* Add the eventhook for the thread termination */
    kevent_add_hook("thread", lib->name, __libinit_thread_eventhook, NULL);

    /* Add the thread to the process */
    proc_add_thread(target_proc, lib_entry_thread);

    pause_scheduler();

    /* Add the fucker to the scheduler */
    scheduler_add_thread(lib_entry_thread, SCHED_PRIO_MID);

    /* Block until we get the thread death event for the lib entry thread */
    thread_block_unpaused(lib_wait_thread);

    /* Chill */
    resume_scheduler();

    scheduler_yield();

    /* Remove the hook when we're done */
    kevent_remove_hook("thread", lib->name);

    /* Remove the weird memory */
    kmem_user_dealloc(target_proc, libtramp_uvirt, SMALL_PAGE_SIZE);

    return 0;
}
