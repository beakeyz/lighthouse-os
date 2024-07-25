#include "video.h"
#include "libgfx/shared.h"
#include "lightos/driver/drv.h"

BOOL lwindow_update(lwindow_t* wnd)
{
    /* No need to update, don't do shit =) */
    if ((wnd->wnd_flags & LWND_FLAG_NEED_UPDATE) != LWND_FLAG_NEED_UPDATE)
        return FALSE;

    return driver_send_msg(wnd->lwnd_handle, LWND_DCC_UPDATE_WND, wnd, sizeof(*wnd));
}

BOOL lwindow_request_framebuffer(lwindow_t* wnd, lframebuffer_t* fb)
{
    BOOL res;

    if (!wnd || !fb)
        return FALSE;

    res = driver_send_msg(wnd->lwnd_handle, LWND_DCC_REQ_FB, wnd, sizeof(*wnd));

    if (!res)
        return FALSE;

    wnd->wnd_flags |= LWND_FLAG_HAS_FB;

    *fb = wnd->fb;
    return TRUE;
}

BOOL lwindow_draw_rect(lwindow_t* wnd, uint32_t x, uint32_t y, uint32_t width, uint32_t height, lcolor_t clr)
{
    uintptr_t vaddr;
    uint32_t _clr = 0;
    uint32_t bytes_pp;

    if (!wnd || !lwindow_has_fb(wnd))
        return FALSE;

    /* No dimentions, don't need to do jack */
    if (!width || !height)
        return FALSE;

    /* Compute the initial video address */
    bytes_pp = (wnd->fb.bpp >> 3);
    vaddr = wnd->fb.fb + y * wnd->fb.pitch + (x * bytes_pp);

    /* Compute the internal color variable */
    _clr |= (((uint32_t)clr.r << wnd->fb.red_lshift));
    _clr |= (((uint32_t)clr.g << wnd->fb.green_lshift));
    _clr |= (((uint32_t)clr.b << wnd->fb.blue_lshift));

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++)
            *(uint32_t volatile*)(vaddr + j * bytes_pp) = _clr;
        /* Add the pitch to offset to the start of the next 'scanline' */
        vaddr += wnd->fb.pitch;
    }

    /* We did shit, need an update */
    wnd->wnd_flags |= LWND_FLAG_NEED_UPDATE;

    return lwindow_update(wnd);
}

BOOL lwindow_draw_buffer(lwindow_t* wnd, uint32_t startx, uint32_t starty, lclr_buffer_t* buffer)
{
    uintptr_t vaddr;
    uint32_t bytes_pp;
    void* color_buffer;

    if (!wnd || !lwindow_has_fb(wnd))
        return FALSE;

    if (!buffer || !buffer->width || !buffer->height)
        return FALSE;

    /* Compute the initial video address */
    bytes_pp = (wnd->fb.bpp >> 3);
    vaddr = wnd->fb.fb + (uint64_t)starty * wnd->fb.pitch + (startx * bytes_pp);
    color_buffer = buffer->buffer;

    for (uint32_t i = 0; i < buffer->height; i++) {
        for (uint32_t j = 0; j < buffer->width; j++) {
            *(uint32_t volatile*)(vaddr + j * bytes_pp) = *(uint32_t*)color_buffer;

            color_buffer += bytes_pp;
        }
        /* Add the pitch to offset to the start of the next 'scanline' */
        vaddr += wnd->fb.pitch;
    }

    /* We did shit, need an update */
    wnd->wnd_flags |= LWND_FLAG_NEED_UPDATE;

    return lwindow_update(wnd);
}
