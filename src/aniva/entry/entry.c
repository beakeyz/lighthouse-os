#include "entry.h"
#include "dev/core.h"
#include "dev/debug/early_tty.h"
#include "dev/device.h"
#include "dev/disk/device.h"
#include "dev/disk/ramdisk.h"
#include "dev/disk/volume.h"
#include "dev/driver.h"
#include "dev/io/hid/hid.h"
#include "dev/loader.h"
#include "dev/pci/pci.h"
#include "dev/usb/usb.h"
#include "dev/video/core.h"
#include "fs/core.h"
#include "irq/interrupts.h"
#include "kevent/event.h"
#include "libk/bin/elf.h"
#include "libk/cmdline/parser.h"
#include "libk/flow/error.h"
#include "libk/lib.h"
#include "libk/multiboot.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/buffer.h"
#include "mem/zalloc/zalloc.h"
#include "oss/core.h"
#include "proc/core.h"
#include "proc/kprocs/idle.h"
#include "proc/kprocs/reaper.h"
#include "proc/proc.h"
#include "system/acpi/acpi.h"
#include "system/processor/processor.h"
#include "system/profile/profile.h"
#include "system/resource.h"
#include "time/core.h"
#include <dev/debug/serial.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <sched/scheduler.h>
#include <stdio.h>

system_info_t g_system_info;
static proc_t* root_proc;

void __init _start(struct multiboot_tag* mb_addr, uint32_t mb_magic);
void kthread_entry(void);

driver_version_t kernel_version = DRIVER_VERSION(0, 0, 1);

/*
 * NOTE: has to be run after driver initialization
 */
kerror_t __init try_fetch_initramdisk(void)
{
    uintptr_t ramdisk_addr;
    struct multiboot_tag_module* mod = g_system_info.ramdisk;

    if (!mod)
        return -1;

    const paddr_t module_start = mod->mod_start;
    const paddr_t module_end = mod->mod_end;

    KLOG_DBG("Ramdisk: from %llx to %llx\n", module_start, module_end);

    /* Prefetch data */
    const size_t cramdisk_size = module_end - module_start;

    /* Map user pages */
    ASSERT(!kmem_kernel_alloc((void**)&ramdisk_addr, module_start, cramdisk_size, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_WRITABLE));

    /* Create ramdisk object */
    volume_device_t* ramdisk = create_generic_ramdev_at(ramdisk_addr, cramdisk_size);

    if (!ramdisk)
        return -1;

    return 0;
}

static void register_kernel_data(paddr_t p_mb_addr)
{
    memset(&g_system_info, 0, sizeof(g_system_info));

    /* Register our desire to use legacy PIT early */
    g_system_info.irq_chip_type = IRQ_CHIPTYPE_PIT;

    /* Kernel image info */
    g_system_info.sys_flags = NULL;
    g_system_info.kernel_start_addr = (vaddr_t)&_kernel_start;
    g_system_info.kernel_end_addr = (vaddr_t)&_kernel_end;
    g_system_info.kernel_version = kernel_version.version;
    g_system_info.phys_multiboot_addr = p_mb_addr;
    g_system_info.virt_multiboot_addr = kmem_ensure_high_mapping(p_mb_addr);

    /* Cache the physical address of important tags */
    g_system_info.rsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);
    g_system_info.xsdp = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);
    g_system_info.firmware_fb = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);
    g_system_info.ramdisk = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_MODULE);
    g_system_info.cmdline = get_mb2_tag((void*)g_system_info.virt_multiboot_addr, MULTIBOOT_TAG_TYPE_CMDLINE);

    if (g_system_info.firmware_fb)
        g_system_info.sys_flags |= SYSFLAGS_HAS_FRAMEBUFFER;
}

/*!
 * @brief: Start the absolute foundation of the kernel
 */
static kerror_t _start_early_if(struct multiboot_tag* mb_addr, uint32_t mb_magic)
{
    /*
     * TODO: move serial to driver level, have early serial that gets migrated to a driver later
     * TODO: create a nice logging system, with log clients that connect to a single place to post
     * logs. The logging server can collect these logs and do all sorts of stuff with them, like save
     * them to a file, display them to the current console provider, or beam them over serial or USB
     * connectors =D
     */

    // Logging system asap
    init_early_logging();

    // First logger
    init_serial();

    /* Our fist hello to serial */
    KLOG_DBG("Hi from within (%s)\n", "Aniva");

    // Verify magic number
    if (mb_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        KLOG_ERR("Expected magic bootloader number to be %x, but got %x =(\n", MULTIBOOT2_BOOTLOADER_MAGIC, mb_magic);
        return -KERR_INVAL;
    }

    /* Quick bootloader interface info */
    KLOG_DBG("Multiboot address from the bootloader is at: %p\n", (void*)mb_addr);

    /* Prepare for stage 1 */
    register_kernel_data((paddr_t)mb_addr);

    return KERR_NONE;
}

/*!
 * @brief: Stage one of kernel initialization
 *
 * Here we setup the essentials to get a basic interface to the user and say our first hello =)
 */
static kerror_t _start_system_management(void)
{
    // parse multiboot
    init_multiboot((void*)g_system_info.virt_multiboot_addr);

    // init bootstrap processor
    init_processor(&g_bsp, 0);

    /*
     * bootstrap the kernel heap
     * We're pretty nicely under way to be able to move this call
     * under init_kmem_manager, since we almost don't need the heap
     * in that system anymore. When that happens, we are able to completely
     * initialize the kheap off of kmem_manager, since we can just
     * ask it for bulk memory that we can then create a memory_allocator
     * from
     */
    init_kheap();

    // we need memory
    init_kmem_manager((void*)g_system_info.virt_multiboot_addr);

    // Fully initialize logging right after the memory setup
    init_logging();

    // Initialize an early console
    init_early_tty();

    KLOG_INFO("Initialized tty");

    // Setup interrupts (Fault handlers and IRQ request framework)
    init_interrupts();

    // initialize cpu-related things that need the memorymanager and the heap
    init_processor_late(&g_bsp);

    return KERR_NONE;
}

/*!
 * @brief: Stage 2 of system init
 *
 * Init more advanced memory management and subsystems
 */
static kerror_t _start_subsystems(void)
{
    // we need more memory
    init_zalloc();

    // we need resources
    init_kresources();

    // Initialize libk
    init_libk();

    // Initialize buffer
    init_aniva_buffers();

    // Initialize kevent
    init_kevents();

    // Make sure we know how to access the PCI configuration space at this point
    init_pci_early();

    /* Initialize OSS */
    init_oss();

    /* Initialize the filesystem core */
    init_fs_core();

    /* Init the infrastructure needed for drivers */
    init_driver_subsys();

    /* Init the kernel device subsystem */
    init_devices();

    /* Initialize global disk device subsystem */
    init_volumes();

    /* Initialize the video subsystem */
    init_video();

    /* Initialize HID driver subsystem */
    init_hid();

    /* Initialize the USB subsystem */
    init_usb();

    return KERR_NONE;
}

/*!
 * @brief: Main start routine of Aniva
 *
 * This is the order in which everything is initialized:
 * 1: Gather system info from the bootloader in the form of multiboot2
 * 2: Initialize system memory management
 * 3: Create a simple graphical interface with the user
 * 4: Initialize the kernels different subsystem to support devices, drivers, processes, ect.
 * 5: Initialize the process core, which enables process/thread creation and termination
 * 6: Initialize the system drivers, which in turn generate a device tree inside the OSS (under 'Dev/')
 * 7: ...
 *
 * NOTE / TODO: We parse ACPI before we setup driver shit, while we do discover devices through ACPI. This
 * is an issue, since we want all device management to be streamlined through our driver model. A way to get
 * around this is to package ACPI in a same sort of system that we currently have for PCI, but a little bit more
 * standardized and streamlined. It works as followed: We walk the 'bus' which contains devices before
 * we load any drivers and we simply register all devices we find (OSS). Then, when we need to load drivers,
 * we can walk the centralized location where all the devices are registered, which ensures we don't load too
 * many useless drivers for devices that the system does not have. So we need to implement this two-step process
 * like this:
 * 1) Walk busses and register devices
 * 2) Walk the registerd devices and load drivers for them
 */
NOINLINE void __init _start(struct multiboot_tag* mb_addr, uint32_t mb_magic)
{
    kerror_t kinit_err;

    disable_interrupts();

    /*
     * Stage 0: Start most basic shit
     */
    kinit_err = _start_early_if(mb_addr, mb_magic);

    ASSERT_MSG(kinit_err == KERR_NONE, "Stage 0 of the kernel init failed for some reason =(");

    /*
     * Stage 1: Start basic memory management and a firmware-hosted
     * terminal interface to the user
     */
    kinit_err = _start_system_management();

    ASSERT_MSG(kinit_err == KERR_NONE, "Stage 1 of the kernel init failed for some reason =(");

    /*
     * Stage 2: Start the system subsystems
     */
    kinit_err = _start_subsystems();

    ASSERT_MSG(kinit_err == KERR_NONE, "Stage 2 of the kernel init failed for some reason =(");

    /* Initialize the ACPI subsystem */
    init_acpi();

    /* Initialize the PCI subsystem after ACPI */
    init_pci();

    /* Initialize the timer system */
    init_timer_system();

    /* Initialize the subsystem responsible for managing processes */
    init_proc_core();

    /* Initialize the kernel idle process */
    init_kernel_idle();

    /* Initialize scheduler on the bsp */
    init_scheduler(0);

    root_proc = create_kernel_proc(kthread_entry, NULL);

    ASSERT_MSG(root_proc, "Failed to create a root process!");

    /* Create a reaper thread to kill processes through an async pipeline */
    init_reaper(root_proc);

    /* Register our kernel process */
    set_kernel_proc(root_proc);

    /* Add it to the scheduler */
    ASSERT(proc_schedule(root_proc, NULL, NULL, NULL, NULL, SCHED_PRIO_HIGHEST) == 0);

    /* Start the scheduler (Should never return) */
    start_scheduler();

    // Verify not reached
    kernel_panic("Somehow came back to the kernel entry function!");
}

/*!
 * @brief: Our kernel thread entry point
 *
 * This function is not supposed to be called explicitly, but rather it serves as the
 * 'third stage' of our kernel, with stage one being our assembly setup and stage two
 * being the start of our C code.
 *
 * FIXME: We need to rationalize why we jump from the kernel boot context into this thread context. We do it to support
 * kernel subsystems that require async stuff (like USB drivers or some crap). Right now the order in which stuff happens
 * in this file seems kind of arbitrary, so we need to revisit this =/
 */
void kthread_entry(void)
{
    /* Make sure the scheduler won't ruin our day */
    // pause_scheduler();

    /*
     * Install and load initial drivers
     * NOTE: arch specific devices like APIC, PIT, RTC, etc. are initialized
     * by the core system and thus the device objects accociated with these devices
     * will look like they have no driver that manages them
     *
     * This loads the in-kernel drivers that we need for further boot (FS, Bus, ect.). Most drivers for devices
     * and other shit are found in Root/System/<driver name>.drv
     *
     * TODO: We want to change the boot sequence to the following:
     * Load in-kernel drivers -> Mount ramdisk -> Load device drivers from Root/System -> Load disk devices and find our rootdevice
     * -> Move the ramdisk to Initrd/ -> Mount the rootdevice to Root/ -> Lock system parts of the rootdevice -> Load userspace
     */
    init_aniva_driver_registry();

    /* Try to fetch the initrd which we can mount initial root to */
    try_fetch_initramdisk();

    /* Initialize the ramdisk as our initial root */
    init_root_ram_volume();

    /* Initialize hardware (device.c) */
    init_hw();

    /* Scan for pci devices and initialize any matching drivers */
    init_pci_drivers();

    /* Probe for a root device */
    init_root_volume();

    /*
     * Late environment stuff right before we are done bootstrapping kernel systems
     */

    /* (libk/bin/elf.c): Load the driver for dynamic executables */
    init_dynamic_loader();

    /* Do late initialization of the default profiles */
    init_profiles_late();

    /*
    system_time_t bStartTime;
    system_time_t bEndTime;
    time_get_system_time(&bStartTime);

    void** buffers;

    //kmem_kernel_alloc_range((void**)&buffers, sizeof(void*) * 10000, NULL, KMEM_FLAG_WRITABLE);
    buffers = kzalloc(sizeof(void*) * 10000);

    for (int i = 0; i < 10000; i++)
        buffers[i] = kzalloc(1024);

    for (int i = 0; i < 10000; i++)
        kzfree(buffers[i], 1024);

    kzfree(buffers, sizeof(void*) * 10000);

    time_get_system_time(&bEndTime);

    KLOG_INFO("kmalloc test (Start time: %ds, %dms, %dus) (End time: %ds, %dms, %dus)\n",
        bStartTime.s_since_boot, bStartTime.ms_since_last_s, bStartTime.us_since_last_ms,
        bEndTime.s_since_boot, bEndTime.ms_since_last_s, bEndTime.us_since_last_ms, );

    a_allocator_t alloc;
    void* buffer = kmalloc(1 * Mib);

    init_basic_aalloc(&alloc, buffer, Mib);

    time_get_system_time(&bStartTime);

    buffers = aalloc_allocate(&alloc, (sizeof(void*) * 10000));

    for (int i = 0; i < 10000; i++)
        buffers[i] = aalloc_allocate(&alloc, 1024);

    time_get_system_time(&bEndTime);

    KLOG_INFO("aalloc test (Start time: %ds, %dms, %dus) (End time: %ds, %dms, %dus)\n",
        bStartTime.s_since_boot, bStartTime.ms_since_last_s, bStartTime.us_since_last_ms,
        bEndTime.s_since_boot, bEndTime.ms_since_last_s, bEndTime.us_since_last_ms, );

    size_t used, free;

    aalloc_get_info(&alloc, &used, &free);

    KLOG_INFO("alloc info: used_sz=0x%llx, free_sz=0x%llx\n", used, free);

    kernel_panic("TEST");
    */

    /*
     * Setup is done: we can start scheduling stuff
     * At this point, the kernel should have created a bunch of userspace processes that are ready to run on the next schedules. Most of the
     * 'userspace stuff' will consist of user tracking, configuration and utility processes. Any windowing will be done by the kernel this driver
     * is an external driver that we will load from the ramfs, since it's not a driver that is an absolute non-trivial piece. When we fail to load
     */
    // resume_scheduler();

    /* Allocate a quick buffer for our init process */
    driver_t* drv;
    char init_buffer[256] = { 0 };

    /* Format the buffer */
    sfmt(init_buffer, "Root/Users/Admin/Core/init %s", opt_parser_get_bool("use_kterm") ? "--use-kterm" : " ");

    /* Construct the sysvar vector */
    sysvar_vector_t vec = {
        SYSVAR_VEC_BYTE("USE_KTERM", opt_parser_get_bool("use_kterm")),
        SYSVAR_VEC_BYTE("NO_PIPES", false),
        SYSVAR_VEC_END,
    };

    /* Execute the buffer */
    ASSERT_MSG(proc_exec(init_buffer, vec, get_admin_profile(), PROC_KERNEL | PROC_SYNC) != nullptr, "Failed to launch init process");

    /*
     * Remove the early TTY right before we finish low-level system setup. After
     * this point we're able to support our own debug capabilities
     */
    destroy_early_tty();

    drv = nullptr;

    if (!opt_parser_get_bool("use_kterm"))
        drv = load_external_driver("Root/System/lwnd.drv");

    if (!drv)
        drv = load_external_driver("Root/System/kterm.drv");

    /* Launch the usb monitor */
    proc_exec("Root/Users/Admin/Core/usbmntr", NULL, get_admin_profile(), PROC_KERNEL);

    while (true)
        /* Block this thread to save cycles */
        thread_sleep(root_proc->m_init_thread);

    /* Should not happen lmao */
    kernel_panic("Reached end of start_thread");
}
