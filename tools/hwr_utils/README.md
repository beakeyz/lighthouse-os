# How do we map devices to drivers?

(Concept)

Hardware register files =o

We can have files which store data in the following format:
<device type>##<device id> : <driver path>

For example, let's say we have an nvidia GPU on the PCI bus with the device ID 0x10de for nvidia PCI devices. The entry
inside an .hwr file as described above would become:
pci_dev##10de : Storage/Root/System/nvidia.drv

Now I don't know what would be better here, since we have two options:
1) Compile the format described above into a binary format, much like ELF
2) Write a parser that can interpret the format above directly

## Advantages and disadvantages

### Option 1

It seems like option 1 has a lot of extra complexity attached, since we need a concrete outline of the new file format, a 
compiler to translate 'sourcefiles' to .hwr files and a kernel-side interpreter of the binary files. This does come with the 
advantage that we can catch format errors at compile-time and we can ensure that the file which the kernel gets (assuming it 
does not get touched in the meantime) a format which is correct. This makes option 1 probably faster at runtime than option 2

### Option 2

This option is rather simple. We only need the kernel to be able to recognise and interpret the format above correctly. Drawbacks
are that storing the format like this makes it quite chunky and probably slow. 
