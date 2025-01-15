# OSS (Object Storage System)

A single subsystem that manages path-organised systems like filesystems, events, drivers, devices, ect.

Our kernel needs a way to present its resources and possibilities in a pleasant manner to its processes, just 
like any kernel. Systems in the past have done this through the philosophy of 'everything is a file' like Unix.
Unix-like systems provide a unified vfs through which pretty much any part of the system is accessible. Windows
does... Something? They are weird imo. We're going to attempt to Modernize the idea of a vfs, since we're in 2025
at the time of writing this and I don't think we need to pretend my keyboard is actually a file, even though it
is a super elegant way to generalize the system into a consistent whole.

## System layout

NOTE 1: This file has been restructured on the 12th of january; 2025.
NOTE 2: This system acts as the border between userspace and kernel objects like files or drivers, which is why we need this
guy to be as stable as possible.


