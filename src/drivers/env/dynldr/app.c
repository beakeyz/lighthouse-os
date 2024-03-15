#include "fs/file.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "priv.h"
#include "proc/proc.h"

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
  ret->library_list = init_list();
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
  FOREACH(i, app->library_list) {
    dynamic_library_t* lib = i->data;

    unload_dynamic_lib(lib);
  }

  /* Deallocate the file buffer if we still have that */
  destroy_elf_image(&app->image);

  FOREACH(n, app->symbol_list) {
    loaded_sym_t* sym = n->data;

    kzfree(sym, sizeof(*sym));
  }

  destroy_list(app->library_list);
  destroy_list(app->symbol_list);
  destroy_hashmap(app->exported_symbols);

  kfree(app);

  app->proc = nullptr;
}

void* proc_map_into_kernel(proc_t* proc, vaddr_t uaddr, size_t size)
{
  paddr_t paddr;

  if (!proc)
    return nullptr;

  paddr = kmem_to_phys(
    proc->m_root_pd.m_root,
    uaddr
  );

  if (!paddr)
    return nullptr;

  return (void*)Must(__kmem_kernel_alloc(
        /* Physical address */
        paddr,
        /* Our size */
        size,
        NULL,
        /* Basic kernel flags */
        KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE
        )
  );
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

  printf("A");

  if (!libcount)
    return NULL;

  uaddr = Must(kmem_user_alloc_range(app->proc, libarr_size, NULL, NULL));
  kaddr = proc_map_into_kernel(app->proc, uaddr, libarr_size);

  printf("B");
  if (!kaddr)
    return NULL;
  printf("C\n");

  memset(kaddr, 0, libarr_size);

  lib_idx = NULL;
  c_liblist_node = app->library_list->end;

  /* Cycle through the libraries in reverse */
  while (c_liblist_node) {
    /* Get the entry */
    c_entry = ((dynamic_library_t*)c_liblist_node->data)->entry;

    /* Only add if this library has an entry */
    if (c_entry)
      kaddr[lib_idx++] = c_entry;

    printf("Got entry: %p\n", c_entry);
    
    c_liblist_node = c_liblist_node->prev;
  }

  /* NOTE: at this point lib_idx will represent the amount of libraries that have entries we need to call */
  *entrycount = lib_idx;

  /* Don't need the kernel address anymore */
  kmem_unmap_range(nullptr, (vaddr_t)kaddr, GET_PAGECOUNT((vaddr_t)kaddr, libarr_size));

  return uaddr;
}

loaded_sym_t* loaded_app_find_symbol(loaded_app_t* app, const char* symname)
{
  loaded_sym_t* ret;
  dynamic_library_t* c_lib;

  FOREACH(i, app->library_list) {
    c_lib = i->data;

    ret = hashmap_get(c_lib->symbol_map, (hashmap_key_t)symname);

    if (ret) {
      //printf("Found %s at %llx in %s\n", symname, ret->uaddr, c_lib->name);
      return ret;
    }
  }

  return hashmap_get(app->exported_symbols, (hashmap_key_t)symname);
}

/*!
 * @brief: Dirty routine to get the hardcoded symbols we need to install the app trampoline
 */
static inline kerror_t _get_librt_symbols(dynamic_library_t* librt, loaded_sym_t** appentry, loaded_sym_t** libentries, loaded_sym_t** libcount, loaded_sym_t** apptramp)
{
  *appentry = (loaded_sym_t*)hashmap_get(librt->symbol_map, "__app_entrypoint");

  if (!(*appentry))
    return -KERR_INVAL;
  
  *libentries = (loaded_sym_t*)hashmap_get(librt->symbol_map, "__lib_entrypoints");

  if (!(*libentries))
    return -KERR_INVAL;

  *libcount = (loaded_sym_t*)hashmap_get(librt->symbol_map, "__lib_entrycount");

  if (!(*libcount))
    return -KERR_INVAL;

  *apptramp = (loaded_sym_t*)hashmap_get(librt->symbol_map, "___app_trampoline");

  if (!(*apptramp))
    return -KERR_INVAL;

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
  proc_t* proc;
  dynamic_library_t* librt;

  proc = app->proc;

  printf("A\n");

  /* Try to load the runtime library for this app */
  if (!KERR_OK(load_dynamic_lib("librt.slb", app, &librt)))
    return -KERR_INVAL;

  printf("B\n");

  /* Get the symbols we need to prepare the trampoline */
  if (!KERR_OK(_get_librt_symbols(librt, 
          &app_entrypoint_sym,
          &lib_entrypoints_sym,
          &lib_entrycount_sym,
          &tramp_start_sym
          ))) {
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

  /* FIXME: args? */
  proc_set_entry(proc, (FuncPtr)tramp_start_sym->uaddr, NULL, NULL);
  return 0;
}
