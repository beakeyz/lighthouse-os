# OSS (Object Separation System)

A single subsystem that manages path-organised systems like filesystems, events, drivers, devices, ect.

NOTE: this is a concept, it's not yet ready to replace the current classic vfs

## System layout

During system runtime it is possible to register oss_nodes that function as the root for a specific type of object. Where a 
typical vfs consisted of nodes that 'generate' vobjects, we will take a little different approach here.

Take the user-available filesystem for example. The oss core will now expose an api similar to vfs_resolve (Which would simply walk
the vfs tree until it found a node it could query for a vobj) which walks the oss nodes. We want to easily support relative paths,
so we'll make sure that there is a good API for that. When dealing with absolute paths the following rules apply:

Consider a path that points to a random file on disk. It would look something like this:

```:/Root/System/kterm.drv```

Let's look at how the path is structured:
 - :/ : Specifies that this is an absolute path on the local system
 - Root: The first 'token' of an oss path specifies which oss_node to talk to
 - System/kterm.drv: The oss_node at Root implements a filesystem, so we'll directly pass the rest of the path to this filesystem so it can give us a vobj

This was an example of a classical filesystem access that simply asks for the location of a certain file. Now what if we want
to access a particular process-provided event? No problem! We can do it with this path:

```:/Events/<proc name>/<event name>```

Structure:
 - :/ : Absolute path specifier
 - Events: The events oss_node. Since this does not specify a autonomous filesystem, we need to keep walking the oss nodes
 - <proc name> : process-provided events are grouped by process name
 - <event name> : and finally the actual eventname we're interested in. This will be vobj that's directly stored on the oss_node

 Following this logic, here are a few more quick examples of valid paths:
 (Direct driver access) : ':/Drv/<driver category>/<driver name>'
 (Direct device access) : ':/Dev/<driver category>/<driver name>/<device name>'
