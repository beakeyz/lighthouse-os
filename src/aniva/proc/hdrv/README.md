# Default Aniva handle drivers

Aniva exposes it's kernel objects to userspace via its OSS (object storage system) and it hands out handles to threads
which act as keys to kernel handles. Userspace processes can then execute operations directly on these handles, which 
propegate through the system and eventually land at the core implementation (i.e. Reading from or writing to a file).

## The current (Old) system

What we have been doing, is simply having a giant switch statement for every syscall where the caller wants to act on a handle.
This means there is a lot of code duplication and debugging and maintaining becomes very hard

## Handle driver implementation as a solution

With handle drivers, we offer users an even greater degree of flexibility, because we can essentially plug different operations
to different handle types. The handle type HNDL_TYPE_FILE, might point to the default handle driver during boot, but the user might
decide to override certain functions or even choose to swap out the entire handle driver. Furthermore, syscall stubs will look much
cleaner and there won't be any need anymore for large spaghetti code in those blocks. 

The implementation will be the following:
For every handle type, there is a slot for a handle driver. This driver will implement functions like: Open, Close, Read, Write, ect.
This means that when userspace want to perform an operation on a handle, we will find the driver associated with the handle type and
we can pass the operation request directly to it. In the case that we don't find a driver for the specified type, we can simply let
userspace know that these types of handles are not supported by the system, in it's current state.
