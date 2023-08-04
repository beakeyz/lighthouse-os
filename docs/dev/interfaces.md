
# Device Interfaces

Per device-type, we want to be able to provide as much in-house support as possible. This is why we 
implement interfaces per device-type that will provide a basic standardised way to communicate with the hardware.
These interfaces themselves don't know anything about the hardware, which is why they themselves talk to drivers 
that can be installed and loaded by these interfaces.
