#include "kterm/shared.h"
#include "lightos/driver/drv.h"
#include "lightos/lib/lightos.h"
#include "string.h"

static HANDLE kterm_handle;

/*!
 * @brief: Get the CWD directly from kterm
 */
int kterm_get_cwd(const char** cwd)
{
    /* Allocate a static buffer on the stack */
    const char buffer[256] = { 0 };

    /* Do a lil ioctl */
    if (!driver_send_msg(kterm_handle, KTERM_DRV_GET_CWD, (void*)buffer, sizeof(buffer)))
        return -1;

    /* Copy the shit */
    *cwd = strdup(buffer);
    return 0;
}

int kterm_create_box(uint32_t* p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content)
{
    return 0;
}

int kterm_update_box(uint32_t p_id, uint32_t x, uint32_t y, uint8_t color_idx, const char* title, const char* content)
{
    return 0;
}

/*!
 * @brief: Entrypoint for the kterm userkit
 *
 * Checks if kterm is loaded
 */
LIGHTENTRY int kterm_ulib_entry()
{
    BOOL res;

    res = open_driver("other/kterm", NULL, NULL, &kterm_handle);

    /* Could not open the driver, fuck */
    if (!res)
        return -1;

    return 0;
}
