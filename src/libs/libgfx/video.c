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

    if (!wnd || !lwindow_has_fb(wnd))
        return FALSE;

    /* No dimentions, don't need to do jack */
    if (!width || !height)
        return FALSE;

    /* Compute the initial video address */
    vaddr = wnd->fb.fb + y * wnd->fb.pitch + (x * (wnd->fb.bpp >> 3));

    /* Compute the internal color variable */
    _clr |= ((clr.r << wnd->fb.red_lshift) & wnd->fb.red_mask);
    _clr |= ((clr.g << wnd->fb.green_lshift) & wnd->fb.green_mask);
    _clr |= ((clr.b << wnd->fb.blue_lshift) & wnd->fb.blue_mask);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++)
            *(uint32_t volatile*)(vaddr + j) = _clr;
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

    if (!wnd || !lwindow_has_fb(wnd))
        return FALSE;

    if (!buffer || !buffer->width || !buffer->height)
        return FALSE;

    /* Compute the initial video address */
    vaddr = wnd->fb.fb + starty * wnd->fb.pitch + (startx * (wnd->fb.bpp >> 3));

    for (uint32_t i = 0; i < buffer->height; i++) {
        for (uint32_t j = 0; j < buffer->width; j++) {
            *(uint32_t volatile*)(vaddr + j) = *(uint32_t*)buffer->buffer;

            buffer->buffer += (wnd->fb.bpp >> 3);
        }
        /* Add the pitch to offset to the start of the next 'scanline' */
        vaddr += wnd->fb.pitch;
    }

    /* We did shit, need an update */
    wnd->wnd_flags |= LWND_FLAG_NEED_UPDATE;

    return lwindow_update(wnd);
}
