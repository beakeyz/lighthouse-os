#ifndef __ANIVA_GENERIC_PARTITION_DATA__
#define __ANIVA_GENERIC_PARTITION_DATA__
#include <libk/stddef.h>

typedef struct {
  const char *m_name;

  uint64_t m_start_lba;
  uint64_t m_end_lba;

  // TODO: uuid, type, ect.

  uint64_t m_flags;

} generic_partition_t;

generic_partition_t create_generic_partition(const char* name, uintptr_t start_lba, uintptr_t end_lba, uint64_t flags);

#endif // !__ANIVA_GENERIC_PARTITION_DATA__
