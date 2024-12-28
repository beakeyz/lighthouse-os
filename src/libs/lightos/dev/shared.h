#ifndef __LIGHTOS_DEVICES_SHARED_STRUCTS__
#define __LIGHTOS_DEVICES_SHARED_STRUCTS__

#include <stdint.h>

/*
 * Enum with low level connection types of a device
 *
 * This enum describes how the host talks to a particular device
 */
enum DEVICE_CTYPE {
    DEVICE_CTYPE_UNKNOWN = 0,

    DEVICE_CTYPE_PCI,
    DEVICE_CTYPE_USB,
    DEVICE_CTYPE_SOFTDEV,
    DEVICE_CTYPE_CPU,
    DEVICE_CTYPE_I2C,
    DEVICE_CTYPE_GPIO,
    DEVICE_CTYPE_PS2,
    DEVICE_CTYPE_SATA,
    DEVICE_CTYPE_ATA,
    DEVICE_CTYPE_ACPI,

    DEVICE_CTYPE_OTHER
};

static inline const char* devinfo_get_ctype(enum DEVICE_CTYPE type)
{
    switch (type) {
    case DEVICE_CTYPE_UNKNOWN:
        return "Unknown";
    case DEVICE_CTYPE_PCI:
        return "PCI";
    case DEVICE_CTYPE_USB:
        return "USB";
    case DEVICE_CTYPE_SOFTDEV:
        return "softdev";
    case DEVICE_CTYPE_CPU:
        return "cpu";
    case DEVICE_CTYPE_I2C:
        return "I2C";
    case DEVICE_CTYPE_GPIO:
        return "GPIO";
    case DEVICE_CTYPE_PS2:
        return "PS/2";
    case DEVICE_CTYPE_SATA:
        return "Sata";
    case DEVICE_CTYPE_ATA:
        return "ATA";
    case DEVICE_CTYPE_ACPI:
        return "Acpi";
    case DEVICE_CTYPE_OTHER:
        return "Other";
    }

    return "N/A";
}

/*
 * Data block which a device fills when there is
 * a query for the deviceinfo
 */
typedef struct devinfo {
    char devicename[64];
    char manufacturer[48];

    /* Describe the origins of this devce */
    uint16_t vendorid;
    uint16_t deviceid;
    uint16_t class;
    uint16_t subclass;

    uint8_t endpoint_flags;

    /* Some higher level description(s) */
    enum DEVICE_CTYPE ctype;

    /* Device specific data */
    uint32_t dev_specific_size;
    void* dev_specific_info;
} devinfo_t, DEVINFO;

/*
 * Device control codes
 *
 * Big enum which describes every possible control code that can be sent to
 * any device. Devices may export a control table, which tells any client
 * which controlcodes are implemented by the device driver(s)
 *
 * TODO: Replace with regular device endpoints
 *
 * The problem with endpoints is that they want entire subsystems of a single device to be
 * implemented by a single device driver. It also lacks flexibility in terms of overlapping
 * device classes
 */
enum DEVICE_CTLC {
    DEVICE_CTLC_NONE,

    /* Generic device control codes */
    DEVICE_CTLC_CREATE,
    DEVICE_CTLC_DESTROY,
    DEVICE_CTLC_DISABLE,
    DEVICE_CTLC_ENABLE,
    DEVICE_CTLC_WRITE,
    DEVICE_CTLC_READ,
    DEVICE_CTLC_ALLOC, // Allocate memory inside the devices local memory
    DEVICE_CTLC_DEALLOC, // Deallocate memory inside the devices local memory
    DEVICE_CTLC_FLUSH,
    DEVICE_CTLC_GETINFO, // Fills a generic DEVINFO structs
    DEVICE_CTLC_ENABLE_SUBSYS,
    DEVICE_CTLC_DISABLE_SUBSYS,
    DEVICE_CTLC_SET_POLL_FREQ,
    DEVICE_CTLC_GET_CLK_FREQ,

    /* Power control codes */
    DEVICE_CTLC_POWER_ON,
    DEVICE_CTLC_POWER_OFF,
    DEVICE_CTLC_POWER_LO,
    DEVICE_CTLC_SUSPEND,
    DEVICE_CTLC_RESUME,
    DEVICE_CTLC_REMOVE,
    DEVICE_CTLC_GET_PWR,

    /* HID control codes */
    DEVICE_CTLC_HID_POLL,

    /* Volume control codes */
    DEVICE_CTLC_VOLUME_BREAD,
    DEVICE_CTLC_VOLUME_BWRITE,
    DEVICE_CTLC_ATA_IDENTIFY, // Executes an ATA identify command

    /* Video control codes */
    DEVICE_CTLC_GET_FBINFO,

    /* Device event codes */
    DEVICE_CTLC_KEVENT_POLL,
    DEVICE_CTLC_KEVENT_AWAIT_FIRE,
    DEVICE_CTLC_KEVENT_FIRE,

    /* The amount of control codes there are */
    N_DEVICE_CTLC
};

#endif // !__LIGHTOS_DEVICES_SHARED_STRUCTS__
