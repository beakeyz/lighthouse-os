#include "kterm.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/external.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/kterm/exec.h"
#include "dev/loader.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "fs/core.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/bin/ksyms.h"
#include "libk/data/hashmap.h"
#include "libk/flow/doorbell.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/resource.h"
#include <system/processor/processor.h>
#include <mem/kmem_manager.h>

#include "font.h"

#define KTERM_MAX_BUFFER_SIZE 256

#define KTERM_FB_ADDR (EARLY_FB_MAP_BASE) 
#define KTERM_FONT_HEIGHT 8
#define KTERM_FONT_WIDTH 8

#define KTERM_CURSOR_WIDTH 2

int kterm_init();
int kterm_exit();
uintptr_t kterm_on_packet(aniva_driver_t* driver, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

static void kterm_flush_buffer();
static void kterm_write_char(char c);
static void kterm_process_buffer();

//static void kterm_draw_pixel(uintptr_t x, uintptr_t y, uint32_t color);
static void kterm_draw_pixel_raw(uintptr_t x, uintptr_t y, uint32_t color);
static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uintptr_t color);

static void kterm_draw_cursor();
static const char* kterm_get_buffer_contents();
static uint32_t volatile* kterm_get_pixel_address(uintptr_t x, uintptr_t y);
static void kterm_scroll(uintptr_t lines);

static int kterm_print(const char* msg);
static int kterm_println(const char* msg);

logger_t kterm_logger = {
  .title = "kterm",
  .flags = LOGGER_FLAG_NO_CHAIN,
  .f_logln = kterm_println,
  .f_log = kterm_print,
};

// This driver depends on ps2, so this is legal for now
void kterm_on_key(ps2_key_event_t event);

/* Command buffer */
static kdoor_t __processor_door;
static kdoorbell_t* __kterm_cmd_doorbell;

/* Buffer information */
static uintptr_t __kterm_current_line;
static uintptr_t __kterm_buffer_ptr;
static char __kterm_char_buffer[KTERM_MAX_BUFFER_SIZE];
static char __kterm_stdin_buffer[KTERM_MAX_BUFFER_SIZE];

/* Framebuffer information */
static fb_info_t __kterm_fb_info;

static void kterm_clear_raw()
{
  for (uintptr_t x = 0; x < __kterm_fb_info.width; x++) {
    for (uintptr_t y = 0; y < __kterm_fb_info.height; y++) {
      kterm_draw_pixel_raw(x, y, 00);
    }
  }
}

static void kterm_clear() 
{
  //kterm_doubl_buffer_clear();
  //kterm_swap();

  kterm_clear_raw();

  __kterm_current_line = 0;
  kterm_draw_cursor();
}

void kterm_command_worker() 
{
  char processor_buffer[KTERM_MAX_BUFFER_SIZE];

  init_kdoor(&__processor_door, processor_buffer, sizeof(processor_buffer));

  Must(register_kdoor(__kterm_cmd_doorbell, &__processor_door));

  while (true) {

    if (!kdoor_is_rang(&__processor_door)) {
      scheduler_yield();
      continue;
    }

    /* TODO: actually process cmd */
    char* contents = __processor_door.m_buffer;
    uint32_t buffer_size = __processor_door.m_buffer_size;

    if (!strcmp(contents, "acpitables")) {

      acpi_parser_t* parser;

      get_root_acpi_parser(&parser);

      kterm_print("acpi static table info: \n");
      kterm_print("rsdp address: ");
      kterm_print(to_string(kmem_to_phys(nullptr, (uintptr_t)parser->m_rsdp)));
      kterm_print("\n");
      kterm_print("xsdp address: ");
      kterm_print(to_string(kmem_to_phys(nullptr, (uintptr_t)parser->m_xsdp)));
      kterm_print("\n");
      kterm_print("is xsdp: ");
      kterm_print(parser->m_is_xsdp ? "true\n" : "false\n");
      kterm_print("rsdp discovery method: ");
      kterm_print(parser->m_rsdp_discovery_method.m_name);
      kterm_print("\n");
      kterm_print("tables found: ");
      kterm_print(to_string(parser->m_tables->m_length));
      kterm_print("\n");
    } else if (!strcmp(contents, "help")) {
      kterm_print("available commands: \n");
      kterm_print(" - help: print some helpful info\n");
      kterm_print(" - acpitables: print the acpi tables present in the system\n");
      kterm_print(" - ztest: spawn a zone allocator and test it\n");
      kterm_print(" - testramdisk: spawn a ramdisk and test it\n");
      kterm_print(" - exit: panic the kernel\n");
    } else if (!strcmp(contents, "clear")) {
      kterm_clear();
    } else if (!strcmp(contents, "exit")) {

      /* NOTE this is just to show that this system works =D */
      void (*pnc)(char*) = (FuncPtr)get_ksym_address("kernel_panic");

      pnc("TODO: exit/shutdown");

    } else if (!strcmp(contents, "ztest")) {
      zone_allocator_t* allocator = create_zone_allocator_ex(nullptr, 0, 4 * Kib, sizeof(uintptr_t), 0);

      uintptr_t* test_data = zalloc(allocator, sizeof(uintptr_t));
      *test_data = 6969;

      kterm_print("Allocated 8 bytes: ");
      kterm_print(to_string(*test_data));

      uintptr_t* test_data2 = zalloc(allocator, sizeof(uintptr_t));
      *test_data2 = 420;

      kterm_print("\nAllocated 8 bytes: ");
      kterm_print(to_string(*test_data2));

      zfree(allocator, test_data, sizeof(uintptr_t));

      kterm_print("\nDeallocated 8 bytes: ");
      kterm_print(to_string(*test_data));

      zfree(allocator, test_data2, sizeof(uintptr_t));

      kterm_print("\nDeallocated 8 bytes: ");
      kterm_print(to_string(*test_data2));

      kterm_print("\n Total zone allocator size: ");
      kterm_print(to_string(allocator->m_total_size));

      kterm_print("\nSuccessfully created Zone!\n");

      destroy_zone_allocator(allocator, true);

      kterm_print("\nSuccessfully destroyed Zone!\n");

    } else if (!strcmp(contents, "testramdisk")) {

      kterm_print("Finding node...\n");
      vobj_t* obj = vfs_resolve("Root/init");

      ASSERT_MSG(obj, "Could not get vobj from test");
      ASSERT_MSG(obj->m_type == VOBJ_TYPE_FILE, "Object was not a file!");
      file_t* file = vobj_get_file(obj);
      ASSERT_MSG(file, "Could not get file from test");

      kterm_print("File size: ");
      kterm_print(to_string(file->m_buffer_size));
      kterm_print("\n");

      println("Trying to make proc");
      Must(elf_exec_static_64_ex(file, false, false));
      println("Made proc");

    } else {
      kterm_try_exec(contents, buffer_size);
    }

    /* Clear the buffer ourselves */
    memset(__processor_door.m_buffer, 0, __processor_door.m_buffer_size);

    Must(kdoor_reset(&__processor_door));
  }
}

aniva_driver_t g_base_kterm_driver = {
  .m_name = "kterm",
  .m_type = DT_OTHER,
  .f_init = kterm_init,
  .f_exit = kterm_exit,
  .f_msg = kterm_on_packet,
  .m_dependencies = {"graphics/efifb", "io/ps2_kb"},
  .m_dep_count = 2,
};
EXPORT_DRIVER_PTR(g_base_kterm_driver);

static int kterm_write(aniva_driver_t* d, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  char* str = (char*)buffer;

  /* Make sure the end of the buffer is null-terminated and the start is non-null */
  if (str[*buffer_size] != NULL || str[0] == NULL)
    return DRV_STAT_INVAL;

  /* TODO; string.h: char validation */

  kterm_print(str);

  return DRV_STAT_OK;
}

static int kterm_read(aniva_driver_t* d, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  (void)offset;

  /* Ew */
  if (!buffer || !buffer_size || !(*buffer_size))
    return DRV_STAT_INVAL;

  /* Wait until we have shit to read */
  while (*__kterm_stdin_buffer == NULL)
    scheduler_yield();

  /* Make sure we don't transfer garbo */
  memset(buffer, NULL, *buffer_size);

  /* Set the buffersize to our string in preperation for the copy */
  *buffer_size = strlen(__kterm_stdin_buffer);

  /* Yay, copy */
  memcpy(buffer, __kterm_stdin_buffer, *buffer_size);

  /* Reset stdin_buffer */
  memset(__kterm_stdin_buffer, NULL, sizeof(__kterm_stdin_buffer));

  return DRV_STAT_OK;
}

/*
 * The aniva kterm driver is a text-based terminal program that runs directly in kernel-mode. Any processes it 
 * runs get a handle to the driver as stdin, stdout and stderr, with room for any other handle
 */
int kterm_init() {

  __kterm_current_line = 0;
  __kterm_cmd_doorbell = create_doorbell(1, NULL);

  ASSERT_MSG(driver_manifest_write(&g_base_kterm_driver, kterm_write), "Failed to manifest kterm write");
  ASSERT_MSG(driver_manifest_read(&g_base_kterm_driver, kterm_read), "Failed to manifest kterm read");

  /* Zero out the cmd buffer */
  memset(__kterm_stdin_buffer, 0, sizeof(__kterm_stdin_buffer));
  memset(__kterm_char_buffer, 0, sizeof(__kterm_char_buffer));

  /* TODO: when we implement our HID driver, this should be replaced with a event endpoint we can probe */
  // register our keyboard listener
  driver_send_msg_sync("io/ps2_kb", KB_REGISTER_CALLBACK, kterm_on_key, sizeof(uintptr_t));

  /* FIXME: integrate video device integration */
  // map our framebuffer
  size_t size;
  uintptr_t ptr = KTERM_FB_ADDR;

  viddev_mapfb_t fb_map = {
    .size = &size,
    .virtual_base = ptr,
  };

  /*
   * This is the wrong way of getting information to the graphics API, because it is not a given that
   * the loaded graphics driver is indeed efifb. It might very well be virtio or some other nonsense. 
   * In that case, we need a function that gives us the current graphics driver path
   */
  driver_send_msg("graphics/efifb", VIDDEV_DCC_MAPFB, &fb_map, sizeof(fb_map));
  driver_send_msg_a("graphics/efifb", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &__kterm_fb_info, sizeof(fb_info_t));

  /* TODO: we should probably have some kind of kernel-managed structure for async work */
  Must(spawn_thread("kterm_cmd_worker", kterm_command_worker, NULL));

  /* Register our logger to the logger subsys */
  register_logger(&kterm_logger);

  /* Make sure we are the logger with the highest prio */
  logger_swap_priority(kterm_logger.id, 0);

  // flush our terminal buffer
  kterm_flush_buffer();

  kterm_draw_cursor();

  //memset((void*)KTERM_FB_ADDR, 0, kterm_fb_info.used_pages * SMALL_PAGE_SIZE);
  kterm_print("\n");
  kterm_print(" -- Welcome to the aniva kterm driver --\n");
  processor_t* processor = get_current_processor();

  kterm_print(processor->m_info.m_vendor_id);
  kterm_print("\n");
  kterm_print(processor->m_info.m_model_id);
  kterm_print("\n");
  kterm_print("Available cores: ");
  kterm_print(to_string(processor->m_info.m_max_available_cores));
  kterm_print("\n");

  return 0;
}

int kterm_exit() {
  // TODO: unmap framebuffer
  kernel_panic("kterm_exit");
  return 0;
}

uintptr_t kterm_on_packet(aniva_driver_t* driver, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size) {

  switch (code) {
    case KTERM_DRV_DRAW_STRING: 
      {
        const char* str = *(const char**)buffer;
        size_t buffer_size = size;

        /* Make sure the end of the buffer is null-terminated and the start is non-null */
        if (str[buffer_size] != NULL || str[0] == NULL)
          return DRV_STAT_INVAL;

        println(str);
        kterm_print(str);
        //kterm_println("\n");
        break;
      }
    case KTERM_DRV_CLEAR:
      {
        kterm_clear();
      }
  }

  return DRV_STAT_OK;
}

void kterm_on_key(ps2_key_event_t event) {

  if (!doorbell_has_door(__kterm_cmd_doorbell, 0))
    return;

  if (event.m_pressed)
    kterm_write_char(event.m_typed_char);
}

static void kterm_flush_buffer() {
  memset(__kterm_char_buffer, 0, sizeof(char) * KTERM_MAX_BUFFER_SIZE);
  __kterm_buffer_ptr = 0;
}

static void kterm_write_char(char c) {
  if (__kterm_buffer_ptr >= KTERM_MAX_BUFFER_SIZE) {
    // time to flush the buffer
    return;
  }

  switch (c) {
    case '\b':
      if (__kterm_buffer_ptr > KTERM_CURSOR_WIDTH) {
        __kterm_buffer_ptr--;
        __kterm_char_buffer[__kterm_buffer_ptr] = NULL;
        kterm_draw_char(__kterm_buffer_ptr * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, 0, 0x00);
      } else {
        __kterm_buffer_ptr = KTERM_CURSOR_WIDTH;
      }
      break;
    case (char)0x0A:
      /* Enter */
      kterm_process_buffer();
      break;
    default:
      if (c >= 0x20) {
        kterm_draw_char(__kterm_buffer_ptr * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, c, 0xFFFFFFFF);
        __kterm_char_buffer[__kterm_buffer_ptr] = c;
        __kterm_buffer_ptr++;
      }
      break;
  }
}

/*!
 * @brief Process an incomming command
 *
 * TODO: should we queue incomming commands with a more sophisticated context handeling
 * system?
 */
static void kterm_process_buffer() {
  char buffer[KTERM_MAX_BUFFER_SIZE];
  size_t buffer_size;

  /* Copy the buffer localy, so we can clear it early */
  const char* contents = kterm_get_buffer_contents();
  buffer_size = strlen(contents);

  memcpy(buffer, contents, buffer_size);

  if (buffer_size >= KTERM_MAX_BUFFER_SIZE) {
    /* Bruh, we cant process this... */
    return;
  }

  /* Make sure we add the newline so we also flush the char buffer */
  kterm_print("\n");

  /* Redirect to the stdin buffer when there is an active command still */
  if (kdoor_is_rang(&__processor_door)) {
    memset(__kterm_stdin_buffer, 0, sizeof(__kterm_stdin_buffer));
    memcpy(__kterm_stdin_buffer, buffer, buffer_size);

    __kterm_stdin_buffer[buffer_size] = '\n';
    return;
  }

  doorbell_write(__kterm_cmd_doorbell, buffer, buffer_size, 0);

  doorbell_ring(__kterm_cmd_doorbell);
}

static void kterm_draw_cursor() {
  kterm_draw_char(0 * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '>', 0xFFFFFFFF);
  kterm_draw_char(1 * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, ' ', 0xFFFFFFFF);
  __kterm_buffer_ptr = KTERM_CURSOR_WIDTH;
}

static void kterm_draw_pixel_raw(uintptr_t x, uintptr_t y, uint32_t color) {
  if (__kterm_fb_info.pitch == 0 || __kterm_fb_info.bpp == 0)
    return;

  if (x >= 0 && y >= 0 && x < __kterm_fb_info.width && y < __kterm_fb_info.height) {
    *(uint32_t volatile*)(KTERM_FB_ADDR + __kterm_fb_info.pitch * y + x * __kterm_fb_info.bpp / 8) = color;
  }
}

static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uintptr_t color) {
  char* glyph = font8x8_basic[(uint32_t)c];
  for (uintptr_t _y = 0; _y < KTERM_FONT_HEIGHT; _y++) {
    for (uintptr_t _x = 0; _x < KTERM_FONT_WIDTH; _x++) {
      kterm_draw_pixel_raw(x + _x, y + _y, 0x00);

      if (glyph[_y] & (1 << _x)) {
        kterm_draw_pixel_raw(x + _x, y + _y, color);
      }
    }
  }
}

static const char* kterm_get_buffer_contents() {
  uintptr_t index = 0;

  while (!__kterm_char_buffer[index]) {
    index++;
  }

  return &__kterm_char_buffer[index];
}

static int kterm_println(const char* msg) {
  kterm_print(msg);
  kterm_print("\n");

  return 0;
}

static int kterm_print(const char* msg) {

  uintptr_t index = 0;
  uintptr_t kterm_buffer_ptr_copy = __kterm_buffer_ptr;
  while (msg[index]) {
    char current_char = msg[index];
    if (current_char == '\n') {
      __kterm_current_line++;

      if (__kterm_current_line * KTERM_FONT_HEIGHT >= __kterm_fb_info.height) {
        kterm_scroll(1);
      }

      kterm_flush_buffer();
      kterm_draw_cursor();
      kterm_buffer_ptr_copy = KTERM_CURSOR_WIDTH;
    } else {
      kterm_draw_char(kterm_buffer_ptr_copy * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, current_char, 0xFFFFFFFF);
      kterm_buffer_ptr_copy++;

      if (kterm_buffer_ptr_copy * KTERM_FONT_WIDTH > __kterm_fb_info.width) {
        __kterm_current_line++;
        kterm_buffer_ptr_copy = 0;
      }
    }
    index++;
  }
  __kterm_buffer_ptr = kterm_buffer_ptr_copy;

  return 0;
}

/*
void println_kterm(const char* msg) {

  if (!driver_is_ready(get_driver("other/kterm"))) {
    return;
  }

  kterm_print(msg);
  kterm_print("\n");
}
*/

// TODO: add a scroll direction (up, down, left, ect)
// FIXME: scrolling still gives weird artifacts
static void kterm_scroll(uintptr_t lines) 
{
  volatile uint32_t* top = kterm_get_pixel_address(0, 0);
  (void)top;
  __kterm_current_line -= 3;
  kernel_panic("TODO: implement scrolling");
}

static uint32_t volatile* kterm_get_pixel_address(uintptr_t x, uintptr_t y) {
  if (__kterm_fb_info.pitch == 0 || __kterm_fb_info.bpp == 0)
    return 0;

  if (x >= 0 && y >= 0 && x < __kterm_fb_info.width && y < __kterm_fb_info.height) {
    return (uint32_t volatile*)((vaddr_t)KTERM_FB_ADDR + __kterm_fb_info.pitch * y + x * __kterm_fb_info.bpp / 8);
  }
  return 0;
}

