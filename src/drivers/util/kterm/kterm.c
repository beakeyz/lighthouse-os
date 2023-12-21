#include "kterm.h"
#include "LibGfx/include/driver.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/external.h"
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
#include "kevent/event.h"
#include "kevent/types/keyboard.h"
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
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"
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

#define KTERM_MAX_BUFFER_SIZE 512
#define KTERM_KEYBUFFER_CAPACITY 512

#define KTERM_FB_ADDR (EARLY_FB_MAP_BASE) 
#define KTERM_FONT_HEIGHT 8
#define KTERM_FONT_WIDTH 8

#define KTERM_CURSOR_WIDTH 2
#define KTERM_MAX_PALLET_ENTRY_COUNT 256

enum KTERM_BASE_CLR {
  BASE_CLR_RED = 0,
  BASE_CLR_YELLOW,
  BASE_CLR_GREEN,
  BASE_CLR_CYAN,
  BASE_CLR_BLUE,
  BASE_CLR_PURPLE,
  BASE_CLR_PINK,

  BASE_CLR_COUNT
};

/* 
 * Single entry in the kterm color pallet
 */
struct kterm_terminal_pallet_entry {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t padding;
};

struct kterm_terminal_char {
  uint8_t pallet_idx;
  uint8_t ch;
};

/*
 * This is only accessed when mode is KTERM_MODE_GRAPHICS
 */
struct {
  uint32_t* c_fb;
  size_t fb_size;
  uint32_t startx, starty;
  uint32_t width, height;
  proc_t* client_proc;
} _active_grpx_app;


static enum kterm_mode mode;

/* Grid of caracters on the screen in terminal mode */
static struct kterm_terminal_char* _characters;

/* Entire kterm color pallet. Maximum 256 colors available in this pallet */
static struct kterm_terminal_pallet_entry* _clr_pallet;

/* Keybuffer for graphical applications */
struct kevent_kb_keybuffer _keybuffer;

/* Amound of chars on the x-axis */
static uint32_t _chars_xres;
/* Amound of chars on the y-axis */
static uint32_t _chars_yres;

/* Current x-index of the cursor */
static uint32_t _chars_cursor_x;
/* Current y-index of the cursor */
static uint32_t _chars_cursor_y;
/* Current index of the color used for printing */
static uint32_t _current_color_idx;

static const char* _old_dfld_lwnd_path_value;

/* Should we print a tag on every newline? */
static bool _print_newline_tag;

/* Command buffer */
static kdoor_t __processor_door;
static kdoorbell_t* __kterm_cmd_doorbell;
static thread_t* __kterm_worker_thread;

/* Buffer information */
static uintptr_t __kterm_buffer_ptr;
static char __kterm_char_buffer[KTERM_MAX_BUFFER_SIZE];
static char __kterm_stdin_buffer[KTERM_MAX_BUFFER_SIZE];

/* Framebuffer information */
static fb_info_t __kterm_fb_info;

int kterm_init();
int kterm_exit();
uintptr_t kterm_on_packet(aniva_driver_t* driver, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

static void kterm_flush_buffer();
static void kterm_write_char(char c);
static void kterm_process_buffer();

static void kterm_handle_newline_tag();
static inline struct kterm_terminal_char* kterm_get_term_char(uint32_t x, uint32_t y);

static void kterm_cursor_shiftback_x();

//static void kterm_draw_pixel(uintptr_t x, uintptr_t y, uint32_t color);
static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uint32_t color, bool defer_update);

static void kterm_enable_newline_tag();
static void kterm_disable_newline_tag();
static const char* kterm_get_buffer_contents();
static uint32_t volatile* kterm_get_pixel_address(uintptr_t x, uintptr_t y);
static void kterm_scroll(uintptr_t lines);

logger_t kterm_logger = {
  .title = "kterm",
  .flags = LOGGER_FLAG_NO_CHAIN,
  .f_logln = kterm_println,
  .f_log = kterm_print,
};

int kterm_on_key(kevent_ctx_t* event);

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

static inline void kterm_clear_raw()
{
  kterm_draw_rect(0, 0, __kterm_fb_info.width, __kterm_fb_info.height, 0x00);
}

static uint32_t kterm_color_for_pallet_idx(uint32_t idx)
{
  uint32_t clr;
  struct kterm_terminal_pallet_entry* entry;

  /* Return black */
  if (idx >= KTERM_MAX_PALLET_ENTRY_COUNT)
    return NULL;

  entry = &_clr_pallet[idx];
  clr = ((uint32_t)entry->red << __kterm_fb_info.colors.red.offset_bits) |
        ((uint32_t)entry->green << __kterm_fb_info.colors.green.offset_bits) |
        ((uint32_t)entry->blue << __kterm_fb_info.colors.blue.offset_bits);

  return clr;
}

static void kterm_update_term_char(struct kterm_terminal_char* char_start, uint32_t count)
{
  char* glyph;
  /* x and y components inside the screen */
  uintptr_t offset;
  uint32_t color;
  uint32_t x;
  uint32_t y;

  if (!count)
    return;

  /* Calculate the offset of the starting character */
  offset = ((uintptr_t)char_start - (uintptr_t)_characters) / sizeof(*char_start);

  do {
    /* Grab our font */
    glyph = font8x8_basic[(uint32_t)char_start->ch];

    x = (offset % _chars_xres) * KTERM_FONT_WIDTH;
    y = (ALIGN_DOWN(offset , _chars_xres) / _chars_xres) * KTERM_FONT_HEIGHT;

    color = kterm_color_for_pallet_idx(char_start->pallet_idx);

    for (uintptr_t _y = 0; _y < KTERM_FONT_HEIGHT; _y++) {
      for (uintptr_t _x = 0; _x < KTERM_FONT_WIDTH; _x++) {

        if (glyph[_y] & (1 << _x)) kterm_draw_pixel_raw(x + _x, y + _y, color);
        /* TODO: draw a background color */
        else kterm_draw_pixel_raw(x + _x, y + _y, 0x00);
      }
    }

    count--;
    char_start++;
    offset++;
  } while (count);
}

/*!
 * @brief: Fill the kterm color pallet with the default colors
 *
 */
static void kterm_fill_pallet()
{
  struct kterm_terminal_pallet_entry* entry;
  enum KTERM_BASE_CLR base_clr;
  uint32_t dimness;
  uint32_t tint_count;

  _clr_pallet[0] = (struct kterm_terminal_pallet_entry){ 0xff, 0xff, 0xff, 0xff, };
  _clr_pallet[1] = (struct kterm_terminal_pallet_entry){ 0x00, 0x00, 0x00, 0xff, };
  _clr_pallet[2] = (struct kterm_terminal_pallet_entry){ 0x21, 0x21, 0x21, 0xff, };

  tint_count = ALIGN_DOWN(KTERM_MAX_PALLET_ENTRY_COUNT - 3, BASE_CLR_COUNT) / BASE_CLR_COUNT;

  /* 
   * Start at 3
   * Since 0, 1 and 2 are reserved for white, black and gray
   * 
   * The pallet is divided into 7 base colors:
   *  Red
   *  Yellow
   *  Green
   *  Cyan
   *  Blue
   *  Purple
   *  Pink
   */
  for (uint32_t i = 0; i < ALIGN_DOWN(KTERM_MAX_PALLET_ENTRY_COUNT - 3, BASE_CLR_COUNT); i++) {
    entry = &_clr_pallet[3 + i];

    memset(entry, 0, sizeof(*entry));

    /* This is going to tell us what base color we're working on */
    base_clr = ALIGN_DOWN(i, tint_count) / tint_count;
    dimness = (i % tint_count) * 8; 

    /* Compute the amount for the components per base color */
    switch (base_clr) {
      /* RGB: these colors only have one component */
      case BASE_CLR_RED:
        entry->red = 0xff - dimness;
        break;
      case BASE_CLR_GREEN:
        entry->green = 0xff - dimness;
        break;
      case BASE_CLR_BLUE:
        entry->blue = 0xff - dimness;
        break;
      /* Intermediate tints, these tints have two components with one variable */
      case BASE_CLR_YELLOW:
        entry->red = 0xff;
        entry->green = 0xff - dimness;
        break;
      case BASE_CLR_CYAN:
        entry->green = 0xff;
        entry->blue = 0xff - dimness;
        break;
      case BASE_CLR_PURPLE:
        entry->blue = 0xff;
        entry->red = 0xff - dimness;
        break;
      case BASE_CLR_PINK:
        entry->blue = 0x7f - dimness;
        entry->red = 0xff;
      default:
        break;
    }
  }
}

static void kterm_set_print_color(uint32_t idx)
{
  if (idx >= KTERM_MAX_PALLET_ENTRY_COUNT)
    idx = 0;

  _current_color_idx = idx;
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
    "procinfo",
    "Print info about the current running processes",
    kterm_cmd_procinfo,
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

static void kterm_clear_terminal_chars()
{
  struct kterm_terminal_char* ch;

  for (uint32_t i = 0; i < _chars_yres; i++) {
    for (uint32_t j = 0; j < _chars_xres; j++) {

      ch = kterm_get_term_char(j, i);

      if (!ch->ch)
        continue;

      /* Draw null chars */
      kterm_draw_char(j, i, NULL, 1, true);
    }
  }

  kterm_update_term_char(_characters, _chars_xres * _chars_yres);
}

/*!
 * @brief: Redraw the entire terminal screen after a modeswitch for example
 */
static void kterm_redraw_terminal_chars()
{
  struct kterm_terminal_char* ch;

  for (uint32_t i = 0; i < _chars_yres; i++) {
    for (uint32_t j = 0; j < _chars_xres; j++) {

      ch = kterm_get_term_char(j, i);

      kterm_draw_char(j, i, ch->ch, ch->pallet_idx, false);
    }
  }
}

/*!
 * @brief Clear the screen and reset the cursor
 *
 * Nothing to add here...
 */
void kterm_clear() 
{
  _chars_cursor_y = 0;
  _chars_cursor_x = 0;

  /* Clear every terminal character */
  kterm_clear_terminal_chars();

  kterm_handle_newline_tag();
}

void kterm_switch_to_terminal()
{
  if (mode == KTERM_MODE_TERMINAL || !_active_grpx_app.c_fb)
    return;

  mode = KTERM_MODE_TERMINAL;
}

bool kterm_ismode(enum kterm_mode _mode)
{
  return (mode == _mode);
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

void kterm_init_lwnd_emulation()
{
  const char* var_buffer;
  proc_profile_t* profile;
  profile_var_t* var;

  ASSERT_MSG(profile_scan_var("Global/DFLT_LWND_PATH", &profile, &var) == 0, "Could not find global variable for the default LWND path while initializing kterm!");

  profile_var_get_str_value(var, &var_buffer);

  /* Make sure this is owned by us */
  _old_dfld_lwnd_path_value = strdup(var_buffer);

  /* Make sure the system knows we emulate lwnd */
  profile_var_write(var, PROFILE_STR("other/kterm"));
}

/*
 * The aniva kterm driver is a text-based terminal program that runs directly in kernel-mode. Any processes it 
 * runs get a handle to the driver as stdin, stdout and stderr, with room for any other handle
 */
int kterm_init() 
{
  (void)kterm_redraw_terminal_chars;

  _old_dfld_lwnd_path_value = NULL;
  __kterm_cmd_doorbell = create_doorbell(1, NULL);

  kterm_disable_newline_tag();

  ASSERT_MSG(driver_manifest_write(&base_kterm_driver, kterm_write), "Failed to manifest kterm write");
  ASSERT_MSG(driver_manifest_read(&base_kterm_driver, kterm_read), "Failed to manifest kterm read");

  /* Zero out the cmd buffer */
  memset(__kterm_stdin_buffer, 0, sizeof(__kterm_stdin_buffer));
  memset(__kterm_char_buffer, 0, sizeof(__kterm_char_buffer));

  /* Register ourselves to the keyboard event */
  kevent_add_hook("keyboard", "kterm", kterm_on_key); 

  /* TODO: integrate video device accel */
  // map our framebuffer
  size_t size;
  uintptr_t ptr = KTERM_FB_ADDR;

  viddev_mapfb_t fb_map = {
    .size = &size,
    .virtual_base = ptr,
  };

  Must(driver_send_msg("core/video", VIDDEV_DCC_MAPFB, &fb_map, sizeof(fb_map)));
  Must(driver_send_msg_a("core/video", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &__kterm_fb_info, sizeof(fb_info_t)));

  void* _kb_buffer = kmalloc(KTERM_KEYBUFFER_CAPACITY * sizeof(kevent_kb_ctx_t));

  init_kevent_kb_keybuffer(&_keybuffer, _kb_buffer, KTERM_KEYBUFFER_CAPACITY);

  /* Initialize our lwnd emulation capabilities */
  kterm_init_lwnd_emulation();

  /* Make sure there is no garbage on the screen */
  kterm_clear_raw();

  _chars_xres = __kterm_fb_info.width / KTERM_FONT_WIDTH;
  _chars_yres = __kterm_fb_info.height / KTERM_FONT_HEIGHT;

  _chars_cursor_x = 0;
  _chars_cursor_y = 0;

  kterm_set_print_color(0);

  /*
   * Allocate a range for our characters
   */
  _characters = (void*)Must(__kmem_kernel_alloc_range(_chars_xres * _chars_yres * sizeof(struct kterm_terminal_char), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  /* Allocate the color pallet */
  _clr_pallet = (void*)Must(__kmem_kernel_alloc_range(KTERM_MAX_PALLET_ENTRY_COUNT * sizeof(struct kterm_terminal_pallet_entry), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  kterm_fill_pallet();

  /* Register our logger to the logger subsys */
  register_logger(&kterm_logger);

  /* Make sure we are the logger with the highest prio */
  logger_swap_priority(kterm_logger.id, 0);

  // flush our terminal buffer
  kterm_flush_buffer();

  mode = KTERM_MODE_TERMINAL;

  //memset((void*)KTERM_FB_ADDR, 0, kterm_fb_info.used_pages * SMALL_PAGE_SIZE);
  kterm_print("\n");
  kterm_print(" -- Welcome to the aniva kterm driver --\n");
  processor_t* processor = get_current_processor();

  kterm_print(" CPU: ");
  kterm_print(processor->m_info.m_model_id);
  kterm_print("\n");
  kterm_print(" Available cores: ");
  kterm_print(to_string(processor->m_info.m_max_available_cores));
  kterm_print("\n\n For any information about kterm, type: \'help\'\n");

  kterm_enable_newline_tag();

  /* Prompt a newline tag draw */
  kterm_println(NULL);

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

/*!
 * @brief: Our main msg endpoint
 *
 * Make sure we support at least the most basic form of LWND emulation
 */
uintptr_t kterm_on_packet(aniva_driver_t* driver, dcc_t code, void __user* buffer, size_t size, void* out_buffer, size_t out_size) 
{
  lwindow_t* uwnd = NULL;
  proc_t* c_proc = get_current_proc();

  /* Check pointer */
  if (buffer && IsError(kmem_validate_ptr(c_proc, (uintptr_t)buffer, size)))
    return DRV_STAT_INVAL;

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
      kterm_clear();
      break;
    case LWND_DCC_CREATE:
      /* Switch the kterm mode to KTERM_MODE_GRAPHICS and prepare a canvas for the graphical application to run
       */
      if (size != sizeof(*uwnd))
        return DRV_STAT_INVAL;

      uwnd = buffer;

      mode = KTERM_MODE_GRAPHICS;

      uwnd->wnd_id = 1;
      break;
    case LWND_DCC_CLOSE:
      /* Don't do anything: We switch back to terminal mode once the app exits
       */

      //Must(kmem_user_dealloc(c_proc, (vaddr_t)_active_grpx_app.c_fb, _active_grpx_app.fb_size));
      break;
    case LWND_DCC_MINIMIZE:
      /* Won't be implemented
       */
    case LWND_DCC_RESIZE:
      /* Won't be implemented
       */
      break;
    case LWND_DCC_REQ_FB:
      /* Allocate a portion of the framebuffer for the process
       */
      if (size != sizeof(*uwnd))
        return DRV_STAT_INVAL;

      uwnd = buffer;

      /* Clamp width */
      if (uwnd->current_width > __kterm_fb_info.width)
        uwnd->current_width = __kterm_fb_info.width;

      /* Clamp height */
      if (uwnd->current_height > __kterm_fb_info.height)
        uwnd->current_height = __kterm_fb_info.height;

      _active_grpx_app.client_proc = c_proc;
      _active_grpx_app.width = uwnd->current_width;
      _active_grpx_app.height = uwnd->current_height;

      /* Compute best startx and starty */
      _active_grpx_app.startx = (__kterm_fb_info.width >> 1) - (_active_grpx_app.width >> 1);
      _active_grpx_app.starty = (__kterm_fb_info.height >> 1) - (_active_grpx_app.height >> 1);

      /* Allocate this crap in the userprocess */
      _active_grpx_app.fb_size = ALIGN_UP(_active_grpx_app.width * _active_grpx_app.height * sizeof(uint32_t), SMALL_PAGE_SIZE);
      _active_grpx_app.c_fb = uwnd->fb = (void*)Must(kmem_user_alloc_range(c_proc, _active_grpx_app.fb_size, NULL, KMEM_FLAG_WRITABLE));

      memset(_active_grpx_app.c_fb, 0, _active_grpx_app.fb_size);

    case LWND_DCC_UPDATE_WND:
      {
        /* Update the windows framebuffer to the frontbuffer
         */

        uintptr_t current_offset = 0;

        for (uint32_t y = 0; y < _active_grpx_app.height; y++) {
          for (uint32_t x = 0; x < _active_grpx_app.width; x++) {
            kterm_draw_pixel_raw(_active_grpx_app.startx + x, _active_grpx_app.starty + y, *(uint32_t*)(_active_grpx_app.c_fb + current_offset));

            current_offset++;
          }
        }
        break;
      }
    case LWND_DCC_GETKEY:
      /* Give the graphical process information about any keyevents
       */
      if (size != sizeof(*uwnd))
        return DRV_STAT_INVAL;

      uwnd = buffer;

      lkey_event_t* u_event;
      kevent_kb_ctx_t* event = keybuffer_read_key(&_keybuffer);

      if (!event)
        break;

      /*
       * TODO: create functions that handle with this reading and writing to user keybuffers
       */

      /* Grab a buffer entry */
      u_event = &uwnd->keyevent_buffer[uwnd->keyevent_buffer_write_idx++];

      /* Write the data */
      u_event->keycode = event->keycode;
      u_event->pressed_char = event->pressed_char;
      u_event->pressed = event->pressed;
      u_event->mod_flags = NULL;

      println(to_string(event->keycode));

      /* Make sure we cycle the index */
      uwnd->keyevent_buffer_write_idx %= uwnd->keyevent_buffer_capacity;

      break;
  }

  return DRV_STAT_OK;
}

int kterm_handle_terminal_key(kevent_kb_ctx_t* kbd)
{
  if (!doorbell_has_door(__kterm_cmd_doorbell, 0) || !kbd->pressed)
    return 0;

  kterm_write_char(kbd->pressed_char);
  return 0;
}

/*! 
 * @brief: Handle a keypress in graphics mode
 *
 * Simply writes the event into the keybuffer
 */
int kterm_handle_graphics_key(kevent_kb_ctx_t* kbd)
{
  keybuffer_write_key(&_keybuffer, kbd);
  return 0;
}

/*!
 * @brief: Kterm kevent
 *
 * When
 * KTERM_MODE_TERMINAL: simply pass to kterm
 * KTERM_MODE_GRAPHICS: pass to the keybuffer of the current active app
 *                     -> Also check for the graphics mode escape sequence to force-quit the application
 */
int kterm_on_key(kevent_ctx_t* ctx)
{
  kevent_kb_ctx_t* k_event = kevent_ctx_to_kb(ctx);

  switch (mode) {
    case KTERM_MODE_TERMINAL:
      return kterm_handle_terminal_key(k_event);
    case KTERM_MODE_GRAPHICS:
      return kterm_handle_graphics_key(k_event);
    default:
      break;
  }

  return 0;
}

/*! 
 * @brief: Make sure there is no garbage in our character buffer
 */
static void kterm_flush_buffer() 
{
  memset(__kterm_char_buffer, 0, sizeof(__kterm_char_buffer));
  __kterm_buffer_ptr = 0;
}

static inline void kterm_draw_cursor_glyph()
{
  kterm_draw_char(_chars_cursor_x, _chars_cursor_y, KTERM_CURSOR_GLYPH, _current_color_idx, false);
}

/*!
 * @brief Handle a keypress pretty much
 *
 * FIXME: much like with kterm_print, fix the voodoo shit with KTERM_CURSOR_WIDTH and __kterm_char_buffer
 */
static void kterm_write_char(char c) 
{
  char msg[2] = { 0 };

  if (__kterm_buffer_ptr >= KTERM_MAX_BUFFER_SIZE) {
    // time to flush the buffer
    return;
  }

  msg[0] = c;

  switch (c) {
    case '\b':
      if (__kterm_buffer_ptr > 0) {

        /* Erase old cursor (NOTE: color does not matter, since we are drawing a null-char) */
        kterm_draw_char(_chars_cursor_x, _chars_cursor_y, NULL, 0, false);

        kterm_cursor_shiftback_x();

        /* Draw new cursor */
        kterm_draw_cursor_glyph();

        __kterm_buffer_ptr--;
        __kterm_char_buffer[__kterm_buffer_ptr] = NULL;

        //kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '_', 0, 0);
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
        kterm_print(msg);
        __kterm_char_buffer[__kterm_buffer_ptr] = c;
        __kterm_buffer_ptr++;

        kterm_draw_cursor_glyph();
        //kterm_draw_char((KTERM_CURSOR_WIDTH + __kterm_buffer_ptr) * KTERM_FONT_WIDTH, __kterm_current_line * KTERM_FONT_HEIGHT, '_', 0, 0);
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
  kterm_println(NULL);

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

static void kterm_enable_newline_tag()
{
  _print_newline_tag = true;
}

static void kterm_disable_newline_tag()
{
  _print_newline_tag = false;
}

/*!
 * @brief: Print the configured newline tag
 *
 * This uses the backend print routines to draw a newline tag
 * Called by print and println when '_print_newline_tag' is enabled
 */
static void kterm_handle_newline_tag()
{
  /* Only print newline tags if that is enabled */
  if (!_print_newline_tag)
    return;

  /* 
   * Print a basic newline tag 
   * TODO: allow for custom tags
   */
  kterm_set_print_color(127);
  kterm_print("$> ");
  kterm_set_print_color(0);

  kterm_draw_cursor_glyph();
}

static inline struct kterm_terminal_char* kterm_get_term_char(uint32_t x, uint32_t y)
{
  return &_characters[y * _chars_xres + x];
}

/*!
 * @brief: Draw the character @c into the terminal
 *
 * Make sure we're in KTERM_MODE_TERMINAL, otherwise this means nothing
 */
static void kterm_draw_char(uintptr_t x, uintptr_t y, char c, uint32_t color_idx, bool defer_update)
{
  struct kterm_terminal_char* term_chr;

  if (mode != KTERM_MODE_TERMINAL)
    return;

  /* Grab the terminal character entry */
  term_chr = kterm_get_term_char(x, y);

  term_chr->ch = c;
  term_chr->pallet_idx = color_idx;

  if (defer_update)
    return;

  kterm_update_term_char(term_chr, 1);
}

static inline const char* kterm_get_buffer_contents() 
{
  if (__kterm_char_buffer[0] == NULL)
    return nullptr;

  return __kterm_char_buffer;
}

/*!
 * @brief: Shift the cursor forward by one
 */
static void kterm_cursor_shift_x()
{
  _chars_cursor_x++;

  if (_chars_cursor_x >= _chars_xres-1) {
    _chars_cursor_x = 0;
    _chars_cursor_y++;
  }

  if (_chars_cursor_y >= _chars_yres-1) {
    kernel_panic("TODO: scroll when we reach the end of _chars_yres");
    kterm_scroll(1);
  }
}

/*!
 * @brief: Shift the cursor x-component back by one
 *
 * FIXME: Allow shifting back over the y-axis until we've reached the start of the line where we started
 * typing. Right now, when the user types until they reach _chars_xres, there is a rollover, which we don't  check for here
 */
static void kterm_cursor_shiftback_x()
{
  if (_chars_cursor_x) {
    _chars_cursor_x--;
    return;
  }

  _chars_cursor_x = _chars_xres-1;
  if (_chars_yres)
    _chars_yres--;
}

/*!
 * @brief: Shift the cursor down a line by one and reset it's x-component
 */
static void kterm_cursor_shift_y()
{
  kterm_draw_char(_chars_cursor_x, _chars_cursor_y, NULL, NULL, false);

  _chars_cursor_x = 0;
  _chars_cursor_y ++;

  if (_chars_cursor_y >= _chars_yres-1) {
    kernel_panic("TODO: scroll when we reach the end of _chars_yres");
    kterm_scroll(1);
  }
}

/*!
 * @brief: This prints a line on the kterm and, if _print_newline_tag is enabled, prints a newline tag
 */
int kterm_println(const char* msg) 
{
  if (msg)
    kterm_print(msg);

  /* Flush the buffer on every println */
  kterm_flush_buffer();

  kterm_cursor_shift_y();
  kterm_handle_newline_tag();
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
  uint32_t idx;

  if (!msg || mode != KTERM_MODE_TERMINAL)
    return 0;

  idx = 0;

  while (msg[idx]) {

    /*
     * When we encounter a newline character during a kterm_print, we should simply do the following:
     * Shift y of the cursor and don't do anything else
     */
    if (msg[idx] == '\n') {
      kterm_cursor_shift_y();
      goto cycle;
    }

    /* 
     * Draw the char 
     * FIXME: currently we only draw these white, support other colors
     */
    kterm_draw_char(_chars_cursor_x, _chars_cursor_y, msg[idx], _current_color_idx, false);
    kterm_cursor_shift_x();

cycle:
    idx++;
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

