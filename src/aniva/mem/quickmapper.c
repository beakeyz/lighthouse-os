#include "quickmapper.h"
#include "dev/debug/serial.h"
#include "kmem_manager.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "system/processor/processor.h"
#include <libk/stddef.h>

static uint32_t s_quickmap_flags;
static paddr_t s_current_quickmapped_addr;

void init_quickmapper() {
  
  /* Let's ensure we get our page */
  ASSERT_MSG(kmem_map_page(nullptr, QUICKMAP_BASE, 0, KMEM_CUSTOMFLAG_GET_MAKE, 0), "Can't get page for quickmapper");

  s_quickmap_flags = 0;
  s_current_quickmapped_addr = 0;

}

ErrorOrPtr try_quickmap(paddr_t p) {
  return quick_map_ex(p, 0);
}

ErrorOrPtr try_quickunmap() {

  /* No quickmapped address */
  if ((s_quickmap_flags & QUICKMAP_DIRTY) != QUICKMAP_DIRTY) {
    return Error();
  }

  /* No quickmapped address v2 */
  if (!s_current_quickmapped_addr)
    return Error();

  s_quickmap_flags &= ~QUICKMAP_DIRTY;
  s_current_quickmapped_addr = 0;

  return Success(0);
}

vaddr_t get_quickmapped_addr() {
  return s_current_quickmapped_addr;
}

vaddr_t quick_map(paddr_t p) {

  ErrorOrPtr result = try_quickmap(p);
  
  /* Failed, try to unmap */
  if (result.m_status != ANIVA_SUCCESS) {
    try_quickunmap();
    return Must(try_quickmap(p));
  }

  /* Yay, return dat bitch */
  return result.m_ptr;
}

void quick_unmap() {
  Must(try_quickunmap());
}

ErrorOrPtr quick_map_ex(paddr_t p, uint32_t page_flags) {

  processor_t* current_processor;
  bool map_result;
  size_t pdelta;
  paddr_t aligned_phys;
  vaddr_t ret;

  /* Don't map Zero */
  if (!p || (s_quickmap_flags & QUICKMAP_DIRTY) == QUICKMAP_DIRTY) {
    return Error();
  }

  current_processor = get_current_processor();

  if (!current_processor)
    return Error();
  
  /* No quickmapping in any page map other than the kernels */
  if (current_processor->m_page_dir && current_processor->m_page_dir != kmem_get_krnl_dir()) {
    return Error();
  }

  aligned_phys = ALIGN_DOWN(p, SMALL_PAGE_SIZE);
  /* Since we align downwards, we subtract p from the aligned counterpart */
  pdelta = (p - aligned_phys);

  map_result = kmem_map_page(nullptr, QUICKMAP_BASE, aligned_phys, 0, page_flags);

  if (!map_result) {
    return Error();
  }

  ret = QUICKMAP_BASE + pdelta;

  /* Mark dirty */
  s_current_quickmapped_addr = ret;
  s_quickmap_flags |= QUICKMAP_DIRTY;

  return Success(ret);
}
