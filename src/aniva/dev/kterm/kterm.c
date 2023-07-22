#include "kterm.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/manifest.h"
#include "fs/core.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "interrupts/interrupts.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "libk/string.h"
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

/* Command processing information */
typedef struct {
  /* Commands can't be longer than 128 bytes :clown: */
  char buffer[KTERM_MAX_BUFFER_SIZE];
} kterm_cmd_t;

mutex_t* s_kterm_cmd_lock;
kterm_cmd_t s_kterm_cmd_buffer;

int kterm_init();
int kterm_exit();
uintptr_t kterm_on_packet(packet_payload_t payload, packet_response_t** response);

static void kterm_flush_buffer();
static void kterm_write_char(char c);
static void kterm_process_buffer();

static void kterm_draw_pixel(uintptr_t x, uintptr_t y, uint32_t color);
static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uintptr_t color);

static void kterm_draw_cursor();
static const char* kterm_get_buffer_contents();
static vaddr_t kterm_get_pixel_address(uintptr_t x, uintptr_t y);
static void kterm_scroll(uintptr_t lines);
static void kterm_println(const char* msg);

// This driver depends on ps2, so this is legal for now
void kterm_on_key(ps2_key_event_t event);

/* Buffer information */
static uintptr_t kterm_current_line;
static uintptr_t kterm_buffer_ptr;
static char kterm_char_buffer[KTERM_MAX_BUFFER_SIZE];

static fb_info_t kterm_fb_info;

void kterm_command_worker() {

  for (;;) {

    if (s_kterm_cmd_buffer.buffer[0] != 0) {
      mutex_lock(s_kterm_cmd_lock);

      /* TODO: process cmd */
      const char* contents = s_kterm_cmd_buffer.buffer;

      kterm_println("\n");

      if (!strcmp(contents, "acpitables")) {
        kterm_println("acpi static table info: \n");
        kterm_println("rsdp address: ");
        kterm_println(to_string(kmem_to_phys(nullptr, (uintptr_t)g_parser_ptr->m_rsdp)));
        kterm_println("\n");
        kterm_println("xsdp address: ");
        kterm_println(to_string(kmem_to_phys(nullptr, (uintptr_t)g_parser_ptr->m_xsdp)));
        kterm_println("\n");
        kterm_println("is xsdp: ");
        kterm_println(g_parser_ptr->m_is_xsdp ? "true\n" : "false\n");
        kterm_println("rsdp discovery method: ");
        kterm_println(g_parser_ptr->m_rsdp_discovery_method.m_name);
        kterm_println("\n");
        kterm_println("tables found: ");
        kterm_println(to_string(g_parser_ptr->m_tables->m_length));
      } else if (!strcmp(contents, "help")) {
        kterm_println("available commands: \n");
        kterm_println(" - help: print some helpful info\n");
        kterm_println(" - acpitables: print the acpi tables present in the system\n");
        kterm_println(" - ztest: spawn a zone allocator and test it\n");
        kterm_println(" - testramdisk: spawn a ramdisk and test it\n");
        kterm_println(" - exit: panic the kernel");
      } else if (!strcmp(contents, "exit")) {
        kernel_panic("TODO: exit/shutdown");
      } else if (!strcmp(contents, "ztest")) {
        zone_allocator_t* allocator = create_zone_allocator_ex(nullptr, 0, 4 * Kib, sizeof(uintptr_t), 0);

        uintptr_t* test_data = zalloc(allocator, sizeof(uintptr_t));
        *test_data = 6969;

        kterm_println("Allocated 8 bytes: ");
        kterm_println(to_string(*test_data));

        uintptr_t* test_data2 = zalloc(allocator, sizeof(uintptr_t));
        *test_data2 = 420;

        kterm_println("\nAllocated 8 bytes: ");
        kterm_println(to_string(*test_data2));

        zfree(allocator, test_data, sizeof(uintptr_t));

        kterm_println("\nDeallocated 8 bytes: ");
        kterm_println(to_string(*test_data));

        zfree(allocator, test_data2, sizeof(uintptr_t));

        kterm_println("\nDeallocated 8 bytes: ");
        kterm_println(to_string(*test_data2));

        kterm_println("\n Total zone allocator size: ");
        kterm_println(to_string(allocator->m_total_size));

        kterm_println("\nSuccessfully created Zone!\n");

        destroy_zone_allocator(allocator, true);

        kterm_println("\nSuccessfully destroyed Zone!\n");

      } else if (!strcmp(contents, "testramdisk")) {

        kterm_println("Finding node...\n");
        vobj_t* obj = vfs_resolve("Root/init");

        ASSERT_MSG(obj, "Could not get vobj from test");
        ASSERT_MSG(obj->m_type == VOBJ_TYPE_FILE, "Object was not a file!");
        file_t* file = vobj_get_file(obj);
        ASSERT_MSG(file, "Could not get file from test");

        kterm_println("File size: ");
        kterm_println(to_string(file->m_buffer_size));
        kterm_println("\n");

        println("Trying to make proc");
        Must(elf_exec_static_64_ex(file, false, false));
        println("Made proc");

      } else {

        proc_t* p;

        println(contents);

        if (contents[0] == NULL)
          goto end_processing;

        vobj_t* obj = vfs_resolve(contents);

        if (!obj) {
          kterm_println("Could not find object!");
          goto end_processing;
        }

        file_t* file = vobj_get_file(obj);

        if (!file) {
          kterm_println("Could not execute object!");
          goto end_processing;
        }

        ErrorOrPtr result = elf_exec_static_64_ex(file, false, true);

        if (IsError(result)) {
          kterm_println("Coult not execute object!");
          vobj_close(obj);
          goto end_processing;
        }

        p = (proc_t*)Release(result);

        vobj_close(obj);

        khandle_type_t driver_type = KHNDL_TYPE_DRIVER;

        khandle_t _stdin;
        khandle_t _stdout;
        khandle_t _stderr;

        create_khandle(&_stdin, &driver_type, &g_base_kterm_driver);
        create_khandle(&_stdout, &driver_type, &g_base_kterm_driver);
        create_khandle(&_stderr, &driver_type, &g_base_kterm_driver);

        _stdin.flags |= HNDL_FLAG_READACCESS;
        _stdout.flags |= HNDL_FLAG_WRITEACCESS;
        _stderr.flags |= HNDL_FLAG_RW;

        bind_khandle(&p->m_handle_map, &_stdin);
        bind_khandle(&p->m_handle_map, &_stdout);
        bind_khandle(&p->m_handle_map, &_stderr);

        /*
         * Create mapping to ensure this process has access to the framebuffer when doing syscalls 
         * NOTE: this is probably temporary
         */
        Must(__kmem_alloc_ex(
              p->m_root_pd.m_root,
              p,
              kterm_fb_info.paddr,
              KTERM_FB_ADDR,
              kterm_fb_info.used_pages * SMALL_PAGE_SIZE,
              KMEM_CUSTOMFLAG_NO_REMAP,
              KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL
              ));

        /* Apply the flags to our resource */
        resource_apply_flags(KTERM_FB_ADDR, kterm_fb_info.used_pages * SMALL_PAGE_SIZE, KRES_FLAG_MEM_KEEP_PHYS, GET_RESOURCE(p->m_resource_bundle, KRES_TYPE_MEM));

        debug_resources(p->m_resource_bundle, KRES_TYPE_MEM);

        sched_add_priority_proc(p, true);
      }

end_processing:
      kterm_println("\n");

      memset(&s_kterm_cmd_buffer, 0, sizeof(s_kterm_cmd_buffer));

      mutex_unlock(s_kterm_cmd_lock);
    }

    scheduler_yield();
  }
}

aniva_driver_t g_base_kterm_driver = {
  .m_name = "kterm",
  .m_type = DT_OTHER,
  .f_init = kterm_init,
  .f_exit = kterm_exit,
  .f_drv_msg = kterm_on_packet,
  .m_dependencies = {"graphics/fb", "io/ps2_kb"},
  .m_dep_count = 2,
};
EXPORT_DRIVER(g_base_kterm_driver);

static int kterm_write(aniva_driver_t* d, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  char* str = (char*)buffer;

  /* Make sure the end of the buffer is null-terminated and the start is non-null */
  if (str[*buffer_size] != NULL || str[0] == NULL)
    return DRV_STAT_INVAL;

  /* TODO; string.h: char validation */

  kterm_println(str);

  return DRV_STAT_OK;
}

static int kterm_read(aniva_driver_t* d, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  kernel_panic("kterm_read(TODO: impl)");
}

/*
 * The aniva kterm driver is a text-based terminal program that runs directly in kernel-mode. Any processes it 
 * runs get a handle to the driver as stdin, stdout and stderr, with room for any other handle
 */
int kterm_init() {

  kterm_current_line = 0;
  s_kterm_cmd_lock = create_mutex(0);

  ASSERT_MSG(driver_manifest_write(&g_base_kterm_driver, kterm_write), "Failed to manifest kterm write");
  ASSERT_MSG(driver_manifest_read(&g_base_kterm_driver, kterm_read), "Failed to manifest kterm read");

  /* Zero out the cmd buffer */
  memset(&s_kterm_cmd_buffer, 0, sizeof(kterm_cmd_t));

  // register our keyboard listener
  destroy_packet_response(driver_send_packet_sync("io/ps2_kb", KB_REGISTER_CALLBACK, kterm_on_key, sizeof(uintptr_t)));

  // map our framebuffer
  uintptr_t ptr = KTERM_FB_ADDR;
  destroy_packet_response(driver_send_packet_sync("graphics/fb", FB_DRV_MAP_FB, &ptr, sizeof(uintptr_t)));

  packet_response_t* response = driver_send_packet_sync("graphics/fb", FB_DRV_GET_FB_INFO, NULL, 0);
  if (response) {
    kterm_fb_info = *(fb_info_t*)response->m_response_buffer;
    destroy_packet_response(response);
  }

  /* TODO: we should probably have some kind of kernel-managed structure for async work */
  Must(spawn_thread("Command worker", kterm_command_worker, NULL));
  println("Spawned thread");

  /*
   * We could create a kevent hook for every process creation, which
   * could then automatically map the framebuffer for us
   */
  //create_keventhook

  // flush our terminal buffer
  kterm_flush_buffer();

  kterm_draw_cursor();

  //memset((void*)KTERM_FB_ADDR, 0, kterm_fb_info.used_pages * SMALL_PAGE_SIZE);
  kterm_println("\n");
  kterm_println(" -- Welcome to the aniva kterm driver --\n");
  Processor_t* processor = get_current_processor();

  kterm_println(processor->m_info.m_vendor_id);
  kterm_println("\n");
  kterm_println(processor->m_info.m_model_id);
  kterm_println("\n");
  kterm_println("Available cores: ");
  kterm_println(to_string(processor->m_info.m_max_available_cores));
  kterm_println("\n");

  return 0;
}

int kterm_exit() {
  // TODO: unmap framebuffer
  kernel_panic("kterm_exit");
  return 0;
}

uintptr_t kterm_on_packet(packet_payload_t payload, packet_response_t** response) {

  switch (payload.m_code) {
    case KTERM_DRV_DRAW_STRING: 
      {
        const char* str = *(const char**)payload.m_data;
        size_t buffer_size = payload.m_data_size;

        /* Make sure the end of the buffer is null-terminated and the start is non-null */
        if (str[buffer_size] != NULL || str[0] == NULL)
          return DRV_STAT_INVAL;

        println(str);
        kterm_println(str);
        //kterm_println("\n");
        break;
      }
  }

  return DRV_STAT_OK;
}

void kterm_on_key(ps2_key_event_t event) {
  if (event.m_pressed)
    kterm_write_char(event.m_typed_char);
}

static void kterm_flush_buffer() {
  memset(kterm_char_buffer, 0, sizeof(char) * KTERM_MAX_BUFFER_SIZE);
  kterm_buffer_ptr = 0;
}

static void kterm_write_char(char c) {
  if (kterm_buffer_ptr >= KTERM_MAX_BUFFER_SIZE) {
    // time to flush the buffer
    return;
  }

  switch (c) {
    case '\b':
      if (kterm_buffer_ptr > KTERM_CURSOR_WIDTH) {
        kterm_buffer_ptr--;
        kterm_draw_char(kterm_buffer_ptr * KTERM_FONT_WIDTH, kterm_current_line * KTERM_FONT_HEIGHT, 0, 0x00);
      } else {
        kterm_buffer_ptr = KTERM_CURSOR_WIDTH;
      }
      break;
    case (char)0x0A:
      kterm_process_buffer();
      break;
    default:
      if (c >= 0x20) {
        kterm_draw_char(kterm_buffer_ptr * KTERM_FONT_WIDTH, kterm_current_line * KTERM_FONT_HEIGHT, c, 0xFFFFFFFF);
        kterm_char_buffer[kterm_buffer_ptr] = c;
        kterm_buffer_ptr++;
      }
      break;
  }
}

static void kterm_process_buffer() {
  const char* contents = kterm_get_buffer_contents();
  const size_t contents_size = strlen(contents);

  if (contents_size >= KTERM_MAX_BUFFER_SIZE) {
    /* Bruh, we cant process this... */
    return;
  }

  if (mutex_is_locked(s_kterm_cmd_lock))
    return;

  println("Locking");
  mutex_lock(s_kterm_cmd_lock);

  println("Locked");

  memcpy(s_kterm_cmd_buffer.buffer, contents, contents_size);

  mutex_unlock(s_kterm_cmd_lock);
}

static void kterm_draw_cursor() {
  kterm_draw_char(0 * KTERM_FONT_WIDTH, kterm_current_line * KTERM_FONT_HEIGHT, '>', 0xFFFFFFFF);
  kterm_draw_char(1 * KTERM_FONT_WIDTH, kterm_current_line * KTERM_FONT_HEIGHT, ' ', 0xFFFFFFFF);
  kterm_buffer_ptr = KTERM_CURSOR_WIDTH;
}

static void kterm_draw_pixel(uintptr_t x, uintptr_t y, uint32_t color) {
  if (kterm_fb_info.pitch == 0 || kterm_fb_info.bpp == 0)
    return;

  if (x >= 0 && y >= 0 && x < kterm_fb_info.width && y < kterm_fb_info.height) {
    *(uint32_t*)(KTERM_FB_ADDR + kterm_fb_info.pitch * y + x * kterm_fb_info.bpp / 8) = color;
  }
}

static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uintptr_t color) {
  char* glyph = font8x8_basic[c];
  for (uintptr_t _y = 0; _y < KTERM_FONT_HEIGHT; _y++) {
    for (uintptr_t _x = 0; _x < KTERM_FONT_WIDTH; _x++) {
      kterm_draw_pixel(x + _x, y + _y, 0x00);

      if (glyph[_y] & (1 << _x)) {
        kterm_draw_pixel(x + _x, y + _y, color);
      }
    }
  }
}

static const char* kterm_get_buffer_contents() {
  uintptr_t index = 0;

  while (!kterm_char_buffer[index]) {
    index++;
  }

  return &kterm_char_buffer[index];
}

static void kterm_println(const char* msg) {

  uintptr_t index = 0;
  uintptr_t kterm_buffer_ptr_copy = kterm_buffer_ptr;
  while (msg[index]) {
    char current_char = msg[index];
    if (current_char == '\n') {
      kterm_current_line++;

      if (kterm_current_line * KTERM_FONT_HEIGHT >= kterm_fb_info.height) {
        kterm_scroll(1);
      }

      kterm_flush_buffer();
      kterm_draw_cursor();
      kterm_buffer_ptr_copy = KTERM_CURSOR_WIDTH;
    } else {
      kterm_draw_char(kterm_buffer_ptr_copy * KTERM_FONT_WIDTH, kterm_current_line * KTERM_FONT_HEIGHT, current_char, 0xFFFFFFFF);
      kterm_buffer_ptr_copy++;

      if (kterm_buffer_ptr_copy * KTERM_FONT_WIDTH > kterm_fb_info.width) {
        kterm_current_line++;
        kterm_buffer_ptr_copy = 0;
      }
    }
    index++;
  }
  kterm_buffer_ptr = kterm_buffer_ptr_copy;
}

void println_kterm(const char* msg) {

  if (!driver_is_ready(&g_base_kterm_driver)) {
    return;
  }

  const size_t msg_len = strlen(msg);

  kterm_println(msg);
  kterm_println("\n");
}

// TODO: add a scroll direction (up, down, left, ect)
// TODO: scrolling still gives weird artifacts
// FIXME: very fucking slow
static void kterm_scroll(uintptr_t lines) {
  vaddr_t screen_top = kterm_get_pixel_address(0, 0);
  vaddr_t screen_end = kterm_get_pixel_address(kterm_fb_info.width - 1, kterm_fb_info.height - 1);
  vaddr_t scroll_top = kterm_get_pixel_address(0, lines * KTERM_FONT_HEIGHT);
  vaddr_t scroll_end = kterm_get_pixel_address(kterm_fb_info.width - 1, kterm_fb_info.height - lines * KTERM_FONT_HEIGHT - 1);
  size_t scroll_size = screen_end - scroll_top;

  memcpy((void*)screen_top, (void*)scroll_top, scroll_size);
  memset((void*)scroll_end, 0, screen_end - scroll_end);
  kterm_current_line--;
}

static vaddr_t kterm_get_pixel_address(uintptr_t x, uintptr_t y) {
  if (kterm_fb_info.pitch == 0 || kterm_fb_info.bpp == 0)
    return 0;

  if (x >= 0 && y >= 0 && x < kterm_fb_info.width && y < kterm_fb_info.height) {
    return (vaddr_t)(KTERM_FB_ADDR + kterm_fb_info.pitch * y + x * kterm_fb_info.bpp / 8);
  }
  return 0;
}

