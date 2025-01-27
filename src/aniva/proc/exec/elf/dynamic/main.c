#include "fs/file.h"
#include "kevent/event.h"
#include "kevent/types/proc.h"
#include "kevent/types/thread.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include "proc/core.h"
#include "proc/exec/exec.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <lightos/api/handle.h>
#include <proc/exec/elf/dynamic.h>

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

        if (loaded_app_get_proc(ret) == proc)
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
 * @brief: Load a dynamically linked program directly from @file
 */
static kerror_t _loader_ld_appfile(file_t* file, proc_t* p_proc)
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

    FOREACH(i, app->unordered_liblist)
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

    target_proc = loaded_app_get_proc(lib->app);
    lib_wait_thread = get_current_scheduling_thread();

    if (!target_proc)
        return -KERR_NULL;

    /* Allocate a page for the trampoline code inside the process */
    ASSERT(!kmem_user_alloc_range((void**)&libtramp_uvirt, target_proc, SMALL_PAGE_SIZE, NULL, NULL));
    ASSERT(!kmem_get_kernel_address(&libtramp_kvirt, libtramp_uvirt, target_proc->m_root_pd.m_root));

    /* Copy the code into the process */
    memcpy((void*)libtramp_kvirt, __lib_trampoline, SMALL_PAGE_SIZE);

    /* Make sure it knows to leave physical memory alone */
    // resource_apply_flags(libtramp_uvirt, SMALL_PAGE_SIZE, KRES_FLAG_MEM_KEEP_PHYS, target_proc->m_resource_bundle->resources[KRES_TYPE_MEM]);

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

static error_t __elf_dynamic_execute(oss_object_t* object, proc_t* target)
{
    file_t* file;

    if (!object)
        return -EINVAL;

    file = oss_object_unwrap(object, OT_FILE);

    if (!file)
        return -EINVAL;

    return _loader_ld_appfile(file, target);
}

static const char* __elf_dynamic_get_fsymbol(proc_t* proc, vaddr_t addr)
{
    loaded_sym_t* sym;
    loaded_app_t* app;

    app = _get_app_from_proc(proc);

    if (!app)
        return nullptr;

    sym = loaded_app_find_symbol_by_addr(app, (void*)addr);

    if (!sym || !sym->name)
        return nullptr;

    return sym->name;
}

static vaddr_t __elf_dynamic_get_faddr(proc_t* proc, const char* symbol)
{
    loaded_app_t* app;
    loaded_sym_t* sym;

    app = _get_app_from_proc(proc);

    if (!app)
        return NULL;

    /* Try to find the symbol across the libraries and the functions in the app itself */
    sym = loaded_app_find_symbol(app, symbol);

    if (!sym || (sym->flags & LDSYM_FLAG_EXPORT) != LDSYM_FLAG_EXPORT)
        return NULL;

    /* Return the user address */
    return sym->uaddr;
}

static process_library_t* __elf_dynamic_load_lib(proc_t* proc, oss_object_t* object)
{
    error_t error;
    loaded_app_t* app;
    dynamic_library_t* library;

    app = _get_app_from_proc(proc);

    if (!app)
        return nullptr;

    error = load_dynamic_lib(object->key, app, &library);

    if (error)
        return nullptr;

    await_lib_init(library);

    /*
     * We can simply return the raw pointer to the dynamic library, since only we
     * know how to operate it anyway
     */
    return library;
}

aniva_exec_method_t dynamic_method = {
    .key = "dynamic_elf",
    .f_execute = __elf_dynamic_execute,
    .f_get_func_symbol = __elf_dynamic_get_fsymbol,
    .f_get_func_addr = __elf_dynamic_get_faddr,
    .f_load_lib = __elf_dynamic_load_lib,
};

void init_dynamic_elf_exec()
{
    _loaded_apps = init_list();
    _dynld_lock = create_mutex(NULL);

    kevent_add_hook("proc", "dyn_elf_proc_watcher", _dyn_loader_proc_hook, NULL);

    register_exec_method(&dynamic_method);
}
