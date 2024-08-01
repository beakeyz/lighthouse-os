#include "dev/driver.h"
#include "dev/io/hid/event.h"
#include "dev/io/hid/hid.h"
#include "dev/video/core.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/display/screen.h"
#include "drivers/env/lwnd/windowing/stack.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "drivers/env/lwnd/windowing/workspace.h"
#include "kevent/types/proc.h"
#include "libk/flow/error.h"
#include "lightos/event/key.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include <libgfx/shared.h>

/*
 * Video device that acts as a communications portal for us to
 * actual video hardware (both GPU and display)
 */
static video_device_t* _lwnd_vdev;
/* Keyboard HID device for keyboard input */
static hid_device_t* _lwnd_kbddev;
/*
 * Static framebuffer info
 * I don't really know how prone this is to change in the future, since
 * I only have one video driver working right now, but I'm not sure if
 * every driver is going to use the same framebuffer framework in the future
 */
static fb_info_t _fb_info;
/* Handle to the internal FB mapping in the video device */
static fb_handle_t _lwnd_fb_handle;

/* Main lwnd screen */
static lwnd_screen_t* _lwnd_0_screen;
static lwnd_workspace_t* _lwnd_c_ws;
static lwnd_wndstack_t* _lwnd_stack;

static enum ANIVA_SCANCODES _forcequit_sequence[] USED = {
    ANIVA_SCANCODE_LALT,
    ANIVA_SCANCODE_LSHIFT,
    ANIVA_SCANCODE_Q,
};

/*!
 * @brief: This is a temporary key handler to test out window event and shit
 *
 * Sends the key event over to the focussed window if there isn't a special key combination pressed
 */
static int lwnd_on_key(hid_event_t* ctx)
{
    if ((ctx->key.flags & HID_EVENT_KEY_FLAG_PRESSED) != HID_EVENT_KEY_FLAG_PRESSED)
        return 0;

    switch (ctx->key.scancode) {
    case ANIVA_SCANCODE_W:
        lwnd_window_move(_lwnd_0_screen, _lwnd_stack->top_window, _lwnd_stack->top_window->x, _lwnd_stack->top_window->y - 10);
        break;
    case ANIVA_SCANCODE_S:
        lwnd_window_move(_lwnd_0_screen, _lwnd_stack->top_window, _lwnd_stack->top_window->x, _lwnd_stack->top_window->y + 10);
        break;
    case ANIVA_SCANCODE_A:
        lwnd_window_move(_lwnd_0_screen, _lwnd_stack->top_window, _lwnd_stack->top_window->x - 10, _lwnd_stack->top_window->y);
        break;
    case ANIVA_SCANCODE_D:
        lwnd_window_move(_lwnd_0_screen, _lwnd_stack->top_window, _lwnd_stack->top_window->x + 10, _lwnd_stack->top_window->y);
        break;
    case ANIVA_SCANCODE_TAB:
        wndstack_cycle_windows(_lwnd_stack);
        break;
    case ANIVA_SCANCODE_RIGHTBRACKET:
        proc_exec("Root/Apps/doom -iwad Root/Apps/doom1.wad", get_user_profile(), NULL);
        break;
    case ANIVA_SCANCODE_LEFTBRACKET:
        proc_exec("Root/Apps/gfx_test", get_user_profile(), NULL);
        break;
    default:
        break;
    }
    return 0;
}

#include <kevent/event.h>

static int lwnd_on_process_event(kevent_ctx_t* ctx, void* param)
{
    lwnd_workspace_t* workspace;
    kevent_proc_ctx_t* proc_ctx = ctx->buffer;

    switch (proc_ctx->type) {
    case PROC_EVENTTYPE_DESTROY:
        /*
         * FIXME: When we support multiple screens, do this for all screens
         */
        for (uint32_t i = 0; i < _lwnd_0_screen->n_workspace; i++) {
            workspace = lwnd_screen_get_workspace(_lwnd_0_screen, i);

            if (!workspace || !workspace->stack)
                continue;

            /* Remove all windows of this process */
            lwnd_workspace_remove_windows_of_process(workspace, proc_ctx->process);
        }
        break;
    default:
        break;
    }
    return 0;
}

/*!
 * @brief: Main compositing loop
 *
 * A few things need to happen here:
 * 1) Loop over the windows and draw visible stuff
 * 2) Queue events to windows that want them
 */
static void USED lwnd_main()
{
    hid_event_t* c_kb_ctx;

    while (true) {

        if (!_lwnd_0_screen)
            goto yield_and_continue;

        c_kb_ctx = NULL;
        _lwnd_c_ws = lwnd_screen_get_c_workspace(_lwnd_0_screen);
        _lwnd_stack = _lwnd_c_ws->stack;

        /* Check if there is a keypress ready */
        if (hid_device_poll(_lwnd_kbddev, &c_kb_ctx) == 0)
            lwnd_on_key(c_kb_ctx);

        /*
         * Loop over all the windows from front to back to render all windows in
         * the right order, at the right depth
         */
        for (lwnd_window_t* w = _lwnd_stack->top_window; w != nullptr; w = w->next_layer) {

            /*
             * Per window, check if it's visible, by breaking the window in smaller parts
             * and rendering those seperately
             */
            if (!lwnd_window_should_update(w))
                continue;

            /* Split the fucker */
            lwnd_window_split(_lwnd_stack, w, false);

            /* Draw the fucker */
            lwnd_window_draw(w, _lwnd_0_screen);

            /* We've done the update =) */
            lwnd_window_clear_update(w);

            /* Make sure the background knows about this */
            lwnd_window_update(_lwnd_stack->background_window);
        }

        lwnd_wndstack_update_background(_lwnd_stack);

        /*
        _lwnd_stack->bottom_window->next_layer = _lwnd_stack->background_window;

        for (lwnd_window_t* w = _lwnd_stack->top_window; w != nullptr; w = w->next_layer) {
            for (lwnd_wndrect_t* r = w->rects; r; r = r->next_part) {
                lwnd_draw_dbg_box(&_fb_info, w->x + r->x, w->y + r->y, r->w, r->h, ((fb_color_t) { { 0x00, 00, 0xff } }));
            }
        }
        _lwnd_stack->bottom_window->next_layer = nullptr;
        */

    yield_and_continue:
        scheduler_yield();
    }
}

/*
 * Yay, we are weird!
 * We will try to manage window creation, movement, minimalisation, ect. directly from kernelspace.
 * To supplement this, this driver will spawn helper threads, provide concise messaging, ect.
 *
 * When a process wants to get a window, it can use the GFX library to request one. The GFX library in turn
 * will send an IOCTL through a syscall with which it contacts the lwnd driver. This driver knows all
 * about the current active video driver, so all we need to do is keep track of the pixels, and the windows.
 */
int init_window_driver()
{
    println("Initializing window driver!");

    memset(&_fb_info, 0, sizeof(_fb_info));

    println("Initializing allocators!");

    profile_set_activated(get_user_profile());

    _lwnd_vdev = get_active_vdev();
    _lwnd_kbddev = get_hid_device("usbkbd_0");

    if (!_lwnd_kbddev)
        _lwnd_kbddev = get_hid_device("i8042");
    /*
     * Try and get framebuffer info from the active video device
     * TODO: when we implement 2D acceleration, we probably need to do something else here
     * TODO: implement screens on the drivers side with 'connectors'
     */
    // Must(driver_send_msg_a("core/video", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &_fb_info, sizeof(_fb_info)));
    vdev_get_mainfb(_lwnd_vdev->device, &_lwnd_fb_handle);

    /* Fill our framebuffer info struct */
    ASSERT_MSG(vdev_get_fbinfo(_lwnd_vdev->device, _lwnd_fb_handle, EARLY_FB_MAP_BASE, &_fb_info) == 0, "Failed to get fbinfo from video device");

    println("Initializing screen!");

    kevent_add_hook("proc", "lwnd", lwnd_on_process_event, NULL);

    /* Create the main screen */
    _lwnd_0_screen = create_lwnd_screen(_lwnd_vdev, &_fb_info);

    if (!_lwnd_0_screen)
        return -KERR_NULL;

    println("Starting deamon!");
    ASSERT_MSG(spawn_thread("lwnd_main", lwnd_main, NULL), "Failed to create lwnd main thread");

    return 0;
}

int exit_window_driver()
{
    kernel_panic("TODO: exit window driver");
    return 0;
}

/*!
 * @brief: Main lwnd message endpoint
 *
 * Every dc message to lwnd should have a lwindow in its buffer. If this is not the case, we instantly
 * reject the message
 */
uintptr_t msg_window_driver(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
    proc_t* calling_process;
    lwindow_t* uwnd;

    lwnd_window_t* wnd;
    lwnd_workspace_t* c_workspace;

    calling_process = get_current_proc();

    /* Check size */
    if (!calling_process)
        return DRV_STAT_INVAL;

    /* Check pointer */
    if ((kmem_validate_ptr(calling_process, (uintptr_t)buffer, size)) || size != sizeof(*uwnd))
        return DRV_STAT_INVAL;

    uwnd = (lwindow_t*)buffer;

    /* Check the title buffer */
    if (kmem_validate_ptr(calling_process, (u64)uwnd->title, sizeof(const char*)))
        return DRV_STAT_INVAL;

    /* Grab workspace */
    c_workspace = lwnd_screen_get_c_workspace(_lwnd_0_screen);

    switch (code) {
    case LWND_DCC_CREATE:
        KLOG_DBG("Trying to create lwnd window!\n");
        /* Create the window */
        if ((wnd = create_window(calling_process, uwnd->title, 10, 10, uwnd->current_width, uwnd->current_height)) == nullptr)
            return DRV_STAT_INVAL;

        /* Also register the window to the current stack */
        if (wndstack_add_window(c_workspace->stack, wnd))
            return DRV_STAT_DUPLICATE;
        break;
    case LWND_DCC_CLOSE:
        KLOG_DBG("Trying to close lwnd window!\n");
        wnd = wndstack_find_window(c_workspace->stack, uwnd->title);

        if (!wnd)
            return DRV_STAT_NOT_FOUND;

        if (wndstack_remove_window(c_workspace->stack, wnd))
            return DRV_STAT_NOT_FOUND;

        destroy_window(wnd);
        break;
    case LWND_DCC_REQ_FB:
        KLOG_DBG("Trying to request framebuffer for lwnd window!\n");
        wnd = wndstack_find_window(c_workspace->stack, uwnd->title);

        if (!wnd)
            return DRV_STAT_NOT_FOUND;

        if (lwnd_window_request_fb(wnd, _lwnd_0_screen))
            return DRV_STAT_INVAL;

        /* Fill in user framebuffer info */
        uwnd->fb.fb = wnd->this_fb->addr;
        uwnd->fb.height = wnd->this_fb->height;
        uwnd->fb.pitch = wnd->this_fb->pitch;
        uwnd->fb.bpp = wnd->this_fb->bpp;
        uwnd->fb.red_lshift = wnd->this_fb->colors.red.offset_bits;
        uwnd->fb.green_lshift = wnd->this_fb->colors.green.offset_bits;
        uwnd->fb.blue_lshift = wnd->this_fb->colors.blue.offset_bits;
        uwnd->fb.alpha_lshift = wnd->this_fb->colors.alpha.offset_bits;

        uwnd->fb.red_mask = wnd->this_fb->colors.red.length_bits << uwnd->fb.red_lshift;
        uwnd->fb.green_mask = wnd->this_fb->colors.green.length_bits << uwnd->fb.green_lshift;
        uwnd->fb.blue_mask = wnd->this_fb->colors.blue.length_bits << uwnd->fb.blue_lshift;
        uwnd->fb.alpha_mask = wnd->this_fb->colors.alpha.length_bits << uwnd->fb.alpha_lshift;

        KLOG_DBG("Red mask: %x\n", uwnd->fb.red_lshift);
        KLOG_DBG("Green mask: %x\n", uwnd->fb.green_mask);
        KLOG_DBG("Blue mask: %x\n", uwnd->fb.blue_mask);

        break;
    case LWND_DCC_UPDATE_WND:
        wnd = wndstack_find_window(c_workspace->stack, uwnd->title);

        if (!wnd)
            return DRV_STAT_NOT_FOUND;

        /* Mark as needing updating */
        lwnd_window_full_update(wnd);
        break;
    case LWND_DCC_GETKEY:
        // KLOG_DBG("Trying to get key event!\n");
        break;
    }
    return DRV_STAT_OK;
}

/*
 * The Light window driver
 *
 * This 'service' will act as generic window manager in other systems, like linux, but we've packed
 * it in a kernel driver. This has a few advantages:
 *  o Better communication with the graphics drivers
 *  o Faster load times
 *  o Safer
 * It also has a few downsides or counterarguments
 *  o No REAL need to be a kernel component
 *  o Harder to customize
 */
EXPORT_DRIVER(window_driver) = {
    .m_name = "lwnd",
    .m_type = DT_SERVICE,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = init_window_driver,
    .f_exit = exit_window_driver,
    .f_msg = msg_window_driver,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};
