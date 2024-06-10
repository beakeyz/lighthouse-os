#include "pipe.h"
#include "mem/heap.h"

usb_pipe_t* create_usb_pipe(struct usb_pipe_ops* ops, struct usb_hcd* hcd)
{
    usb_pipe_t* ret;

    if (!ops || !hcd)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->ops = ops;
    ret->hcd = hcd;

    return ret;
}

void init_usb_pipe(
    usb_pipe_t* pipe, void* priv, enum USB_PIPE_TYPE type, enum USB_SPEED speed,
    uint8_t d_addr, uint8_t ep_addr, uint8_t hub_addr, uint8_t hub_port, uint8_t direction,
    uint8_t interval, uint8_t max_burst, size_t max_transfer_size)
{
    if (!pipe)
        return;

    pipe->priv = priv;

    pipe->type = type;
    pipe->speed = speed;
    pipe->dev_addr = d_addr;
    pipe->endpoint_addr = ep_addr;
    pipe->hub_addr = hub_addr;
    pipe->hub_port = hub_port;
    pipe->direction = direction;
    pipe->interval = interval;
    pipe->max_burst = max_burst;
    pipe->max_transfer_size = max_transfer_size;
}

void destroy_usb_pipe(usb_pipe_t* pipe)
{
    if (pipe->ops && pipe->ops->f_destroy)
        ASSERT(pipe->ops->f_destroy(pipe));

    kfree(pipe);
}
