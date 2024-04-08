#ifndef __ANIVA_USB_PIPE__
#define __ANIVA_USB_PIPE__

#include "dev/usb/usb.h"
#include "libk/flow/error.h"
#include <libk/stddef.h>

struct usb_pipe_ops;
struct usb_hcd;
struct usb_xfer;

enum USB_PIPE_TYPE {
  USB_PIPE_TYPE_CONTROL = 0,
  USB_PIPE_TYPE_INTERRUPT,
  USB_PIPE_TYPE_BULK,
  USB_PIPE_TYPE_ISO,
};

#define USB_PIPE_OUTGOING 0x00
#define USB_PIPE_INCOMMING 0x01

/*
 * This beast makes sure that any data we want to get to a specific device also
 * gets where we want it to go. Pipes communicate to devies for us with the HCD as
 * their bridge.
 *
 * Pipes are created by the HCD when it finds a slot/port (???) with a device attached.
 * after the pipe is initialised, the HCD first sends a device identify request. The result
 * of this is used to map the device on the device tree. Any other pipes are created lazily (???)
 */
typedef struct usb_pipe {
  uint8_t dev_addr;
  uint8_t endpoint_addr;

  uint8_t hub_addr;
  uint8_t hub_port;

  uint8_t direction;
  uint8_t interval;
  uint8_t max_burst;

  size_t max_transfer_size;

  enum USB_PIPE_TYPE type;
  enum USB_SPEED speed;

  void* priv;

  struct usb_hcd* hcd;
  struct usb_pipe_ops* ops;
} usb_pipe_t;

usb_pipe_t* create_usb_pipe(struct usb_pipe_ops* ops, struct usb_hcd* hcd);
void init_usb_pipe(
    usb_pipe_t* pipe, void* priv, enum USB_PIPE_TYPE type, enum USB_SPEED speed,
    uint8_t d_addr, uint8_t ep_addr, uint8_t hub_addr, uint8_t hub_port, uint8_t direction, 
    uint8_t interval, uint8_t max_burst, size_t max_transfer_size);

void destroy_usb_pipe(usb_pipe_t* pipe);

typedef struct usb_pipe_ops {
  /* Tries to submit a request to a pipe @pipe. Fails if the request type and the pipe type mismatch */
  kerror_t (*f_enqueue)(usb_pipe_t* pipe, struct usb_xfer* request);

  kerror_t (*f_destroy)(usb_pipe_t* pipe);
} usb_pipe_ops_t;

#endif // !__ANIVA_USB_PIPE__
