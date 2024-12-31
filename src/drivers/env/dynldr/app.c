#include "fs/file.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "priv.h"
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

    ret->proc = proc;
    ret->unordered_liblist = init_list();
    ret->ordered_liblist = init_list();
    ret->symbol_list = init_list();
    /* Variable size hashmap for the exported symbols */
    ret->exported_symbols = create_hashmap(1 * Mib, HASHMAP_FLAG_SK);
    ret->entry = (DYNAPP_ENTRY_t)ret->image.elf_hdr->e_entry;

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

    app->proc = nullptr;

    kfree(app);
}

void* proc_map_into_kernel(proc_t* proc, vaddr_t uaddr, size_t size)
{
    paddr_t pbase;

    if (!proc)
        return nullptr;

    pbase = kmem_to_phys_aligned(
        proc->m_root_pd.m_root,
        uaddr);

    if (!pbase)
        return nullptr;

    ASSERT(kmem_map_range(
                nullptr,
                kmem_from_phys(pbase, KERNEL_MAP_BASE),
                pbase,
                GET_PAGECOUNT(uaddr, size),
                KMEM_CUSTOMFLAG_GET_MAKE,
                KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

    return (void*)(kmem_from_phys(pbase, KERNEL_MAP_BASE) + (uaddr - ALIGN_DOWN(uaddr, SMALL_PAGE_SIZE)));
}

static uintptr_t allocate_lib_entrypoint_vec(loaded_app_t* app, uintptr_t* entrycount)
{
    size_t libcount;
    size_t lib_idx;
    size_t libarr_size;
    uintptr_t uaddr;
    node_t* c_liblist_node;
    DYNLIB_ENTRY_t c_entry;
    DYNLIB_ENTRY_t* kaddr;

    *entrycount = NULL;
    libcount = loaded_app_get_lib_count(app);
    libarr_size = ALIGN_UP(libcount * sizeof(uintptr_t), SMALL_PAGE_SIZE);

    if (!libcount)
        return NULL;

    /* This has to be readonly */
    ASSERT(!kmem_user_alloc_range((void**)&uaddr, app->proc, libarr_size, NULL, NULL));
    kaddr = proc_map_into_kernel(app->proc, uaddr, libarr_size);

    if (!kaddr)
        return NULL;

    memset(kaddr, 0, libarr_size);

    lib_idx = NULL;
    c_liblist_node = app->ordered_liblist->head;

    /* Cycle through the libraries in reverse */
    while (c_liblist_node) {
        /* Get the entry */
        c_entry = ((dynamic_library_t*)c_liblist_node->data)->entry;

        KLOG_DBG("Adding entry 0x%p from library %s\n", c_entry, ((dynamic_library_t*)c_liblist_node->data)->name);

        /* Only add if this library has an entry */
        if (c_entry)
            kaddr[lib_idx++] = c_entry;

        c_liblist_node = c_liblist_node->next;
    }

    /* NOTE: at this point lib_idx will represent the amount of libraries that have entries we need to call */
    *entrycount = lib_idx;

    /* Don't need the kernel address anymore */
    kmem_unmap_range(nullptr, (vaddr_t)kaddr, GET_PAGECOUNT((vaddr_t)kaddr, libarr_size));

    return uaddr;
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

    return hashmap_get(app->exported_symbols, (hashmap_key_t)symname);
}

/*!
 * @brief: Dirty routine to get the hardcoded symbols we need to install the app trampoline
 */
static inline kerror_t _get_librt_symbols(dynamic_library_t* librt, loaded_sym_t** appentry, loaded_sym_t** libentries, loaded_sym_t** libcount, loaded_sym_t** apptramp, loaded_sym_t** quick_exit)
{
    *appentry = (loaded_sym_t*)hashmap_get(librt->symbol_map, RUNTIMELIB_APP_ENTRY_SYMNAME);

    if (!(*appentry))
        return -KERR_NULL;

    *libentries = (loaded_sym_t*)hashmap_get(librt->symbol_map, RUNTIMELIB_LIB_ENTRIES_SYMNAME);

    if (!(*libentries))
        return -KERR_NULL;

    *libcount = (loaded_sym_t*)hashmap_get(librt->symbol_map, RUNTIMELIB_LIB_ENTRYCOUNT_SYMNAME);

    if (!(*libcount))
        return -KERR_NULL;

    *apptramp = (loaded_sym_t*)hashmap_get(librt->symbol_map, RUNTIMELIB_APP_TRAMP_SYMNAME);

    if (!(*apptramp))
        return -KERR_NULL;

    *quick_exit = (loaded_sym_t*)hashmap_get(librt->symbol_map, RUNTIMELIB_QUICK_EXIT_SYMNAME);

    if (!(*quick_exit))
        return -KERR_NULL;

    return 0;
}

/*!
 * @brief: Copy the app entry trampoline code into the apps userspace
 *
 * Also sets the processes entry to the newly allocated address
 */
kerror_t loaded_app_set_entry_tramp(loaded_app_t* app)
{
    /* Kernel addresses */
    uint64_t* app_entrypoint_kaddr;
    uint64_t* lib_entrypoints_kaddr;
    uint64_t* lib_entrycount_kaddr;
    /* Internal symbols of the app */
    loaded_sym_t* app_entrypoint_sym;
    loaded_sym_t* lib_entrypoints_sym;
    loaded_sym_t* lib_entrycount_sym;
    loaded_sym_t* tramp_start_sym;
    loaded_sym_t* quick_exit_sym;
    proc_t* proc;
    dynamic_library_t* librt;

    proc = app->proc;

    /*
     * Try to load the runtime library for this app
     * FIXME: Did we want the name of the runtime lib inside a profile variable? Hardcoding
     * the name of the library here seems kind of fucked...
     */
    if (!KERR_OK(load_dynamic_lib(RUNTIMELIB_NAME, app, &librt)))
        return -KERR_INVAL;

    /* Get the symbols we need to prepare the trampoline */
    if (!KERR_OK(_get_librt_symbols(librt,
            &app_entrypoint_sym,
            &lib_entrypoints_sym,
            &lib_entrycount_sym,
            &tramp_start_sym,
            &quick_exit_sym))) {
        return -KERR_INVAL;
    }

    /* We got the symbols, let's find map them into the kernel and place the correct values */
    app_entrypoint_kaddr = proc_map_into_kernel(proc, app_entrypoint_sym->uaddr, sizeof(uint64_t));
    lib_entrypoints_kaddr = proc_map_into_kernel(proc, lib_entrypoints_sym->uaddr, sizeof(uint64_t));
    lib_entrycount_kaddr = proc_map_into_kernel(proc, lib_entrycount_sym->uaddr, sizeof(uint64_t));

    *app_entrypoint_kaddr = (uint64_t)app->entry;
    *lib_entrypoints_kaddr = allocate_lib_entrypoint_vec(app, lib_entrycount_kaddr);

    /* Unmap from the kernel (These addresses might just all be in the same page (highly likely)) */
    kmem_unmap_page(nullptr, (vaddr_t)app_entrypoint_kaddr);
    kmem_unmap_page(nullptr, (vaddr_t)lib_entrypoints_kaddr);
    kmem_unmap_page(nullptr, (vaddr_t)lib_entrycount_kaddr);

    /* Set the exit on the app */
    app->exit = (FuncPtr)quick_exit_sym->uaddr;

    /* FIXME: args? */
    proc_set_entry(proc, (FuncPtr)tramp_start_sym->uaddr, NULL, NULL);

    return 0;
}
