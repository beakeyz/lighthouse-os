#ifndef __LIGHTENV_DRIVER__
#define __LIGHTENV_DRIVER__

#include "LibSys/system.h"
#include "LibSys/handle_def.h"

#include "ctl.h"

#define DRV_FLAG_CORE (0x00000001)
#define DRV_FLAG_HUB (0x00000002)
#define DRV_FLAG_STORAGE (0x00000004)
#define DRV_FLAG_USER (0x00000008)
#define DRV_FLAG_EXTERNAL (0x00000010)

/* TODO: complete this list */
#define DRV_CLASS_USB_HUB 1
#define DRV_CLASS_USB_DEV 2
#define DRV_CLASS_PCI_HUB 3
#define DRV_CLASS_PCI_DEV 4

/*
 * Get a handle to a driver
 *
 * @returns: FALSE on a fail, TRUE on success
 * @handle: buffer where the handle is placed. Caller should check if this is not HNDL_INVALID
 */
extern BOOL open_driver(
  __IN__ const char* name,
  __IN__ DWORD flags,
  __IN__ DWORD mode,
  __OUT__ HANDLE* handle
);

/*
 * Load a driver from a .drv file
 *
 * @returns: FALSE when the path is invalid or the load fails, 
 * TRUE on success
 * @path: path to the driver binary. Should always end in .drv, which
 * is the filetype for driver binaries in the aniva kernel
 */
extern BOOL load_driver(
  __IN__ const char* path,
  __IN__ DWORD flags,
  __OUT__ __OPTIONAL__ HANDLE* handle
);

/*
 * Unload a driver. Also closes the handle
 */
extern BOOL unload_driver(
  __IN__ HANDLE handle
);

extern BOOL driver_send_msg(
 __IN__ HANDLE handle,
 __IN__ DWORD code,
 __IN__ VOID* buffer,
 __IN__ size_t size
);

extern BOOL driver_query_info(
 __IN__ HANDLE handle,
 __OUT__ drv_info_t* info_buffer
);

#endif // !__LIGHTENV_DRIVER__
