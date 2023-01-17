# Lighthouse OS
Lighthouse OS is yet another operating system that is built cuz it 
can be built. It constits of a kernel called Aniva and a conseptual
userspace called LightEnv

yey, ANOTHER stinking kernel. As if the world needed another lmao

## Finished shit
- basically nothing, I am still working on everything at once xD
- well actually it boots, so I guess thats working

## TODOS:
- finish kmalloc
- figure out how to propperly setup my kernel pagetables, cuz they kinda funky atm xD
- apic / pic
- _\\___> interupts
- _\\___> device drivers
- vfs

## Build & Run

for building and running at the same time:
```bash
make debug
```

for building an ISO file:
```bash
make make-iso
```

for running the ISO:
```bash
make run-iso
```

In the future I will need to find a way to create a ramdisk/sysroot/apps and smash them together with the kernel
