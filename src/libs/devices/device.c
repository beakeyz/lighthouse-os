#include "device.h"
#include "devices/shared.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cached_device {
    const char* path;
    DEV_HANDLE handle;
    uint32_t refcount;

    struct cached_device* next;
};

static struct cached_device* _cached_devices;

static struct cached_device** _get_cached_device(const char* path)
{
    struct cached_device** ret;

    ret = &_cached_devices;

    while (*ret) {

        if (strncmp(path, (*ret)->path, strlen(path)) == 0)
            break;

        ret = &(*ret)->next;
    }
    return ret;
}

static struct cached_device** _get_cached_device_by_handle(DEV_HANDLE handle)
{
    struct cached_device** ret;

    ret = &_cached_devices;

    while (*ret) {

        if ((*ret)->handle == handle)
            break;

        ret = &(*ret)->next;
    }

    return ret;
}

DEV_HANDLE open_device(const char* path, DWORD flags)
{
    DEV_HANDLE ret;
    struct cached_device** dev_slot;
    struct cached_device* new_dev;

    dev_slot = _get_cached_device(path);

    if (*dev_slot) {
        (*dev_slot)->refcount++;
        return (*dev_slot)->handle;
    }

    ret = open_handle(path, HNDL_TYPE_DEVICE, flags, NULL);

    if (handle_verify(ret) == FALSE)
        return ret;

    /* Allocate new slot */
    new_dev = malloc(sizeof(*new_dev));
    new_dev->handle = ret;
    new_dev->path = strdup(path);
    new_dev->refcount = 1;

    /* Put this bitch in */
    *dev_slot = new_dev;

    return ret;
}

BOOL close_device(DEV_HANDLE handle)
{
    struct cached_device* this_device;
    struct cached_device** dev_slot;

    dev_slot = _get_cached_device_by_handle(handle);

    /* This would be very werid lmao */
    if (!(*dev_slot))
        goto close_and_exit;

    this_device = *dev_slot;

    /* This device is still being referenced */
    if (this_device->refcount--) {
        /* Don't do shit */
        if (this_device->refcount)
            return TRUE;
    }

    /* Unlink */
    *dev_slot = this_device->next;

    free(this_device);

close_and_exit:
    return close_handle(handle);
}

BOOL handle_is_device(HANDLE handle)
{
    BOOL res;
    HANDLE_TYPE type;

    /* Check the handle type */
    res = get_handle_type(handle, &type);

    if (!res || type != HNDL_TYPE_DEVICE)
        return FALSE;

    return TRUE;
}

size_t device_read(DEV_HANDLE handle, VOID* buf, size_t bsize, QWORD offset)
{
    BOOL res;
    QWORD end_offset;

    /* Check if we're actually reading from a device */
    res = handle_is_device(handle);

    if (!res)
        return NULL;

    /* Set the offset to read at */
    res = handle_set_offset(handle, offset);

    if (!res)
        return NULL;

    /* Do the actual read on the handle */
    res = handle_read(handle, bsize, buf);

    if (!res)
        return NULL;

    end_offset = NULL;

    /* Get the offset at the end of the read */
    res = handle_get_offset(handle, &end_offset);

    if (!res || offset > end_offset)
        return NULL;

    return (end_offset - offset);
}

size_t device_write(DEV_HANDLE handle, VOID* buf, size_t bsize, QWORD offset)
{
    BOOL res;
    QWORD end_offset;

    /* Check if we're actually writing to a device */
    res = handle_is_device(handle);

    if (!res)
        return NULL;

    /* Set the offset to write at */
    res = handle_set_offset(handle, offset);

    if (!res)
        return NULL;

    /* Do the actual write on the handle */
    res = handle_write(handle, bsize, buf);

    if (!res)
        return NULL;

    end_offset = NULL;

    /* Get the offset at the end of the write */
    res = handle_get_offset(handle, &end_offset);

    if (!res || offset > end_offset)
        return NULL;

    return (end_offset - offset);
}

BOOL device_enable(DEV_HANDLE handle)
{
    return device_send_ctl(handle, DEVICE_CTLC_ENABLE);
}

BOOL device_disable(DEV_HANDLE handle)
{
    return device_send_ctl(handle, DEVICE_CTLC_DISABLE);
}

BOOL device_send_ctl(DEV_HANDLE handle, enum DEVICE_CTLC code)
{
    return device_send_ctl_ex(handle, code, NULL, NULL, NULL);
}

BOOL device_send_ctl_ex(DEV_HANDLE handle, enum DEVICE_CTLC code, uintptr_t offset, VOID* buf, size_t bsize)
{
    if (syscall_5(SYSID_SEND_CTL, handle, code, offset, (uintptr_t)buf, bsize))
        return FALSE;
    return TRUE;
}

BOOL device_query_info(DEV_HANDLE handle, DEVINFO* binfo)
{
    return device_send_ctl_ex(handle, DEVICE_CTLC_GETINFO, NULL, binfo, sizeof(*binfo));
}

int init_devices()
{
    _cached_devices = NULL;
    return 0;
}
