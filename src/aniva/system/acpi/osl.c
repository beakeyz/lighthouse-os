#ifndef __ANIVA_ACPI_OSL__
#define __ANIVA_ACPI_OSL__

/*
 * Anivia impl of the ACPICA OS Service Layer
 *
 * TODO: Check if the OSL has been initialized and if calls happen before this point, we should
 * spoof success. Linux does this as well
 */

#include "dev/pci/pci.h"
#include "entry/entry.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc/zalloc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/sem.h"
#include "sync/spinlock.h"
#include "system/acpi/acpi.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpiosxf.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/acpica/platform/acaniva.h"
#include "system/acpi/acpica/platform/acenv.h"

void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(const char* Format, ...)
{
    va_list args;
    va_start(args, Format);

    V_KLOG_DBG(Format, args);

    va_end(args);
}

void ACPI_INTERNAL_VAR_XFACE AcpiOsVprintf(const char* Format, va_list Args)
{
    V_KLOG_DBG(Format, Args);
}

ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
    return (ACPI_PHYSICAL_ADDRESS)find_acpi_root_ptr();
}

ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* PredefinedObject, ACPI_STRING* NewValue)
{
    *NewValue = NULL;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_TABLE_HEADER** NewTable)
{
    *NewTable = NULL;
    return AE_SUPPORT;
}

ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* ExistingTable, ACPI_PHYSICAL_ADDRESS* NewAddress, UINT32* NewTableLength)
{
    *NewAddress = NULL;
    *NewTableLength = NULL;
    return AE_SUPPORT;
}

void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
    void* ret;

    // printf("Trying to map OS memory addr=%llx len=%llx\n", PhysicalAddress, Length);
    if (__kmem_kernel_alloc(
            &ret,
            PhysicalAddress,
            Length,
            NULL,
            KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE))
        return nullptr;

    // printf("Mapped OS memory\n");
    return ret;
}

void AcpiOsUnmapMemory(void* where, ACPI_SIZE length)
{
    /*
     * NOTE: We might need to deallocate over overlapping chunks, which means we might need
     * to deallocate a physical frame marked as unused
     */
    __kmem_dealloc_ex(nullptr, nullptr, (vaddr_t)where, length, false, true, false);
}

ACPI_STATUS AcpiOsGetPhysicalAddress(void* LogicalAddress, ACPI_PHYSICAL_ADDRESS* PhysicalAddress)
{
    if (!PhysicalAddress)
        return AE_BAD_PARAMETER;

    // println("Calling: AcpiOsGetPhysicalAddress");
    *PhysicalAddress = kmem_to_phys(nullptr, (vaddr_t)LogicalAddress);

    if (!(*PhysicalAddress))
        return AE_NOT_FOUND;
    // println("Called: AcpiOsGetPhysicalAddress");
    return AE_OK;
}

void* AcpiOsAllocate(ACPI_SIZE Size)
{
    void* ret;

    // println("Called AcpiOsAllocate");

    if (!Size)
        return NULL;

    ret = kmalloc(Size);

    ASSERT_MSG(ret, "ACPI osl: Could not allocate this shit!");

    // printf("Allocation: 0x%p\n", ret);

    memset(ret, 0, Size);

    return ret;
}

void AcpiOsFree(void* Memory)
{
    // println("Called AcpiOsFree");
    ASSERT_MSG(Memory, "ACPICA gave bogus memory to free!");

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
ACPI_STATUS AcpiOsCreateCache(char* CacheName, UINT16 ObjectSize, UINT16 MaxDepth, struct zone_allocator** ReturnCache)
{
    zone_allocator_t* cache;

    cache = create_zone_allocator(ObjectSize * MaxDepth, ObjectSize, NULL);

    if (!cache)
        return AE_NO_MEMORY;

    *ReturnCache = cache;
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteCache(ACPI_CACHE_T* Cache)
{
    destroy_zone_allocator(Cache, true);
    return AE_OK;
}

ACPI_STATUS AcpiOsPurgeCache(ACPI_CACHE_T* Cache)
{
    zone_allocator_clear(Cache);
    return AE_OK;
}

/*!
 * @brief: Allocate an object in the cache
 */
void* AcpiOsAcquireObject(ACPI_CACHE_T* Cache)
{
    void* ret;

    // println("AcpiOsAcquireObject");

    ret = zalloc_fixed(Cache);

    if (!ret)
        return nullptr;
    // printf("done AcpiOsAcquireObject (0x%p)\n", ret);

    memset(ret, 0, Cache->m_entry_size);

    return ret;
}

ACPI_STATUS AcpiOsReleaseObject(ACPI_CACHE_T* Cache, void* Object)
{
    zfree_fixed(Cache, Object);
    return AE_OK;
}

BOOLEAN AcpiOsReadable(void* Memory, ACPI_SIZE Length)
{
    size_t page_count;
    pml_entry_t* page;

    page_count = ALIGN_UP(Length, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++) {
        /* If the page isn't found, we obviously can't read it :clown */
        if (kmem_get_page(&page, nullptr, (uint64_t)Memory + i * SMALL_PAGE_SIZE, NULL, NULL))
            return FALSE;
    }

    return TRUE;
}

BOOLEAN AcpiOsWritable(void* Memory, ACPI_SIZE Length)
{
    size_t page_count;
    pml_entry_t* page;

    page_count = ALIGN_UP(Length, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

    for (uint64_t i = 0; i < page_count; i++) {
        /*
         * NOTE: kmem_get_page will align down the memory address to the first page-aligned boundry
         * so we don't have to worry about the alignment here
         */
        if (kmem_get_page(&page, nullptr, (uint64_t)Memory + i * SMALL_PAGE_SIZE, NULL, NULL))
            return FALSE;

        /* Check for the writable bit, which indicates whether we can write to this page (LOL) */
        if (!pml_entry_is_bit_set(page, PTE_WRITABLE))
            return FALSE;
    }

    return TRUE;
}

ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64* Value, UINT32 Width)
{
    // println("Calling AcpiOsReadMemory");
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

    // println("Called AcpiOsReadMemory");
    return AE_OK;
}

ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value, UINT32 Width)
{
    // println("Calling AcpiOsWriteMemory");
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
    // println("Called AcpiOsWriteMemory");
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width)
{
    // println("AcpiOsReadPort");
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
    // println("End: AcpiOsReadPort");
    return AE_OK;
}

ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width)
{
    // println("AcpiOsWritePort");
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
    // println("End: AcpiOsWritePort");
    return AE_OK;
}

ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg, UINT64* Value, UINT32 Width)
{
    int error = -1;

    switch (Width) {
    case 0 ... 8:
        error = pci_read8(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint8_t*)Value);
        break;
    case 9 ... 16:
        error = pci_read16(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint16_t*)Value);
        break;
    case 17 ... 32:
        error = pci_read32(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint32_t*)Value);
        break;
    }

    if (!error)
        return AE_OK;

    return AE_ERROR;
}

ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg, UINT64 Value, UINT32 Width)
{
    int error = -1;

    switch (Width) {
    case 0 ... 8:
        error = pci_write8(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint8_t)Value);
        break;
    case 9 ... 16:
        error = pci_write16(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint16_t)Value);
        break;
    case 17 ... 32:
        error = pci_write32(PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, Reg, (uint32_t)Value);
        break;
    }

    if (!error)
        return AE_OK;

    return AE_ERROR;
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

/*!
 *
 * FIXME: Should this return a fid?
 */
ACPI_THREAD_ID AcpiOsGetThreadId()
{
    thread_t* c;

    c = get_current_scheduling_thread();

    if (!c)
        return 1;

    return c->m_tid;
}

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void* Context)
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
    uint32_t delta;

    do {
        delta = 1000;

        /* Cap it */
        if (delta > Microseconds)
            delta = Microseconds;

        /* Spin */
        udelay(delta);

        /* Subtract */
        Microseconds -= delta;
    } while (Microseconds);
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

ACPI_STATUS AcpiOsCreateMutex(ACPI_MUTEX* OutHandle)
{
    *OutHandle = create_mutex(NULL);

    ASSERT_MSG(*OutHandle, __func__);
    return AE_OK;
}

void AcpiOsDeleteMutex(ACPI_MUTEX Handle)
{
    destroy_mutex(Handle);
}

ACPI_STATUS AcpiOsAcquireMutex(ACPI_MUTEX Handle, UINT16 Timeout)
{
    mutex_t* m;

    // println("Mut take");

    m = (mutex_t*)Handle;

    if (!system_has_multithreading())
        return AE_OK;

    if (!m)
        return AE_BAD_PARAMETER;

    if (Timeout != (uint16_t)-1 && mutex_is_locked(m))
        return AE_TIME;

    mutex_lock(m);
    // println("Mut take done");
    return AE_OK;
}

void AcpiOsReleaseMutex(ACPI_MUTEX Handle)
{
    mutex_t* m;

    if (!system_has_multithreading())
        return;

    m = (mutex_t*)Handle;

    ASSERT_MSG(m, "ACPICA gave us an invalid mutex!");

    // println("Mut rel");
    mutex_unlock(m);
    // println("Mut rel done");
}

/*
 * Semaphores
 */

ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE* OutHandle)
{
    *OutHandle = create_semaphore(MaxUnits, InitialUnits, 1);

    ASSERT_MSG(*OutHandle, __func__);
    return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
    destroy_semaphore(Handle);
    return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
    if (!system_has_multithreading())
        return AE_OK;

    if (Units > 1)
        return AE_SUPPORT;

    // println("sem wait");
    sem_wait(Handle, NULL);
    // println("sem wait donej:warn("");");
    return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units)
{
    if (!system_has_multithreading())
        return AE_OK;

    if (Units > 1)
        return AE_SUPPORT;

    // println("sem post");
    sem_post(Handle);
    // println("sem post done");
    return AE_OK;
}

/*
 * Spinlocks
 */

ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle)
{
    *OutHandle = create_spinlock();

    ASSERT_MSG(*OutHandle, __func__);
    return AE_OK;
}

void AcpiOsDeleteLock(ACPI_SPINLOCK Handle)
{
    destroy_spinlock(Handle);
}

ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
    if (!system_has_multithreading())
        return 0;

    // println("AcpiOsAcquireLock");
    spinlock_lock(Handle);
    // println("AcpiOsAcquireLock done");
    return 0;
}

void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
    if (!system_has_multithreading())
        return;

    // println("AcpiOsReleaseLock");
    (void)Flags;
    spinlock_unlock(Handle);
    // println("AcpiOsReleaseLock done");
}

/*
 * Interrupts and stuff
 */

ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void* Context)
{
    KLOG_DBG("ACPI OSL: Trying to allocate interrupt: %lld\n", InterruptLevel);

    if (InterruptLevel != AcpiGbl_FADT.SciInterrupt)
        return AE_BAD_PARAMETER;

    /* ACPI requires a direct call */
    if (irq_allocate(InterruptLevel, NULL, IRQHANDLER_FLAG_DIRECT_CALL, Handler, Context, "ACPI osl alloc"))
        return AE_NOT_ACQUIRED;

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
    // kernel_panic("TODO: AcpiOsGetTimer");
    // printf("Called AcpiOsGetTimer! Returned zero\n");
    return 0;
}

ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
    kernel_panic("TODO: AcpiOsSignal");
    return AE_OK;
}

ACPI_STATUS AcpiOsGetLine(char* Buffer, UINT32 BufferLength, UINT32* BytesRead)
{
    kernel_panic("TODO: AcpiOsGetLine");
    return AE_OK;
}

ACPI_STATUS AcpiOsInitializeDebugger()
{
    // kernel_panic("TODO: AcpiOsInitializeDebugger");
    println("ACPI: initialized osl debugger!");
    return AE_OK;
}

void AcpiOsTerminateDebugger()
{
    // kernel_panic("TODO: AcpiOsTerminateDebugger");
    println("ACPI: terminated osl debugger!");
}

/*
 * ACPI subsys init and fini
 */

ACPI_STATUS AcpiOsInitialize()
{
    // kernel_panic("TODO: impl AcpiOsInitialize");
    return AE_OK;
}

ACPI_STATUS AcpiOsTerminate()
{
    // kernel_panic("TODO: impl AcpiOsTerminate");
    return AE_OK;
}

#endif /* ifndef __ANIVA_ACPI_OSL__ */
