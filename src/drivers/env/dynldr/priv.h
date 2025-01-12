#ifndef __ANIVA_DYN_LOADER_PRIV__
#define __ANIVA_DYN_LOADER_PRIV__

#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/dynldr.h"
#include "lightos/lightos.h"
#include "proc/core.h"
#include <fs/file.h>
#include <libk/stddef.h>

struct proc;
struct loaded_app;
struct loaded_sym;

extern void __lib_trampoline(f_libinit linit);

typedef struct elf_image {
    struct proc* proc;

    /* ELF header/object stuff */
    struct elf64_hdr* elf_hdr;
    struct elf64_phdr* elf_phdrs;
    struct elf64_dyn* elf_dyntbl;

    size_t elf_dyntbl_mapsize;
    size_t elf_dynsym_tblsz;
    struct elf64_shdr* elf_symtbl_hdr;
    struct elf64_shdr* elf_strtbl_hdr;
    struct elf64_shdr* elf_lightentry_hdr;
    struct elf64_shdr* elf_lightexit_hdr;
    struct elf64_sym* elf_dynsym;

    const char* elf_shstrtab;
    const char* elf_dynstrtab;

    /* Size of the kerne-allocated buffer of the file */
    size_t kernel_image_size;
    void* kernel_image;
    /* Size of the buffer allocated inside the user memoryspace */
    size_t user_image_size;
    void* user_base;
} elf_image_t;

extern kerror_t load_elf_image(elf_image_t* image, struct proc* proc, file_t* file);
extern void destroy_elf_image(elf_image_t* image);

/*
 * A dynamic library that has been loaded as support for a dynamic app
 */
typedef struct dynamic_library {
    const char* name;
    const char* path;

    elf_image_t image;

    /* Reference the loaded app to get access to environment info */
    struct loaded_app* app;
    hashmap_t* symbol_map;
    list_t* symbol_list;
    thread_t* lib_wait_thread;
    thread_t* lib_init_thread;
    f_libinit entry;
    f_libexit exit;
} dynamic_library_t;

extern kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app, dynamic_library_t** blib);
extern kerror_t load_dependant_lib(const char* path, dynamic_library_t* library);
extern kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app);
extern kerror_t unload_dynamic_lib(dynamic_library_t* lib);

extern kerror_t await_lib_init(dynamic_library_t* lib);

/*
 * A single loaded app
 *
 * A dynamically linked app which we have loaded through this driver.
 */
typedef struct loaded_app {
    /* ELF stuff */
    elf_image_t image;

    f_light_entry entry;
    FuncPtr exit;

    /* Symbol name for a custom entry (TODO: implement) */
    const char* custom_entry_sym;

    hashmap_t* exported_symbols;
    /* This list contains all the symbols we find throughout the load process of a single app.
     * The list also owns every single loaded_sym object. */
    list_t* symbol_list;
    /* Unordered list of libraries */
    list_t* unordered_liblist;
    /* List of libraries in the order that they where loaded */
    list_t* ordered_liblist;
} loaded_app_t;

static inline struct proc* loaded_app_get_proc(loaded_app_t* app)
{
    return app->image.proc;
}

extern loaded_app_t* create_loaded_app(file_t* file, struct proc* proc);
extern void destroy_loaded_app(loaded_app_t* app);

extern kerror_t load_app(file_t* file, loaded_app_t** out_app, struct proc** proc);
extern kerror_t unload_app(loaded_app_t* app);

extern kerror_t register_app(loaded_app_t* app);
extern kerror_t unregister_app(loaded_app_t* app);

static inline uint32_t loaded_app_get_lib_count(loaded_app_t* app)
{
    return app->unordered_liblist->m_length;
}

extern struct loaded_sym* loaded_app_find_symbol(loaded_app_t* app, const char* symname);
extern struct loaded_sym* loaded_app_find_symbol_in_app(loaded_app_t* app, const char* symname);
extern struct loaded_sym* loaded_app_find_symbol_by_addr(loaded_app_t* app, void* addr);

extern kerror_t loaded_app_set_entry_tramp(loaded_app_t* app);

extern kerror_t _elf_load_phdrs(elf_image_t* image);
extern kerror_t _elf_do_headers(elf_image_t* image);
extern kerror_t _elf_do_relocations(elf_image_t* image, loaded_app_t* app);
/*
 * This is the only step that is dependent on high level dynldr abstractions =/
 * We are currently forced to pass this crap (@app) because this function might chainload
 * other libraries and it needs to know what app these libraries are going to be a part of lmao
 *
 * FIXME: Make this independant of this crap?
 */
extern kerror_t _elf_load_dyn_sections(elf_image_t* image, loaded_app_t* app);
extern kerror_t _elf_do_symbols(list_t* symbol_list, hashmap_t* exported_symbol_map, loaded_app_t* app, elf_image_t* image);

#define LDSYM_FLAG_IS_CPY 0x00000001
#define LDSYM_FLAG_EXPORT 0x00000002

/*
 * A single symbol that we have loaded in a context
 */
typedef struct loaded_sym {
    const char* name;
    vaddr_t uaddr;
    vaddr_t offset;
    uint32_t usecount;
    uint32_t flags;
} loaded_sym_t;

#endif // !__ANIVA_DYN_LOADER_PRIV__
