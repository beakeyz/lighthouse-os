#ifndef __ANIVA_GENERIC_PARTITION_DATA__
#define __ANIVA_GENERIC_PARTITION_DATA__
#include <libk/stddef.h>

typedef struct {
  uint64_t m_start_lba;
  uint64_t m_end_lba;

  // TODO: uuid, type, ect.

  uint64_t m_flags;
} generic_partition_t;

#endif // !__ANIVA_GENERIC_PARTITION_DATA__
