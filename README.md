DISCLAIMER: Most of this code is very old and even more of it is built with lacking knowledge of the underlying systems. If reading this ends up giving you an aneurysm, feel free to open an issue about it, but I am not responsible for any brain damage you might end up getting xD

# Lighthouse OS

Lighthouse OS is yet another operating system that is built cuz it 
can be built. It constits of a kernel called Aniva and a userspace
that I've very creatively called LightEnv

yey, ANOTHER stinking kernel. As if the world needed one lmao

## Finished shit

 - Boots (miracle)
 - Runs shit (another miracle)
 - Memory is managed
 - Two implementations for in-kernel heaps
 - Logging system
 - Driver framework
 - Dynamic driver loading/unloading/linking
 - Filesystems that go brrrr
 - insane assembly (fr ong)
 - Working ring 3
 - libc implementation
 - Our own custom userspace libraries (yay)
 - A kernel terminal driver (kterm)
 - Quite a lot actualy, more than I thought...
 - 

I am so insane at naming shit, holy fuck

## TODOS:

 - kevents: fun idea, probably quite usefull, but we need to create a full mental image of how this should go AND code the thing together =))))
 - USB drivers: help me
 - HID drivers: help me ^ 2
 - NET drivers: I don't even have to say it
 - Just a lot of drivers really
 - A working windowmanager that is a lil fast
 - Video drivers to support a lil fast windowmanager
 - Drivers kill me
 - 

## Build & Run

Before you can build/run anything, you need to build and install the custom
toolchain for this kernel. This is done by running the `build-cross.sh` script
which will download and extract the sources and then prompt the user a few times
to get shit up and running. This process can take a while, but there might be prompts
that need your attention, so leaving it completely alone is not recommended.

### Build

For building this project there is really only one option (the makefile needs to either be edited or removed lol):
In the ./project folder, there is an executable python script called x.py which
you can run from the project root (That would become `./project/x.py` or something) 
and opens a cute dialog window for easy tooling =) (Don't look at the python code please)

### Run

If you want to try this shit out, make sure you have QEMU installed to PATH on your device. Running this 
has two main ways of doing it (Option 2 is better in my unbiased oppinion):
 1) Using the Makefile: run `make iso` to create a .iso file and `make run` to launch QEMU with the grub bootloader into our kernel
 2) Using light-loader: The preferred way to run this shit is by using the bootloader that is designed for the OS. Advanced instructions are
    found in the light-loader git repo, but it comes down to copying the kernel and ramdisk into the folder of the bootloader and running the
    bootloader

