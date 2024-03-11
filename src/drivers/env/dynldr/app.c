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

  /* Copy the entire page */
  memcpy((void*)trampoline_kaddr, &_app_trampoline, SMALL_PAGE_SIZE);

  /* Don't need the kernel address anymore */
  kmem_unmap_page(nullptr, trampoline_kaddr);

  /* FIXME: args? */
  proc_set_entry(proc, (FuncPtr)trampoline_uaddr, NULL, NULL);
  return 0;
}
