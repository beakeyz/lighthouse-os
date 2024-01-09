#ifndef __ANIVA_ACPI_OSL__
#define __ANIVA_ACPI_OSL__

/*
 * Anivia impl of the ACPICA OS Service Layer
 *
 * TODO: Check if the OSL has been initialized and if calls happen before this point, we should
 * spoof success. Linux does this as well
 */

#include "acpica/acpi.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/sem.h"
#include "sync/spinlock.h"
#include "system/acpi/acpi.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpiosxf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/acpica/platform/acaniva.h"
#include "system/acpi/acpica/platform/acenv.h"

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char *Format, ...)
{
  va_list args;
  va_start(args, Format);

  vprintf(Format, args);

  va_end(args);
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsVprintf(const char *Format, va_list Args)
{
  vprintf(Format, Args);
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
  return (ACPI_PHYSICAL_ADDRESS)find_acpi_root_ptr();
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
  *NewValue = NULL;
  return AE_OK;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
  *NewTable = NULL;
  return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength)
{
  *NewAddress = NULL;
  *NewTableLength = NULL;
  return AE_OK;
}

void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
  vaddr_t vaddr;

  println("Calling: AcpiOsMapMemory");

  Length = ALIGN_UP(Length, SMALL_PAGE_SIZE);

  vaddr = Must(__kmem_kernel_alloc(PhysicalAddress, Length, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  println("Called: AcpiOsMapMemory");
  return (void*)(vaddr);
}

void AcpiOsUnmapMemory(void *where, ACPI_SIZE length)
{
  println("Calling: AcpiOsUnmapMemory");
  __kmem_kernel_dealloc((vaddr_t)where, length);
  println("Called: AcpiOsUnmapMemory");
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
  if (!PhysicalAddress)
    return AE_BAD_PARAMETER;

  *PhysicalAddress = kmem_to_phys(nullptr, (vaddr_t)LogicalAddress);
  return AE_OK;
}

void *AcpiOsAllocate(ACPI_SIZE Size)
{
  return kmalloc(Size);
}

void AcpiOsFree(void *Memory)
{
  kfree(Memory);
}

/*!
 * @brief: This function creates a cache object
 * 
 * @returns:
 * AE_OK: The cache was successfully created.
 * AE_BAD_PARAMETER: At least one of the following is true:
 *  The ReturnCache pointer is NULL.
 *  The ObjectSize is less than 16.
 * AE_NO_MEMORY: Insufficient dynamic memory to complete the operation
 */
ACPI_STATUS AcpiOsCreateCache(char *CacheName, UINT16 ObjectSize, UINT16 MaxDepth, struct zone_allocator **ReturnCache)
{
  zone_allocator_t* cache;

  cache = create_zone_allocator(ObjectSize*MaxDepth, ObjectSize, NULL);

  if (!cache)
    return AE_NO_MEMORY;

  *ReturnCache = cache;
  return AE_OK;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T *Cache)
{
  destroy_zone_allocator(Cache, true);
  return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T *Cache)
{
  kernel_panic("TODO: AcpiOsPurgeCache");
  return AE_OK;
}

/*!
 * @brief: Allocate an object in the cache
 */
void* AcpiOsAcquireObject(ACPI_CACHE_T *Cache)
{
  return zalloc_fixed(Cache);
}

ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T *Cache, void *Object)
{
  zfree_fixed(Cache, Object);
  return AE_OK;
}

BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length)
{
  kernel_panic("TODO: AcpiOsReadable");
  return FALSE;
}

BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length)
{
  kernel_panic("TODO: AcpiOsWritable");
  return FALSE;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 *Value, UINT32 Width)
{
  void* vaddr = (void*)kmem_from_phys(Address, KERNEL_MAP_BASE);

  switch (Width) {
    case 0 ... 8:
      *Value = mmio_read_byte(vaddr);
      break;
    case 9 ... 16:
      *Value = mmio_read_word(vaddr);
      break;
    case 17 ... 32:
      *Value = mmio_read_dword(vaddr);
      break;
    default:
      kernel_panic("AcpiOsReadMemory: unsupported width!");
      return AE_SUPPORT;
  }
  return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
  void* vaddr = (void*)kmem_from_phys(Address, KERNEL_MAP_BASE);

  switch (Width) {
    case 0 ... 8:
      mmio_write_byte(vaddr, (uint8_t)Value);
      break;
    case 9 ... 16:
      mmio_write_word(vaddr, (uint16_t)Value);
      break;
    case 17 ... 32:
      mmio_write_dword(vaddr, (uint32_t)Value);
      break;
    case 33 ... 64:
      mmio_write_qword(vaddr, (uintptr_t)Value);
      break;
    default:
      kernel_panic("AcpiOsWriteMemory: unsupported width!");
      return AE_SUPPORT;
  }
  return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32 *Value, UINT32 Width)
{
  switch (Width) {
    case 0 ... 8:
      *Value = in8(Address);
      break;
    case 9 ... 16:
      *Value = in16(Address);
      break;
    case 17 ... 32:
      *Value = in32(Address);
      break;
    default:
      kernel_panic("AcpiOsReadPort: unsupported width!");
      return AE_SUPPORT;
  }
  return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
  switch (Width) {
    case 0 ... 8:
      out8(Address, Value);
      break;
    case 9 ... 16:
      out16(Address, Value);
      break;
    case 17 ... 32:
      out32(Address, Value);
      break;
    default:
      kernel_panic("AcpiOsWriteMemory: unsupported width!");
      return AE_SUPPORT;
  }
  return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 *Value, UINT32 Width)
{
  kernel_panic("TODO: AcpiOsReadPciConfiguration");
  return AE_OK;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
  kernel_panic("TODO: AcpiOsWritePciConfiguration");
  return AE_OK;
}

void AcpiOsWaitEventsComplete(void)
{
  kernel_panic("TODO: AcpiOsWaitEventsComplete");
}

ACPI_STATUS AcpiOsWaitCommandReady()
{
  kernel_panic("TODO: AcpiOsWaitCommandReady");
  return AE_OK;
}

ACPI_STATUS AcpiOsNotifyCommandComplete()
{
  kernel_panic("TODO: AcpiOsNotifyCommandComplete");
  return AE_OK;
}

ACPI_THREAD_ID AcpiOsGetThreadId()
{
  thread_t* c;

  c = get_current_scheduling_thread();

  if (!c)
    return 0xFFFFFFFF;

  return c->m_tid;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
  kernel_panic("TODO: AcpiOsExecute");
  return AE_OK;
}

void AcpiOsSleep(UINT64 Milliseconds)
{
  kernel_panic("TODO: AcpiOsSleep");
}

void AcpiOsStall(UINT32 Microseconds)
{
  kernel_panic("TODO: AcpiOsStall");
}

ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue)
{
  kernel_panic("TODO: AcpiOsEnterSleep");
  return AE_OK;
}

/*
 * Mutexes, Spinlocks and Semaphores
 *
 * Mutex
 */

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX *OutHandle)
{
  *OutHandle = create_mutex(NULL);
  return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle)
{
  destroy_mutex(Handle);
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout)
{
  mutex_t* m;

  m = (mutex_t*)Handle;

  if (!m)
    return AE_BAD_PARAMETER;

  if (!Timeout && mutex_is_locked(m))
    return AE_NOT_ACQUIRED;

  mutex_lock(m);
  return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
{
  mutex_t* m;

  m = (mutex_t*)Handle;

  ASSERT_MSG(m, "ACPICA gave us an invalid mutex!");

  mutex_unlock(m);
}

/*
 * Semaphores
 */

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
  *OutHandle = create_semaphore(MaxUnits, InitialUnits, 1);
  return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
  destroy_semaphore(Handle);
  return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
  if (Units > 1)
    return AE_SUPPORT;

  sem_wait(Handle, NULL);
  return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
  if (Units > 1)
    return AE_SUPPORT;

  sem_post(Handle);
  return AE_OK;
}

/*
 * Spinlocks
 */

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
  *OutHandle = create_spinlock();
  return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
  destroy_spinlock(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
  spinlock_lock(Handle);
  return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
  (void)Flags;
  spinlock_unlock(Handle);
}

/*
 * Interrupts and stuff
 */

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context)
{
  kernel_panic("TODO: AcpiOsInstallInterruptHandler");
  return AE_OK;
}

ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
  kernel_panic("TODO: AcpiOsRemoveInterruptHandler");
  return AE_OK;
}

/*
 * Misc
 */

UINT64 AcpiOsGetTimer()
{
  kernel_panic("TODO: AcpiOsGetTimer");
  return 0;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void *Info)
{
  kernel_panic("TODO: AcpiOsSignal");
  return AE_OK;
}

ACPI_STATUS AcpiOsGetLine(char *Buffer, UINT32 BufferLength, UINT32 *BytesRead)
{
  kernel_panic("TODO: AcpiOsGetLine");
  return AE_OK;
}

ACPI_STATUS AcpiOsInitializeDebugger()
{
  //kernel_panic("TODO: AcpiOsInitializeDebugger");
  println("ACPI: initialized osl debugger!");
  return AE_OK;
}

void AcpiOsTerminateDebugger()
{
  //kernel_panic("TODO: AcpiOsTerminateDebugger");
  println("ACPI: terminated osl debugger!");
}

/*
 * ACPI subsys init and fini
 */

ACPI_STATUS AcpiOsInitialize()
{
  //kernel_panic("TODO: impl AcpiOsInitialize");
  return AE_OK; 
}

ACPI_STATUS AcpiOsTerminate()
{
  //kernel_panic("TODO: impl AcpiOsTerminate");
  return AE_OK;
}

#endif /* ifndef __ANIVA_ACPI_OSL__ */
