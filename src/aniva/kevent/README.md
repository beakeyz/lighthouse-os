# Kernel events

Kernel events will be overseen by the kernel at all times, but they can be managed by the threads that 'create' them.

## Summary

When a thread wants to create an event, it goes to the kernel and it makes sure everything gets setup properly. Then,
the thread will receive a pipe, which will be registered in the kernel and is then bound to the thread (or driver or 
any other kobject). Other threads can then hook into this pipe, or eventline, by creating an eventregister which then
prompts the kernel that a threads wants to be part of that eventline. 

## Priority

A thread that is attached to an event-line, can request the kernel to place it higher up the chain. The thread will then
be first to be notified when an event goes through. This is handy for when there are a lot of threads on the pipe and one
thread has to have the data sooner than other threads for some reason. The kernel can deny this request however, leaving 
the thread as-is, or even deciding that it does not need the priority that is currently has and place it somewhere lower.

## Events

The thread that is in charge of the pipe can send kernel-events down for the listening threads. This makes broadcasting 
data between threads easier, as the data will only reach threads that are actively listening for that information. Events
are a form of one-way broadcasting, but in some cases, we may allow threads to alter the data in the pipeline.

## Data mutation

If a listening thread has permission, it may mutate the data coming in from the pipe for the threads coming after it. 
This means we can have a nice chain of operators working in a sort of factory-like fashion. This leads us to data handling.

## Data handling

When a thread is finished processing the data that is coming in (i.e. the handler function returns), the (mutated) data 
is passed on to the next thread in the chain. This thread might be doing some other stuff, so that will be paused when
there is data ready for it to be processed (We will check if that is the case every time we schedule into this thread).
After the thread has finished crunching all of its data, it proceeds with what it was doing. Note here that it is possible
for a thread to be busy with some data when there is already some new data coming in, from the same pipe, or a different one.
If that is the case, we should just queue it up and wait for it to be finished.

This also creates a potential problem, where one thread acts as a kind of choke-point because there might, for example,
be multiple event-lines running through it. When this happens, we could have the scheduler detect this bottleneck and then 
spawn load-balancer threads, with the sole purpose of dealing with the overflowing amount of data coming though this one thread. 
The best practise is to never let it come to that though.
