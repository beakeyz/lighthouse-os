#include "framebuffer.h"
#include "mem/heap.h"

int generic_draw_rect(fb_info_t* info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr)
{
    if (!info || !info->kernel_addr || !width || !height)
        return -1;

    const uint32_t bytes_per_pixel = info->bpp / 8;
    const uint32_t r_bits_offset = info->colors.red.offset_bits / 8;
    const uint32_t g_bits_offset = info->colors.green.offset_bits / 8;
    const uint32_t b_bits_offset = info->colors.blue.offset_bits / 8;
    const uint32_t a_bits_offset = info->colors.alpha.offset_bits / 8;

    uint32_t current_x_offset;
    uint32_t current_y_offset = y * info->pitch;

    for (uint32_t i = 0; i < height; i++) {

        current_x_offset = x;

        for (uint32_t j = 0; j < width; j++) {

            if (current_x_offset >= info->width)
                break;

            *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + r_bits_offset) = clr.components.r;
            *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + g_bits_offset) = clr.components.g;
            *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + b_bits_offset) = clr.components.b;
            if (bytes_per_pixel > 3 || !a_bits_offset)
                *(uint8_t volatile*)(info->kernel_addr + current_x_offset * bytes_per_pixel + current_y_offset + a_bits_offset) = clr.components.a;

            current_x_offset++;
        }

        if (y + i >= info->height)
            break;

        current_y_offset += info->pitch;
    }

    return 0;
}

int fb_helper_add_mapping(fb_helper_t* helper, uint32_t x, uint32_t y, uint32_t width, uint32_t height, size_t size, vaddr_t base)
{
    fb_mapping_t* mapping;

    if (!helper || !size || !base)
        return -1;

    mapping = helper->fb_mappings;

    if (!mapping) {
        mapping = kmalloc(sizeof(*mapping));

        mapping->x = x;
        mapping->y = y;
        mapping->width = width;
        mapping->height = height;
        mapping->size = size;
        mapping->base = base;

        helper->fb_mappings = mapping;

        return 0;
    }

    do {
        /* We already have this mapping */
        if (mapping->base == base)
            return -1;

        if (!mapping->next) {
            mapping->next = kmalloc(sizeof(*mapping));

            mapping->next->x = x;
            mapping->next->y = y;
            mapping->next->width = height;
            mapping->next->height = height;
            mapping->next->size = size;
            mapping->next->base = base;

            return 0;
        }

        mapping = mapping->next;
    } while (mapping);

    return -1;
}

int fb_helper_remove_mapping(fb_helper_t* helper, vaddr_t base)
{
    fb_mapping_t* target;
    fb_mapping_t** mapping;

    if (!helper || !base)
        return -1;

    mapping = &helper->fb_mappings;

    if (!(*mapping))
        return -1;

    do {

        /* Found it! */
        if ((*mapping)->base == base) {
            /* Save the target mapping */
            target = *mapping;

            /* Fix the link */
            *mapping = (*mapping)->next;

            /* Free the memory */
            kfree(target);

            return 0;
        }

        mapping = &(*mapping)->next;
    } while (*mapping);

    /* Could not find it */
    return -1;
}

int fb_helper_get_mapping(fb_helper_t* helper, vaddr_t base, fb_mapping_t** bmapping)
{
    fb_mapping_t* walker;

    if (!helper || !base || !bmapping)
        return -1;

    walker = helper->fb_mappings;

    while (walker) {

        /* Found it! */
        if (walker->base == base) {
            *bmapping = walker;
            return 0;
        }

        walker = walker->next;
    }

    return -1;
}
