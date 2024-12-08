#include "lightos/dev/shared.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include <lightos/dev/device.h>
#include <lightos/lib.h>
#include <lightos/proc/process.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

typedef int (*KTERM_UPDATE_BOX_fn)(uint32_t p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content);
static KTERM_UPDATE_BOX_fn __kterm_update_fn;

static inline void __try_get_kterm_lib()
{
    HANDLE kterm;
    KTERM_UPDATE_BOX_fn update_box;

    __kterm_update_fn = nullptr;

    /* Try to load the kterm library */
    if (!lib_load("kterm.lib", &kterm))
        return;

    /* Grab the update box function from the library */
    if (!lib_get_function(kterm, "kterm_update_box", (void**)&update_box))
        return;

    __kterm_update_fn = update_box;
}

static void __update_poll_rate(uint64_t polls_this_second)
{
    char buffer[64];

    if (!__kterm_update_fn)
        return;

    snprintf(buffer, sizeof(buffer), "Rate: %lld Hz\n", polls_this_second);

    __kterm_update_fn(0, 0, 0, 1, "USB keyboard", buffer);
}

int main()
{
    DEV_HANDLE device;
    DEVINFO info = { 0 };
    size_t nr_poll_count = 0;
    size_t nr_polls_since_last_sec = 0;
    size_t elapsed_time_s = 0;
    size_t process_time_ms = 0;

    /* Try to get the kterm library */
    __try_get_kterm_lib();

    /* Grab the device */
    device = open_device("Dev/hid/usbkbd_0", HNDL_FLAG_RW);

    /* Check if it exist */
    if (!handle_verify(device))
        return ENODEV;

    /* Set up this shit */
    info.dev_specific_info = &nr_poll_count;
    info.dev_specific_size = sizeof(nr_poll_count);

    do {
        elapsed_time_s = process_time_ms / 1000ULL;
        process_time_ms = get_process_time();

        /* No new second yet, cycle; */
        if (elapsed_time_s == (process_time_ms / 1000ULL))
            goto sleep_and_cycle;

        /* Just bail if this isn't working */
        if (!device_query_info(device, &info))
            break;

        /* Update the poll rate for this second */
        __update_poll_rate(nr_poll_count - nr_polls_since_last_sec);

        nr_polls_since_last_sec = nr_poll_count;
    sleep_and_cycle:
        /* Sleep a bit */
        usleep(1);
    } while (true);

    return 0;
}
