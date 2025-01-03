#ifndef __LIGHTENV_MEMFLAGS__
#define __LIGHTENV_MEMFLAGS__

/*
 * Flags for userspace memory attributes
 *
 * This header is also used inside the kernel
 */

/* Minimum of 4 Kib in order to save pool integrity */
#define MIN_POOLSIZE (0x1000)
#define MEMPOOL_ALIGN (0x1000)
#define MEMPOOL_ENTSZ_ALIGN (8)

/* This range of memory should be readable */
#define VMEM_FLAG_READ 0x00000001UL
/* This range of memory should be writable */
#define VMEM_FLAG_WRITE 0x00000002UL
/* This range of memory may contain executable code */
#define VMEM_FLAG_EXEC 0x00000004UL
/* Other processes may map the same (part of this) region of memory */
#define VMEM_FLAG_SHARED 0x00000008UL
/* Deletes the specified mapping */
#define VMEM_FLAG_DELETE 0x00000010UL

#endif // !__LIGHTENV_MEMFLAGS__
