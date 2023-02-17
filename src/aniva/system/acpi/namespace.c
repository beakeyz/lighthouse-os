#include "namespace.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "mem/heap.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"

acpi_aml_seg_t *acpi_load_segment(void* table_ptr, int idx) {
  acpi_aml_seg_t *ret = kmalloc(sizeof(acpi_aml_seg_t));
  memset(ret, 0, sizeof(*ret));

  ret->aml_table = table_ptr;
  ret->idx = idx;

  draw_char(0, 100, 'k');

  kmem_kernel_alloc((uintptr_t)ret->aml_table, sizeof(acpi_aml_table_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);


  draw_char(8, 100, 'a');
  return ret;
}

acpi_ns_node_t *acpi_create_root() {
  return nullptr;
}
