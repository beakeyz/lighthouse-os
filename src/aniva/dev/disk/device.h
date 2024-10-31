#ifndef __ANIVA_DISK_VOLUME_DEVICE_H__
#define __ANIVA_DISK_VOLUME_DEVICE_H__

#include <dev/device.h>
#include <volumeio/shared.h>

struct volume;

/*
 * Standard volume opperations
 */
typedef struct volume_dev_ops {
    f_device_ctl_t f_read;
    f_device_ctl_t f_write;
    f_device_ctl_t f_bread;
    f_device_ctl_t f_bwrite;
    f_device_ctl_t f_ata_ident;
    f_device_ctl_t f_flush;
    f_device_ctl_t f_getinfo;
} volume_dev_ops_t;

#define VOLUME_DEV_FLAG_READONLY 0x00000001
#define VOLUME_DEV_FLAG_HIDDEN 0x00000002
#define VOLUME_DEV_FLAG_BUFFER 0x00000004

typedef struct volume_device {
    /* Parent device. A volume device doesn't get a parent until it is registered */
    device_t* dev;

    /*
     * A volume devices ID
     * This is used to generate a device name
     * ID=4 would give name="vd5" for "volume device 5"
     * this is in line with volumes, who get "v[id]" instead of "vd[id]"
     */
    u32 id;
    /* Some device flags, specific to volume devices */
    u32 flags;

    /* Volume info block */
    volume_info_t info;
    volume_dev_ops_t* ops;

    /* A volume device (usually) contains volumes */
    size_t nr_volumes;
    struct volume* vec_volumes;

    /* Private driver stuff */
    void* private;
} volume_device_t;

volume_device_t* create_volume_device(volume_info_t* info, volume_dev_ops_t* ops, u32 flags, void* private);
void destroy_volume_device(volume_device_t* volume_dev);

int volume_dev_set_info(volume_device_t* device, volume_info_t* pinfo);

int volume_dev_read(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size);
int volume_dev_write(volume_device_t* volume_dev, u64 offset, void* buffer, size_t size);
int volume_dev_flush(volume_device_t* volume_dev);

#endif // !__ANIVA_DISK_VOLUME_DEVICE_H__
