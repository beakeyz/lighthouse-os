#include "fs/file.h"
#include "libk/bin/elf.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "priv.h"
#include "proc/proc.h"

loaded_app_t* create_loaded_app(file_t* file)
{
  loaded_app_t* ret;

  if (!file)
    return nullptr;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  /*
   * We set this explicitly to NULL. Ideally we would like an app to have it's base at the start of the memory space
   * but this may not always work...
   */
  ret->image_base = NULL;
  /* Allocate the entire file (temporarily), so we can use section scanning routines */
  ret->file_size = file->m_total_size;
  ret->file_buffer = (void*)Must(__kmem_kernel_alloc_range(ret->file_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  /* Read the file */
  if (!file_read(file, ret->file_buffer, ret->file_size, 0))
    goto dealloc_and_exit;

  ret->elf_hdr = ret->file_buffer;
  ret->entry = (DYNAPP_ENTRY_t)ret->elf_hdr->e_entry;

  if (!elf_verify_header(ret->elf_hdr))
    goto dealloc_and_exit;

  ret->elf_phdrs = elf_load_phdrs_64(file, ret->elf_hdr);

  return ret;

dealloc_and_exit:

  if (ret->file_buffer)
    __kmem_kernel_dealloc((vaddr_t)ret->file_buffer, ret->file_size);

  if (ret->elf_dyntbl)
    __kmem_kernel_dealloc((vaddr_t)ret->elf_dyntbl, ret->elf_dyntbl_mapsize);

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

  /* FIXME: Murder all libraries in this app? */
  destroy_hashmap(app->lib_map);

  /* Deallocate the file buffer if we still have that */
  if (app->file_buffer)
    __kmem_kernel_dealloc((vaddr_t)app->file_buffer, app->file_size);

  kfree(app->elf_phdrs);
  kfree(app->proc);
}

void* loaded_app_map_into_kernel(loaded_app_t* app, vaddr_t uaddr, size_t size)
{
  if (!app || !app->proc)
    return nullptr;

  return (void*)Must(__kmem_kernel_alloc(
        /* Physical address */
        kmem_to_phys(
          app->proc->m_root_pd.m_root,
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
