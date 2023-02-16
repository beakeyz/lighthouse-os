#include "namespace.h"
#include "mem/heap.h"
#include "libk/string.h"

acpi_aml_seg_t *acpi_load_segment(void* table_ptr, int idx) {
  acpi_aml_seg_t *ret = kmalloc(sizeof(acpi_aml_seg_t));
  memset(ret, 0, sizeof(*ret));

  ret->aml_table = table_ptr;
  ret->idx = idx;
  return ret;
}

acpi_ns_node_t *acpi_create_root() {
  return nullptr;
}