#include "irq/faults/faults.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "mem/pg.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"

/*
 * FIXME: this handler is going to get called a bunch of
 * different times when running the system, so this and a
 * few handlers that serve the same fate should get overlapping
 * and rigid handling.
 */
enum FAULT_RESULT pagefault_handler(const aniva_fault_t* fault, registers_t* regs)
{
    uintptr_t err_addr = read_cr2();
    uintptr_t cs = regs->cs;
    paddr_t p_addr;
    pml_entry_t* page;
    kmem_info_t kmem_info;
    uint32_t error_word = regs->err_code;

    thread_t* current_thread = get_current_scheduling_thread();
    proc_t* current_proc = get_current_proc();
    processor_t* c_cpu = get_current_processor();

    println(" ----- PAGEFAULT ----- (O.o) ");
    printf("Error at ring: %lld\n", (cs & 3));
    printf("Error at addr: 0x%llx\n", err_addr);
    printf("Error type: %s\n", ((error_word & 1) == 1) ? "ProtVi" : "Non-Present");
    printf("Tried to %s memory\n", (error_word & 2) == 2 ? "write into" : "read from");

    kmem_get_info(&kmem_info, c_cpu->m_cpu_num);

    printf(" - kmem info -\n");
    printf(" Total memory size: 0x%llx\n", kmem_info.total_pages << PAGE_SHIFT);
    printf(" Free pages: %lld\n", kmem_info.total_pages - kmem_info.used_pages);
    printf(" Used pages: %lld\n", kmem_info.used_pages);
    printf(" Kernel CR3: %p\n", kmem_get_krnl_dir());

    if (current_proc && current_thread) {
        printf("fault occured in: %s:%s (cr3: 0x%llx)\n",
            current_proc->m_name,
            current_thread->m_name,
            current_proc->m_root_pd.m_phys_root);
        p_addr = kmem_to_phys(current_proc->m_root_pd.m_root, err_addr);

        if (KERR_OK(kmem_get_page(&page, current_proc->m_root_pd.m_root, err_addr, NULL, NULL)))
            printf("Physical address: 0x%llx with flags: (%s%s%s%s)\n",
                p_addr,
                pml_entry_is_bit_set(page, PTE_PRESENT) ? "-Present-" : "-",
                pml_entry_is_bit_set(page, PTE_USER) ? "-User-" : "-",
                pml_entry_is_bit_set(page, PTE_WRITABLE) ? "-W-" : "-RO-",
                pml_entry_is_bit_set(page, PTE_NO_CACHE) ? "-" : "-C-");
    } else
        printf("Fault occured during kernel boot (Yikes)\n");

    if (!current_thread)
        goto panic;

    if (!current_proc)
        goto panic;

    /* Drivers and other kernel processes should get insta-nuked */
    /* FIXME: a driver can be a non-kernel process. Should user drivers meet the same fate as kernel drivers? */
    if ((current_proc->m_flags & PROC_KERNEL) == PROC_KERNEL)
        goto panic;

    if ((current_proc->m_flags & PROC_DRIVER) == PROC_DRIVER)
        goto panic;

    // /* NOTE: This yields and exits the interrupt context cleanly */
    // Must(try_terminate_process(current_proc));
    // return regs;
    return FR_KILL_PROC;

panic:
    kernel_panic("pagefault! (TODO: more info)");
}
