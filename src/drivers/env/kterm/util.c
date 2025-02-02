#include "util.h"
#include "dev/core.h"
#include "dev/ctl.h"
#include "dev/device.h"
#include "dev/disk/device.h"
#include "dev/disk/volume.h"
#include "dev/driver.h"
#include "lightos/dev/shared.h"
#include "drivers/env/kterm/kterm.h"
#include "entry/entry.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "lightos/volume/shared.h"
#include <dev/loader.h>
#include <proc/env.h>

static const char* __help_str = "Welcome to the Aniva kernel terminal application (kterm)\n"
                                "kterm provides a few internal utilities and a way to execute binaries from the filesystem\n"
                                "\nA few commands available directly from kterm are: \n";

static ALWAYS_INLINE void kterm_print_keyvalue(const char* key, const char* value)
{
    kterm_print(key);
    kterm_print(": ");
    if (value)
        kterm_println(value);
    else
        kterm_println("N/A");
}

uint32_t kterm_cmd_help(const char** argv, size_t argc)
{
    kterm_println(__help_str);

    for (uint32_t i = 0; i < kterm_cmd_count; i++) {
        struct kterm_cmd* c = &kterm_commands[i];

        if (!c->argv_zero)
            continue;

        kterm_print_keyvalue(c->argv_zero, c->desc);
    }
    return 0;
}

uint32_t kterm_cmd_exit(const char** argv, size_t argc)
{
    kernel_panic("TODO: how to exit the system?");
    return 0;
}

uint32_t kterm_cmd_clear(const char** argv, size_t argc)
{
    kterm_clear();
    return 0;
}

/*!
 * @brief Get the amount of megabytes from a page count
 *
 * Nothing to add here...
 */
static inline uint64_t __page_count_to_mib(uint64_t page_count)
{
    return ((page_count * SMALL_PAGE_SIZE) / (Mib));
}

uint32_t kterm_cmd_sysinfo(const char** argv, size_t argc)
{
    kmem_info_t km_info;
    memory_allocator_t kallocator = { 0 };
    acpi_parser_t* parser;

    kterm_println(nullptr);

    kterm_print_keyvalue("System name", "Aniva");
    kterm_print_keyvalue("System version", to_string(kernel_version.version));

    get_root_acpi_parser(&parser);

    if (parser) {
        kterm_print_keyvalue("ACPI table count", to_string(parser->m_tables->m_length));

        kterm_print_keyvalue("ACPI rsdp paddr", to_string(parser->m_rsdp_phys));
        kterm_print_keyvalue("ACPI xsdp paddr", to_string(parser->m_xsdp_phys));

        kterm_print_keyvalue("ACPI revision", to_string((uint64_t)parser->m_acpi_rev));
        kterm_print_keyvalue("ACPI method", parser->m_rsdp_discovery_method.m_name);
    }

    kheap_copy_main_allocator(&kallocator);

    kterm_print_keyvalue("KHeap space free", to_string(kallocator.m_free_size));
    kterm_print_keyvalue("KHeap space used", to_string(kallocator.m_used_size));

    kmem_get_info(&km_info, 0);

    kterm_print_keyvalue("physical memory (pages) free", to_string(km_info.total_pages - km_info.used_pages));
    kterm_print_keyvalue("physical memory (pages) used", to_string(km_info.used_pages));

    return 0;
}

bool print_drv_info(oss_node_t* node, oss_obj_t* obj, void* arg0)
{
    u32 idx = 0;
    driver_t* driver;

    if (node)
        return !oss_node_itterate(node, print_drv_info, arg0);

    driver = oss_obj_unwrap(obj, driver_t);

    if (obj->type != OSS_OBJ_TYPE_DRIVER || !driver)
        return false;

    printf("%16.16s: %32.32s (Loaded: %s)\n", driver->m_url, (driver->m_image_path == nullptr) ? "Internal" : driver->m_image_path, ((driver->m_flags & DRV_LOADED) == DRV_LOADED) ? "Yes" : "No");

    FOREACH(i, driver->m_dev_list)
    {
        device_t* dev = i->data;

        printf("  - (%d) %s\n", idx++, dev->name);
    }

    return true;
}

/*!
 * @brief: Print information about the installed and loaded drivers
 */
uint32_t kterm_cmd_drvinfo(const char** argv, size_t argc)
{
    kterm_println(" - Printing drivers...");

    foreach_driver(print_drv_info, NULL);

    return 0;
}

void a()
{
}

/*!
 * @brief: kinda like neofetch, but fun
 */
uint32_t kterm_cmd_hello(const char** argv, size_t argc)
{
    kterm_println("   ,----,");
    kterm_println("  /  .'  \\");
    kterm_println(" /  ;     \\");
    kterm_println("|  |       \\");
    kterm_println("|  |   /\\   \\");
    kterm_println("|  |  /; \\   \\");
    kterm_println("|  |  |\\  ;   \\");
    kterm_println("|  |  | \\  \\ ,'");
    kterm_println("|  |  |  '--'");
    kterm_println("|  |  |");
    kterm_println("|  | ,'");
    kterm_println("`--''");
    kterm_println("");
    kterm_println("(Aniva): Hello to you too =) ");

    // proc_t* p = create_proc(NULL, NULL, NULL, "test", (FuncPtr)a, NULL, NULL);

    // destroy_proc(p);

    return 0;
}

/*!
 * @brief: Manage kernel drivers
 *
 * Flags:
 *  -u -> unload the specified driver
 *  -ui -> uninstalls the specified driver
 *  -v -> verbose
 *  -h -> print help
 * Usage:
 * drvld [flags] [path]
 */
uint32_t kterm_cmd_drvld(const char** argv, size_t argc)
{
    kerror_t error;
    driver_t* driver;
    const char* drv_path = nullptr;
    bool should_unload = false;
    bool should_uninstall = false;
    bool should_install = false;
    bool should_help = false;

    if (!cmd_has_args(argc)) {
        kterm_println("No arguments found!");
        return 1;
    }

    /*
     * Scan the argument vector
     */
    for (uint32_t i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (arg[0] != '-' && !drv_path) {
            drv_path = arg;
            continue;
        }

        if (strcmp(arg, "-u") == 0) {
            should_unload = true;
            continue;
        }

        if (strcmp(arg, "-ui") == 0) {
            should_uninstall = true;
            continue;
        }

        if (strcmp(arg, "-i") == 0) {
            should_install = true;
            continue;
        }

        if (strcmp(arg, "-v") == 0) {
            /*
             * TODO: when we specify verbose, we should temporarily link kterm
             * into the standard logging register which makes every log go through
             * kterm
             */
            continue;
        }

        if (strcmp(arg, "-h") == 0) {
            should_help = true;
            continue;
        }
    }

    if (should_help) {

        kterm_println("Usage: drvld [flags] [path]");
        kterm_println("Flags:");
        kterm_println(" -u -> Unload the specified driver");
        kterm_println(" -ui -> Unload and uninstall the specified driver");
        kterm_println(" -v -> Be more verbose");
        kterm_println(" -h -> What ur seeing right now");

        return 0;
    }

    driver = get_driver(drv_path);

    if (should_uninstall) {

        if (!driver) {
            kterm_println("Failed to find that driver!");
            return 1;
        }

        error = uninstall_driver(driver);

        if ((error)) {
            kterm_println("Failed to uninstall that driver!");
            return 2;
        }

        return 0;
    }

    if (should_unload) {
        error = unload_driver(drv_path);

        if ((error)) {
            kterm_println("Failed to unload that driver!");
            return 1;
        }

        return 0;
    }

    if (should_install) {
        if (!install_external_driver(drv_path)) {
            kterm_println("Successfully installed driver!");
            return 0;
        }

        return 1;
    }

    /* If this path it to a valid installed driver, load that */
    if (driver && !(load_driver(driver))) {
        kterm_println("Successfully loaded driver!");
        return 0;
    }

    /* Finally, try to load an external driver */
    driver = load_external_driver(drv_path);

    if (!driver) {
        kterm_println("Failed to load that driver!");
        return 1;
    }

    kterm_println("Successfully loaded external driver!");
    return 0;
}

uint32_t kterm_cmd_diskinfo(const char** argv, size_t argc)
{
    uint32_t device_index;
    volume_device_t* device;
    volume_t* this_part;

    if (argc != 2) {
        kterm_println("Invalid args!");
        return 1;
    }

    if (argv[1][0] < '0' || argv[1][0] > '9') {
        kterm_println("Invalid argument!");
        return 1;
    }

    device_index = argv[1][0] - '0';

    device = volume_device_find(device_index);

    if (!device) {
        kterm_println("Could not find that device!");
        return 2;
    }

    const char* path = oss_obj_get_fullpath(device->dev->obj);

    kterm_print_keyvalue("Disk name", device->info.label);
    kterm_print_keyvalue("Disk path", path);
    kterm_print_keyvalue("Disk logical sector size", to_string(device->info.logical_sector_size));
    kterm_print_keyvalue("Disk physical sector size", to_string(device->info.physical_sector_size));
    kterm_print_keyvalue("Disk size", to_string(device->info.max_offset));
    kterm_print_keyvalue("Disk partition type", device->info.type == VOLUME_TYPE_GPT ? "gpt" : (device->info.type == VOLUME_TYPE_MBR ? "mbr" : "unknown"));
    kterm_print_keyvalue("Disk partition count", to_string(device->nr_volumes));

    kfree((void*)path);

    this_part = device->vec_volumes;

    while (this_part) {
        kterm_print_keyvalue("Partition Name", this_part->info.label);
        kterm_print_keyvalue("  \\ sector size", to_string(this_part->info.logical_sector_size));
        kterm_print_keyvalue("  \\ Partition start offset", to_string(this_part->info.min_offset));
        kterm_print_keyvalue("  \\ Partition end offset", to_string(this_part->info.max_offset));

        this_part = this_part->next;
    }

    return 0;
}

static bool procinfo_callback(proc_t* proc)
{
    // kterm_print_keyvalue(proc->m_name, to_string(proc->m_id));
    KLOG("%s (%d)\n", proc->m_name, proc->m_env->attr.ptype);
    return true;
}

/*!
 * @brief: Get a list of current processes
 */
uint32_t kterm_cmd_procinfo(const char** argv, size_t argc)
{
    KLOG("Active procs: %d\n", get_proc_count());
    foreach_proc(procinfo_callback, NULL);
    return 0;
}

static int devinfo_ctl_cb(device_t* dev, struct device_ctl* ctl, struct device_ctl** p_result)
{
    printf("  Ctl 0x%4.4x (Called %d times)\n", device_ctl_get_code(ctl), device_ctl_get_callcount(ctl));
    return 0;
}

uint32_t kterm_cmd_devinfo(const char** argv, size_t argc)
{
    const char* dev_path;
    device_t* dev;

    if (argc != 2)
        return 1;

    dev_path = argv[1];

    if (!dev_path || !dev_path[0])
        return 2;

    dev = device_open(dev_path);

    if (!dev)
        return 3;

    printf("Found device: %s\n", dev->name);
    printf("  Type: %s\n", devinfo_get_ctype(dev->type));
    printf("  Power: %s\n", (dev->flags & DEV_FLAG_POWERED) == DEV_FLAG_POWERED ? "on" : "off");

    foreach_device_ctl(dev->ctlmap, devinfo_ctl_cb, NULL);

    device_close(dev);
    return 0;
}
