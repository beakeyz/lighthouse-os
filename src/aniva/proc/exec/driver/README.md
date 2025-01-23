# Executable drivers lol (.drv)

Crazy thought; what if we make .drv files executable? A process with the right privileges can then simply instruct the kernel
to execute a driver file, which will then get loaded. This does mean that we will need to make drivers a kind of process...

## Drivers as a kind of process

It seems logical actually, that drivers can be represented by processes. They essentially are super-privileged processes that may provide 
certain services to both the kernel and to the userspace. They should also be able to enjoy all the infrastructure that (should, TODO lol) 
exist for processes. Think about IPC, exported memory buffers, system variables, ect. 

A driver process would only live in the scheduler for a very short time: only the time it takes for the driver to get itself setup is getting
spent in the scheduler. After that, the initialization thread of the driver process will exit, but the kernel won't murder this process yet, because
the driver object should still hold a reference to the process object, by connection. It is only when the driver object is killed, that the
process will join its fate. This also means system variables on the driver process will live as long as the driver object lives.

## Security

This probably is anything but secure. I just think its a nice display of the flexibility of the new oss. It also paves the way for more direct
communication between userspace and drivers. This means we might (in the future) be able to send commands to devices from userspace, pretty much
instantly, since we won't have to go through the entire kernel pipeline in order to talk to devices. We can simply open an exported memory buffer
of a driver processes and write certain commands there, or just send an IPC packet to the driver process. It can then directly decide what to do
with it
