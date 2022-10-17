#ifndef __KMAIN__
#define __KMAIN__

#include <arch/x86/mem/pml.h>
#include <libk/stddef.h>
#include "multiboot.h"

extern uintptr_t _kernel_start;
extern uintptr_t _kernel_end;
extern uintptr_t _kernel_physical_end;

extern pml_t boot_pdpt[];
extern pml_t boot_pdt[];

typedef struct multiboot_color _color_t;

typedef struct {
    uint32_t type;
    uint32_t size;

    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;

    union
    {
        struct
        {
            uint16_t framebuffer_palette_num_colors;
            _color_t framebuffer_palette[0];
        };
        struct
        {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
    };
  };


} framebuffer_mmap_bypass_t;

#endif // !__KMAIN__