#include "draw.h"
#include "libgfx/video.h"
#include "lightui/window.h"

int lightui_draw_rect(lightui_window_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t clr)
{
    uintptr_t vaddr;
    uint32_t _clr = 0;
    uint32_t bytes_pp;

    if (!wnd)
        return -1;

    /* No dimentions, don't need to do jack */
    if (!width || !height)
        return -1;

    /* Compute the initial video address */
    bytes_pp = (wnd->gfxwnd.fb.bpp >> 3);
    vaddr = (uintptr_t)wnd->user_fb_start + y * wnd->gfxwnd.fb.pitch + (x * bytes_pp);

    /* Compute the internal color variable */
    _clr |= (((uint32_t)clr.r << wnd->gfxwnd.fb.red_lshift));
    _clr |= (((uint32_t)clr.g << wnd->gfxwnd.fb.green_lshift));
    _clr |= (((uint32_t)clr.b << wnd->gfxwnd.fb.blue_lshift));

    if ((x + width) >= wnd->lui_width)
        width -= ((x + width) - wnd->lui_width);

    if ((y + height) >= wnd->lui_height)
        height -= ((y + height) - wnd->lui_height);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++)
            *(uint32_t volatile*)(vaddr + j * bytes_pp) = _clr;
        /* Add the pitch to offset to the start of the next 'scanline' */
        vaddr += wnd->gfxwnd.fb.pitch;
    }

    /* We did shit, need an update */
    wnd->gfxwnd.wnd_flags |= LWND_FLAG_NEED_UPDATE;

    return !lwindow_update(&wnd->gfxwnd);
}

int lightui_draw_buffer(lightui_window_t* wnd, uint32_t x, uint32_t y, lclr_buffer_t* buffer)
{
    uintptr_t vaddr;
    uint32_t bytes_pp;
    void* color_buffer;

    if (!wnd)
        return -1;

    if (!buffer || !buffer->width || !buffer->height)
        return -1;

    /* Compute the initial video address */
    bytes_pp = (wnd->gfxwnd.fb.bpp >> 3);
    vaddr = (uintptr_t)wnd->user_fb_start + (uint64_t)y * wnd->gfxwnd.fb.pitch + (x * bytes_pp);
    color_buffer = buffer->buffer;

    if ((x + buffer->width) >= wnd->lui_width)
        buffer->width -= ((x + buffer->width) - wnd->lui_width);

    if ((y + buffer->height) >= wnd->lui_height)
        buffer->height -= ((y + buffer->height) - wnd->lui_height);

    for (uint32_t i = 0; i < buffer->height; i++) {
        for (uint32_t j = 0; j < buffer->width; j++) {
            *(uint32_t volatile*)(vaddr + j * bytes_pp) = *(uint32_t*)color_buffer;

            color_buffer += bytes_pp;
        }
        /* Add the pitch to offset to the start of the next 'scanline' */
        vaddr += wnd->gfxwnd.fb.pitch;
    }

    /* We did shit, need an update */
    wnd->gfxwnd.wnd_flags |= LWND_FLAG_NEED_UPDATE;

    return !lwindow_update(&wnd->gfxwnd);
}
