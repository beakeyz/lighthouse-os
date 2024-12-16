# In-kernel tests

The kernel can be booted in test mode, where it will try to execute a vector of tests after boot but before any
userspace is loaded. If there aren't any errors, userspace will be launched. If there where errors, the user
will be presented with an overview of which errors occured. They can then choose whether to proceed, or abort the boot.
