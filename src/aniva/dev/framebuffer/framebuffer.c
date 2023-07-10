#include "framebuffer.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "entry/entry.h"
#include "interrupts/control/pic.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include <dev/driver.h>

// hihi the entire kernel has access to this :clown:

static struct multiboot_tag_framebuffer* s_fb_tag;
static uintptr_t s_fb_addr;
static uint8_t s_bpp;
static uint32_t s_pitch;
static uint32_t s_used_pages;
static uint32_t s_width;
static uint32_t s_height;

int fb_driver_init();
int fb_driver_exit();
uintptr_t fb_driver_on_packet(packet_payload_t payload, packet_response_t** response);

// FIXME: this driver only works for the multiboot fb that we get passed
// by the bootloaded. We need to create another driver for using an external 
// GPU
aniva_driver_t g_base_fb_driver = {
  .m_name = "fb",
  .m_type = DT_GRAPHICS,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_ident = DRIVER_IDENT(2, 0),
  .f_init = fb_driver_init,
  .f_exit = fb_driver_exit,
  .f_drv_msg = fb_driver_on_packet,
  .m_port = 2,
};
EXPORT_DRIVER(g_base_fb_driver);

static int __set_mb_tag(struct multiboot_tag_framebuffer* tag)
{
  if (!tag) {
    return -1;
  }

  if (tag->common.size == 0 || tag->common.framebuffer_addr == 0) {
    return -1;
  }

  println("Set framebuffer info");
  s_fb_tag = tag;
  s_fb_addr = tag->common.framebuffer_addr;
  s_width = tag->common.framebuffer_width;
  s_height = tag->common.framebuffer_height;
  s_bpp = tag->common.framebuffer_bpp;
  s_pitch = tag->common.framebuffer_pitch;
  s_used_pages = (s_pitch * s_height + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;

  /* Mark framebuffer memory as used, just to be sure */
  for (uintptr_t i = 0; i < s_used_pages; i++) {
    kmem_set_phys_page_used(kmem_get_page_idx(s_fb_addr) + i);
  }

  return 0;
}

int fb_driver_init() {

  int result;
  struct multiboot_tag_framebuffer* fb;

  if (!g_system_info.has_framebuffer) {
    return -1;
  }

  println("Initialized fb driver!");
  s_fb_tag = nullptr;
  s_bpp = 0;
  s_pitch = 0;
  s_width = 0;
  s_height = 0;
  s_used_pages = 0;

  /* Early ready mark */
  driver_set_ready("graphics/fb");

  fb = get_mb2_tag((void*)g_system_info.multiboot_addr, MULTIBOOT_TAG_TYPE_FRAMEBUFFER);

  result = __set_mb_tag(fb);

  return result;
}

int fb_driver_exit() {
  return 0;
}

uintptr_t fb_driver_on_packet(packet_payload_t payload, packet_response_t** response) {

  int result;

  switch (payload.m_code) {
    case FB_DRV_MAP_FB: {
      // TODO: save a list of mappings, so we can unmap it on driver unload
      uintptr_t virtual_base = *(uintptr_t*)payload.m_data;
      print("Mapped framebuffer: ");
      println(to_string(virtual_base));
      print("Size: ");
      println(to_string(s_used_pages * SMALL_PAGE_SIZE));
      //kmem_map_range(nullptr, virtual_base, s_fb_addr, s_used_pages, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_NO_PHYS_REALIGN, 0);
      __kmem_alloc_ex(nullptr, nullptr, s_fb_addr, virtual_base, s_used_pages * SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_NO_REMAP, 0);
      return 0;
      break;
    }
    case FB_DRV_SET_FB:
      break;
    case FB_DRV_SET_WIDTH:
      break;
    case FB_DRV_SET_HEIGHT:
      break;
    case FB_DRV_RESET:
      break;
    case FB_DRV_GET_FB_INFO: {
      fb_info_t info = {
        .bpp = s_bpp,
        .pitch = s_pitch,
        .width = s_width,
        .height = s_height,
        .used_pages = s_used_pages,
        .paddr = s_fb_addr,
      };
      println("sent info");
      println(to_string(info.bpp));
      *response = create_packet_response(&info, sizeof(info));
      break;
    }
  }
  return result;
}
