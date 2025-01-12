#include "fs/file.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "lightos/lightos.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "priv.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include <libk/string.h>

loaded_app_t* create_loaded_app(file_t* file, proc_t* proc)
{
    loaded_app_t* ret;

    if (!file)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    /* Allocate the entire file (temporarily), so we can use section scanning routines */
    if (!KERR_OK(load_elf_image(&ret->image, proc, file)))
        goto dealloc_and_exit;

    ret->unordered_liblist = init_list();
    ret->ordered_liblist = init_list();
    ret->symbol_list = init_list();
    /* Variable size hashmap for the exported symbols */
    ret->exported_symbols = create_hashmap(1 * Mib, HASHMAP_FLAG_SK);
    ret->entry = NULL;

    return ret;

dealloc_and_exit:
    kfree(ret);
    return nullptr;
}

/*!
 * @brief: Free the memory of @app
 *
 * NOTE: a loaded app has no ownership over ->proc
 */
void destroy_loaded_app(loaded_app_t* app)
{
    /* Murder all libraries in this app */
    FOREACH(i, app->unordered_liblist)
    {
        dynamic_library_t* lib = i->data;

        unload_dynamic_lib(lib);
    }

    /* Deallocate the file buffer if we still have that */
    destroy_elf_image(&app->image);

    FOREACH(n, app->symbol_list)
    {
        loaded_sym_t* sym = n->data;

        kzfree(sym, sizeof(*sym));
    }

    destroy_list(app->unordered_liblist);
    destroy_list(app->ordered_liblist);
    destroy_list(app->symbol_list);
    destroy_hashmap(app->exported_symbols);

    kfree(app);
}

static inline error_t lightos_ctx_bind_proc_handle(lightos_appctx_t* ctx, loaded_app_t* app)
{
    khandle_t handle;

    if (!app || !app->image.proc)
        return -EINVAL;

    /* Initialize the handle */
    init_khandle_ex(&handle, HNDL_TYPE_PROC, HNDL_FLAG_R, app->image.proc);

    /* Bind the thing */
    return bind_khandle(&app->image.proc->m_handle_map, &handle, (u32*)&ctx->self);
}

static inline error_t lightos_ctx_find_main(lightos_appctx_t* ctx, loaded_app_t* app)
{
    loaded_sym_t* sym;
    u32 idx;
    const char* possible_entry_symbols[] = {
        "main", // For legacy applications
        "lentry", // ???
        "start", // just idk
        "light_entry",
        app->custom_entry_sym,
        NULL,
    };

    /* Start at zero (duh) */
    idx = 0;

    while (possible_entry_symbols[idx]) {
        KLOG_DBG("Trying to find entry symbol: %s\n", possible_entry_symbols[idx]);
        /* Make sure we only look inside the symbols of the app itself */
        sym = loaded_app_find_symbol_in_app(app, possible_entry_symbols[idx]);

        if (sym)
            break;

        idx++;
    }

    if (!sym)
        return -ENOENT;

    KLOG_DBG("found entry symbol: %s (addr=0x%llx)\n", possible_entry_symbols[idx], sym->uaddr);

    /* Set the entry to the first symbol we find matching this shit */
    ctx->entry = (f_light_entry)sym->uaddr;

    return 0;
}

static inline error_t lightos_ctx_gather_libinit_vec(lightos_appctx_t* ctx, loaded_app_t* app)
{
    f_libinit c_entry;

    /* Cycle through the libraries in reverse */
    FOREACH(i, app->ordered_liblist)
    {
        /* Get the entry */
        c_entry = ((dynamic_library_t*)i->data)->entry;

        KLOG_DBG("Adding entry 0x%p from library %s\n", c_entry, ((dynamic_library_t*)i->data)->name);

        /* Only add if this library has an entry */
        if (c_entry)
            ctx->init_vec[ctx->nr_libentries++] = c_entry;
    }

    return 0;
}

static error_t allocate_lightos_appctx(loaded_app_t* app, lightos_appctx_t** p_ctx)
{
    error_t error;
    size_t libcount;
    size_t lightctx_size;
    lightos_appctx_t* ctx = NULL;

    if (!p_ctx)
        return -EINVAL;

    /* Count the libraries */
    libcount = loaded_app_get_lib_count(app);
    /* Calculate how many pages we need for this shit */
    lightctx_size = ALIGN_UP(sizeof(lightos_appctx_t) + libcount * sizeof(uintptr_t), SMALL_PAGE_SIZE);

    /* Allocate a bit of space here */
    error = kmem_user_alloc_range((void**)&ctx, app->image.proc, lightctx_size, NULL, KMEM_FLAG_WRITABLE);

    if (error)
        return error;

    /* What */
    if (!ctx)
        return -ENULL;

    /* Zero the buffer */
    memset(ctx, 0, lightctx_size);

    /* Gather the init vector for the libraries */
    error = lightos_ctx_gather_libinit_vec(ctx, app);

    if (error)
        return error;

    /* Set the entry */
    error = lightos_ctx_find_main(ctx, app);

    if (error)
        return error;

    /* Try to bind a handle to the process here */
    error = lightos_ctx_bind_proc_handle(ctx, app);

    if (error)
        return error;

    /* Export the pointer to the app context */
    *p_ctx = ctx;

    return 0;
}

loaded_sym_t* loaded_app_find_symbol_by_addr(loaded_app_t* app, void* addr)
{
    loaded_sym_t* c_sym;
    loaded_sym_t* ret;
    dynamic_library_t* c_lib;

    c_sym = ret = nullptr;

    /* Address inside of the app image? */
    if (addr >= app->image.user_base && addr < (app->image.user_base + app->image.user_image_size)) {

        /* Address is inside the apps image, scan the symbol list */
        FOREACH(j, app->symbol_list)
        {
            c_sym = j->data;

            /* Address can't be part of this symbol */
            if ((void*)c_sym->uaddr > addr)
                continue;

            if (c_sym->uaddr < (!ret ? 0 : ret->uaddr))
                continue;

            ret = c_sym;
        }

        return ret;
    }

    FOREACH(i, app->unordered_liblist)
    {
        c_lib = i->data;

        /* Address is outside of the library image =/ */
        if (addr < c_lib->image.user_base || addr > (c_lib->image.user_base + c_lib->image.user_image_size))
            continue;

        FOREACH(j, c_lib->symbol_list)
        {
            c_sym = j->data;

            /* Address can't be part of this symbol */
            if ((void*)c_sym->uaddr > addr)
                continue;

            if (c_sym->uaddr < (!ret ? 0 : ret->uaddr))
                continue;

            ret = c_sym;
        }

        return ret;
    }

    return nullptr;
}

loaded_sym_t* loaded_app_find_symbol_in_app(loaded_app_t* app, const char* symname)
{
    return hashmap_get(app->exported_symbols, (hashmap_key_t)symname);
}

loaded_sym_t* loaded_app_find_symbol(loaded_app_t* app, const char* symname)
{
    loaded_sym_t* ret;
    dynamic_library_t* c_lib;

    FOREACH(i, app->unordered_liblist)
    {
        c_lib = i->data;

        ret = hashmap_get(c_lib->symbol_map, (hashmap_key_t)symname);

        if (ret)
            return ret;
    }

    return loaded_app_find_symbol_in_app(app, symname);
}

/*!
 * @brief: Copy the app entry trampoline code into the apps userspace
 *
 * Also sets the processes entry to the newly allocated address
 */
kerror_t loaded_app_set_entry_tramp(loaded_app_t* app)
{
    error_t error;
    proc_t* proc;
    /* Context struct we need to build */
    lightos_appctx_t* ctx;

    /* Set the target process */
    proc = loaded_app_get_proc(app);

    /* Allocate a context block for this app */
    error = allocate_lightos_appctx(app, &ctx);

    /* TODO: Error handling */
    ASSERT(!error);

    /* FIXME: args? */
    proc_set_entry(proc, (FuncPtr)app->entry, (u64)ctx, NULL);

    return 0;
}
