#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "priv.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"
#include "sched/scheduler.h"
#include <oss/obj.h>

/*
 * Load procedures for the dynamic loader
 *
 * How to create a local API that can be scalable
 * We want to load both apps and libraries the same way, but they are handled internally differently. For example,
 * the order in which the things are actually loaded is different, but how it's done under the hood is the same. Our
 * internal API should reflect this. Routines to do with handling ELF components, like relocating, resolving symbols, ect.
 * should be independent of wether the ELF stuff is part of the loaded app or simply a supporting library.
 *
 * TODO: Should we make a elf_object struct that is shared between the loaded_app and dynamic_library structs?
 */

#define SYMFLAG_ORIG_APP 0x01
#define SYMFLAG_ORIG_LIB 0x02
#define SYMFLAG_EXPORTED 0x04

/*
 * Context of the current load state
 *
 * This struct keeps track of all the symbols we've already loaded and know the location of, both
 * in the kernel memory as in the processes memory. 
 * 
 * Never allocated on the heap
 */
typedef struct load_ctx {
  uint32_t cur_libcount;

  UNOWNED const char* appname;
  OWNED list_t* loaded_symbols;
  OWNED hashmap_t* exported_symbols;
} load_ctx_t;

void init_load_ctx(load_ctx_t* ctx)
{
  memset(ctx, 0, sizeof(*ctx));

  /*
   * Create a variable-size hashmap to store all the symbols we find exported.
   * There might be a fuck ton of symbols lmao
   */
  ctx->exported_symbols = create_hashmap(0x1000, HASHMAP_FLAG_SK);
  ctx->loaded_symbols = init_list();
}

void destroy_load_ctx(load_ctx_t* ctx)
{
  FOREACH(n, ctx->loaded_symbols) {
    loaded_sym_t* sym = n->data;

    kfree(sym);
  }

  destroy_list(ctx->loaded_symbols);
  destroy_hashmap(ctx->exported_symbols);
}

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
  ret->dependencies = init_list();

  return ret;
}

static void _destroy_dynamic_lib(dynamic_library_t* lib)
{
  if (lib->dyn_symbols)
    destroy_hashmap(lib->dyn_symbols);

  destroy_elf_image(&lib->image);

  destroy_list(lib->dependencies);
  kfree((void*)lib->name);
  kfree((void*)lib->path);
  kfree(lib);
}

/*!
 * @brief: Load the complete image of a dynamic library
 */
static kerror_t _dynlib_load_image(file_t* file, dynamic_library_t* lib)
{
  kerror_t error;

  printf("A\n");
  error = load_elf_image(&lib->image, lib->app->proc, file);

  if (!KERR_OK(error))
    return error;

  printf("B\n");
  error = _elf_load_phdrs(&lib->image);

  if (!KERR_OK(error))
    return error;

  return 0;
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
  strncpy(&search_path[strlen(search_dir)+1], path, strlen(path));

  return search_path;
}

/*!
 * @brief: Load a dynamic library from a file into a target app
 *
 * Copies the image into the apps address space and resolves the symbols, after
 * we've loaded the dependencies for this library
 */
kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app)
{
  kerror_t error;
  const char* search_path;
  const char* search_dir;
  file_t* lib_file;
  dynamic_library_t* lib;
  profile_var_t* libs_var;

  profile_scan_var(LIBSPATH_VARPATH, NULL, &libs_var);
  profile_var_get_str_value(libs_var, &search_dir);

  lib = nullptr;
  search_path = _append_path_to_searchdir(search_dir, path);

  if (!search_path)
    return -KERR_NOMEM;

  /* First, try to create a library object */
  lib = _create_dynamic_lib(target_app, path, search_path);

  kfree((void*)search_path);

  if (!lib)
    return -KERR_INVAL;

  /* Then try to open the target file */
  lib_file = file_open(lib->path);

  if (!lib_file)
    goto dealloc_and_exit;

  printf("Trying to load dynamic library %s\n", lib_file->m_obj->name);

  error = _dynlib_load_image(lib_file, lib);
  
  file_close(lib_file);

  if (error)
    goto dealloc_and_exit;

  /*
   * Now we do epic loading or some shit idk
   */
  kernel_panic("TODO: load_dynamic_lib");

dealloc_and_exit:
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
  kernel_panic("TODO: unload_dynamic_lib");
}

static kerror_t load_app_dyn_sections(loaded_app_t* app)
{
  kerror_t error;
  struct elf64_dyn* dyns_entry;

  /* Do the generic section load */
  if (!KERR_OK(_elf_load_dyn_sections(&app->image)))
    return -KERR_INVAL;

  /* Reset the itterator */
  dyns_entry = app->image.elf_dyntbl;

  while (dyns_entry->d_tag) {

    if (dyns_entry->d_tag != DT_NEEDED)
      goto cycle;

    /* Load the library */
    error = load_dynamic_lib((const char*)(app->image.elf_strtab + dyns_entry->d_un.d_val), app);

    if (!KERR_OK(error) && kerror_is_fatal(error))
      break;

cycle:
    dyns_entry++;
  }

  return error;
}

static kerror_t _load_phdrs(file_t* file, loaded_app_t* app)
{
  return _elf_load_phdrs(&app->image);
}

/*!
 * @brief: Clean up temporary data generated by the load
 */
static void _finalise_load(load_ctx_t* ctx)
{
  /* Murder the context */
  destroy_load_ctx(ctx);
}

static kerror_t _do_load(file_t* file, loaded_app_t* app)
{
  load_ctx_t ctx;

  /* Get a clean context to store load info */
  init_load_ctx(&ctx);

  /* 
   * Read the program headers to find basic binary info about the
   * app we are currently trying to load
   */
  if (_load_phdrs(file, app))
    return -KERR_INVAL;

  /*
   * Load the dynamic sections that are included in the binary we are trying to
   * load. Here we also cache any libraries and libraries of the libraries, ect.
   */
  if (load_app_dyn_sections(app))
    return -KERR_INVAL;

  /*
   * Do the actual chainload in the correct order to load all the libraries needed. This
   * includes doing their relocations
   */
  //_load_deps();

  /*
   * Do all the relocations in the binary of the loaded app
   */
  //_relocate_app();

  /* Clean up our mess */
  _finalise_load(&ctx);
  return KERR_NONE;
}

/*!
 * @brief: Load a singular dynamically linked executable
 *
 * This will result in a new process wrapped inside a loaded_app struct when 
 * the returnvalue is KERR_NONE
 */
kerror_t load_app(file_t* file, loaded_app_t** out_app)
{
  kerror_t error;
  proc_id_t pid;
  proc_t* proc;
  proc_t* cur_proc;
  loaded_app_t* app;

  if (!out_app || !file)
    return -KERR_INVAL;

  /* Create an addressspsace for this bitch */
  cur_proc = get_current_proc();
  proc = create_proc(cur_proc, &pid, (char*)file->m_obj->name, NULL, NULL, NULL);

  if (!proc)
    return -KERR_NOMEM;

  /* This gathers the needed ELF info */
  app = create_loaded_app(file, proc);

  /* Assume no memory */
  if (!app)
    return -KERR_NOMEM;

  /*
   * We've loaded the main object. Now, let's:
   * 0) Find a place to actually load this shit :clown:
   * 1) Relocate the static parts of the binary
   * 2) Find the libraries we need to load and load them in the correct order
   * 3) Execute the process
   */

  error = _do_load(file, app);

  /* Idk */
  ASSERT(KERR_OK(error));

//dealloc_and_exit:
  //kernel_panic("load_app failed!");
  return error;
}

kerror_t unload_app(loaded_app_t* app)
{
  kernel_panic("TODO: unload_app");
}