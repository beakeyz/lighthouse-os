#include "util.h"
#include "dev/core.h"
#include "dev/disk/generic.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "drivers/util/kterm/kterm.h"
#include "entry/entry.h"
#include "fs/vobj.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include <dev/external.h>
#include <dev/loader.h>

static const char* __help_str = 
"Welcome to the Aniva kernel terminal application (kterm)\n"
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
static inline uint32_t __page_count_to_mib(uint32_t page_count)
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

  kterm_print_keyvalue("physical memory free", to_string(__page_count_to_mib(km_info.free_pages)));
  kterm_print_keyvalue("physical memory used", to_string(__page_count_to_mib(km_info.used_pages)));

  return 0;
}

bool print_drv_info(hive_t* _, void* _manifest)
{
  dev_manifest_t* manifest = _manifest;

  if (!manifest)
    return false;

  kterm_print_keyvalue("Path", manifest->m_url);
  kterm_print_keyvalue("Binary path", manifest->m_driver_file_path);
  if (!manifest->m_handle)
    kterm_print_keyvalue("Name", NULL);
  else
    kterm_print_keyvalue("Name", manifest->m_handle->m_name);
  kterm_print_keyvalue("Loaded", ((manifest->m_flags & DRV_LOADED) == DRV_LOADED) ? "Yes" : "No");

  kterm_println("");

  return true;
}

/*!
 * @brief: Print information about the installed and loaded drivers
 */
uint32_t kterm_cmd_drvinfo(const char** argv, size_t argc)
{
  kterm_println(" - Printing drivers...");

  foreach_driver(print_drv_info);

  return 0;
}

/*!
 * @brief: kinda like neofetch, but fun
 */
uint32_t kterm_cmd_hello(const char** argv, size_t argc)
{
  kterm_println("   ,----,        ");
  kterm_println("  /  .'  \\       ");
  kterm_println(" /  ;     \\     ");
  kterm_println("|  |       \\    ");
  kterm_println("|  |   /\\   \\   ");
  kterm_println("|  |  /; \\   \\  ");
  kterm_println("|  |  |/  \\   \\ ");
  kterm_println("|  |  | \\  \\ ,' ");
  kterm_println("|  |  |  '--'   ");
  kterm_println("|  |  |         ");
  kterm_println("|  | ,'         ");
  kterm_println("`--''           ");
  kterm_println("");
  kterm_println("(Aniva): Hello to you too =) ");

  (void)get_driver("other/kterm");

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
  ErrorOrPtr result;
  extern_driver_t* driver;
  dev_manifest_t* manifest;
  const char* drv_path = nullptr;
  bool should_unload = false;
  bool should_uninstall = false;
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

  if (should_uninstall) {

    manifest = get_driver(drv_path);

    if (!manifest) {
      kterm_println("Failed to find that driver!");
      return 1;
    }

    result = uninstall_driver(manifest);

    if (IsError(result)) {
      kterm_println("Failed to uninstall that driver!");
      return 2;
    }

    return 0;
  } 

  if (should_unload) {
    result = unload_driver(drv_path);

    if (IsError(result)) {
      kterm_println("Failed to unload that driver!");
      return 1;
    }

    return 0;
  }

  driver = load_external_driver(drv_path);

  if (!driver) {
    kterm_println("Failed to load that driver!");
    return 1;
  }

  return 0;
}

uint32_t kterm_cmd_diskinfo(const char** argv, size_t argc)
{
  uint32_t device_index;
  disk_dev_t* device;
  partitioned_disk_dev_t* this_part;

  if (argc != 2) {
    kterm_println("Invalid args!");
    return 1;
  }

  if (argv[1][0] < '0' || argv[1][0] > '9') {
    kterm_println("Invalid argument!");
    return 1;
  }

  device_index = argv[1][0] - '0';

  device = find_gdisk_device(device_index);

  if (!device) {
    kterm_println("Could not find that device!");
    return 2;
  }

  kterm_print_keyvalue("Disk name", device->m_device_name);
  kterm_print_keyvalue("Disk path", device->m_path);
  kterm_print_keyvalue("Disk logical sector size", to_string(device->m_logical_sector_size));
  kterm_print_keyvalue("Disk physical sector size", to_string(device->m_physical_sector_size));
  kterm_print_keyvalue("Disk size", to_string(device->m_max_blk * device->m_logical_sector_size));
  kterm_print_keyvalue("Disk partition type", device->m_partition_type == PART_TYPE_GPT ? "gpt" : (device->m_partition_type == PART_TYPE_MBR ? "mbr" : "unknown"));
  kterm_print_keyvalue("Disk partition count", to_string(device->m_partitioned_dev_count));

  this_part = device->m_devs;

  while (this_part) {
    kterm_print_keyvalue("Partition Name", this_part->m_name);
    kterm_print_keyvalue("  \\ sector size", to_string(this_part->m_block_size));
    kterm_print_keyvalue("  \\ Partition start lba", to_string(this_part->m_start_lba));
    kterm_print_keyvalue("  \\ Partition end lba", to_string(this_part->m_end_lba));

    this_part = this_part->m_next;
  }

  return 0;
}

static bool procinfo_callback(proc_t* proc)
{
  kterm_print_keyvalue(proc->m_name, to_string(proc->m_id));
  return true;
}

/*!
 * @brief: Get a list of current processes
 */
uint32_t kterm_cmd_procinfo(const char** argv, size_t argc)
{
  println("YAY");
  kterm_print_keyvalue("Active procs", to_string(get_proc_count()));
  foreach_proc(procinfo_callback);
  return 0;
}
