#include "kterm.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/external.h"
#include "dev/io/ps2/kbd.h"
#include "dev/loader.h"
#include "dev/manifest.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "drivers/util/kterm/util.h"
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
#include "mem/malloc.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/acpi/acpi.h"
#include "system/acpi/parser.h"
#include "system/resource.h"
#include <system/processor/processor.h>
#include <mem/kmem_manager.h>
#include <dev/video/message.h>

#include "font.h"
#include "exec.h"

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
static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uintptr_t color);

static void kterm_draw_cursor();
static const char* kterm_get_buffer_contents();
static uint32_t volatile* kterm_get_pixel_address(uintptr_t x, uintptr_t y);
static void kterm_scroll(uintptr_t lines);

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
static thread_t* __kterm_worker_thread;

/* Buffer information */
static uintptr_t __kterm_current_line;
static uintptr_t __kterm_buffer_ptr;
static char __kterm_char_buffer[KTERM_MAX_BUFFER_SIZE];
static char __kterm_stdin_buffer[KTERM_MAX_BUFFER_SIZE];

/* Framebuffer information */
static fb_info_t __kterm_fb_info;

static inline void kterm_draw_pixel_raw(uint32_t x, uint32_t y, uint32_t color) 
{
  if (__kterm_fb_info.bpp && x >= 0 && y >= 0 && x < __kterm_fb_info.width && y < __kterm_fb_info.height)
    *(uint32_t volatile*)(KTERM_FB_ADDR + __kterm_fb_info.pitch * y + x * __kterm_fb_info.bpp / 8) = color;
}

static inline void kterm_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
  uint32_t current_offset = __kterm_fb_info.pitch * y + x * __kterm_fb_info.bpp / 8;
  const uint32_t increment = (__kterm_fb_info.bpp / 8);

  for (uint32_t i = 0; i < height; i++) {
    for (uint32_t j = 0; j < width; j++) {

      *(uint32_t volatile*)(KTERM_FB_ADDR + current_offset + j * increment) = color;
    }
    current_offset += __kterm_fb_info.pitch;
  }
}

/* TODO: remove
 */
static ALWAYS_INLINE void kterm_print_keyvalue(const char* key, const char* value)
{
  kterm_print(key);
  kterm_print(": ");
  if (value)
    kterm_println(value);
  else 
    kterm_println("N/A");
}

/* TODO: remove
 */
static uint32_t __test_hashmap(const char** argv, size_t args)
{
  memory_allocator_t kallocator;
  
  kheap_copy_main_allocator(&kallocator);

  kterm_print_keyvalue("KHeap space free", to_string(kallocator.m_free_size));
  kterm_print_keyvalue("KHeap space used", to_string(kallocator.m_used_size));

  hashmap_t* t = create_hashmap(16, NULL);

  kterm_print_keyvalue("hashmap max size", to_string(t->m_max_entries));
  kterm_print_keyvalue("hashmap size", to_string(t->m_size));

  hashmap_put(t, "yay", (void*)69);
  hashmap_put(t, "yay^2", (void*)70);
  hashmap_put(t, "vroom", (void*)70);

  kterm_print_keyvalue("hashmap size", to_string(t->m_size));

  destroy_hashmap(t);

  kheap_copy_main_allocator(&kallocator);

  kterm_print_keyvalue("KHeap space free", to_string(kallocator.m_free_size));
  kterm_print_keyvalue("KHeap space used", to_string(kallocator.m_used_size));

  return 0;
}

struct kterm_cmd kterm_commands[] = {
  {
    "help",
    "Display help about kterm and it's commands",
    kterm_cmd_help,
  },
  {
    "hello",
    "Say hello",
    kterm_cmd_hello,
  },
  {
    "exit",
    "Exit the system and shut down",
    kterm_cmd_exit,
  },
  {
    "clear",
    "Clear the command window",
    kterm_cmd_clear,
  },
  {
    "sysinfo",
    "Print generic system information",
    kterm_cmd_sysinfo,
  },
  {
    "drvinfo",
    "Print information about the installed drivers",
    kterm_cmd_drvinfo,
  },
  {
    "drvld",
    "Manage drivers",
    kterm_cmd_drvld,
  },
  {
    "diskinfo",
    "Print info about a disk device",
    kterm_cmd_diskinfo,
  },
  {
    "testhashmap",
    "Test the hashmap kernel util",
    __test_hashmap,
  },
  {
    "vidinfo",
    "Print info about the current video device",
    nullptr,
  },
  {
    "dispinfo",
    "Print info about the available displays",
    nullptr,
  },
  {
    "netinfo",
    "Print info about the current network configuration",
    nullptr,
  },
  {
    "pwrinfo",
    "Print info about the current power and thermal status",
    nullptr,
  },
  {
    "envinfo",
    "Print info about the current environment (Profiles and variables)",
    kterm_cmd_envinfo,
  },
  {
    "envset",
    "Interract with the environment",
    nullptr,
  },
  /* VFS functions */
  {
    "ls",
    "List the current working directory contents",
    nullptr,
  },
  {
    "cd",
    "Change the current working directory",
    nullptr,
  },
  {
    /* Unixes pwd lmao */
    "cwd",
    "Print the current working directory",
    nullptr,
  },
  /*
   * NOTE: this is exec and this should always
   * be placed at the end of this list, otherwise
   * kterm_grab_handler_for might miss some commands
   */
  {
    nullptr,
    nullptr,
    kterm_try_exec,
  }
};

uint32_t kterm_cmd_count = sizeof(kterm_commands) / sizeof(kterm_commands[0]);

/*!
 * @brief Find a command for @cmd
 *
 * If we find no match, we return the handler of kterm_exec
 */
static f_kterm_command_handler_t kterm_grab_handler_for(char* cmd)
{
  struct kterm_cmd* current;

  if (!cmd || !(*cmd))
    return nullptr;

  current = nullptr;
  
  for (uint32_t i = 0; i < kterm_cmd_count; i++) {
    current = &kterm_commands[i];
  
    /* Reached exec, exit */
    if (!current->argv_zero)
      break;

    if (strcmp(cmd, current->argv_zero) == 0)
      return current->handler;
  }

  /* Should not happen */
  if (!current)
    return nullptr;

  return current->handler;
}


static void kterm_clear_raw()
{
  //__kterm_fb_info.ops->f_draw_rect(&__kterm_fb_info, 0, 0, __kterm_fb_info.width, __kterm_fb_info.height, (fb_color_t){ .raw_clr = 0x00 });
  kterm_draw_rect(0, 0, __kterm_fb_info.width, __kterm_fb_info.height, 0x00);
}

static int kterm_get_argument_count(char* cmd_buffer, size_t* count)
{
  size_t current_count;

  if (!cmd_buffer || !count)
    return -1;

  /* Empty string =/ */
  if (!(*cmd_buffer))
    return -2;

  current_count = 1;

  while (*cmd_buffer) {

    if (*cmd_buffer == ' ') {
      current_count++;
    }
    
    cmd_buffer++;
  }

  *count = current_count;

  return 0;
}

/*!
 * @brief Parse a single command buffer
 *
 * Generate a argument vector and an argument count
 * like unix (sorta). The 'command' will be located at
 * argv[0] with its arguments at argv[1 + n]
 */
static int kterm_parse_cmd_buffer(char* cmd_buffer, char** argv_buffer)
{
  char previous_char;
  char* current;
  size_t current_count;
  
  if (!cmd_buffer || !argv_buffer)
    return -1;

  current = cmd_buffer;
  current_count = NULL;
  previous_char = NULL;

  /* Make sure the 'command' is in the argv */
  argv_buffer[current_count++] = cmd_buffer;

  /*
   * Loop to see if we find any other arguments
   */
  while (*current) {
  
    if (*current == ' ' && previous_char != ' ') {
      argv_buffer[current_count] = current+1;

      current_count++;

      /*
       * Mutate cmd_buffer and place a null-byte here, in
       * order to terminate the vector entry 
       */
      *current = NULL;
    }
    
    previous_char = *current;
    current++;
  }

  return 0;
}

/*!
 * @brief Clear the screen and reset the cursor
 *
 * Nothing to add here...
 */
void kterm_clear() 
{
  kterm_clear_raw();

  __kterm_current_line = 0;
  kterm_draw_cursor();
}

/*!
 * @brief Clear the cmd buffer and reset the doors
 *
 * Called when we are done processing a command
 */
static inline void kterm_clear_cmd_buffer()
{
  /* Clear the buffer ourselves */
  memset(__processor_door.m_buffer, 0, __processor_door.m_buffer_size);

  Must(kdoor_reset(&__processor_door));
}

void kterm_command_worker() 
{
  int error;
  f_kterm_command_handler_t current_handler;
  char processor_buffer[KTERM_MAX_BUFFER_SIZE] = { 0 };

  init_kdoor(&__processor_door, processor_buffer, sizeof(processor_buffer));

  Must(register_kdoor(__kterm_cmd_doorbell, &__processor_door));

  while (true) {

    if (!kdoor_is_rang(&__processor_door)) {
      scheduler_yield();
      continue;
    }

    size_t argument_count = 0;
    char* contents = __processor_door.m_buffer;

    error = kterm_get_argument_count(contents, &argument_count);

    /* Yikes */
    if (error || !argument_count) {
      kterm_clear_cmd_buffer();
      continue;
    }

    char* argv[argument_count];

    /* Clear argv */
    memset(argv, 0, sizeof(char*) * argument_count);

    error = kterm_parse_cmd_buffer(contents, argv);

    if (error)
      goto exit_cmd_processing;

    /*
     * We have now indexed our entire command, so we can start matching
     * the actions against argv[0]
     */
    current_handler = kterm_grab_handler_for(argv[0]);

    if (!current_handler)
      goto exit_cmd_processing;

    error = current_handler((const char**)argv, argument_count);

    if (error) {
      /* TODO: implement kernel printf / logf lmao */
      kterm_print("Command failed with code: ");
      kterm_println(to_string(error));
    }

exit_cmd_processing:
    kterm_clear_cmd_buffer();
  }
}

EXPORT_DRIVER(base_kterm_driver) = {
  .m_name = "kterm",
  .m_type = DT_OTHER,
  .f_init = kterm_init,
  .f_exit = kterm_exit,
  .f_msg = kterm_on_packet,
  .m_dependencies = {"core/video", "io/ps2_kb"},
  .m_dep_count = 2,
};

static int kterm_write(aniva_driver_t* d, void* buffer, size_t* buffer_size, uintptr_t offset)
{
  char* str = (char*)buffer;

  /* Make sure the end of the buffer is null-terminated and the start is non-null */
  //if (IsError(kmem_validate_ptr(get_current_proc(), (uintptr_t)buffer, 1)))
    //return DRV_STAT_INVAL;

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
int kterm_init() 
{
  __kterm_current_line = 0;
  __kterm_cmd_doorbell = create_doorbell(1, NULL);

  ASSERT_MSG(driver_manifest_write(&base_kterm_driver, kterm_write), "Failed to manifest kterm write");
  ASSERT_MSG(driver_manifest_read(&base_kterm_driver, kterm_read), "Failed to manifest kterm read");

  /* Zero out the cmd buffer */
  memset(__kterm_stdin_buffer, 0, sizeof(__kterm_stdin_buffer));
  memset(__kterm_char_buffer, 0, sizeof(__kterm_char_buffer));

  /* TODO: when we implement our HID driver, this should be replaced with a event endpoint we can probe */
  // register our keyboard listener
  driver_send_msg("io/ps2_kb", KB_REGISTER_CALLBACK, kterm_on_key, sizeof(uintptr_t));

  /* FIXME: integrate video device integration */
  // map our framebuffer
  size_t size;
  uintptr_t ptr = KTERM_FB_ADDR;

  viddev_mapfb_t fb_map = {
    .size = &size,
    .virtual_base = ptr,
  };

  /*
   * (old, TODO: remove) This is the wrong way of getting information to the graphics API, because it is not a given that
   * the loaded graphics driver is indeed efifb. It might very well be virtio or some other nonsense. 
   * In that case, we need a function that gives us the current graphics driver path
   *
   * Situation has changed since. We now have a core video device that will always be present, regardless of video driver (If 
   * the current driver is a nice one). As a driver, process, library, ect. relying on this device, we need to be aware of any 
   * changes that are made to the device. If we grab any dynamic stuff, like the framebuffer, that is prone to change, we need
   * to always be aware of these changes. This is why we need to subscribe ourselves to the device_driver_change kernel event.
   * This ensures that we are always up to date on the current situation of our video device, and we are able to remap our framebuffer
   * so we don't skip a beat
   */
  Must(driver_send_msg("core/video", VIDDEV_DCC_MAPFB, &fb_map, sizeof(fb_map)));
  Must(driver_send_msg_a("core/video", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &__kterm_fb_info, sizeof(fb_info_t)));

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

  /* TODO: we should probably have some kind of kernel-managed structure for async work */
  __kterm_worker_thread = spawn_thread("kterm_cmd_worker", kterm_command_worker, NULL);
  
  /* Make sure we create this fucker */
  ASSERT_MSG(__kterm_worker_thread, "Failed to create kterm command worker!");

  return 0;
}

int kterm_exit() {
  /*
   * TODO:
   * - unmap framebuffer
   * - unregister logger
   * - wait until the worker thread exists
   * - ect.
   */
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

void kterm_on_key(ps2_key_event_t event)
{
  if (!doorbell_has_door(__kterm_cmd_doorbell, 0) || !event.m_pressed)
    return;

  kterm_write_char(event.m_typed_char);
}

static void kterm_flush_buffer() 
{
  memset(__kterm_char_buffer, 0, sizeof(__kterm_char_buffer));
  __kterm_buffer_ptr = 0;
}

/*!
 * @brief Handle a keypress pretty much
 *
 * FIXME: much like with kterm_print, fix the voodoo shit with KTERM_CURSOR_WIDTH and __kterm_char_buffer
 */
static void kterm_write_char(char c) 
{
  if (__kterm_buffer_ptr >= KTERM_MAX_BUFFER_SIZE) {
    // time to flush the buffer
    return;
  }

  switch (c) {
    case '\b':
      if (__kterm_buffer_ptr > 0) {
        kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, 0, 0x00);
        __kterm_buffer_ptr--;
        __kterm_char_buffer[__kterm_buffer_ptr] = NULL;
        kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '_', 0xFFFFFFFF);
      } else {
        __kterm_buffer_ptr = 0;
      }
      break;
    case (char)0x0A:
      /* Enter */
      kterm_process_buffer();
      break;
    default:
      if (c >= 0x20) {
        kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, c, 0xFFFFFFFF);
        __kterm_char_buffer[__kterm_buffer_ptr] = c;
        __kterm_buffer_ptr++;
        kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '_', 0xFFFFFFFF);
      }
      break;
  }
}

/*!
 * @brief Process an incomming command
 *
 */
static void kterm_process_buffer() 
{
  char buffer[KTERM_MAX_BUFFER_SIZE] = { 0 };
  size_t buffer_size = 0;

  /* Copy the buffer localy, so we can clear it early */
  const char* contents = kterm_get_buffer_contents();

  if (contents) {
    buffer_size = strlen(contents);

    if (buffer_size >= KTERM_MAX_BUFFER_SIZE) {
      /* Bruh, we cant process this... */
      return;
    }

    memcpy(buffer, contents, buffer_size);
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

  doorbell_write(__kterm_cmd_doorbell, buffer, buffer_size + 1, 0);

  doorbell_ring(__kterm_cmd_doorbell);
}

static void kterm_draw_cursor() {
  kterm_draw_char(0 * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '>', 0xFFFFFFFF);
  kterm_draw_char(1 * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, ' ', 0xFFFFFFFF);
  __kterm_buffer_ptr = 0;
  kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '_', 0xFFFFFFFF);
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

static inline const char* kterm_get_buffer_contents() 
{
  if (__kterm_char_buffer[0] == NULL)
    return nullptr;

  return __kterm_char_buffer;
}

int kterm_println(const char* msg) 
{
  if (msg)
    kterm_print(msg);
  kterm_print("\n");
  return 0;
}

/*!
 * @brief Print a string to our terminal
 *
 * FIXME: the magic we have with KTERM_CURSOR_WIDTH and kterm_buffer_ptr_copy should get nuked 
 * and redone xD
 */
int kterm_print(const char* msg) 
{
  uintptr_t index = 0;

  while (msg[index]) {
    char current_char = msg[index];
    if (current_char == '\n') {
      /* Erase the cursor on the previous line */
      kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, ' ', 0xFFFFFFFF);
      __kterm_current_line++;

      if (__kterm_current_line * KTERM_FONT_HEIGHT >= __kterm_fb_info.height) {
        kterm_scroll(1);
      }

      kterm_flush_buffer();
      kterm_draw_cursor();
      __kterm_buffer_ptr = NULL;
    } else {
      kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, current_char, 0xFFFFFFFF);
      __kterm_buffer_ptr++;

      if ((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH > __kterm_fb_info.width) {
        __kterm_current_line++;
        __kterm_buffer_ptr = 0;
      }
    }
    index++;
  }

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

