# Our different allocators

TODO: lay out what allocators are available here

## Kernel heap

The kernel heap is setup in heap.c and every kernel allocation goes through that.
It is currently a very bad implementation of a linked list heap, but I want to improve it later in the future.
It works right now by giving it 2 Mib of pre-reserved memory in the kernel binary that can be fully utilised by
allocations. Dynamic allocation still does not work for that...

## Memory allocator (malloc)

The default memory allocator is a linked list allocator that is quite beefy in its design. Every 'node' takes
up a chunk of allocated memory that starts off with a header that contains information about the node. (size, 
flags, ect.) The actual memory is located directly after the header, meaning that we could potentially have 
a piece of code alter information in this header, simply by offsetting the address of the allocation, that it
receives from the allocator, by a certain amount. This is one of the many issues this implementation has, so 
a **major** TODO is to reimplement that allocator

## Zone allocator (zalloc)

This allocator works by splitting up the memory blocks into zones. Every zone is registered in a 'zone store',
which is simply a few pages (or one page) that we have reserved purely for storing addresses to zones. This means
That a store of one page can hold
```4096 / 8 = 512 zones```
minus the size of the zone-store header that contains information about the store. This implementation means that 
we can endlessly keep adding zones to our allocator, but the downside is that we are limited to only a few allocation-
sizes. We could potentially overcome this by spreading allocations over multiple zone-sizes, for example, by having one allocation
of let's say 32 bytes take up 4 entries in a zone of an entry-size of 8. This has an advantage over storing it in a zone which 
has entry-sizes of 64 bytes, since then you are wasting the other 32 bytes. The challenge we face here is: How do we
store information about an allocation like this efficiently, so that it is beneficial for us to use this method over 
just dumping it in a 64-byte zone...

