# The Prism backend driver

The GOTO language for the light operating system: Prism
We want it to both be able to act like a scripting language (like sh or bash on linux) and system utility/driver language. This means the language needs two things:

1) Simplicity and high ease of use: It needs to be user-friendly in order to enable easy environment customisation or task automation
2) Power: It needs to be able to interface very close to the hardware level, when it it used as a driver language

This is why we can't simply make a userspace compiler and be happy with it. This language should be the main way (besides just plain userspace binaries) to talk to
the system, as a human. 
This document acts as a reference manual for both the language and for the framework around it. We will both be outlining the syntax as well as the actual way the 
language is implemented on the kernel-side and how it should be implemented (or put to use) on the user side

## Driver model

Here is a simple 'diagram' of how the commands flow:

```
Userspace program that wants to ------->|
execute Prism code                      |
                                        |-----------------------|       
Kernel component that wants to  ------->| Prism backend driver  |------> Functions
execute Prism code                      |_______________________|       
```

The driver (like all things in the kernel) respects system resources, so when a userprocess is using a file, there should be no way that some process, executing Prism code, can
overwrite that. All is to say: Prism code should not get priority over regular binaries. The only benefit it has is that it's context is different, since it will be launched as
a separate Admin process in a separate thread.

## Language spec
## ???
