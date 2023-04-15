# Lighthouse OS

Lighthouse OS is yet another operating system that is built cuz it 
can be built. It constits of a kernel called Aniva and a conseptual
userspace called LightEnv

yey, ANOTHER stinking kernel. As if the world needed another lmao

## Finished shit

Too lazy

## TODOS:

- [0 ] Libk/LibEnv: A RO reference to memory
    We could have some kind of mechanism with which we can
    create a reference to some memory, but we create a kind
    of mirror of the actual memory, which gets updated every
    time the real memory changes. This could be handy for 
    userspace apps that need to be able to read certain kernel
    memoryblocks, without them being able to modify the memory.

    ```
              (read)                (read/write)
    userspace   ->   mirror reference   <->   kernel
    ```

    In short, some kind of intermediate state for memory-blocks,
    where only one party can write meaningful information to the 
    block. THIS MEANS THE KERNEL SHOULD NEVER DIGEST ANY DATA IN
    THE MIRROR as it could be contaminated with stuff the user-
    space put in there...

## Build & Run

Either use python project utilities,
or the makefile located in the project root

