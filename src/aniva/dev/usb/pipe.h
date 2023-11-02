#ifndef __ANIVA_USB_PIPE__
#define __ANIVA_USB_PIPE__

#include <libk/stddef.h>

enum USB_PIPE_TYPE {
  USB_PIPE_TYPE_STREAM = 0,
  USB_PIPE_TYPE_MESSAGE,
};

/*
 * TODO: structure
 */
typedef struct usb_pipe {
  enum USB_PIPE_TYPE type;
  uint32_t bandwidth;
  uint32_t transfer_type;
} usb_pipe_t;

#endif // !__ANIVA_USB_PIPE__
