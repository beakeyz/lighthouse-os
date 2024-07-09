# Driver component of lightos userpipes (UPI -> User Pipe Interface)

This driver manages pipe lifetimes and transactions over userpipes

## TODOs

- Graceful cleanup of leftover pipes on preemptive driver unload
- Handle signals sent over the pipes
- No longer block the entire driver when a process wants to do a pipe operation, only lock needed bits
- Fix pipe lifetimes lmao
