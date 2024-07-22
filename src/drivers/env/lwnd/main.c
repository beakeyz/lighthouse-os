#include "dev/io/hid/event.h"
#include "dev/io/hid/hid.h"
#include "dev/video/core.h"
#include "dev/video/device.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/windowing/stack.h"
#include "drivers/env/lwnd/windowing/window.h"
#include "libk/flow/error.h"
#include "lightos/event/key.h"
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include <libgfx/shared.h>

static video_device_t* _lwnd_vdev;
static hid_device_t* _lwnd_kbddev;
static fb_info_t _fb_info;
static fb_handle_t _lwnd_fb_handle;

static lwnd_wndstack_t* _lwnd_stack;

static enum ANIVA_SCANCODES _forcequit_sequence[] = {
    ANIVA_SCANCODE_LALT,
    ANIVA_SCANCODE_LSHIFT,
    ANIVA_SCANCODE_Q,
};

static int lwnd_on_key(hid_event_t* ctx);

static void __tmp_draw_fragmented_windo(lwnd_window_t* window, fb_color_t clr)
{
    for (lwnd_wndrect_t* r = window->rects; r; r = r->next_part)
        generic_draw_rect(&_fb_info, window->x + r->x, window->y + r->y, r->w, r->h, clr);
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

        c_kb_ctx = NULL;

        /* Check if there is a keypress ready */
        if (hid_device_poll(_lwnd_kbddev, &c_kb_ctx) == 0)
            lwnd_on_key(c_kb_ctx);

        fb_color_t color = ((fb_color_t) { { 0xff, 0x00, 0x00, 0x00 } });

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

            lwnd_window_split(&_fb_info, w);

            if (w->rects)
                __tmp_draw_fragmented_windo(w, color);
            else
                generic_draw_rect(&_fb_info, w->x, w->y, w->width, w->height, color);

            w->flags &= ~LWND_WINDOW_FLAG_NEED_UPDATE;

            color.components.g += 0xff;
        }

        scheduler_yield();
    }
}

/*!
 * @brief: This is a temporary key handler to test out window event and shit
 *
 * Sends the key event over to the focussed window if there isn't a special key combination pressed
 */
int lwnd_on_key(hid_event_t* ctx)
{
    if ((ctx->key.flags & HID_EVENT_KEY_FLAG_PRESSED) != HID_EVENT_KEY_FLAG_PRESSED)
        return 0;

    switch (ctx->key.scancode) {
    case ANIVA_SCANCODE_W:
        lwnd_window_move(_lwnd_stack->top_window, _lwnd_stack->top_window->x, _lwnd_stack->top_window->y - 10);
        break;
    case ANIVA_SCANCODE_S:
        lwnd_window_move(_lwnd_stack->top_window, _lwnd_stack->top_window->x, _lwnd_stack->top_window->y + 10);
        break;
    case ANIVA_SCANCODE_A:
        lwnd_window_move(_lwnd_stack->top_window, _lwnd_stack->top_window->x - 10, _lwnd_stack->top_window->y);
        break;
    case ANIVA_SCANCODE_D:
        lwnd_window_move(_lwnd_stack->top_window, _lwnd_stack->top_window->x + 10, _lwnd_stack->top_window->y);
        break;
    default:
        break;
    }
    return 0;
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
    _lwnd_kbddev = get_hid_device("usbkbd");

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

    generic_draw_rect(&_fb_info, 0, 0, _fb_info.width, _fb_info.height, (fb_color_t) { { 0x1f, 0x1f, 0x1f, 0xff } });

    println("Starting deamon!");
    ASSERT_MSG(spawn_thread("lwnd_main", lwnd_main, NULL), "Failed to create lwnd main thread");

    /* TEST */
    _lwnd_stack = create_lwnd_wndstack(16);

    lwnd_window_t* window_1 = create_window("Test 1", 100, 100, 200, 180);
    lwnd_window_t* window_2 = create_window("Test 2", 120, 120, 50, 50);

    wndstack_add_window(_lwnd_stack, window_1);
    wndstack_add_window(_lwnd_stack, window_2);

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

    calling_process = get_current_proc();

    /* Check size */
    if (!calling_process)
        return DRV_STAT_INVAL;

    /* Check pointer */
    if ((kmem_validate_ptr(calling_process, (uintptr_t)buffer, size)))
        return DRV_STAT_INVAL;

    switch (code) {

    case LWND_DCC_CREATE:
        break;
    case LWND_DCC_CLOSE:
        break;
    case LWND_DCC_REQ_FB:
        break;
    case LWND_DCC_UPDATE_WND:
        break;
    case LWND_DCC_GETKEY:
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
