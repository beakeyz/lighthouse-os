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

## Internal vs. External

Aniva now allows for external loading of drivers. This means that we can support a wider range of devices and we can achieve 
a bigger modularity of functionality. 

Most internal drivers are things that we want the kernel to always be able to suport, like filesystems or basic firmware protocols 
for i.e. graphics (GOP, VESA). External drivers are means for devices that are more of a niche, like specific network adapters, or
HID devices, et cetera. When we switch into the first process (the kernel core process) after the initialization of most subsystems,
We load all the precompiled drivers that should be present on every system.

NOTE: It is super important that the kernel does not get bloated with precompiled drivers that can also be loaded at a later time. 
We should keep track of this, since not doing so might enlarge the kernel binary and/or affect boot times.

After the initial load of all the drivers is done, we should conduct a post loading check (plc) to ensure all systems are functioning
as they should. After that we do a few things to eventually get userspace up and running:

 1) Check the BASE profile for variables that map out the environment
 2) Enumerate those variables to load external drivers for this system
 3) Check if the BASE profile wants us to load a driver like kterm for 'userspace'
  - if yes, load it and we're pretty much up and running from that point
  - if no, we need to continue loading stuff and find the startup app
 4) Find the startup profile from BASE
 5) Launch the startup app that is associated with the startup profile under the var STARTUP_APP

After the userspace is running, drivers can be dynamically loaded and unloaded. Some might break the system, some might not but when certain
drivers are not working as expected, it's important that the option is always available with a minimal amount of volatility.

In the event of an unrecoverable driver failure, we pull a quick windows and confront the user with a beautiful screen that outlines 'exactly'
what went wrong and asks the user to restart =)
