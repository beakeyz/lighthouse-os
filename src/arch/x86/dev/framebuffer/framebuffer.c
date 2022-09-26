#include "framebuffer.h"
#include "arch/x86/dev/debug/serial.h"
#include "arch/x86/interupts/control/pic.h"
#include "arch/x86/mem/kmem_manager.h"
#include "libc/stddef.h"
#include "libc/string.h"

static fb_data_t framebuffer_data;

void init_fb(struct multiboot_tag_framebuffer *mb_fb) {
    framebuffer_data.address = (void*)(uintptr_t)FRAMEBUFFER_VIRTUAL_BASE;
    framebuffer_data.phys_address = mb_fb->common.framebuffer_addr;
    framebuffer_data.height = mb_fb->common.framebuffer_height;
    framebuffer_data.width = mb_fb->common.framebuffer_width;
    framebuffer_data.bpp = mb_fb->common.framebuffer_bpp;
    framebuffer_data.pitch = mb_fb->common.framebuffer_pitch;
    framebuffer_data.memory_size = mb_fb->common.framebuffer_pitch * mb_fb->common.framebuffer_height;

    uint32_t fb_page_count = framebuffer_data.memory_size / SMALL_PAGE_SIZE;

    for (uintptr_t i = 0; i <= fb_page_count; i++) {
        pml_t* page = kmem_get_page((uintptr_t)framebuffer_data.address + i * SMALL_PAGE_SIZE, KMEM_GET_MAKE);
        kmem_map_memory(page, framebuffer_data.phys_address + (i * SMALL_PAGE_SIZE), KMEM_FLAG_WRITABLE);
    }
    
    // quick kinda color tester
    uint32_t color = 0xFFFFFFFF;
	for (size_t y = 0; y < get_data().height; ++y) {
		for (size_t x = 0; x < get_data().width; ++x) {
            color -= 1;
            if (color == 0x00) {
                color = 0xFFFFFFFF;
            }
            draw_pixel(x, y, color);
		}
	}
}

void draw_pixel (uint64_t x, uint64_t y, uint32_t color) {
    if (get_data().bpp == 32) {
        void* addr = get_data().address;
        ((uint32_t*)addr)[y * (get_data().pitch/8) + x] = color;
    } else {
        // yikes
        println("No impl for bpp != 32");
    }
}
    
fb_data_t get_data() {
    return framebuffer_data;
}
