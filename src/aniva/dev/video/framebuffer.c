#include "framebuffer.h"
#include "mem/heap.h"

int generic_draw_rect(fb_info_t* info, uint32_t x, uint32_t y, uint32_t width, uint32_t height, fb_color_t clr)
{
    if (!info || !info->kernel_addr || !width || !height)
        return -1;

    const uint32_t bytes_per_pixel = info->bpp >> 3;

    void* offset = info->kernel_addr + (void*)(u64)(y * info->pitch + x * bytes_per_pixel);
    u32 fb_color = (clr.components.r << info->colors.red.offset_bits) | (clr.components.g << info->colors.green.offset_bits) | (clr.components.b << info->colors.blue.offset_bits);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {

            if ((x + j) >= info->width)
                break;

            *(uint32_t volatile*)(offset + j * bytes_per_pixel) = fb_color;
        }

        if ((y + i) >= info->height)
            break;

        offset += info->pitch;
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
