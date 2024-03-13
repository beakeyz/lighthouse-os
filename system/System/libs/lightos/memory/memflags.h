#ifndef __LIGHTENV_MEMFLAGS__
#define __LIGHTENV_MEMFLAGS__

/*
 * Flags for userspace memory attributes
 *
 * This header is also used inside the kernel
 */

/* Minimum of 4 Kib in order to save pool integrity */
#define MIN_POOLSIZE            (0x1000)
#define MEMPOOL_ALIGN           (0x1000)

enum MEMPOOL_TYPE {
  MEMPOOL_TYPE_DEFAULT      = 0,
  MEMPOOL_TYPE_BUFFER       = 1,
  MEMPOOL_TYPE_IO           = 2
};

/* I/O opperation permissions */
#define MEMPOOL_FLAG_R              (0x00000001)
#define MEMPOOL_FLAG_W              (0x00000002)
#define MEMPOOL_FLAG_RW             (MEMPOOL_FLAG_R | MEMPOOL_FLAG_W)

/* Proctect this pool from process-to-process memory allocation (allocator sharing) */
#define MEMPOOL_FLAG_PTPALLOC_PROT  (0x00000004)
/* Pool location resides in memory */
#define MEMPOOL_FLAG_MEMBACKED      (0x00000008)
/* Pool location resides on disk */
#define MEMPOOL_FLAG_DISKBACKED     (0x00000010)
/* This pool is used by the processes memory allocation stack */
#define MEMPOOL_FLAG_MALLOCPOOL     (0x00000020)

#endif // !__LIGHTENV_MEMFLAGS__
