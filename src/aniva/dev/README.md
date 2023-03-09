# The Aniva driver-model

Drivers are basically just programs that run in kernel-mode. We give drivers access to everything in the kernel and thus 
they need to be written in the right way.

## Running

Drivers are given their own socket to operate on. This means they can recieve signals and respond to them, using the kAPI

## (Un)Loading

The kernel keeps a big list of the available drivers that it can load or unload. This list can grow, as other parts of the 
kernel or even user-mode programs can register new drivers. When they are loaded is discussed in the *Dependencies* tab

## Dependencies

Drivers can give up multiple dependencies that they rely on. This has to follow some rules, set by the kernel
(As of now, this feature is TODO)
When loading the driver, the kernel goes and tries to resolve and load the drivers listed. This means that the 
kernel can come pre-loaded with certain drivers, but these are not ACTUALLY loaded until they are needed somewhere 
(either by the kernel, or by another driver). When the kernel can't resolve a dependency, it finds out whether the kernel
can function without it. If this is not the case, it should crash until we find a way to work around this limitation =/

- TODO: work this out further
