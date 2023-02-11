#include "base_allocator.h"
#include "kmalloc.h"
#include "kmem_manager.h"
#include "heap.h"

static ALWAYS_INLINE void* dummy_alloc(size_t size);
static ALWAYS_INLINE void dummy_dealloc(void* address);

generic_heap_t *initialize_generic_heap(PagingComplex_t* root_table, vaddr_t virtual_base, size_t initial_size, uintptr_t flags) {
  generic_heap_t *ret = kmalloc(sizeof(generic_heap_t));

  if (ret == nullptr) {
    return nullptr;
  }

  const size_t pages_needed = (initial_size + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;

  ret->f_allocate = (HEAP_ALLOCATE) dummy_alloc;
  ret->f_deallocate = (HEAP_DEALLOCATE) dummy_dealloc;

  ret->m_current_total_size = pages_needed * SMALL_PAGE_SIZE;
  ret->m_virtual_base = virtual_base;
  ret->m_flags = flags;

  uintptr_t page_flags = 0;

  if ((flags & GHEAP_KERNEL) != 0) {
    page_flags |= KMEM_FLAG_KERNEL;
  }
  if ((flags & GHEAP_READONLY) == 0) {
    page_flags |= KMEM_FLAG_WRITABLE;
  }

  for (int i = 0; i < pages_needed; i++) {
    const paddr_t phys_page = Must(kmem_prepare_new_physical_page());
    const vaddr_t virtual_offset = virtual_base + (i * SMALL_PAGE_SIZE);

    bool result = kmem_map_page(root_table, virtual_offset, phys_page, KMEM_CUSTOMFLAG_GET_MAKE, page_flags);

    if (!result) {
      // unmap pages that we did successfully map
      for (int j = 0; j < i; j++) {
        const vaddr_t reset_virtual_offset = virtual_base + (j * SMALL_PAGE_SIZE);
        const paddr_t reset_physical_offset = kmem_to_phys(root_table, reset_virtual_offset);

        kmem_set_phys_page_free(kmem_get_page_idx(reset_physical_offset));
        kmem_unmap_page(root_table, virtual_offset);
      }
      return nullptr;
    }
  }

  return ret;
}

void* dummy_alloc(size_t size) {
  return nullptr;
}
void dummy_dealloc(void* address) {
}