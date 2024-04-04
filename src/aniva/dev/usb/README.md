# The aniva USB subsystem

USB is not that hard, but when I started to write this subsystem, I had no idea how to structure it. Turns out it comes down to a few easy concepts.
Boxes with USB ports tend to have USB controllers on their motherboards (hopefully). These are what we call HCs (Host controllers). We talk to the USB
devices through these controllers, which we can find on the PCI bus. When a specific HC is getting initialized, there are a few important points:

1) We need to keep note of the protocols, speeds and bandwidths that the HC supports
2) We need to create a 'device' on the usb device group for this HC with a unique name
3) We need to correctly enumerate all the devices and hubs on this HC and assign them both device addresses AND device objects

When I wrote this subsystem, I did not yet have such scaffolding for devices, but now we do. This means we can start doing meaningful parsing of the HCs
we encounter while scanning the PCI bus, because we can now put the devices we find in a correct hierarchy with the relevant private data assigned to them.

After all the HCs are parsed to our content, we should have a situation where trying to talk to a specific device object results in correct commands to the HC
and the desired results of those commands. The logical next step is to load the class drivers which depend on this fact, like mass-storage drivers, or HID drivers.
These drivers themselves walk the device tree we just created, in stead of the raw HC. These drivers should then (TODO) be able to mutate the devices
in such a way to implement new endpoints that extend the devices capabilities. In the case of a mass-storage driver for example, it might have just found
a matching device on the usb device tree and initialized it. After this, in order for the rest of the system to be able to use the block device-like capabilities
of the device, the driver needs to let the device implement the function it knows of how to talk to the HC in such a way to get block I/O going. The flow would look
like this:

```
User/System/whatever -> device -> endpoints -> mass-storage driver -> HC driver -> USB hardware
```

This also gives us a little problem though. This device we're talking about it both a disk device as well as a USB device. We currently don't really allow for this,
since a device object only has a singular private field. To solve this (and to also solve the idea of device mutability) we'll need to change device endpoints to be
a linked list of endpoint objects which all can have their own private fields. So,

```
device <- usb_device <- disk_device
  |
endpoint 0
  |
endpoint n
```

Which is not currently possible, would become

```
device <- usb_device
  |
disk_endpoint <- disk_device
```

This is because at it's core, the device is still a USB device, but it implements features of a disk device. This is how we need to think about devices under aniva.
