#include "dev/video/core.h"
#include "dev/video/framebuffer.h"
#include "drivers/env/lwnd/alloc.h"
#include "drivers/env/lwnd/io.h"
#include "drivers/env/lwnd/screen.h"
#include "drivers/env/lwnd/window/app.h"
#include "drivers/env/lwnd/window/wallpaper.h"
#include "fs/file.h"
#include "kevent/event.h"
#include "kevent/types/keyboard.h"
#include "kevent/types/proc.h"
#include "libk/bin/elf.h"
#include "libk/flow/error.h"
#include "lightos/event/key.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/profile/profile.h"
#include "window.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/video/device.h>

static fb_info_t _fb_info;
static lwnd_mouse_t _mouse;
static lwnd_keyboard_t _keyboard;
static kevent_kb_ctx_t* _lwnd_key_ctx_buffer;
static uint32_t _lwnd_keybuffer_r_ptr;
static struct kevent_kb_keybuffer _lwnd_keybuffer;

static video_device_t* _lwnd_vdev;
static fb_handle_t _lwnd_fb_handle;

// static uint32_t _main_screen;
// static vector_t* _screens;
static lwnd_screen_t* main_screen;

static enum ANIVA_SCANCODES _forcequit_sequence[] = {
    ANIVA_SCANCODE_LALT,
    ANIVA_SCANCODE_LSHIFT,
    ANIVA_SCANCODE_Q,
};

static int lwnd_on_key(kevent_kb_ctx_t* ctx);

/*!
 * @brief: Main compositing loop
 *
 * A few things need to happen here:
 * 1) Loop over the windows and draw visible stuff
 * 2) Queue events to windows that want them
 */
static void USED lwnd_main()
{
    bool recursive_update;
    lwnd_window_t* current_wnd;
    lwnd_screen_t* current_screen;
    kevent_kb_ctx_t* c_kb_ctx;

    (void)_mouse;
    (void)_keyboard;

    /* Launch an app to test our shit */
    // create_test_app(main_screen);

    current_screen = main_screen;

    while (true) {

        recursive_update = false;

        /* Check for event BEFORE we draw */
        lwnd_screen_handle_events(current_screen);

        mutex_lock(current_screen->draw_lock);

        /* Loop over the stack from top to bottom */
        for (uint32_t i = current_screen->highest_wnd_idx - 1;; i--) {
            current_wnd = current_screen->window_stack[i];

            if (!current_wnd)
                continue;

            /* Lock this window */
            mutex_lock(current_wnd->lock);

            /* Check if this windows wants to move */
            /* Clear the old bits in the pixel bitmap (These pixels will be overwritten with whatever is under them)*/
            /* Set the new bits in the pixel bitmap */
            /* Draw whatever is visible of the internal window buffer to its new location */

            /* If a window needs to sync, everything under it also needs to sync */
            if (!recursive_update)
                recursive_update = lwnd_window_should_redraw(current_wnd);
            else
                current_wnd->flags |= LWND_WNDW_NEEDS_REPAINT;

            /* Funky draw */
            (void)lwnd_draw(current_wnd);

            /* Unlock this window */
            mutex_unlock(current_wnd->lock);

            if (!i)
                break;
        }

        mutex_unlock(current_screen->draw_lock);

        c_kb_ctx = keybuffer_read_key(&_lwnd_keybuffer, &_lwnd_keybuffer_r_ptr);

        if (c_kb_ctx)
            (void)lwnd_on_key(c_kb_ctx);

        scheduler_yield();
    }
}

/*!
 * @brief: This is a temporary key handler to test out window event and shit
 *
 * Sends the key event over to the focussed window if there isn't a special key combination pressed
 */
int lwnd_on_key(kevent_kb_ctx_t* ctx)
{
    lwnd_window_t* wnd;

    if (!main_screen || !main_screen->event_lock || mutex_is_locked(main_screen->event_lock))
        return 0;

    wnd = lwnd_screen_get_top_window(main_screen);

    if (!wnd)
        return 0;

    enum ANIVA_SCANCODES keys[] = { ANIVA_SCANCODE_LALT, ANIVA_SCANCODE_Q };

    if (kevent_is_keycombination_pressed(ctx, _forcequit_sequence, arrlen(_forcequit_sequence))) {
        switch (wnd->type) {
        case LWND_TYPE_PROCESS:
            try_terminate_process(wnd->client.proc);
            break;
        }

        return 0;
    }

    if (kevent_is_keycombination_pressed(ctx, keys, 2)) {
        KLOG_DBG("   Executing DOOM\n");
        file_t* doom_f = file_open("Root/Apps/doom");

        if (!doom_f)
            return 0;

        proc_t* doom_p = elf_exec_64(doom_f, false);

        file_close(doom_f);

        if (!doom_p)
            return 0;

        sched_add_priority_proc(doom_p, SCHED_PRIO_HIGH, true);
        return 0;
    }

    /* TODO: save this instantly to userspace aswell */
    lwnd_save_keyevent(wnd, ctx);
    return 0;
}

static int on_proc(kevent_ctx_t* _ctx, void* param)
{
    uint32_t wnd_count, wnd_idx;
    lwnd_window_t* c_wnd;
    lwnd_screen_t* c_screen;
    kevent_proc_ctx_t* ctx;

    if (_ctx->buffer_size != sizeof(*ctx))
        return 0;

    wnd_idx = 0;
    wnd_count = 0;
    c_screen = main_screen;
    ctx = _ctx->buffer;

    switch (ctx->type) {
    case PROC_EVENTTYPE_DESTROY:

        /*
         * When a process gets destroyed, we need to check if it had windows open with us.
         * Right now we just itterate all the windows, which is fine if we don't have that many windows.
         *
         * FIXME: We probably also need to itterate the screens, but since we only have one screen rn, that should be fine
         * TODO: Cache the windows per process, so we don't have to do weird itterations when processes die
         */
        do {
            c_wnd = c_screen->window_stack[wnd_idx++];

            if (!c_wnd)
                continue;

            wnd_count++;

            if (c_wnd->type != LWND_TYPE_PROCESS)
                continue;

            /* Skip windows that are not from this process */
            if (c_wnd->client.proc != ctx->process)
                continue;

            lwnd_screen_unregister_window(c_screen, c_wnd);

            destroy_lwnd_window(c_wnd);
        } while (wnd_count < c_screen->window_count);
        break;
    default:
        break;
    }

    return 0;
}

static int __on_key(kevent_ctx_t* ctx, void* param)
{
    if (!ctx->buffer || ctx->buffer_size != sizeof(kevent_kb_ctx_t))
        return 0;

    /* Save the keypress */
    keybuffer_write_key(&_lwnd_keybuffer, ctx->buffer);
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

    int error;

    memset(&_fb_info, 0, sizeof(_fb_info));

    println("Initializing allocators!");
    error = init_lwnd_alloc();

    if (error)
        return -1;

    profile_set_activated(get_user_profile());

    /* Initialize the lwnd keybuffer */
    _lwnd_keybuffer_r_ptr = NULL;
    _lwnd_key_ctx_buffer = kmalloc(sizeof(*_lwnd_key_ctx_buffer) * 32);
    init_kevent_kb_keybuffer(&_lwnd_keybuffer, _lwnd_key_ctx_buffer, 32);

    _lwnd_vdev = get_active_vdev();
    /*
     * Try and get framebuffer info from the active video device
     * TODO: when we implement 2D acceleration, we probably need to do something else here
     * TODO: implement screens on the drivers side with 'connectors'
     */
    // Must(driver_send_msg_a("core/video", VIDDEV_DCC_GET_FBINFO, NULL, NULL, &_fb_info, sizeof(_fb_info)));
    vdev_get_mainfb(_lwnd_vdev->device, &_lwnd_fb_handle);

    /* Fill our framebuffer info struct */
    _fb_info.size = vdev_get_fb_size(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.bpp = vdev_get_fb_bpp(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.pitch = vdev_get_fb_pitch(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.width = vdev_get_fb_width(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.height = vdev_get_fb_height(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.red.offset_bits = vdev_get_fb_red_offset(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.red.length_bits = vdev_get_fb_red_length(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.green.offset_bits = vdev_get_fb_green_offset(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.green.offset_bits = vdev_get_fb_green_offset(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.blue.length_bits = vdev_get_fb_blue_length(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.colors.blue.length_bits = vdev_get_fb_blue_length(_lwnd_vdev->device, _lwnd_fb_handle);
    _fb_info.addr = EARLY_FB_MAP_BASE;
    _fb_info.kernel_addr = EARLY_FB_MAP_BASE;

    /* Map the thing */
    vdev_map_fb(_lwnd_vdev->device, _lwnd_fb_handle, EARLY_FB_MAP_BASE);

    /* TODO: register to I/O core */
    kevent_add_hook("keyboard", "lwnd", __on_key, NULL);
    kevent_add_hook("proc", "lwnd", on_proc, NULL);

    println("Initializing screen!");
    main_screen = create_lwnd_screen(&_fb_info, LWND_SCREEN_MAX_WND_COUNT);

    if (!main_screen)
        return -1;

    println("Initializing wallpaper!");
    init_lwnd_wallpaper(main_screen);

    println("Starting deamon!");
    ASSERT_MSG(spawn_thread("lwnd_main", lwnd_main, NULL), "Failed to create lwnd main thread");

    file_t* test_f = file_open("Root/Apps/gfx_test");

    if (!test_f)
        return 0;

    proc_t* test_p = elf_exec_64(test_f, false);

    file_close(test_f);

    if (!test_p)
        return 0;

    sched_add_priority_proc(test_p, SCHED_PRIO_LOW, true);

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
    window_id_t lwnd_id;
    lwnd_window_t* internal_wnd;
    lwindow_t* uwindow;
    proc_t* calling_process;

    calling_process = get_current_proc();

    /* Check size */
    if (!calling_process || size != sizeof(*uwindow))
        return DRV_STAT_INVAL;

    /* Check pointer */
    if ((kmem_validate_ptr(calling_process, (uintptr_t)buffer, size)))
        return DRV_STAT_INVAL;

    /* Unsafe assignment */
    uwindow = buffer;

    static size_t offset_yay = 0;

    switch (code) {

    case LWND_DCC_CREATE:
        /*
         * Create the window while we know we aren't drawing anything
         * NOTE: This takes the draw lock
         */
        lwnd_id = create_app_lwnd_window(main_screen, offset_yay, offset_yay, uwindow, calling_process);

        if (lwnd_id == LWND_INVALID_ID)
            return DRV_STAT_INVAL;

        offset_yay += 20;
        uwindow->wnd_id = lwnd_id;
        break;

    case LWND_DCC_CLOSE:
        internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

        if (!internal_wnd)
            return DRV_STAT_INVAL;

        lwnd_screen_unregister_window(main_screen, internal_wnd);

        destroy_lwnd_window(internal_wnd);
        break;
    case LWND_DCC_REQ_FB:
        mutex_lock(main_screen->draw_lock);

        /* Grab the window */
        internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

        mutex_unlock(main_screen->draw_lock);

        if (!internal_wnd)
            return DRV_STAT_INVAL;

        uwindow->fb = internal_wnd->user_fb_ptr;
        break;

    case LWND_DCC_UPDATE_WND:
        mutex_lock(main_screen->draw_lock);

        internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

        if (!internal_wnd) {
            mutex_unlock(main_screen->draw_lock);
            return DRV_STAT_INVAL;
        }

        lwnd_window_update(internal_wnd, true);

        mutex_unlock(main_screen->draw_lock);
        break;
    case LWND_DCC_GETKEY:
        /* Grab the window */
        internal_wnd = lwnd_screen_get_window(main_screen, uwindow->wnd_id);

        lkey_event_t* u_event;
        kevent_kb_ctx_t kbd_ctx;

        if (lwnd_load_keyevent(internal_wnd, &kbd_ctx))
            return DRV_STAT_INVAL;

        /* Grab a buffer entry */
        u_event = &uwindow->keyevent_buffer[uwindow->keyevent_buffer_write_idx++];

        /* Write the data */
        u_event->keycode = kbd_ctx.keycode;
        u_event->pressed_char = kbd_ctx.pressed_char;
        u_event->pressed = kbd_ctx.pressed;
        u_event->mod_flags = NULL;

        /* Make sure we cycle the index */
        uwindow->keyevent_buffer_write_idx %= uwindow->keyevent_buffer_capacity;

        break;
    }
    /*
     * TODO:
     * - Check what the process wants to do
     * - Check if the process is cleared to do that thing
     * - do the thing
     * - fuck off
     */
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
