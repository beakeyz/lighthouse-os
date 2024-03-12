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
  ret->exported_symbols = create_hashmap(0x1000, HASHMAP_FLAG_SK);
  ret->entry = (DYNAPP_ENTRY_t)ret->image.elf_hdr->e_entry;

  return ret;

dealloc_and_exit:

  if (ret->image.elf_dyntbl)
    __kmem_kernel_dealloc((vaddr_t)ret->image.elf_dyntbl, ret->image.elf_dyntbl_mapsize);

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
  app->proc = nullptr;

  kernel_panic("TODO: fix destroy_loaded_app");

  FOREACH(n, app->symbol_list) {
    loaded_sym_t* sym = n->data;

    kzfree(sym, sizeof(*sym));
  }

  /* FIXME: Murder all libraries in this app? */
  destroy_list(app->library_list);
  destroy_list(app->symbol_list);
  destroy_hashmap(app->exported_symbols);

  /* Deallocate the file buffer if we still have that */
  destroy_elf_image(&app->image);
}

void* proc_map_into_kernel(proc_t* proc, vaddr_t uaddr, size_t size)
{
  if (!proc)
    return nullptr;

  return (void*)Must(__kmem_kernel_alloc(
        /* Physical address */
        kmem_to_phys(
          proc->m_root_pd.m_root,
          uaddr
        ),
        /* Our size */
        size,
        NULL,
        /* Basic kernel flags */
        KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE
        )
  );
}

/* These values should get set before we copy and reset after we've copied */
extern uintptr_t __app_entrypoint;
extern uintptr_t __lib_entrypoints;
extern size_t    __lib_entrycount;

static uintptr_t allocate_lib_entrypoint_vec(loaded_app_t* app, uintptr_t* entrycount)
{
  size_t libcount;
  size_t lib_idx;
  size_t libarr_size;
  uintptr_t uaddr;
  node_t* c_liblist_node;
  DYNLIB_ENTRY_t c_entry;
  DYNLIB_ENTRY_t* kaddr;

  libcount = loaded_app_get_lib_count(app);
  libarr_size = ALIGN_UP(libcount * sizeof(uintptr_t), SMALL_PAGE_SIZE);

  *entrycount = libcount;

  if (!libcount)
    return NULL;

  uaddr = Must(kmem_user_alloc_range(app->proc, libarr_size, NULL, NULL));
  kaddr = proc_map_into_kernel(app->proc, uaddr, libarr_size);

  lib_idx = NULL;
  c_liblist_node = app->library_list->head;

  /* Cycle through the libraries */
  do {
    /* Get the entry */
    c_entry = ((dynamic_library_t*)c_liblist_node->data)->entry;

    /* Only add if this library has an entry */
    if (c_entry)
      kaddr[lib_idx++] = c_entry;
    
    c_liblist_node = c_liblist_node->next;
  } while (c_liblist_node);

  /* Weird info lmao */
  printf("Got buffer at 0x%llx<->0x%p with %lld libs\n", uaddr, kaddr, libcount);
  printf("First library to load is %s at 0x%p\n", ((dynamic_library_t*)app->library_list->head->data)->name, kaddr[0]);

  /* Don't need the kernel address anymore */
  kmem_unmap_range(nullptr, (vaddr_t)kaddr, GET_PAGECOUNT((vaddr_t)kaddr, libarr_size));

  return uaddr;
}

/*!
 * @brief: Copy the app entry trampoline code into the apps userspace
 *
 * Also sets the processes entry to the newly allocated address
 */
kerror_t loaded_app_set_entry_tramp(loaded_app_t* app)
{
  proc_t* proc;
  vaddr_t trampoline_uaddr;
  paddr_t trampoline_paddr;
  vaddr_t trampoline_kaddr;

  /* The trampoline has a memory limit of one page (Idk why but it does lmao) */
  if (((uint64_t)&_app_trampoline_end - (uint64_t)&_app_trampoline) > SMALL_PAGE_SIZE)
    return -KERR_MEM;

  /* This would be fucked */
  if ((vaddr_t)&_app_trampoline % SMALL_PAGE_SIZE != 0)
    return -KERR_MEM;

  proc = app->proc;

  /* Doing the allocations this way ensures the kernel always knows where the memory is located */
  trampoline_kaddr = Must(__kmem_kernel_alloc_range(SMALL_PAGE_SIZE, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));
  trampoline_paddr = kmem_to_phys(nullptr, trampoline_kaddr);
  trampoline_uaddr = Must(kmem_user_alloc(proc, trampoline_paddr, SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_READONLY, NULL));

  __app_entrypoint = (uintptr_t)app->entry;
  __lib_entrypoints = allocate_lib_entrypoint_vec(app, &__lib_entrycount);

  /* Copy the entire page */
  memcpy((void*)trampoline_kaddr, &_app_trampoline, SMALL_PAGE_SIZE);

  /* Don't need the kernel address anymore */
  kmem_unmap_page(nullptr, trampoline_kaddr);

  __app_entrypoint = NULL;
  __lib_entrypoints = NULL;
  __lib_entrycount = NULL;

  printf("Real entry addr=0x%llx\n", trampoline_uaddr);

  /* FIXME: args? */
  proc_set_entry(proc, (FuncPtr)trampoline_uaddr, NULL, NULL);
  return 0;
}
