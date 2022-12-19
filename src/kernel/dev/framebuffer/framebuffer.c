#include "framebuffer.h"
#include "kernel/dev/debug/serial.h"
#include "kernel/interupts/control/pic.h"
#include "kernel/mem/kmem_manager.h"
#include "libk/stddef.h"
#include "libk/string.h"

static fb_data_t framebuffer_data;

// TODO: double buffers
void init_fb(struct multiboot_tag_framebuffer *mb_fb) {
  framebuffer_data.address = (void*)(uintptr_t)FRAMEBUFFER_VIRTUAL_BASE;
  framebuffer_data.phys_address = mb_fb->common.framebuffer_addr;
  framebuffer_data.height = mb_fb->common.framebuffer_height;
  framebuffer_data.width = mb_fb->common.framebuffer_width;
  framebuffer_data.bpp = mb_fb->common.framebuffer_bpp;
  framebuffer_data.pitch = mb_fb->common.framebuffer_pitch;
  framebuffer_data.memory_size = mb_fb->common.framebuffer_pitch * mb_fb->common.framebuffer_height;

  for (uintptr_t i = 0; i < framebuffer_data.memory_size; i+=SMALL_PAGE_SIZE) {
    kmem_map_page(nullptr, (uintptr_t)framebuffer_data.address + i, framebuffer_data.phys_address + i, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER);
  }

  // quick kinda color tester
  uint32_t color = 0xFFFFFFFF;
  for (size_t y = 0; y < get_data().height; ++y) {
    for (size_t x = 0; x < get_data().width; ++x) {
      draw_pixel(x, y, color);
    }
  }
}

void draw_pixel (uint64_t x, uint64_t y, uint32_t color) {
  *(uint32_t*)((uintptr_t)get_data().address + get_data().pitch * y + x * 4) = color;
}
    
fb_data_t get_data() {
    return framebuffer_data;
}
