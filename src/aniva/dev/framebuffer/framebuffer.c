#include "framebuffer.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "interupts/control/pic.h"
#include "libk/error.h"
#include "libk/multiboot.h"
#include "mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include <dev/driver.h>

// hihi the entire kernel has access to this :clown:

static struct multiboot_tag_framebuffer* s_fb_tag;
static uintptr_t s_fb_addr;
static uint8_t s_bpp;
static uint32_t s_pitch;
static uint32_t s_used_pages;
static uint32_t s_width;
static uint32_t s_height;

void fb_driver_init();
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

void fb_driver_init() {
  println("Initialized fb driver!");
  s_fb_tag = nullptr;
  s_bpp = 0;
  s_pitch = 0;
  s_width = 0;
  s_height = 0;
  s_used_pages = 0;
}

int fb_driver_exit() {
  return 0;
}

uintptr_t fb_driver_on_packet(packet_payload_t payload, packet_response_t** response) {
  switch (payload.m_code) {
    case FB_DRV_SET_MB_TAG: {
      struct multiboot_tag_framebuffer* tag = payload.m_data;
      if (tag->common.size != 0 && tag->common.framebuffer_addr != 0) {
        s_fb_tag = tag;
        s_fb_addr = tag->common.framebuffer_addr;
        s_width = tag->common.framebuffer_width;
        s_height = tag->common.framebuffer_height;
        s_bpp = tag->common.framebuffer_bpp;
        s_pitch = tag->common.framebuffer_pitch;
        s_used_pages = (s_pitch * s_height + SMALL_PAGE_SIZE - 1) / SMALL_PAGE_SIZE;
      }

      break;
    }
    case FB_DRV_MAP_FB: {
      // TODO: save a list of mappings, so we can unmap it on driver unload
      uintptr_t virtual_base = *(uintptr_t*)payload.m_data;
      //println(to_string(virtual_base));
      kmem_map_range(nullptr, virtual_base, s_fb_addr, s_used_pages, KMEM_CUSTOMFLAG_GET_MAKE, 0);
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
      };
      println("sent info");
      *response = create_packet_response(&info, sizeof(info));
      break;
    }
  }
  return 0;
}
