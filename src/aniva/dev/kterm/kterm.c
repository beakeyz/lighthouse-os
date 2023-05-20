#include "kterm.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/disk/ahci/ahci_device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/generic.h"
#include "dev/disk/ramdisk.h"
#include "dev/framebuffer/framebuffer.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "fs/core.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "interupts/interupts.h"
#include "libk/async_ptr.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/acpi/parser.h"
#include <system/processor/processor.h>
#include <mem/kmem_manager.h>

#define KTERM_MAX_BUFFER_SIZE 256

#define KTERM_FB_ADDR (EARLY_FB_MAP_BASE) 
#define KTERM_FONT_HEIGHT 8
#define KTERM_FONT_WIDTH 8

#define KTERM_CURSOR_WIDTH 2

#define KTERM_MAX_CMD_LENGTH 128

static char font8x8_basic[128][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0000 (nul)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0001
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0002
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0003
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0004
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0005
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0006
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0007
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0008
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0009
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000A
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000B
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000C
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000D
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000E
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+000F
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0010
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0011
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0012
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0013
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0014
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0015
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0016
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0017
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0018
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0019
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001A
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001B
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001C
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001D
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001E
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+001F
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0020 (space)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // U+0021 (!)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0022 (")
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // U+0023 (#)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // U+0024 ($)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // U+0025 (%)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // U+0026 (&)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0027 (')
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // U+0028 (()
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // U+0029 ())
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // U+002A (*)
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // U+002B (+)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+002C (,)
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // U+002D (-)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+002E (.)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // U+002F (/)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // U+0030 (0)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // U+0031 (1)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // U+0032 (2)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // U+0033 (3)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // U+0034 (4)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // U+0035 (5)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // U+0036 (6)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // U+0037 (7)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // U+0038 (8)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // U+0039 (9)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+003A (:)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+003B (;)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // U+003C (<)
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // U+003D (=)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // U+003E (>)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // U+003F (?)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // U+0040 (@)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // U+0041 (A)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // U+0042 (B)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // U+0043 (C)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // U+0044 (D)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // U+0045 (E)
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // U+0046 (F)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // U+0047 (G)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // U+0048 (H)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0049 (I)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // U+004A (J)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // U+004B (K)
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // U+004C (L)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // U+004D (M)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // U+004E (N)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // U+004F (O)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // U+0050 (P)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // U+0051 (Q)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // U+0052 (R)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // U+0053 (S)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0054 (T)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U+0055 (U)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0056 (V)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // U+0057 (W)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // U+0058 (X)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // U+0059 (Y)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // U+005A (Z)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // U+005B ([)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // U+005C (\)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // U+005D (])
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // U+005E (^)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // U+005F (_)
    {0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0060 (`)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // U+0061 (a)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // U+0062 (b)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // U+0063 (c)
    {0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // U+0064 (d)
    {0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // U+0065 (e)
    {0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // U+0066 (f)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0067 (g)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // U+0068 (h)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0069 (i)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // U+006A (j)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // U+006B (k)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+006C (l)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // U+006D (m)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // U+006E (n)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // U+006F (o)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // U+0070 (p)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // U+0071 (q)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // U+0072 (r)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // U+0073 (s)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // U+0074 (t)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // U+0075 (u)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0076 (v)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // U+0077 (w)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // U+0078 (x)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0079 (y)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // U+007A (z)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // U+007B ({)
    {0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // U+007C (|)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // U+007D (})
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+007E (~)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // U+007F
};

typedef struct {
  /* Commands can't be longer than 128 bytes :clown: */
  char buffer[KTERM_MAX_CMD_LENGTH];
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

static uintptr_t kterm_current_line;
static uintptr_t kterm_buffer_ptr;
static char kterm_char_buffer[256];

static fb_info_t kterm_fb_info;

void kterm_command_worker() {

  for (;;) {

    if (s_kterm_cmd_buffer.buffer[0] != 0) {
      mutex_lock(s_kterm_cmd_lock);

      /* TODO: process cmd */
      const char* contents = s_kterm_cmd_buffer.buffer;

      if (!strcmp(contents, "acpitables")) {
        kterm_println("\n");
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
        kterm_println("\n");
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

        uintptr_t* test_data = allocator->m_heap->f_allocate(allocator, sizeof(uintptr_t));
        *test_data = 6969;

        kterm_println("\nAllocated 8 bytes: ");
        kterm_println(to_string(*test_data));

        uintptr_t* test_data2 = allocator->m_heap->f_allocate(allocator, sizeof(uintptr_t));
        *test_data2 = 420;

        kterm_println("\nAllocated 8 bytes: ");
        kterm_println(to_string(*test_data2));

        allocator->m_heap->f_sized_deallocate(allocator, test_data, sizeof(uintptr_t));

        kterm_println("\nDeallocated 8 bytes: ");
        kterm_println(to_string(*test_data));

        allocator->m_heap->f_sized_deallocate(allocator, test_data2, sizeof(uintptr_t));

        kterm_println("\nDeallocated 8 bytes: ");
        kterm_println(to_string(*test_data2));

        kterm_println("\n Total zone allocator size: ");
        kterm_println(to_string(allocator->m_heap->m_current_total_size));

        kterm_println("\nSuccessfully created Zone!\n");
      } else if (!strcmp(contents, "testramdisk")) {

        vnode_t* ramfs = vfs_resolve("Devices/disk/cramfs");

        if (!ramfs){
          kernel_panic("Could not resolve ramfs");
        }

        kterm_println("\nFinding file...\n");
        vobj_t* obj = vn_find(ramfs, "init");

        ASSERT_MSG(obj, "Could not get vobj from test");
        ASSERT_MSG(obj->m_type == VOBJ_TYPE_FILE, "Object was not a file!");
        file_t* file = vobj_get_file(obj);
        ASSERT_MSG(file, "Could not get file from test");

        kterm_println("File size: ");
        kterm_println(to_string(file->m_size));
        kterm_println("\n");

        println("Trying to make proc");
        Must(elf_exec_static_64(file, false));
        println("Made proc");

      }
      kterm_println("\n");

      memset(&s_kterm_cmd_buffer, 0, sizeof(s_kterm_cmd_buffer));

      mutex_unlock(s_kterm_cmd_lock);
    }

    scheduler_yield();
  }
}

aniva_driver_t g_base_kterm_driver = {
  .m_name = "kterm",
  .m_type = DT_GRAPHICS,
  .f_init = kterm_init,
  .f_exit = kterm_exit,
  .f_drv_msg = kterm_on_packet,
  .m_dependencies = {"graphics/fb", "io/ps2_kb"},
  .m_dep_count = 2,
};
EXPORT_DRIVER(g_base_kterm_driver);

int kterm_init() {
  println("Initialized the kterm!");

  kterm_current_line = 0;
  s_kterm_cmd_lock = create_mutex(0);

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

  // flush our terminal buffer
  kterm_flush_buffer();

  kterm_draw_cursor();

  kernel_panic("Test");

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
  return 0;
}

uintptr_t kterm_on_packet(packet_payload_t payload, packet_response_t** response) {

  switch (payload.m_code) {
    case KTERM_DRV_DRAW_STRING: {
      const char* msg = *(const char**)payload.m_data;
      println(msg);
      kterm_println(msg);
      kterm_println("\n");
      break;
    }
  }

  return 0;
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
  if (kterm_buffer_ptr + 1 >= KTERM_MAX_BUFFER_SIZE) {
    // time to flush the buffer
    return;
  }

  println(to_string((uintptr_t)c));

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

  if (contents_size >= KTERM_MAX_CMD_LENGTH) {
    /* Bruh, we cant process this... */
    return;
  }

  if (mutex_is_locked(s_kterm_cmd_lock))
    return;

  memcpy(s_kterm_cmd_buffer.buffer, contents, contents_size);

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

