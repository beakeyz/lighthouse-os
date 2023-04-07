
#include "generic.h"
#include "mem/heap.h"


generic_partition_t create_generic_partition(const char* name, uintptr_t start_lba, uintptr_t end_lba, uint64_t flags) {
  generic_partition_t ret = {0};

  ret.m_name = name;
  ret.m_flags = flags;
  ret.m_start_lba = start_lba;
  ret.m_end_lba = end_lba;

  return ret;
}
