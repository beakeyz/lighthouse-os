#include "fs/file.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/lightos.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "mem/page_dir.h"
#include "mem/zalloc/zalloc.h"
#include "priv.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include <oss/obj.h>

/*
 * Load procedures for the dynamic loader
 *
 * How to create a local API that can be scalable
 * We want to load both apps and libraries the same way, but they are handled internally differently. For example,
 * the order in which the things are actually loaded is different, but how it's done under the hood is the same. Our
 * internal API should reflect this. Routines to do with handling ELF components, like relocating, resolving symbols, ect.
 * should be independent of wether the ELF stuff is part of the loaded app or simply a supporting library.
 */

static dynamic_library_t* _create_dynamic_lib(loaded_app_t* parent, const char* name, const char* path)
{
    dynamic_library_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->app = parent;
    ret->name = strdup(name);
    ret->path = strdup(path);
    ret->symbol_map = create_hashmap(0x1000, HASHMAP_FLAG_SK);
    ret->symbol_list = init_list();

    return ret;
}

static void _destroy_dynamic_lib(dynamic_library_t* lib)
{
    destroy_elf_image(&lib->image);

    FOREACH(n, lib->symbol_list)
    {
        loaded_sym_t* sym = n->data;

        kzfree(sym, sizeof(*sym));
    }

    destroy_hashmap(lib->symbol_map);
    destroy_list(lib->symbol_list);

    kfree((void*)lib->name);
    kfree((void*)lib->path);
    kfree(lib);
}

/*!
 * @brief: Load the complete image of a dynamic library
 *
 * NOTE: This may be called from some process A as an affector on some other process B. This 
 * means we need to temporarily switch to the addresspace of process B in order to load this library
 */
static kerror_t _dynlib_load_image(file_t* file, dynamic_library_t* lib)
{
    proc_t* c_proc, *app_proc;
    page_dir_t prev_pdir;
    kerror_t error;

    /* Grab the current process */
    c_proc = get_current_proc();
    app_proc = loaded_app_get_proc(lib->app);

    /* This would be really weird */
    if (!app_proc)
        return -EINVAL;

    /* Cache the current addresspace */
    kmem_get_addrspace(&prev_pdir);

    /* Switch to the addresspace of our target */
    kmem_set_addrspace_ex(&app_proc->m_root_pd, c_proc);

    /* Bring the image into memory */
    error = load_elf_image(&lib->image, app_proc, file);

    if (!KERR_OK(error))
        goto switch_back_and_exit;

    error = _elf_load_phdrs(&lib->image);

    if (!KERR_OK(error))
        goto switch_back_and_exit;

    error = _elf_do_headers(&lib->image);

    if (!KERR_OK(error))
        goto switch_back_and_exit;

    /* Might load more libraries */
    error = _elf_load_dyn_sections(&lib->image, lib->app);

    if (!KERR_OK(error))
        goto switch_back_and_exit;

    error = _elf_do_symbols(lib->symbol_list, lib->symbol_map, lib->app, &lib->image);

    if (!KERR_OK(error))
        goto switch_back_and_exit;

    error = _elf_do_relocations(&lib->image, lib->app);

switch_back_and_exit:
    kmem_set_addrspace_ex(&prev_pdir, c_proc);

    return error;

}

static inline const char* _append_path_to_searchdir(const char* search_dir, const char* path)
{
    char* search_path;
    size_t search_path_len;

    search_path_len = strlen(search_dir) + 1 + strlen(path) + 1;
    search_path = kmalloc(search_path_len);

    if (!search_path)
        return nullptr;

    /* Clear */
    memset(search_path, 0, search_path_len);

    /* Copy search dir */
    strncpy(search_path, search_dir, strlen(search_dir));
    /* Set the path seperator */
    search_path[strlen(search_dir)] = '/';
    /* Copy the lib path */
    strncpy(&search_path[strlen(search_dir) + 1], path, strlen(path));

    return search_path;
}

static inline bool app_check_lib(loaded_app_t* app, const char* path)
{
    size_t pathlen;
    dynamic_library_t* lib;

    /* Grab the paths length */
    pathlen = strlen(path);

    /* Apps never have libs with empty paths */
    if (!pathlen)
        return false;

    /* Loop over all libraries in the app */
    FOREACH(i, app->unordered_liblist)
    {
        lib = i->data;

        /* Check if this library has a matching name */
        if (strncmp(path, lib->name, pathlen) == 0)
            return true;
    }

    return false;
}

/*!
 * @brief: Load a dynamic library from a file into a target app
 *
 * Copies the image into the apps address space and resolves the symbols, after
 * we've loaded the dependencies for this library
 */
kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app, dynamic_library_t** blib)
{
    kerror_t error;
    const char* search_path;
    const char* search_dir;
    file_t* lib_file;
    dynamic_library_t* lib;
    sysvar_t* libs_var;

    //KLOG_DBG("Does app have lib...\n");

    if (app_check_lib(target_app, path))
        return 0;

    profile_find_var(LIBSPATH_VARPATH, &libs_var);
    sysvar_get_str_value(libs_var, &search_dir);

    KLOG_DBG("Trying to find search dir...\n");

    lib = nullptr;
    search_path = _append_path_to_searchdir(search_dir, path);

    if (!search_path)
        return -KERR_NOMEM;

    KLOG_DBG("Creating lib structure...\n");

    /* First, try to create a library object */
    lib = _create_dynamic_lib(target_app, path, search_path);

    kfree((void*)search_path);

    if (!lib)
        return -KERR_INVAL;

    KLOG_DBG("Adding and opening %s...\n", lib->path);

    /* Register ourselves preemptively to the app */
    list_append(target_app->unordered_liblist, lib);

    /* Then try to open the target file */
    lib_file = file_open(lib->path);

    if (!lib_file)
        goto dealloc_and_exit;

    KLOG_DBG("Trying to load dynamic library %s\n", lib_file->m_obj->name);

    /* Get the image loaded */
    error = _dynlib_load_image(lib_file, lib);

    file_close(lib_file);

    if (error)
        goto dealloc_and_exit;

    /* Register ourselves preemptively to the app */
    list_append(target_app->ordered_liblist, lib);

    /* Set the entry */
    if (lib->image.elf_lightentry_hdr)
        lib->entry = (f_libinit)lib->image.elf_lightentry_hdr->sh_addr;

    if (lib->image.elf_lightexit_hdr)
        lib->exit = (f_libexit)lib->image.elf_lightexit_hdr->sh_addr;

    if (blib)
        *blib = lib;

    KLOG_DBG("Successfully loaded %s (entry=0x%p)\n", lib->name, lib->entry);
    return 0;
dealloc_and_exit:
    list_remove_ex(target_app->unordered_liblist, lib);

    if (lib)
        _destroy_dynamic_lib(lib);

    return -KERR_INVAL;
}

kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app)
{
    kernel_panic("TODO: reload_dynamic_lib");
}

kerror_t unload_dynamic_lib(dynamic_library_t* lib)
{
    _destroy_dynamic_lib(lib);
    return 0;
}

static inline kerror_t load_app_generic_headers(file_t* file, loaded_app_t* app)
{
    //KLOG_DBG(" ==> Program headers...\n");
    if (_elf_load_phdrs(&app->image))
        return -KERR_INVAL;

    //KLOG_DBG(" ==> Processing program headers...\n");

    if (_elf_do_headers(&app->image))
        return -KERR_INVAL;

    return 0;
}

static inline kerror_t load_app_dyn_sections(loaded_app_t* app)
{
    //KLOG_DBG(" ==> Dyn sections...\n");
    if (_elf_load_dyn_sections(&app->image, app))
        return -KERR_INVAL;

    //KLOG_DBG(" ==> Symbols...\n");
    if (_elf_do_symbols(app->symbol_list, app->exported_symbols, app, &app->image))
        return -KERR_INVAL;

    //KLOG_DBG(" ==> Relocations...\n");
    if (_elf_do_relocations(&app->image, app))
        return -KERR_INVAL;

    return 0;
}

/*!
 * @brief: Clean up temporary data generated by the load and make sure the app is ready to start
 *
 * This means we do the nessecerry destructing, give the process it's entry trampoline
 */
static inline void _finalise_load(loaded_app_t* app)
{
    app->entry = (f_light_entry)((vaddr_t)app->image.elf_hdr->e_entry + (vaddr_t)app->image.user_base);

    loaded_app_set_entry_tramp(app);
}

static kerror_t _do_load(file_t* file, loaded_app_t* app)
{
    //KLOG_DBG("Generic headers...\n");
    /*
     * Read the program headers to find basic binary info about the
     * app we are currently trying to load
     */
    if (load_app_generic_headers(file, app))
        return -KERR_INVAL;

    //KLOG_DBG("Dyn sections...\n");
    /*
     * Load the dynamic sections that are included in the binary we are trying to
     * load. Here we also cache any libraries and libraries of the libraries, ect.
     */
    if (load_app_dyn_sections(app))
        return -KERR_INVAL;

    //KLOG_DBG("Finalise...\n");

    /* Clean up our mess */
    _finalise_load(app);
    return KERR_NONE;
}

/*!
 * @brief: Load a singular dynamically linked executable
 *
 * This will result in a new process wrapped inside a loaded_app struct when
 * the returnvalue is KERR_NONE
 *
 * When loading an app, we will temporarily switch into the addresspace of the process
 * we're trying to create, as to make our lives with memory handling much easier
 */
kerror_t load_app(file_t* file, loaded_app_t** out_app, proc_t** p_proc)
{
    kerror_t error;
    proc_t* proc;
    proc_t* parent_proc;
    page_dir_t prev_pdir;
    loaded_app_t* app;

    if (!out_app || !file)
        return -KERR_INVAL;

    *out_app = NULL;

    /* Cache the previous addresspace */
    kmem_get_addrspace(&prev_pdir);

    /*
     * Grab the current process as a parent
     * Any lucky processes that get executed by the kernel will get the kernel process as their parent.
     * This doe not mean they get to inherit any of it's privileges though =)
     */
    parent_proc = get_current_proc();

    /* Create an addressspsace for this bitch */
    proc = create_proc(is_kernel_proc(parent_proc) ? nullptr : parent_proc, nullptr, (char*)file->m_obj->name, NULL, NULL, NULL);

    if (!proc)
        return -KERR_NULL;

    /* This gathers the needed ELF info */
    app = create_loaded_app(file, proc);

    /* Assume no memory */
    if (!app)
        return -KERR_NOMEM;

    /* Switch the addresspace of the parent process */
    kmem_set_addrspace_ex(&proc->m_root_pd, parent_proc);

    /*
     * We've loaded the main object. Now, let's:
     * 0) Find a place to actually load this shit :clown:
     * 1) Relocate the static parts of the binary
     * 2) Find the libraries we need to load and load them in the correct order
     * 3) Execute the process
     */

    /* Do the load */
    error = _do_load(file, app);

    /* Reset the addresspace of the parent process */
    kmem_set_addrspace_ex(&prev_pdir, parent_proc);

    /* TODO: Do actual cleanup here */
    ASSERT_MSG(KERR_OK(error), "FUCK: Failed to do a dynamic app load =(");

    *p_proc = proc;
    *out_app = app;
    return error;
}

kerror_t unload_app(loaded_app_t* app)
{
    kernel_panic("TODO: unload_app");
}
