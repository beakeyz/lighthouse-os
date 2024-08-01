#include "dev/video/core.h"
#include "dev/video/device.h"
#include "kevent/event.h"

/*
 * Overview of the video subsystem
 *
 * kevents:
 * There are a few kernel events that should indicate generic video related things that might happen at any
 * point in time (Think modesetting changes, connector changes, ect.)
 *
 * Objects:
 * The video subsystem will be represented as an object tree under %/Dev/video.
 * We'll have:
 *  - Gpu/ :: This will house the different video signal suppliers that are connected to the system
 *    - g0/ -> gn/ :: Gpu object nodes
 *      - c0 -> cn :: Gpu connector objects
 *  - Screen/ :: Here we put any screens that can display pixels. The objects attached here will have info
 *               about how the screens are connected to the video devices
 *    - s0 -> sn :: Screen objects. These hold references to the connector they are connected with
 */

void init_video()
{
    init_vdevice();

    /* Any events related to video devices */
    ASSERT(KERR_OK(add_kevent(VDEV_EVENTNAME, KE_DEVICE_EVENT, NULL, 128)));
}
